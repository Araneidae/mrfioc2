
#include <stdlib.h>
#include <epicsExport.h>
#include <dbAccess.h>
#include <devSup.h>
#include <recGbl.h>
#include <devLib.h> // For S_dev_*
#include <alarm.h>

#include <longinRecord.h>
#include <longoutRecord.h>

#include <mrfCommon.h> // for mrfDisableRecord

#include "cardmap.h"
#include "evr/cml_short.h"
#include "property.h"

#include <stdexcept>
#include <string>

/***************** Longin/Longout *****************/

static long long_init_record(dbCommon *prec, DBLINK* lnk)
{
  long ret=0;
try {
  assert(lnk->type==VME_IO);

  EVR* card=getEVR<EVR>(lnk->value.vmeio.card);
  if(!card)
    throw std::runtime_error("Failed to lookup device");

  CMLShort* pul=card->cmlshort(lnk->value.vmeio.signal);
  if(!pul)
    throw std::runtime_error("Failed to lookup CML short registers");

  property<CMLShort,epicsUInt32> *prop;
  std::string parm(lnk->value.vmeio.parm);
  if( parm=="Pattern Low" ){
    prop=new property<CMLShort,epicsUInt32>(
        pul,
        &CMLShort::getPattern<cmlShortLow>,
        &CMLShort::setPattern<cmlShortLow>
    );
  } else if( parm=="Pattern Rise" ){
    prop=new property<CMLShort,epicsUInt32>(
        pul,
        &CMLShort::getPattern<cmlShortRise>,
        &CMLShort::setPattern<cmlShortRise>
    );
  } else if( parm=="Pattern High" ){
    prop=new property<CMLShort,epicsUInt32>(
        pul,
        &CMLShort::getPattern<cmlShortHigh>,
        &CMLShort::setPattern<cmlShortHigh>
    );
  } else if( parm=="Pattern Fall" ){
    prop=new property<CMLShort,epicsUInt32>(
        pul,
        &CMLShort::getPattern<cmlShortFall>,
        &CMLShort::setPattern<cmlShortFall>
    );
  }else
    throw std::runtime_error("Invalid parm string in link");

  prec->dpvt=static_cast<void*>(prop);

  return 0;

} catch(std::runtime_error& e) {
  recGblRecordError(S_dev_noDevice, (void*)prec, e.what());
  ret=S_dev_noDevice;
} catch(std::exception& e) {
  recGblRecordError(S_db_noMemory, (void*)prec, e.what());
  ret=S_db_noMemory;
}
  mrfDisableRecord(prec);
  return ret;
}

static long init_li(longinRecord *pli)
{
  return long_init_record((dbCommon*)pli, &pli->inp);
}

static long read_li(longinRecord* pli)
{
try {
  property<CMLShort,epicsUInt32> *priv=static_cast<property<CMLShort,epicsUInt32>*>(pli->dpvt);

  pli->val = priv->get();

  return 0;
} catch(std::exception& e) {
  recGblRecordError(S_db_noMemory, (void*)pli, e.what());
  return S_db_noMemory;
}
}

static long init_lo(longoutRecord *plo)
{
  return long_init_record((dbCommon*)plo, &plo->out);
}

static long get_ioint_info_li(int dir,dbCommon* prec,IOSCANPVT* io)
{
  return get_ioint_info<CMLShort,epicsUInt32,epicsUInt32>(dir,prec,io);
}

static long write_lo(longoutRecord* plo)
{
try {
  property<CMLShort,epicsUInt32> *priv=static_cast<property<CMLShort,epicsUInt32>*>(plo->dpvt);

  priv->set(plo->val);

  long rbv=priv->get();

  // Probably an indication that this is a 16-bit field
  if(rbv!=plo->val){
    recGblSetSevr((dbCommon*)plo,SOFT_ALARM,MINOR_ALARM);
  }

  return 0;
} catch(std::exception& e) {
  recGblRecordError(S_db_noMemory, (void*)plo, e.what());
  return S_db_noMemory;
}
}

extern "C" {

struct {
  long num;
  DEVSUPFUN  report;
  DEVSUPFUN  init;
  DEVSUPFUN  init_record;
  DEVSUPFUN  get_ioint_info;
  DEVSUPFUN  read;
} devLIEVRCMLShort = {
  5,
  NULL,
  NULL,
  (DEVSUPFUN) init_li,
  (DEVSUPFUN) get_ioint_info_li,
  (DEVSUPFUN) read_li
};
epicsExportAddress(dset,devLIEVRCMLShort);

struct {
  long num;
  DEVSUPFUN  report;
  DEVSUPFUN  init;
  DEVSUPFUN  init_record;
  DEVSUPFUN  get_ioint_info;
  DEVSUPFUN  write;
} devLOEVRCMLShort = {
  5,
  NULL,
  NULL,
  (DEVSUPFUN) init_lo,
  NULL,
  (DEVSUPFUN) write_lo
};
epicsExportAddress(dset,devLOEVRCMLShort);

};
