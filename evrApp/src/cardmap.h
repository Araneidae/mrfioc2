
#ifndef CARDMAP_H_INC
#define CARDMAP_H_INC

#include "evr/evr.h"

extern "C++" {

/**@file cardmap.h
 *
 * Utilities for interaction between IOC Shell functions
 * and device support initialization.
 */

/**@brief Lookup the EVR* associated with 'id'
 *
 *@returns NULL in id has no association.
 */
EVR* getEVRBase(short id);

//! Lookup an EVR* and attempt to cast to the requested sub-class.
template<typename EVRSubclass>
EVRSubclass*
getEVR(short id)
{
  EVR* base=getEVRBase(id);
  if(!base)
    return 0;
  return dynamic_cast<EVRSubclass*>(base);
};

/**@brief Save the association between 'id' and 'dev'.
 *
 * throws std::runtime_error if 'id' has already been used.
 */
void storeEVRBase(short id, EVR* dev);

template<typename EVRSubclass>
void storeEVR(short id,EVRSubclass *dev)
{
  storeEVRBase(id,static_cast<EVR*>(dev));
};

void
visitEVRBase(void*, int (*fptr)(void*,short,EVR*));

} // extern C++

#endif /* CARDMAP_H_INC */
