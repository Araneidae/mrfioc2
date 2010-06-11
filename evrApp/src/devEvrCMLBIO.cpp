
#include <stdlib.h>
#include <epicsExport.h>
#include <dbAccess.h>
#include <devSup.h>
#include <recGbl.h>
#include <devLib.h> // For S_dev_*

#include <biRecord.h>
#include <boRecord.h>

#include "cardmap.h"
#include "evr/evr.h"
#include "evr/cml.h"
#include "dsetshared.h"

#include <stdexcept>
#include <string>

/***************** BI/BO *****************/

static long add_record(dbCommon *prec, DBLINK* lnk)
{
  long ret=0;
try {
  assert(lnk->type==VME_IO);

  EVR* card=getEVR<EVR>(lnk->value.vmeio.card);
  if(!card)
    throw std::runtime_error("Failed to lookup device");

  CML* pul=card->cml(lnk->value.vmeio.signal);
  if(!pul)
    throw std::runtime_error("Failed to lookup CML Short pattern registers");

  property<CML,bool> *prop;
  if (prec->dpvt) {
    prop=static_cast<property<CML,bool>* >(prec->dpvt);
    prec->dpvt=NULL;
  } else
    prop=new property<CML,bool>;

  std::string parm(lnk->value.vmeio.parm);

  if( parm=="Enable" ){
    *prop=property<CML,bool>(
        pul,
        &CML::enabled,
        &CML::enable
    );
  }else if( parm=="Power" ){
    *prop=property<CML,bool>(
        pul,
        &CML::powered,
        &CML::power
    );
  }else if( parm=="Reset" ){
    *prop=property<CML,bool>(
        pul,
        &CML::inReset,
        &CML::reset
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
  return ret;
}

static long add_bi(dbCommon *prec)
{
  biRecord *pbi=(biRecord*)prec;
  return add_record((dbCommon*)pbi, &pbi->inp);
}

static long add_bo(dbCommon *prec)
{
  boRecord *pbo=(boRecord*)prec;
  return add_record((dbCommon*)pbo, &pbo->out);
}

extern "C" {

dsxt dxtBIEVRCML={add_bi,del_record_empty};
static
common_dset devBIEVRCML = {
  5,
  NULL,
  dset_cast(&init_dset<&dxtBIEVRCML>),
  (DEVSUPFUN) init_record_empty,
  dset_cast(&get_ioint_info_property<CML,bool>),
  dset_cast(&read_bi_property<CML>)
};
epicsExportAddress(dset,devBIEVRCML);

dsxt dxtBOEVRCML={add_bo,del_record_empty};
static
common_dset devBOEVRCML = {
  5,
  NULL,
  dset_cast(&init_dset<&dxtBOEVRCML>),
  (DEVSUPFUN) init_record_empty,
  NULL,
  dset_cast(&write_bo_property<CML>)
};
epicsExportAddress(dset,devBOEVRCML);

};
