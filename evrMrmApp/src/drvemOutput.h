/*************************************************************************\
* Copyright (c) 2010 Brookhaven Science Associates, as Operator of
*     Brookhaven National Laboratory.
* mrfioc2 is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 * Author: Michael Davidsaver <mdavidsaver@bnl.gov>
 */

#ifndef EVRMRMOUTPUT_H_INC
#define EVRMRMOUTPUT_H_INC

#include "evr/output.h"

/**
 * Controls only the single output mapping register
 * shared by all (except CML) outputs on MRM EVRs.
 *
 * This class is reused by other subunits which
 * have identical mapping registers.
 */
class MRMOutput : public Output
{
public:
  MRMOutput(const std::string& n, volatile unsigned char *);
  virtual ~MRMOutput();

  virtual epicsUInt32 source() const;
  virtual void setSource(epicsUInt32);

  virtual const char*sourceName(epicsUInt32) const;

private:
  volatile unsigned char *base;
};


#endif // EVRMRMOUTPUT_H_INC
