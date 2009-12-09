
#ifndef PRESCALER_HPP_INC
#define PRESCALER_HPP_INC

#include <epicsTypes.h>

#include <evr/util.h>

class EVR;

class PreScaler : public IOStatus
{
public:
  PreScaler(EVR& o):owner(o){};
  virtual ~PreScaler()=0;

  virtual epicsUInt32 prescaler() const=0;
  virtual void setPrescaler(epicsUInt32)=0;

  EVR& owner;
};

#endif // PRESCALER_HPP_INC
