
#include <stdlib.h>
#include <epicsExport.h>
#include <dbAccess.h>
#include <devSup.h>
#include <recGbl.h>
#include <devLib.h> // For S_dev_*

#include <aiRecord.h>
#include <aoRecord.h>

#include "cardmap.h"
#include "evr/evr.h"
#include "property.h"

#include <stdexcept>
#include <string>

/***************** AI/AO *****************/

static long analog_init_record(dbCommon *prec, DBLINK* lnk)
{
try {
  assert(lnk->type==VME_IO);

  EVR* card=getEVR<EVR>(lnk->value.vmeio.card);
  if(!card)
    throw std::runtime_error("Failed to lookup device");

  property<EVR,double> *prop;
  std::string parm(lnk->value.vmeio.parm);

  if( parm=="Clock" ){
    prop=new property<EVR,double>(
        card,
        &EVR::clock,
        &EVR::clockSet
    );
  }else if( parm=="Timestamp Clock" ){
    prop=new property<EVR,double>(
        card,
        &EVR::clockTS,
        &EVR::clockTSSet
    );
  }else
    throw std::runtime_error("Invalid parm string in link");

  prec->dpvt=static_cast<void*>(prop);

  return 0;

} catch(std::runtime_error& e) {
  recGblRecordError(S_dev_noDevice, (void*)prec, e.what());
  return S_dev_noDevice;
} catch(std::exception& e) {
  recGblRecordError(S_db_noMemory, (void*)prec, e.what());
  return S_db_noMemory;
}
}

static long init_ai(aiRecord *prec)
{
  return analog_init_record((dbCommon*)prec, &prec->inp);
}

static long read_ai(aiRecord* prec)
{
try {
  property<EVR,double> *priv=static_cast<property<EVR,double>*>(prec->dpvt);

  prec->val = priv->get();

  return 2;
} catch(std::exception& e) {
  recGblRecordError(S_db_noMemory, (void*)prec, e.what());
  return S_db_noMemory;
}
}

static long init_ao(aoRecord *prec)
{
  long r=analog_init_record((dbCommon*)prec, &prec->out);
  if(r==0)
    return 2;
  return r;
}

static long get_ioint_info_ai(int dir,dbCommon* prec,IOSCANPVT* io)
{
  return get_ioint_info<EVR,double,double>(dir,prec,io);
}

static long write_ao(aoRecord* prec)
{
try {
  property<EVR,double> *priv=static_cast<property<EVR,double>*>(prec->dpvt);

  priv->set(prec->val);

  return 0;
} catch(std::exception& e) {
  recGblRecordError(S_db_noMemory, (void*)prec, e.what());
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
  DEVSUPFUN  special_linconv;
} devAIEVR = {
  6,
  NULL,
  NULL,
  (DEVSUPFUN) init_ai,
  (DEVSUPFUN) get_ioint_info_ai,
  (DEVSUPFUN) read_ai,
  NULL
};
epicsExportAddress(dset,devAIEVR);

struct {
  long num;
  DEVSUPFUN  report;
  DEVSUPFUN  init;
  DEVSUPFUN  init_record;
  DEVSUPFUN  get_ioint_info;
  DEVSUPFUN  write;
  DEVSUPFUN  special_linconv;
} devAOEVR = {
  6,
  NULL,
  NULL,
  (DEVSUPFUN) init_ao,
  NULL,
  (DEVSUPFUN) write_ao,
  NULL
};
epicsExportAddress(dset,devAOEVR);

};
