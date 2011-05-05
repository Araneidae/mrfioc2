/*************************************************************************\
* Copyright (c) 2010 Brookhaven Science Associates, as Operator of
*     Brookhaven National Laboratory.
* mrfioc2 is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 * Author: Michael Davidsaver <mdavidsaver@bnl.gov>
 */

#include "drvem.h"

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <algorithm>

#include <epicsMath.h>
#include <errlog.h>

#include <mrfCommon.h>
#include <mrfCommonIO.h>
#include <mrfBitOps.h>

#include "evrRegMap.h"

#include "mrfFracSynth.h"

#include "drvemIocsh.h"

#include <dbDefs.h>
#include <dbScan.h>
#include <epicsInterrupt.h>

#define CBINIT(ptr, prio, fn, valptr) \
do { \
  callbackSetPriority(prio, ptr); \
  callbackSetCallback(fn, ptr);   \
  callbackSetUser(valptr, ptr);   \
  (ptr)->timer=NULL;              \
} while(0)

/*Note: All locking involving the ISR done by disabling interrupts
 *      since the OSI library doesn't provide more efficient
 *      constructs like a ISR safe spinlock.
 */

extern "C" {
    /* Arbitrary throttleing of FIFO thread.
     * The FIFO thread has to run at a high priority
     * so the callbacks have low latency.  At the same
     * time we want to prevent starvation of lower
     * priority tasks if too many events are received.
     * This would cause the CA server to be starved
     * preventing remote correction of the problem.
     *
     * This should be the highest event rate which
     * needs to be timestamped.
     *
     * Set to 0.0 to disable
     */
    double mrmEvrFIFOPeriod = 1.0/ 2000.0; /* 1/rate in Hz */
}

// Fractional synthesizer reference clock frequency
static
const double fracref=24.0; // MHz

CardMap<dataBufRx> datarxmap;

EVRMRM::EVRMRM(int i,volatile unsigned char* b)
  :EVR()
  ,evrLock()
  ,id(i)
  ,base(b)
  ,buftx(b+U32_DataTxCtrl, b+U8_DataTx_base)
  ,bufrx(b, 10) // Sets depth of Rx queue
  ,count_recv_error(0)
  ,count_hardware_irq(0)
  ,count_heartbeat(0)
  ,count_FIFO_overflow(0)
  ,outputs()
  ,prescalers()
  ,pulsers()
  ,shortcmls()
  ,drain_fifo_method(*this)
  ,drain_fifo_task(drain_fifo_method, "EVRFIFO",
                   epicsThreadGetStackSize(epicsThreadStackBig),
                   epicsThreadPriorityHigh )
  ,drain_fifo_wakeup(3,sizeof(int))
  ,count_FIFO_sw_overrate(0)
  ,stampClock(0.0)
  ,shadowSourceTS(TSSourceInternal)
  ,shadowCounterPS(0)
  ,timestampValid(false)
  ,lastInvalidTimestamp(0)
  ,lastValidTimestamp(0)
{
    epicsUInt32 v = READ32(base, FWVersion),evr,ver;

    evr=v&FWVersion_type_mask;
    evr>>=FWVersion_type_shift;

    if(evr!=0x1)
        throw std::runtime_error("Address does not correspond to an EVR");

    ver=v&FWVersion_ver_mask;
    ver>>=FWVersion_ver_shift;
    if(ver<3)
        throw std::runtime_error("Firmware version not supported");

    scanIoInit(&IRQmappedEvent);
    scanIoInit(&IRQbufferReady);
    scanIoInit(&IRQheartbeat);
    scanIoInit(&IRQrxError);
    scanIoInit(&IRQfifofull);
    scanIoInit(&timestampValidChange);

    CBINIT(&data_rx_cb   , priorityHigh, &mrmBufRx::drainbuf, &this->bufrx);
    CBINIT(&drain_log_cb , priorityMedium, &EVRMRM::drain_log , this);
    CBINIT(&poll_link_cb , priorityMedium, &EVRMRM::poll_link , this);
    CBINIT(&seconds_tick_cb, priorityMedium,&EVRMRM::seconds_tick , this);

    /*
     * Create subunit instances
     */

    v&=FWVersion_form_mask;
    v>>=FWVersion_form_shift;

    size_t nPul=10; // number of pulsers
    size_t nPS=3;   // number of prescalers
    // # of outputs (Front panel, FP Universal, Rear transition module)
    size_t nOFP=0, nOFPUV=0, nORB=0;
    // # of CML outputs
    size_t nCML=0;
    // # of FP inputs
    size_t nIFP=0;

    switch(v){
    case evrFormCPCI:
        printf("CPCI ");
        nOFPUV=4;
        nIFP=2;
        nORB=6;
        break;
    case evrFormPMC:
        printf("PMC ");
        nOFP=3;
        nIFP=1;
        break;
    case evrFormVME64:
        printf("VME64 ");
        nOFP=7;
        nCML=3; // OFP 4-6 are CML
        nOFPUV=4;
        nORB=16;
        nIFP=2;
        break;
    default:
        printf("Unknown EVR variant %d\n",v);
    }
    printf("Out FP:%u FPUNIV:%u RB:%u IFP:%u\n",
           (unsigned int)nOFP,(unsigned int)nOFPUV,
           (unsigned int)nORB,(unsigned int)nIFP);

    // Special output for mapping bus interrupt
    //outputs[std::make_pair(OutputInt,0)]=new MRMOutput(base+U16_IRQPulseMap);

    inputs.resize(nIFP);
    for(size_t i=0; i<nIFP; i++){
        inputs[i]=new MRMInput(base,i);
    }

    for(size_t i=0; i<nOFP; i++){
        outputs[std::make_pair(OutputFP,i)]=new MRMOutput(base+U16_OutputMapFP(i));
    }

    for(size_t i=0; i<nOFPUV; i++){
        outputs[std::make_pair(OutputFPUniv,i)]=new MRMOutput(base+U16_OutputMapFPUniv(i));
    }

    for(size_t i=0; i<nORB; i++){
        outputs[std::make_pair(OutputRB,i)]=new MRMOutput(base+U16_OutputMapRB(i));
    }

    prescalers.resize(nPS);
    for(size_t i=0; i<nPS; i++){
        prescalers[i]=new MRMPreScaler(*this,base+U32_Scaler(i));
    }

    pulsers.resize(nPul);
    for(size_t i=0; i<nPul; i++){
        pulsers[i]=new MRMPulser(i,*this);
    }

    if(nCML && ver>=4){
        shortcmls.resize(nCML);
        for(size_t i=0; i<nCML; i++){
            shortcmls[i]=new MRMCML(i,*this);
        }
    }else if(nCML){
        printf("CML outputs not supported with this firmware\n");
    }

    for(size_t i=0; i<NELEMENTS(this->events); i++) {
        events[i].code=i;
        events[i].owner=this;
        CBINIT(&events[i].done, priorityLow, &EVRMRM::sentinel_done , &events[i]);
    }

    SCOPED_LOCK(evrLock);

    eventClock=FracSynthAnalyze(READ32(base, FracDiv),
                                fracref,0)*1e6;

    shadowCounterPS=READ32(base, CounterPS);

    if(tsDiv()!=0) {
        shadowSourceTS=TSSourceInternal;
    } else {
        bool usedbus4=READ32(base, Control) & Control_tsdbus;

        if(usedbus4)
            shadowSourceTS=TSSourceDBus4;
        else
            shadowSourceTS=TSSourceEvent;
    }

    eventNotityAdd(MRF_EVENT_TS_COUNTER_RST, &seconds_tick_cb);

    drain_fifo_task.start();
}

EVRMRM::~EVRMRM()
{
    for(outputs_t::iterator it=outputs.begin();
        it!=outputs.end(); ++it)
    {
        delete it->second;
    }
    outputs.clear();
    for(prescalers_t::iterator it=prescalers.begin();
        it!=prescalers.end(); ++it)
    {
        delete (*it);
    }
    //TODO: cleanup the rest
}

epicsUInt32
EVRMRM::model() const
{
    epicsUInt32 v = READ32(base, FWVersion);

    return (v&FWVersion_form_mask)>>FWVersion_form_shift;
}

epicsUInt32
EVRMRM::version() const
{
    epicsUInt32 v = READ32(base, FWVersion);

    return (v&FWVersion_ver_mask)>>FWVersion_ver_shift;
}

bool
EVRMRM::enabled() const
{
    epicsUInt32 v = READ32(base, Control);
    return v&Control_enable;
}

void
EVRMRM::enable(bool v)
{
    SCOPED_LOCK(evrLock);
    if(v)
        BITSET(NAT,32,base, Control, Control_enable|Control_mapena);
    else
        BITCLR(NAT,32,base, Control, Control_enable|Control_mapena);
}

MRMPulser*
EVRMRM::pulser(epicsUInt32 i)
{
    if(i>=pulsers.size())
        throw std::out_of_range("Pulser id is out of range");
    return pulsers[i];
}

const MRMPulser*
EVRMRM::pulser(epicsUInt32 i) const
{
    if(i>=pulsers.size())
        throw std::out_of_range("Pulser id is out of range");
    return pulsers[i];
}

MRMOutput*
EVRMRM::output(OutputType otype,epicsUInt32 idx)
{
    outputs_t::iterator it=outputs.find(std::make_pair(otype,idx));
    if(it==outputs.end())
        return 0;
    else
        return it->second;
}

const MRMOutput*
EVRMRM::output(OutputType otype,epicsUInt32 idx) const
{
    outputs_t::const_iterator it=outputs.find(std::make_pair(otype,idx));
    if(it==outputs.end())
        return 0;
    else
        return it->second;
}

MRMInput*
EVRMRM::input(epicsUInt32 i)
{
    if(i>=inputs.size())
        throw std::out_of_range("Input id is out of range");
    return inputs[i];
}

const MRMInput*
EVRMRM::input(epicsUInt32 i) const
{
    if(i>=inputs.size())
        throw std::out_of_range("Input id is out of range");
    return inputs[i];
}

MRMPreScaler*
EVRMRM::prescaler(epicsUInt32 i)
{
    if(i>=prescalers.size())
        throw std::out_of_range("PreScaler id is out of range");
    return prescalers[i];
}

const MRMPreScaler*
EVRMRM::prescaler(epicsUInt32 i) const
{
    if(i>=prescalers.size())
        throw std::out_of_range("PreScaler id is out of range");
    return prescalers[i];
}

MRMCML*
EVRMRM::cml(epicsUInt32 i)
{
    if(i>=shortcmls.size())
        throw std::out_of_range("CML Short id is out of range");
    return shortcmls[i];
}

const MRMCML*
EVRMRM::cml(epicsUInt32 i) const
{
    if(i>=shortcmls.size())
        throw std::out_of_range("CML Short id is out of range");
    return shortcmls[i];
}

bool
EVRMRM::specialMapped(epicsUInt32 code, epicsUInt32 func) const
{
    if(code>255)
        throw std::out_of_range("Event code is out of range");
    if(func>127 || func<96 ||
        (func<=121 && func>=102) )
    {
        throw std::out_of_range("Special function code is out of range");
    }

    if(code==0)
        return false;

    epicsUInt32 bit  =func%32;
    epicsUInt32 mask=1<<bit;

    epicsUInt32 val=READ32(base, MappingRam(0, code, Internal));

    val&=mask;

    return !!val;
}

void
EVRMRM::specialSetMap(epicsUInt32 code, epicsUInt32 func,bool v)
{
    if(code>255)
        throw std::out_of_range("Event code is out of range");
    /* The special function codes are the range 96 to 127
     */
    if(func>127 || func<96 ||
        (func<=121 && func>=102) )
    {
        throw std::out_of_range("Special function code is out of range");
    }

    if(code==0)
      return;

    /* The way the latch timestamp is implimented in hardware (no status bit)
     * makes it impossible to use the latch mapping and the latch control register
     * bits at the same time.  We use the control register bits.
     * However, there is not much loss of functionality since all events
     * can be timestamped in the FIFO.
     */
    if(func==126)
        throw std::out_of_range("Use of latch timestamp special function code is not allowed");

    epicsUInt32 bit  =func%32;
    epicsUInt32 mask=1<<bit;

    SCOPED_LOCK(evrLock);

    epicsUInt32 val=READ32(base, MappingRam(0, code, Internal));

    if (v && _ismap(code,func-96)) {
        // already set
        throw std::runtime_error("Ignore duplicate mapping");

    } else if(v) {
        _map(code,func-96);
        WRITE32(base, MappingRam(0, code, Internal), val|mask);
    } else {
        _unmap(code,func-96);
        WRITE32(base, MappingRam(0, code, Internal), val&~mask);
    }

//    errlogPrintf("EVR #%d code %02x func %3d %s\n",
//        id, code, func, v?"Set":"Clear");
}

void
EVRMRM::clockSet(double freq)
{
    double err;
    // Set both the fractional synthesiser and microsecond
    // divider.

    freq/=1e6;

    epicsUInt32 newfrac=FracSynthControlWord(
                        freq, fracref, 0, &err);

    if(newfrac==0)
        throw std::out_of_range("New frequency can't be used");

    SCOPED_LOCK(evrLock);

    epicsUInt32 oldfrac=READ32(base, FracDiv);

    if(newfrac!=oldfrac){
        // Changing the control word disturbes the phase
        // of the synthesiser which will cause a glitch.
        // Don't change the control word unless needed.

        WRITE32(base, FracDiv, newfrac);

        eventClock=FracSynthAnalyze(READ32(base, FracDiv),
                                    fracref,0)*1e6;
    }

    // USecDiv is accessed as a 32 bit register, but
    // only 16 are used.
    epicsUInt16 oldudiv=READ32(base, USecDiv);
    epicsUInt16 newudiv=(epicsUInt16)freq;

    if(newudiv!=oldudiv){
        WRITE32(base, USecDiv, newudiv);
    }
}

epicsUInt32
EVRMRM::uSecDiv() const
{
    return READ32(base, USecDiv);
}

bool
EVRMRM::pllLocked() const
{
    return READ32(base, ClkCtrl) & ClkCtrl_cglock;
}

bool
EVRMRM::linkStatus() const
{
    return !(READ32(base, Status) & Status_legvio);
}

void
EVRMRM::setSourceTS(TSSource src)
{
    double clk=clockTS(), eclk=clock();
    epicsUInt16 div=0;

    if(clk<=0 || !isfinite(clk))
        throw std::out_of_range("TS Clock rate invalid");

    switch(src){
    case TSSourceInternal:
    case TSSourceEvent:
    case TSSourceDBus4:
        break;
    default:
        throw std::out_of_range("TS source invalid");
    }

    SCOPED_LOCK(evrLock);

    switch(src){
    case TSSourceInternal:
        // div!=0 selects src internal
        div=(epicsUInt16)(eclk/clk);
        break;
    case TSSourceEvent:
        BITCLR(NAT,32, base, Control, Control_tsdbus);
        // div=0
        break;
    case TSSourceDBus4:
        BITSET(NAT,32, base, Control, Control_tsdbus);
        // div=0
        break;
    }
    WRITE32(base, CounterPS, div);
    shadowCounterPS=div;
    shadowSourceTS=src;
}

double
EVRMRM::clockTS() const
{
    //Note: acquires evrLock 3 times.

    TSSource src=SourceTS();

    if(src!=TSSourceInternal)
        return stampClock;

    epicsUInt16 div=tsDiv();

    return clock()/div;
}

void
EVRMRM::clockTSSet(double clk)
{
    if(clk<0.0 || !isfinite(clk))
        throw std::out_of_range("TS Clock rate invalid");

    TSSource src=SourceTS();
    double eclk=clock();

    if(clk>eclk || clk==0.0)
        clk=eclk;

    SCOPED_LOCK(evrLock);

    if(src==TSSourceInternal){
        epicsUInt16 div=(epicsUInt16)(eclk/clk);
        WRITE32(base, CounterPS, div);

        shadowCounterPS=div;
    }

    stampClock=clk;
}

bool
EVRMRM::interestedInEvent(epicsUInt32 event,bool set)
{
    if (!event || event>255) return false;

    eventCode *entry=&events[event];

    SCOPED_LOCK(evrLock);

    if (   (set  && entry->interested==0) // first interested
        || (!set && entry->interested==1) // or last un-interested
    ) {
        specialSetMap(event, ActionFIFOSave, set);
    }

    if (set)
        entry->interested++;
    else
        entry->interested--;

    return true;
}

bool
EVRMRM::getTimeStamp(epicsTimeStamp *ts,epicsUInt32 event)
{
    if(!ts) throw std::runtime_error("Invalid argument");

    SCOPED_LOCK(evrLock);
    if(!timestampValid) return false;

    if(event>0 && event<=255) {
        // Get time of last event code #

        eventCode *entry=&events[event];

        // Fail if event is not mapped
        if (!entry->interested ||
            ( entry->last_sec==0 &&
              entry->last_evt==0) )
        {
            return false;
        }

        ts->secPastEpoch=entry->last_sec;
        ts->nsec=entry->last_evt;


    } else {
        // Get current absolute time

        epicsUInt32 ctrl=READ32(base, Control);

        // Latch timestamp
        WRITE32(base, Control, ctrl|Control_tsltch);

        ts->secPastEpoch=READ32(base, TSSecLatch);
        ts->nsec=READ32(base, TSEvtLatch);

        /* BUG: There is a firmware bug which occasionally
         * causes the previous write to fail with a VME bus
         * error, and 0 the Control register.
         * Note: When this occurs the card is _disabled_ and
         * unresponsive for a short interval.
         */
        epicsUInt32 ctrl2=READ32(base, Control);
        if (ctrl2!=ctrl) { // tsltch bit is write-only
            printf("Control register write fault %08x %08x\n",ctrl,ctrl2);
            WRITE32(base, Control, ctrl);
        }

    }

    if(!convertTS(ts))
        return false;

    return true;
}

/** @brief In place conversion between raw posix sec+ticks to EPICS sec+nsec.
 @returns false if conversion failed
 */
bool
EVRMRM::convertTS(epicsTimeStamp* ts)
{
    // First validate the input

    //Has it been initialized?
    if(ts->secPastEpoch==0 || ts->nsec==0){
        return false;
    }

    // 1 sec. reset is late
    if(ts->nsec>=1000000000) {
        SCOPED_LOCK(evrLock);
        timestampValid=false;
        lastInvalidTimestamp=ts->secPastEpoch;
        scanIoRequest(timestampValidChange);
        return false;
    }

    // recurrence of an invalid time
    if(ts->secPastEpoch==lastInvalidTimestamp) {
        timestampValid=false;
        scanIoRequest(timestampValidChange);
        return false;
    }

    /* Reported seconds timestamp should be no more
     * then 1sec in the future.
     */
    if(ts->secPastEpoch > lastValidTimestamp+1)
    {
        errlogPrintf("EVR ignoring invalid TS %08x %08x (expect %08x)\n",
                     ts->secPastEpoch, ts->nsec, lastValidTimestamp);
        timestampValid=false;
        scanIoRequest(timestampValidChange);
        return false;
    }

    //Link seconds counter is POSIX time
    ts->secPastEpoch-=POSIX_TIME_AT_EPICS_EPOCH;

    // Convert ticks to nanoseconds
    double period=1e9/clockTS(); // in nanoseconds

    if(period<=0 || !isfinite(period))
        return false;

    ts->nsec=(epicsUInt32)(ts->nsec*period);
    return true;
}

bool
EVRMRM::getTicks(epicsUInt32 *tks)
{
    *tks=READ32(base, TSEvt);
    return true;
}

IOSCANPVT
EVRMRM::eventOccurred(epicsUInt32 event)
{
    if (event>0 && event<=255)
        return events[event].occured;
    else
        return NULL;
}

void
EVRMRM::eventNotityAdd(epicsUInt32 event, CALLBACK* cb)
{
    if (event==0 || event>255)
        throw std::out_of_range("Invalid event number");

    SCOPED_LOCK2(evrLock, guard);

    if (std::find(events[event].notifiees.begin(),
                  events[event].notifiees.end(),
                  cb)
                    != events[event].notifiees.end())
    {
        throw std::runtime_error("callback already registered for this event");
    }

    events[event].notifiees.push_back(cb);

    guard.unlock();

    interestedInEvent(event, true);
}

void
EVRMRM::eventNotityDel(epicsUInt32 event, CALLBACK* cb)
{
    if (event==0 || event>255)
        throw std::out_of_range("Invalid event number");

    SCOPED_LOCK2(evrLock, guard);

    eventCode::notifiees_t::iterator it;

    it=std::find(events[event].notifiees.begin(),
                 events[event].notifiees.end(),
                 cb);
    if (it==events[event].notifiees.end())
        return;

    events[event].notifiees.erase(it);
    guard.unlock();

    interestedInEvent(event, false);
}

epicsUInt16
EVRMRM::dbus() const
{
    return (READ32(base, Status) & Status_dbus_mask) << Status_dbus_shift;
}

// A place to write to which will keep the read
// at the end of the ISR from being optimized out.
// This value should never be used anywhere else.
volatile epicsUInt32 evrMrmIsrFlagsTrashCan;

void
EVRMRM::isr(void *arg)
{
    EVRMRM *evr=static_cast<EVRMRM*>(arg);

    epicsUInt32 flags=READ32(evr->base, IRQFlag);

    epicsUInt32 enable=READ32(evr->base, IRQEnable);

    epicsUInt32 active=flags&enable;

    if(!active)
      return;

    if(active&IRQ_RXErr){
        evr->count_recv_error++;
        scanIoRequest(evr->IRQrxError);

        enable &= ~IRQ_RXErr;
        callbackRequest(&evr->poll_link_cb);
    }
    if(active&IRQ_BufFull){
        // Silence interrupt
        BITSET(NAT,32,evr->base, DataBufCtrl, DataBufCtrl_stop);

        callbackRequest(&evr->data_rx_cb);
        scanIoRequest(evr->IRQbufferReady);
    }
    if(active&IRQ_HWMapped){
        evr->shadowIRQEna &= ~IRQ_HWMapped;
        //TODO: think of a way to use this feature...
    }
    if(active&IRQ_Event){
        //FIFO not-empty
        enable &= ~IRQ_Event;
        int wakeup=0;
        evr->drain_fifo_wakeup.send(&wakeup, sizeof(wakeup));
    }
    if(active&IRQ_Heartbeat){
        evr->count_heartbeat++;
        scanIoRequest(evr->IRQheartbeat);
    }
    if(active&IRQ_FIFOFull){
        enable &= ~IRQ_FIFOFull;
        int wakeup=0;
        evr->drain_fifo_wakeup.send(&wakeup, sizeof(wakeup));

        scanIoRequest(evr->IRQfifofull);
    }

    WRITE32(evr->base, IRQEnable, enable|IRQ_Enable);
    WRITE32(evr->base, IRQFlag, flags);
    // Ensure IRQFlags is written before returning.
    evrMrmIsrFlagsTrashCan=READ32(evr->base, IRQFlag);
}


// Caller must hold evrLock
static
void
eventInvoke(eventCode& event)
{
    scanIoRequest(event.occured);

    for(eventCode::notifiees_t::const_iterator it=event.notifiees.begin();
        it!=event.notifiees.end();
        ++it)
    {
        callbackRequest(*it);
    }
}

void
EVRMRM::drain_fifo()
{
    size_t i;
    printf("EVR FIFO task start\n");

    SCOPED_LOCK2(evrLock, guard);

    while(true) {
        int code, err;

        guard.unlock();

        err=drain_fifo_wakeup.receive(&code, sizeof(code));

        if (err<0) {
            errlogPrintf("FIFO wakeup error %d\n",err);
            epicsThreadSleep(0.1); // avoid message flood
            guard.lock();
            continue;

        } else if(code==1) {
            // Request thread stop
            guard.lock();
            break;
        }

        epicsTime now;
        now=epicsTime::getCurrent();

        guard.lock();

        double since=now-lastFifoRun;

        if (since<mrmEvrFIFOPeriod && since>0) {
            /* To prevent from completely overwelming lower priority tasks
             * (ie channel access) ensure FIFO callback waits for
             * mrmEvrFIFOPeriod seconds between runs.
             */
            guard.unlock();
            epicsThreadSleep(mrmEvrFIFOPeriod-since);
            guard.lock();
        }
        lastFifoRun=now;

        epicsUInt32 status;

        // Bound the number of events taken from the FIFO
        // at one time.
        for(i=0; i<512; i++) {

            status=READ32(base, IRQFlag);
            if (!(status&IRQ_Event))
                break;
            if (status&IRQ_RXErr)
                break;

            epicsUInt32 evt=READ32(base, EvtFIFOCode);
            if (!evt)
                break;

            if (evt>NELEMENTS(events)) {
                // BUG: we get occasional corrupt VME reads of this register
                epicsUInt32 evt2=READ32(base, EvtFIFOCode);
                if (evt2>NELEMENTS(events)) {
                    printf("Really weird event 0x%08x 0x%08x\n", evt, evt2);
                    break;
                } else
                    evt=evt2;
            }
            evt &= 0xff; // (in)santity check

            events[evt].last_sec=READ32(base, EvtFIFOSec);
            events[evt].last_evt=READ32(base, EvtFIFOEvt);

            if (events[evt].again) {
                // ignore extra events in buffer.
            } else if (events[evt].waitingfor>0) {
                // already queued, but occured again before
                // callbacks finished so disable event
                events[evt].again=true;
                specialSetMap(evt, ActionFIFOSave, false);
                count_FIFO_sw_overrate++;
            } else {
                // needs to be queued
                eventInvoke(events[evt]);
                events[evt].waitingfor=NUM_CALLBACK_PRIORITIES;
                for(int p=0; p<NUM_CALLBACK_PRIORITIES; p++) {
                    events[evt].done.priority=p;
                    callbackRequest(&events[evt].done);
                }
            }

        }

        if (status&IRQ_FIFOFull) {
            count_FIFO_overflow++;
        }

        if (status&(IRQ_FIFOFull|IRQ_RXErr)) {
            // clear fifo if link lost or buffer overflow
            BITSET(NAT,32, base, Control, Control_fiforst);
        }

        int iflags=epicsInterruptLock();

        BITSET(NAT,32, base, IRQEnable, IRQ_Event|IRQ_FIFOFull|IRQ_Enable);

        epicsInterruptUnlock(iflags);
    }

    printf("FIFO task exiting\n");
}

void
EVRMRM::sentinel_done(CALLBACK* cb)
{
    void *vptr;
    callbackGetUser(vptr,cb);
    eventCode *sent=static_cast<eventCode*>(vptr);

    SCOPED_LOCK2(sent->owner->evrLock, guard);

    // Is this the last callback queue?
    if (--sent->waitingfor)
        return;

    bool run=sent->again;
    sent->again=false;

    // Re-enable mapping if disabled
    if (run && sent->interested) {
        sent->owner->specialSetMap(sent->code, ActionFIFOSave, true);
    }
}


void
EVRMRM::drain_log(CALLBACK*)
{
}

void
EVRMRM::poll_link(CALLBACK* cb)
{
    void *vptr;
    callbackGetUser(vptr,cb);
    EVRMRM *evr=static_cast<EVRMRM*>(vptr);

    epicsUInt32 flags=READ32(evr->base, IRQFlag);

    if(flags&IRQ_RXErr){
        // Still down
        callbackRequestDelayed(&evr->poll_link_cb, 0.1); // poll again in 100ms
        {
            SCOPED_LOCK2(evr->evrLock, guard);
            evr->timestampValid=false;
            evr->lastInvalidTimestamp=evr->lastValidTimestamp;
            scanIoRequest(evr->timestampValidChange);
        }
        WRITE32(evr->base, IRQFlag, IRQ_RXErr);
    }else{
        scanIoRequest(evr->IRQrxError);
        int iflags=epicsInterruptLock();
        BITSET(NAT,32, evr->base, IRQEnable, IRQ_RXErr|IRQ_Enable);
        epicsInterruptUnlock(iflags);
    }
}

void
EVRMRM::seconds_tick(CALLBACK* cb)
{
    void *vptr;
    callbackGetUser(vptr,cb);
    EVRMRM *evr=static_cast<EVRMRM*>(vptr);

    SCOPED_LOCK2(evr->evrLock, guard);

    // Don't bother to latch since we are only reading the seconds
    epicsUInt32 newSec=READ32(evr->base, TSSec);

    /* When a new seconds value is received it must differ from
     * the previous valid seconds value, and not an invalid value.
     */
    if (evr->lastValidTimestamp==newSec
        || evr->lastInvalidTimestamp==newSec
        || newSec==0)
    {
        if (evr->timestampValid) {
            errlogPrintf("TS reset w/ old or invalid seconds %08x (%08x %08x)\n",
                         newSec, evr->lastValidTimestamp, evr->lastInvalidTimestamp);
            evr->timestampValid=false;
            evr->lastInvalidTimestamp=newSec;
            scanIoRequest(evr->timestampValidChange);
        }
        return;
    } else if (!evr->timestampValid) {
        errlogPrintf("TS becomes valid after fault %08x\n",newSec);
        evr->timestampValid=true;
        scanIoRequest(evr->timestampValidChange);
    }
    evr->lastValidTimestamp=newSec;

}
