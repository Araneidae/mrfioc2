#include "evgSoftEvt.h"

#include <errlog.h> 
#include <stdexcept>

#include <mrfCommonIO.h> 
#include <mrfCommon.h> 

#include "evgRegMap.h"

evgSoftEvt::evgSoftEvt(const std::string& name, volatile epicsUInt8* const pReg):
mrf::ObjectInst<evgSoftEvt>(name),
m_pReg(pReg),
m_lock() {
}

void
evgSoftEvt::enable(bool ena) {
    if(ena)
         BITSET8(m_pReg, SwEventControl, SW_EVT_ENABLE);
    else
         BITCLR8(m_pReg, SwEventControl, SW_EVT_ENABLE);
}

bool
evgSoftEvt::enabled() const {
    return READ8(m_pReg, SwEventControl) & SW_EVT_ENABLE;
}

bool 
evgSoftEvt::pend() const {
    return READ8(m_pReg, SwEventControl) & SW_EVT_PEND;
}
    
void
evgSoftEvt::setEvtCode(epicsUInt32 evtCode) {
    if(evtCode > 255)
        throw std::runtime_error("Event Code out of range.");
    
    SCOPED_LOCK(m_lock);
    while(pend() == 1);
    WRITE8(m_pReg, SwEventCode, evtCode);
}

epicsUInt32
evgSoftEvt::getEvtCode() const {
    return READ8(m_pReg, SwEventCode);
}

