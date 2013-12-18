#include <stdlib.h>

/*
 * mrfIoc2 headers
 */

#include <mrf/object.h> //mrm::Object
#include <evgMrm.h> //evgMrm
#include <drvem.h> //EVRMRM
#include <os/default/mrfIoOpsDef.h>

/*
 *  EPICS headers
 */
#include <iocsh.h>
#include <drvSup.h>
#include <epicsExport.h>
#include <epicsEndian.h>
extern "C"{
#include <regDev.h>
}


#define DBUFF_LEN 2048
#define PROTO_LEN 4

/*
 * mrfDev reg driver private
 */

struct mrfiocDBuffDevice{
	char* 				name;			//regDevName of device
	epicsBoolean 		isEVG;			//epicsTrue if card is EVG
	epicsUInt8* 	    txBufferBase;	//pointer to card memory space, retrieved from mrfioc2
	epicsUInt8* 	    dbcr;			//pointer to DBCR (TXDBCR on EVR) register of mrfioc card
	epicsUInt8			DBEN;			//set to 1 if DBus is shared with data transmission
	epicsUInt32			proto;			//protocol ID (4 bytes)
	epicsUInt8*			txBuffer; 		//pointer to 2k tx buffer
	size_t				txBufferLen; 	//amount of data written in buffer (last touched byte)
	epicsUInt8*			rxBuffer; 		//pointer to 2k rx buffer
	IOSCANPVT			ioscanpvt;
	mrfiocDBuffDevice*	next;			//null if this is the last device
};

static mrfiocDBuffDevice* devices=0;

void mrfiocDBuff_report(regDevice* pvt, int level){
	mrfiocDBuffDevice* device = (mrfiocDBuffDevice*) pvt;
	printf("\t%s dataBuffer is %s. buffer len 0x%x\n",device->name,device->isEVG?"EVG":"EVR",(int)device->txBufferLen);
}

//TODO: move this defines...
#define DBCR				0x20	//Data Buffer Control Register
#define TXDBCR				0x24	//Equivalent to DBCR on EVG
#define DBCR_TXCPT_bit		(1<<20)	//tx complete (ro)
#define DBCR_TXRUN_bit		(1<<19) //tx running (ro)
#define DBCR_TRIG_bit		(1<<18) //trigger tx (rw)
#define DBCR_ENA_bit		(1<<17) //enable data buff (rw)
#define DBCR_MODE_bit		(1<<16) //DBUS shared mode

static void mrfiocDBuff_flush(mrfiocDBuffDevice* device){

#if EPICS_BYTE_ORDER == EPICS_ENDIAN_LITTLE
	//Copy protocol ID
	*((epicsUInt32*) device->txBuffer) = bswap32(device->proto); //Since everything in txBuffer is big endian we need to convert proto ID
	//The data can now be copied onto cards memory, but we need to copy 4-bytes at the time and
	//convert endianess again (since PCI converts BE-LE on per word (4b) basis)
	regDevCopy(4,device->txBufferLen/4,device->txBuffer,device->txBufferBase,0,1);
#else
	//Copy protocol ID
	*((epicsUInt32*) device->txBuffer) = device->proto;
	regDevCopy(4,device->txBufferLen/4,device->txBuffer,device->txBufferBase,0,0);
#endif

	//Enable data buffer
	epicsUInt32 dbcr = (DBCR_ENA_bit | DBCR_MODE_bit | DBCR_TRIG_bit);

	//Set buffer size
	device->txBufferLen = (device->txBufferLen%4)==0 ? device->txBufferLen : device->txBufferLen+4;
	dbcr |= device->txBufferLen;

	//Output to register and trigger tx
	nat_iowrite32(device->dbcr,0);
	nat_iowrite32(device->dbcr,dbcr);

	dbcr = nat_ioread32(device->dbcr);

	printf("DBCR: 0x%x\n",dbcr);
	printf("complete: %d\n",dbcr & DBCR_TXCPT_bit ? 1 : 0);
	printf("running: %d\n",dbcr & DBCR_TXRUN_bit ? 1 : 0);

}


/*
 * Read will make sure that the data is correctly copied into the records.
 * Since data in MRF DBUFF is big endian (since all EVGs are running on BE
 * systems) the data may need to be converted to LE. Data in rxBuffer is
 * always BE.
 */
int mrfiocDBuff_read(
		regDevice* pvt,
		size_t offset,
		unsigned int datalength,
		size_t nelem,
		void* pdata,
		int priority)
{

	printf("mrfiocDBuff_read: from 0x%x len: 0x%x\n",(int)offset,(int)(datalength*nelem));
	mrfiocDBuffDevice* device = (mrfiocDBuffDevice*) pvt;

	if(device->isEVG){
		errlogPrintf("mrfiocDBuff_read: FATAL ERROR, EVG does not have RX capability!\n");
		return -1;
	}

	if(offset+datalength*nelem > 2048){
		errlogPrintf("mrfiocDBuff_read: READ ERROR, address out of range!\n");
		return -1;
	}

	if(offset < PROTO_LEN){
		errlogPrintf("mrfiocDBuff_read: READ ERROR, address out of range!\n");
		return -1;
	}

#if EPICS_BYTE_ORDER == EPICS_ENDIAN_LITTLE
	printf("EPICS_ENDIAN_LITTLE: converting endianess!\n");
	regDevCopy(datalength,nelem,&device->rxBuffer[offset],pdata,0,1);
#else
	regDevCopy(datalength,nelem,&device->rxBuffer[offset],pdata,0,0);
#endif


	return 0;
}

int mrfiocDBuff_write(
		regDevice* pvt,
		size_t offset,
		unsigned int datalength,
		size_t nelem,
		void* pdata,
		void* pmask,
		int priority)
{
	mrfiocDBuffDevice* device = (mrfiocDBuffDevice*) pvt;

	if (!device->txBuffer) {
		errlogPrintf(
				"mrfiocDBuff_write: FATAL ERROR! txBuffer not allocated!\n");
		return -1;
	}

	/*
	 * We use offset 0 (that is illegal for normal use since it is occupied by protoID)
	 * to flush the output buffer. This eliminates the need for extra record..
	 */
	if(offset==0){
		mrfiocDBuff_flush(device);
		return 0;
	}

	if (offset + datalength * nelem >= DBUFF_LEN) {
		printf(
				"mrfDBuffWriteMaskedArray(): byte offset out of range for %s regDevDriver\n",
				device->name);
		return -1;
	}
	if (offset < PROTO_LEN) {
		printf(
				"mrfDBuffWriteMaskedArray(): device %s : byte offset must be greater than %d\n",
				device->name, PROTO_LEN);
		return -1;
	}

	size_t size = datalength*nelem;
	size_t last_byte = size + offset;

	//Copy into the scratch buffer
#if EPICS_BYTE_ORDER == EPICS_ENDIAN_LITTLE
	regDevCopy(datalength,nelem,pdata,&device->txBuffer[offset],0,1);
#else
	regDevCopy(datalength,nelem,pdata,&device->txBuffer[offset],0,0);
#endif
	//Update buffer length
	device->txBufferLen = last_byte;//(device->txBufferLen > last_byte) ? device->txBufferLen : last_byte;

	return 0;
}


IOSCANPVT mrfiocDBuff_getInIoscan(regDevice* pvt, size_t offset){
	mrfiocDBuffDevice* device = (mrfiocDBuffDevice*) pvt;

	if(!device->txBufferBase){
		errlogPrintf("mrfiocDBuff_getInIoscan: FATAL ERROR, device not initialized!\n");
		return NULL;
	}

	return device->ioscanpvt;
}



//RegDev device definition
static const regDevSupport mrfiocDBuffSupport={
		mrfiocDBuff_report,
		mrfiocDBuff_getInIoscan,
		NULL,
		mrfiocDBuff_read,
		mrfiocDBuff_write
};

/**
 * Traverse list of devices and return first
 * one that matches regDevName. Returns null
 * if no devices are found
 */
static mrfiocDBuffDevice* getDevice(const char* regDevName){
	if(!devices) return 0;

	mrfiocDBuffDevice* device=devices;

	while(1){
		if(!strcmp(device->name,regDevName)) return device;
		if(!device->next) return 0;
		device=device->next;
	}
}

/**
 * Appends device to end of the global device list
 */
static void addDevice(mrfiocDBuffDevice* deviceToAdd){
	if(!devices){
		devices=deviceToAdd;
		return;
	}

	mrfiocDBuffDevice* device = devices;
	//transvere to the end of list
	while(1){
		if(device->next) device=device->next;
		else break;
	}

	device->next=deviceToAdd;
}

//callback
static void mrmEvrDataRxCB(void *pvt, epicsStatus ok, epicsUInt8 proto,
            epicsUInt32 len, const epicsUInt8* buf)
{
	mrfiocDBuffDevice* device = (mrfiocDBuffDevice *)pvt;

	//Reconstruct the buffer, we will handle protocols separately since PSI legacy systems use 4bytes for proto id
	epicsUInt8 tmp[2048];
	tmp[0]=proto;
	memcpy(&(tmp[1]),buf,len);

	// Extract protocol ID
	epicsUInt32 receivedProtocolID = *(epicsUInt32*)(tmp);

	//Another tiny hack, mrfioc2 automatically converts endianess so we have to convert it back
#if EPICS_BYTE_ORDER == EPICS_ENDIAN_BIG
	receivedProtocolID = bswap32(receivedProtocolID);
#endif

	printf("Received DBUFF with protocol: %d\n",receivedProtocolID);

	// Accept all protocols if devices was initialized with protocol == 0
	// or if the set protocol matches the received one
	if(!device->proto | (device->proto == receivedProtocolID));
	else return;


	printf("Received new DATA!!! len: %d\n",len);
	// Do first copy swap. Since buffer is read 4 bytes at they have to be swapped.
	// This is a hack so that mrfioc2 doesn't have to be modified...
	// After this copy, the contents of the buffer are the same as in EVG
	regDevCopy(4,len/4,tmp,device->rxBuffer,0,1); //TODO: length sanity check

		int i;
		for(i=0;i<20;i++){
			printf("0x%x ",device->rxBuffer[i]);
		}
		printf("\n");


	scanIoRequest(device->ioscanpvt);


}

/*
 * Initialization, this is the entry point.
 * Function is called from iocsh. Function will try
 * to find desired device (mrfName) and attach mrfiocDBuff
 * support to it.
 *
 * Args:Can not find mrf device: %s
 * 		regDevName - desired name of the regDev device
 * 		mrfName - name of mrfioc2 device (evg,evr,...)
 *
 */
struct regDevice{};

static void mrfiocDBuff_init(const char* regDevName, const char* mrfName, int protocol){
	//Check if device already exists:
	if(getDevice(regDevName)){
		errlogPrintf("mrfiocDBuff_init: FATAL ERROR! device %s already exists!\n",regDevName);
		return;
	}

	mrfiocDBuffDevice* pvt;

	/* Allocate all of the memory */
	pvt = (mrfiocDBuffDevice*) malloc(sizeof(mrfiocDBuffDevice));
	if(!pvt){
		errlogPrintf("mrfiocDBuff_init: FATAL ERROR! Out of memory!\n");
		return;
	}

	pvt->txBufferLen = 0;
	pvt->txBuffer = (epicsUInt8*) malloc(2048); //allocate 2k memory

	if(!pvt->txBuffer){
		errlogPrintf("mrfiocDBuff_init: FATAL ERROR! Could not allocate TX buffer!");
		return;
	}

	pvt->rxBuffer = (epicsUInt8*) malloc(2048); //initialize to 0

	if(!pvt->rxBuffer){
			errlogPrintf("mrfiocDBuff_init: FATAL ERROR! Could not allocate RX buffer!");
			return;
	}

	scanIoInit(&pvt->ioscanpvt);

	/*
	 * Query mrfioc2 device support for device
	 */
	printf("Looking for device %s\n", mrfName);
	mrf::Object *obj = mrf::Object::getObject(mrfName);

	if(!obj){
		errlogPrintf("mrfiocDBuff_init: FAILED! Can not find mrf device: %s\n",mrfName);
		return;
	}

	evgMrm* evg=dynamic_cast<evgMrm*>(obj);
	EVRMRM* evr=dynamic_cast<EVRMRM*>(obj);

	if(evg) epicsPrintf("\t%s is EVG!\n",mrfName);
	if(evr) epicsPrintf("\t%s is EVR!\n",mrfName);

	if(!evg && !evr){
		errlogPrintf("mrfiocDBuff_init: FAILED! %s is neither EVR or EVG!\n",mrfName);
		return;
	}

	//Retrieve device info, base memory pointer, device type...
	if(evr){
		pvt->isEVG=epicsFalse;
		pvt->txBufferBase=(epicsUInt8*) (evr->base + 0x1800);
		pvt->dbcr=(epicsUInt8*) (evr->base + 0x24); //TXDBCR register
		evr->bufrx.dataRxAddReceive(0xff00,mrmEvrDataRxCB, pvt);
	}
	if(evg){
		pvt->isEVG=epicsTrue;
		pvt->txBufferBase=(epicsUInt8*)(evg->getRegAddr() + 0x800);
		pvt->dbcr=(epicsUInt8*)(evg->getRegAddr()+ 0x20); //DBCR register
	}

	/*
	 * Fill in rest of the device info
	 */
	pvt->name=strdup(regDevName);


	pvt->proto=(epicsUInt32) protocol; //protocol ID
	pvt->next=NULL; //terminate the list entry

	//just a quick verification test...
	epicsUInt32 versionReg = nat_ioread32(pvt->txBufferBase+0x2c);
	printf("\t%s device is %s. Version: 0x%x\n",mrfName,(pvt->isEVG?"EVG":"EVR"),versionReg);
	printf("\t%s registered to protocol %d\n",regDevName,pvt->proto);

	addDevice(pvt);
	regDevRegisterDevice(regDevName,&mrfiocDBuffSupport,(regDevice*)pvt);
}

/*
 * EPICS IOCsh command registration
 */

/* 		mrfiocDBuffConfigure   		*/
static const iocshArg mrfiocDBuffConfigureDefArg0 = { "regDevName",iocshArgString};
static const iocshArg mrfiocDBuffConfigureDefArg1 = { "mrfioc2 device name",iocshArgString};
static const iocshArg mrfiocDBuffConfigureDefArg2 = { "protocol",iocshArgInt};
static const iocshArg *const mrfiocDBuffConfigureDefArgs[3] = {&mrfiocDBuffConfigureDefArg0,&mrfiocDBuffConfigureDefArg1,&mrfiocDBuffConfigureDefArg2};

static const iocshFuncDef mrfiocDBuffConfigureDef = {"mrfiocDBuffConfigure", 3, mrfiocDBuffConfigureDefArgs};

static void mrfioDBuffConfigureFunc(const iocshArgBuf* args){
	mrfiocDBuff_init(args[0].sval, args[1].sval, args[2].ival);
}


/* 		mrfiocDBuffFlush   		*/
static const iocshArg mrfiocDBuffFlushDefArg0 = { "regDevName",iocshArgString};
static const iocshArg *const mrfiocDBuffFlushDefArgs[1] = {&mrfiocDBuffFlushDefArg0};

static const iocshFuncDef mrfiocDBuffFlushDef = {"mrfiocDBuffFlush", 1, mrfiocDBuffFlushDefArgs};

static void mrfioDBuffFlushFunc(const iocshArgBuf* args){
	mrfiocDBuffDevice* device = getDevice(args[0].sval);
	if(!device){
		errlogPrintf("Can not find device: %s\n",args[0].sval);
		return;
	}

	mrfiocDBuff_flush(device);
}



/*		registrar			*/

static int mrfiocDBuffRegistrar(void){
	iocshRegister(&mrfiocDBuffConfigureDef,mrfioDBuffConfigureFunc);
	iocshRegister(&mrfiocDBuffFlushDef,mrfioDBuffFlushFunc);

	return 1;
}
//Automatic registration
epicsExportRegistrar(mrfiocDBuffRegistrar);

static int done = mrfiocDBuffRegistrar();





