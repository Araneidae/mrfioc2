#include "evgMrm.h"

#include <iostream>
#include <stdexcept>

#include <errlog.h> 

#include <mrfCommonIO.h> 
#include <mrfCommon.h> 
#include "mrfFracSynth.h"

#include "evgRegMap.h"

evgMrm::evgMrm(const epicsUInt32 id, volatile epicsUInt8* const pReg):
id(id),
pReg(pReg) {

	for(int i = 0; i < evgNumMxc; i++) {
		muxCounter[i] = new evgMxc(i, pReg);
	}

	for(int i = 0; i < evgNumEvtTrig; i++) {
		trigEvt[i] = new evgTrigEvt(i, pReg);
	}

	for(int i = 0; i < evgNumDbusBit; i++) {
		dbus[i] = new evgDbus(i, pReg);
	}

	for(int i = 0; i < evgNumFpInp; i++) {
		FPio[ std::pair<epicsUInt32, std::string>(i,"FP_Input") ] = 
												new evgFPio(FP_Input, i, pReg);
	}

	for(int i = 0; i < evgNumFpOut; i++) {
		FPio[std::pair<epicsUInt32, std::string>(i,"FP_Output")] = 
												new evgFPio(FP_Output, i, pReg);
	}	

	seqRamSup = new evgSeqRamSup(pReg);
	
}

evgMrm::~evgMrm() {
}

evgMxc* 
evgMrm::getMuxCounter(epicsUInt32 muxNum) {
	evgMxc* mxc =  muxCounter[muxNum];
	if(!mxc)
		throw std::runtime_error("ERROR: Multiplexed Counter not initialized");

	return mxc;
}

evgTrigEvt*
evgMrm::getTrigEvt(epicsUInt32 evtTrigNum) {
	evgTrigEvt* pTrigEvt = trigEvt[evtTrigNum];
	if(!pTrigEvt)
		throw std::runtime_error("ERROR: Event Trigger not initialized");

	return pTrigEvt;
}

evgDbus*
evgMrm::getDbus(epicsUInt32 dbusBit) {
	evgDbus* pDbus = dbus[dbusBit];
	if(!pDbus)
		throw std::runtime_error("ERROR: Event Trigger not initialized");

	return pDbus;
}

evgFPio*
evgMrm::getFPio(epicsUInt32 ioNum, std::string type) {
	evgFPio* pFPio = FPio[ std::pair<epicsUInt32, std::string>(ioNum, type) ];
	if(!pFPio)
		throw std::runtime_error("ERROR: FPio not initialized");

	return pFPio;
}

evgSeqRamSup*
evgMrm::getSeqRamSup() {
	return seqRamSup;
}

/** 	Event Clock Source 	**/

//TODO: Set uSecDiv for RF source.
epicsStatus
evgMrm::setClockSource (epicsUInt8 clkSrc) {
	if(clkSrc == ClkSrc)
		return OK;

	if (clkSrc == evgClkSrcInternal) {
		// Use internal fractional synthesizer to generate the clock
		BITCLR8 (pReg, ClockSource, EVG_CLK_SRC_EXTRF);
		ClkSrc = clkSrc;
		setClockSpeed(ClkSpeed);

	} else if(clkSrc >= 1  && clkSrc <= 32) {     
		// Use external RF source to generate the clock
		ClkSrc = clkSrc;
		clkSrc = clkSrc - 1;
    	BITSET8 (pReg, ClockSource, EVG_CLK_SRC_EXTRF);
		WRITE8 (pReg, RfDiv, clkSrc);
	
	} else {
		errlogPrintf("ERROR: Invalid Clock Source.\n");
		return ERROR;	
	}

	return OK;
} //SetOutLinkClockSource()

epicsUInt8
evgMrm::getClockSource() {
	epicsUInt8 clkReg = READ8(pReg, ClockSource);
	return clkReg & EVG_CLK_SRC_EXTRF;
}


/**		Event Clock Speed	**/

epicsStatus
evgMrm::setClockSpeed (epicsFloat64 clkSpeed) {	
	epicsStatus   status = OK;
	if(ClkSrc == evgClkSrcInternal) {
		// Use internal fractional synthesizer to generate the clock
		epicsUInt32    controlWord, oldControlWord;
    	epicsFloat64   error;

    	controlWord = FracSynthControlWord (clkSpeed, MRF_FRAC_SYNTH_REF, 0, &error);
    	if ((!controlWord) || (error > 100.0)) {
        	errlogPrintf ("Cannot set event clock speed to %f MHz.\n", clkSpeed);
        	return ERROR;
    	}
	
		oldControlWord=READ32(pReg, FracSynthWord);

		/* Changing the control word disturbes the phase of 
		 * the synthesiser which will cause a glitch.
    	 * Don't change the control word unless needed.
    	 */
		if(controlWord != oldControlWord){
        	WRITE32(pReg, FracSynthWord, controlWord);
    	
			epicsUInt16 uSecDivider = (epicsUInt16)ClkSpeed;
			printf("Rounded Interge value: %d\n", uSecDivider);
			WRITE16(pReg, uSecDiv, uSecDivider);
		}

		status = OK;
	}

	ClkSpeed = clkSpeed;
	return status;

}//setClockSpeed

epicsFloat64
evgMrm::getClockSpeed() {
	return ClkSpeed;
}


/**		Software Event		**/

epicsStatus 
evgMrm::softEvtEnable(bool ena){	
	if(ena)
		BITSET8(pReg, SwEventControl, SW_EVT_ENABLE);
	else
		BITCLR8(pReg, SwEventControl, SW_EVT_ENABLE);
	
	return OK;
}


bool 
evgMrm::softEvtEnabled() {
	epicsUInt8 swEvtCtrl = READ8(pReg, SwEventControl);
	return swEvtCtrl & SW_EVT_ENABLE;
}


bool 
evgMrm::softEvtPend() {
	epicsUInt8 swEvtCtrl = READ8(pReg, SwEventControl);
	return swEvtCtrl & SW_EVT_PEND;
}

	
epicsStatus 
evgMrm::setSoftEvtCode(epicsUInt8 evtCode) {
	if(evtCode < 0 || evtCode > 255) {
		errlogPrintf("ERROR: Event Code out of range.\n");
		return ERROR;		
	}

	//TODO: if(pend == 0), write in an atomic step to SwEvtCode register. 
	WRITE8(pReg, SwEventCode, evtCode);
	return OK; 
}


epicsUInt8 
evgMrm::getSoftEvtCode() {
	return READ8(pReg, SwEventCode);
}