
#include <stdlib.h>

#include <ellLib.h>

#include "devLibPCIImpl.h"

#define epicsExportSharedSymbols
#include "devLibPCI.h"

/**************** API functions *****************/

epicsShareFunc
int devPCIFindCB(
     const epicsPCIID *idlist,
     devPCISearchFn searchfn,
     void *arg,
     unsigned int opt /* always 0 */
)
{
  if(!idlist || !searchfn)
    return 2;

  if(!pdevLibPCIVirtualOS)
    return 5;

  return (*pdevLibPCIVirtualOS->pDevPCIFind)(idlist,searchfn,arg,opt);
}


struct bdfmatch
{
  unsigned int b,d,f;
  epicsPCIDevice* found;
};

static
int bdfsearch(void* ptr, epicsPCIDevice* cur)
{
  struct bdfmatch *mt=ptr;

  if( cur->bus==mt->b && cur->device==mt->d &&
      cur->function==mt->f )
  {
    mt->found=cur;
    return 1;
  }

  return 0;
}

/*
 * The most common PCI search using only id fields and BDF.
 */
epicsShareFunc
int devPCIFindBDF(
     const epicsPCIID *idlist,
     unsigned int      b,
     unsigned int      d,
     unsigned int      f,
      epicsPCIDevice **found,
     unsigned int      opt
)
{
  int err;
  struct bdfmatch find;

  if(!found)
    return 2;

  find.b=b;
  find.d=d;
  find.f=f;
  find.found=NULL;

  err=devPCIFindCB(idlist,&bdfsearch,&find, opt);
  if(err!=0){
    /* Search failed? */
    return err;
  }

  if(!find.found){
    /* Not found */
    return 1;
  }

  *found=find.found;
  return 0;
}

int
devPCIToLocalAddr(
  epicsPCIDevice *curdev,
  unsigned int bar,
  volatile void **ppLocalAddr,
  unsigned int opt
)
{
  if(!pdevLibPCIVirtualOS)
    return 5;

  if(bar>=PCIBARCOUNT)
    return 2;

  return (*pdevLibPCIVirtualOS->pDevPCIToLocalAddr)(curdev,bar,ppLocalAddr,opt);
}



epicsShareFunc
epicsUInt32
devPCIBarLen(
  epicsPCIDevice *curdev,
          unsigned int  bar
)
{
  if(!pdevLibPCIVirtualOS)
    return 5;

  if(bar>=PCIBARCOUNT)
    return 2;

  return (*pdevLibPCIVirtualOS->pDevPCIBarLen)(curdev,bar);
}

epicsShareFunc
int devPCIConnectInterrupt(
  epicsPCIDevice *curdev,
  void (*pFunction)(void *),
  void  *parameter
)
{
  if(!pdevLibPCIVirtualOS)
    return 5;

  return (*pdevLibPCIVirtualOS->pDevPCIConnectInterrupt)
                (curdev,pFunction,parameter);
}

epicsShareFunc
int devPCIDisconnectInterrupt(
  epicsPCIDevice *curdev,
  void (*pFunction)(void *)
)
{
  if(!pdevLibPCIVirtualOS)
    return 5;

  return (*pdevLibPCIVirtualOS->pDevPCIDisconnectInterrupt)
                (curdev,pFunction);
}
