
#include <stdlib.h>
#include <epicsExport.h>
#include <dbAccess.h>
#include <devSup.h>
#include <recGbl.h>
#include <devLib.h> // For S_dev_*

#include <biRecord.h>
#include <boRecord.h>

#include <mrfCommon.h> // for mrfDisableRecord

#include "cardmap.h"
#include "evr/evr.h"
#include "evr/cml_short.h"
#include "property.h"

#include <stdexcept>
#include <string>

/***************** BI/BO *****************/

static long binary_init_record(dbCommon *prec, DBLINK* lnk)
{
  long ret=0;
try {
  assert(lnk->type==VME_IO);

  EVR* card=getEVR<EVR>(lnk->value.vmeio.card);
  if(!card)
    throw std::runtime_error("Failed to lookup device");

  CMLShort* pul=card->cmlshort(lnk->value.vmeio.signal);
  if(!pul)
    throw std::runtime_error("Failed to lookup CML Short pattern registers");

  property<CMLShort,bool> *prop;
  std::string parm(lnk->value.vmeio.parm);

  if( parm=="Enable" ){
    prop=new property<CMLShort,bool>(
        pul,
        &CMLShort::enabled,
        &CMLShort::enable
    );
  }else if( parm=="Power" ){
    prop=new property<CMLShort,bool>(
        pul,
        &CMLShort::powered,
        &CMLShort::power
    );
  }else if( parm=="Reset" ){
    prop=new property<CMLShort,bool>(
        pul,
        &CMLShort::inReset,
        &CMLShort::reset
    );
  }else
    throw std::runtime_error("Invalid parm string in link");

  prec->dpvt=static_cast<void*>(prop);

  return 2;

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

static long init_bi(biRecord *pbi)
{
  return binary_init_record((dbCommon*)pbi, &pbi->inp);
}

static long read_bi(biRecord* pbi)
{
try {
  property<CMLShort,bool> *priv=static_cast<property<CMLShort,bool>*>(pbi->dpvt);

  pbi->rval = priv->get();

  return 0;
} catch(std::exception& e) {
  recGblRecordError(S_db_noMemory, (void*)pbi, e.what());
  return S_db_noMemory;
}
}

static long init_bo(boRecord *pbo)
{
  return binary_init_record((dbCommon*)pbo, &pbo->out);
}

static long get_ioint_info_bi(int dir,dbCommon* prec,IOSCANPVT* io)
{
  return get_ioint_info<EVR,bool,bool>(dir,prec,io);
}

static long write_bo(boRecord* pbo)
{
try {
  property<CMLShort,bool> *priv=static_cast<property<CMLShort,bool>*>(pbo->dpvt);

  priv->set(pbo->rval);

  return 0;
} catch(std::exception& e) {
  recGblRecordError(S_db_noMemory, (void*)pbo, e.what());
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
} devBIEVRCMLShort = {
  5,
  NULL,
  NULL,
  (DEVSUPFUN) init_bi,
  (DEVSUPFUN) get_ioint_info_bi,
  (DEVSUPFUN) read_bi
};
epicsExportAddress(dset,devBIEVRCMLShort);

struct {
  long num;
  DEVSUPFUN  report;
  DEVSUPFUN  init;
  DEVSUPFUN  init_record;
  DEVSUPFUN  get_ioint_info;
  DEVSUPFUN  write;
} devBOEVRCMLShort = {
  5,
  NULL,
  NULL,
  (DEVSUPFUN) init_bo,
  NULL,
  (DEVSUPFUN) write_bo
};
epicsExportAddress(dset,devBOEVRCMLShort);

};
