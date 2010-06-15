
#ifndef EVRMRMCMLSHORT_HPP_INC
#define EVRMRMCMLSHORT_HPP_INC

#include "evr/cml.h"

class MRMCML : public CML
{
public:
  MRMCML(unsigned char, volatile unsigned char *);
  virtual ~MRMCML();

  virtual cmlMode mode() const;
  virtual void setMode(cmlMode);

  virtual bool enabled() const;
  virtual void enable(bool);

  virtual bool inReset() const;
  virtual void reset(bool);

  virtual bool powered() const;
  virtual void power(bool);

  // For original (Classic) mode

  virtual epicsUInt32 pattern(cmlShort) const;
  virtual void patternSet(cmlShort, epicsUInt32);

  // For Frequency mode

  //! Trigger level
  virtual bool polarityInvert() const;
  virtual void setPolarityInvert(bool);

  virtual epicsUInt32 countHigh() const;
  virtual epicsUInt32 countLow () const;
  virtual void setCountHigh(epicsUInt32);
  virtual void setCountLow (epicsUInt32);

  // For Pattern mode

  virtual epicsUInt32 lenPattern() const;
  virtual epicsUInt32 lenPatternMax() const;
  virtual void getPattern(unsigned char*, epicsUInt32*) const;
  virtual void setPattern(const unsigned char*, epicsUInt32);

private:
  volatile unsigned char *base;
  unsigned char N;
};

#endif // EVRMRMCMLSHORT_HPP_INC
