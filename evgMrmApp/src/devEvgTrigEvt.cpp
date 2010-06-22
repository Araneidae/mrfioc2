#include <iostream>
#include <stdexcept>

#include <boRecord.h>
#include <longoutRecord.h>

#include <devSup.h>
#include <dbAccess.h>
#include <epicsExport.h>

#include "dsetshared.h"

#include <evgInit.h>

static long 
init_record(dbCommon *pRec, DBLINK* lnk) {
	if(lnk->type != VME_IO) {
		errlogPrintf("ERROR: Hardware link not VME_IO\n");
		return(S_db_badField);
	}

	evgMrm* evg = FindEvg(lnk->value.vmeio.card);		
	if(!evg)
		throw std::runtime_error("ERROR: Failed to lookup device");

	evgTrigEvt*  trigEvt = evg->getTrigEvt(lnk->value.vmeio.signal);
	if(trigEvt)
		throw std::runtime_error("ERROR: Failed to lookup device");
	pRec->dpvt = trigEvt;
	return 2;
}


/**		bo - Event Trigger Enable	**/
/*returns: (0,2)=>(success,success no convert) 0==2	*/
static long 
init_bo(boRecord* pbo) {
	return init_record((dbCommon*)pbo, &pbo->out);
}

/*returns: (-1,0)=>(failure,success)*/
static long 
write_bo(boRecord* pbo) {
	evgTrigEvt* trigEvt = (evgTrigEvt*)pbo->dpvt;
	return trigEvt->enable(pbo->val);
}


/** 	longout - Event Trigger Code	**/
/*returns: (-1,0)=>(failure,success)*/
static long 
init_lo(longoutRecord* plo) {
	epicsUInt32 ret = init_record((dbCommon*)plo, &plo->out);
	if (ret == 2)
		ret = 0;
	
	return ret;
}

/*returns: (-1,0)=>(failure,success)*/
static long 
write_lo(longoutRecord* plo) {
	evgTrigEvt* trigEvt = (evgTrigEvt*)plo->dpvt;
	return trigEvt->setEvtCode(plo->val);
}


/** 	device support entry table 		**/
extern "C" {

common_dset devBoEvgTrigEvt = {
    5,
    NULL,
    NULL,
    (DEVSUPFUN)init_bo,
    NULL,
    (DEVSUPFUN)write_bo,
};
epicsExportAddress(dset, devBoEvgTrigEvt);


common_dset devLoEvgTrigEvt = {
    5,
    NULL,
    NULL,
    (DEVSUPFUN)init_lo,
    NULL,
    (DEVSUPFUN)write_lo,
};
epicsExportAddress(dset, devLoEvgTrigEvt);

};