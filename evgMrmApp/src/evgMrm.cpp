#include "evgMrm.h"

#include <iostream>
#include <stdexcept>
#include <math.h>

#include <errlog.h> 

#include <dbAccess.h>
#include <devSup.h>
#include <recSup.h>
#include <recGbl.h>
#include <epicsInterrupt.h>
#include <epicsTime.h>
#include <generalTimeSup.h>

#include <longoutRecord.h>

#include <mrfCommonIO.h> 
#include <mrfCommon.h> 

#ifdef __rtems__
#include <rtems/bspIo.h>
#endif //__rtems__

#include "evgRegMap.h"

evgMrm::evgMrm(const epicsUInt32 id, volatile epicsUInt8* const pReg):
m_pilotCountTS(4),
m_syncTS(false),
m_id(id),
m_pReg(pReg),
m_evtClk(pReg),
m_softEvt(pReg),
m_seqRamMgr(this),
m_softSeqMgr(this),
m_queue(epicsTimerQueueActive::allocate(true)) {
	m_wdTimer = new wdTimer("Watch Dog Timer",m_queue, this);

	try{
		for(int i = 0; i < evgNumEvtTrig; i++)
			m_trigEvt.push_back(new evgTrigEvt(i, pReg));

		for(int i = 0; i < evgNumMxc; i++) 
			m_muxCounter.push_back(new evgMxc(i, this));

		for(int i = 0; i < evgNumDbusBit; i++)
			m_dbus.push_back(new evgDbus(i, pReg));

		for(int i = 0; i < evgNumFpInp; i++) {
			m_input[ std::pair<epicsUInt32, InputType>(i, FP_Input) ] = 
					new evgInput(i, FP_Input, pReg + U32_FPInMap(i));
		}

		for(int i = 0; i < evgNumUnivInp; i++) {
			m_input[ std::pair<epicsUInt32, InputType>(i, Univ_Input) ] = 
					new evgInput(i, Univ_Input, pReg + U32_UnivInMap(i));
		}

		for(int i = 0; i < evgNumTbInp; i++) {
			m_input[ std::pair<epicsUInt32, InputType>(i, TB_Input) ] = 
 					new evgInput(i, TB_Input, pReg + U32_TBInMap(i));
		}

		for(int i = 0; i < evgNumFpOut; i++) {
			m_output[std::pair<epicsUInt32, OutputType>(i, FP_Output)] = 
										new evgOutput(i, pReg, FP_Output);
		}

		for(int i = 0; i < evgNumUnivOut; i++) {
			m_output[std::pair<epicsUInt32, OutputType>(i, Univ_Output)] = 
										new evgOutput(i, pReg, Univ_Output);
		}	

		m_ppsSrc.type = None_Input;
		m_ppsSrc.num = 0;

		init_cb(&irqStop0_cb, priorityHigh, &evgMrm::process_cb, 
														m_seqRamMgr.getSeqRam(0));
		init_cb(&irqStop1_cb, priorityHigh, &evgMrm::process_cb, 
														m_seqRamMgr.getSeqRam(1));
		init_cb(&irqExtInp_cb, priorityHigh, &evgMrm::sendTS, this);
	
		scanIoInit(&ioScanTS);
	} catch(std::exception& e) {
		errlogPrintf("%s\n", e.what());
	}
}

evgMrm::~evgMrm() {
	for(int i = 0; i < evgNumEvtTrig; i++)
		delete m_trigEvt[i];

	for(int i = 0; i < evgNumMxc; i++)
		delete m_muxCounter[i];

	for(int i = 0; i < evgNumDbusBit; i++)
		delete m_dbus[i];

	for(int i = 0; i < evgNumFpInp; i++)
		delete m_input[std::pair<epicsUInt32, InputType>(i, FP_Input)]; 

	for(int i = 0; i < evgNumUnivInp; i++)
		delete m_input[std::pair<epicsUInt32, InputType>(i, Univ_Input)];

	for(int i = 0; i < evgNumTbInp; i++)
		delete m_input[std::pair<epicsUInt32, InputType>(i, TB_Input)];

	for(int i = 0; i < evgNumFpOut; i++)
		delete m_output[std::pair<epicsUInt32, OutputType>(i, FP_Output)]; 

	for(int i = 0; i < evgNumUnivOut; i++)
		delete m_output[std::pair<epicsUInt32, OutputType>(i, Univ_Output)];

	m_queue.release();
}

void 
evgMrm::init_cb(CALLBACK *ptr, int priority, void(*fn)(CALLBACK*), void* valptr) { 
  	callbackSetPriority(priority, ptr); 
  	callbackSetCallback(fn, ptr);   
  	callbackSetUser(valptr, ptr);   
  	(ptr)->timer=NULL;              
}

const epicsUInt32 
evgMrm::getId() {
	return m_id;
}

volatile epicsUInt8*
evgMrm::getRegAddr() {
	return m_pReg;
}

epicsUInt32 
evgMrm::getFwVersion() {
	return READ32(m_pReg, FPGAVersion);
}

epicsUInt32
evgMrm::getStatus() {
	return READ32( m_pReg, Status);
}

epicsStatus
evgMrm::enable(bool ena) {
	if(ena)
		BITSET32(m_pReg, Control, EVG_MASTER_ENA);
	else
		BITCLR32(m_pReg, Control, EVG_MASTER_ENA);

	BITSET32(m_pReg, Control, EVG_DIS_EVT_REC);
	BITSET32(m_pReg, Control, EVG_REV_PWD_DOWN);

	return OK;
}

void
evgMrm::isr(void* arg) {
	evgMrm *evg = (evgMrm*)(arg);

	epicsUInt32 flags = READ32(evg->getRegAddr(), IrqFlag);
	epicsUInt32 enable = READ32(evg->getRegAddr(), IrqEnable);
	epicsUInt32 active = flags & enable;
	
	if(!active)
		return;
	
	if(active & EVG_IRQ_STOP_RAM(0)) {
		callbackRequest(&evg->irqStop0_cb);
		BITCLR32(evg->getRegAddr(), IrqEnable, EVG_IRQ_STOP_RAM(0));
	}

	if(active & EVG_IRQ_STOP_RAM(1)) {
		callbackRequest(&evg->irqStop1_cb);
		BITCLR32(evg->getRegAddr(), IrqEnable, EVG_IRQ_STOP_RAM(1));
	}

	if(active & EVG_IRQ_EXT_INP) {
		callbackRequest(&evg->irqExtInp_cb);
	}

	WRITE32(evg->m_pReg, IrqFlag, flags);
	return;
}

void
evgMrm::process_cb(CALLBACK *pCallback) {
	void* pVoid;
	evgSeqRam* seqRam;
	
	callbackGetUser(pVoid, pCallback);
	seqRam = (evgSeqRam*)pVoid;
	evgSoftSeq* softSeq = seqRam->getSoftSeq();
	if(!softSeq)
		return;
 
	softSeq->sync();
}


void
evgMrm::sendTS(CALLBACK *pCallback) {
	void* pVoid;
	epicsTime time;
	callbackGetUser(pVoid, pCallback);
	evgMrm* evg = (evgMrm*)pVoid;
   
	/*If the time since last update is more than 1.5 secs(i.e. if wdTimer expires) 
	then we need to resync the time*/
	evg->m_wdTimer->start(1 + evgAllowedTsGitter);
	if(evg->m_wdTimer->getTimeoutFlag() == true) {
		evg->m_wdTimer->clearTimeoutFlag();
		evg->m_pilotCountTS = 4;
		return;
	} else	if(evg->m_pilotCountTS) {
		evg->m_pilotCountTS--;
		if(evg->m_pilotCountTS == 0) {
			evg->syncTS();
			time = (epicsTime)evg->getTS();
			printf("Starting timestamping\n");
			time.show(1);
		}
		return;
	}

	evg->m_alarmTS = TS_ALARM_NONE;

	evg->incrementTSsec();
	scanIoRequest(evg->ioScanTS);

	if(evg->m_syncTS) {
		evg->syncTS();
		evg->m_syncTS = false;
	}

	epicsUInt32 sec = evg->getTSsec() + 1 + POSIX_TIME_AT_EPICS_EPOCH;
	for(int i = 0; i < 32; sec <<= 1, i++) {
		if( sec & 0x80000000 ) {
			evg->getSoftEvt()->setEvtCode(0x71);
		} else {
			evg->getSoftEvt()->setEvtCode(0x70);
		}
	}

	struct epicsTimeStamp ts;
	epicsTime ntpTime, storedTime;

	if(epicsTimeOK == generalTimeGetExceptPriority(&ts, 0, 50)) {
		ntpTime = ts;
		storedTime = (epicsTime)evg->getTS();

		double errorTime = ntpTime - storedTime;

		/*If there is an error between storedTime and ntpTime then we just print
		  the relevant information but we send out storedTime*/
		if(fabs(errorTime) > evgAllowedTsGitter) {
			evg->m_alarmTS = TS_ALARM_MINOR;
			printf("NTP time:\n");
			ntpTime.show(1);
			printf("Expected time:\n");
			storedTime.show(1);
			printf("----Timestamping Error of %f Secs----\n", errorTime);
    	}
 	}
}

epicsTimeStamp
evgMrm::getTS() {
	return m_ts;
}

epicsUInt32
evgMrm::getTSsec() {
	return m_ts.secPastEpoch;
}

epicsStatus
evgMrm::incrementTSsec() {
	m_ts.secPastEpoch++;
	return OK;
}

epicsStatus
evgMrm::syncTS() {
	while(epicsTimeOK != generalTimeGetExceptPriority(&m_ts, 0, 50));
	if(m_ts.nsec > 500*pow(10,6))
		incrementTSsec();
	
	m_ts.nsec = 0;

	//Clear alarm
	return OK;
}

epicsStatus
evgMrm::syncTsRequest() {
	m_syncTS = true;
	return OK;
}

epicsStatus
evgMrm::setTsInpType(InputType type) {
	if(m_ppsSrc.type == type)
		return OK;

	/*Check if such an input exists*/
	if(type != None_Input)
		getInput(m_ppsSrc.num, type);

	setupTsIrq(0);
	m_ppsSrc.type = type;
	setupTsIrq(1);

	return OK;
}

epicsStatus
evgMrm::setTsInpNum(epicsUInt32 num) {
	if(m_ppsSrc.num == num)
		return OK;

	/*Check if such an input exists*/
	if(m_ppsSrc.type != None_Input)
		getInput(num, m_ppsSrc.type);

	setupTsIrq(0);
	m_ppsSrc.num = num;
	setupTsIrq(1);
	
	return OK;
}

InputType
evgMrm::getTsInpType() {
	return m_ppsSrc.type;
}

epicsUInt32
evgMrm::getTsInpNum() {
	return m_ppsSrc.num;
}

epicsStatus
evgMrm::setupTsIrq(bool ena) {
	if(m_ppsSrc.type == None_Input)
		return OK;

	
	evgInput* inp = getInput(m_ppsSrc.num, m_ppsSrc.type);
	if(ena)
		inp->enaExtIrq(1);
	else
		inp->enaExtIrq(0);

	return OK;
}

/**	Access	functions 	**/

evgEvtClk*
evgMrm::getEvtClk() {
	return &m_evtClk;
}

evgSoftEvt*
evgMrm::getSoftEvt() {
	return &m_softEvt;
}

evgTrigEvt*
evgMrm::getTrigEvt(epicsUInt32 evtTrigNum) {
	evgTrigEvt* trigEvt = m_trigEvt[evtTrigNum];
	if(!trigEvt)
		throw std::runtime_error("Event Trigger not initialized");

	return trigEvt;
}

evgMxc* 
evgMrm::getMuxCounter(epicsUInt32 muxNum) {
	evgMxc* mxc =  m_muxCounter[muxNum];
	if(!mxc)
		throw std::runtime_error("Multiplexed Counter not initialized");

	return mxc;
}

evgDbus*
evgMrm::getDbus(epicsUInt32 dbusBit) {
	evgDbus* dbus = m_dbus[dbusBit];
	if(!dbus)
		throw std::runtime_error("Event Dbus not initialized");

	return dbus;
}

evgInput*
evgMrm::getInput(epicsUInt32 inpNum, InputType type) {
	evgInput* inp = m_input[ std::pair<epicsUInt32, InputType>(inpNum, type) ];
	if(!inp)
		throw std::runtime_error("Input not initialized");

	return inp;
}

evgOutput*
evgMrm::getOutput(epicsUInt32 outNum, OutputType type) {
	evgOutput* out = m_output[ std::pair<epicsUInt32, OutputType>(outNum, type) ];
	if(!out)
		throw std::runtime_error("Output not initialized");

	return out;
}

evgSeqRamMgr*
evgMrm::getSeqRamMgr() {
	return &m_seqRamMgr;
}

evgSoftSeqMgr*
evgMrm::getSoftSeqMgr() {
	return &m_softSeqMgr;
}


