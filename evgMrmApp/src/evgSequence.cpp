#include "evgSequence.h"

#include <errlog.h>

#include <mrfCommon.h>


evgSequence::evgSequence(const epicsUInt32 id):
m_id(id),
m_trigSrc(0),
m_runMode(single),
m_seqRam(0) {
	//For Testing purpose
 	epicsUInt8 eventCode[] = {1, 2, 3, 5, 127};
 	epicsUInt32 timeStamp[] = {125000000, 250000000, 375000000, 500000000, 625000000};

 	setEventCode(eventCode, 5);
 	setTimeStamp(timeStamp, 5);		
}

const epicsUInt32
evgSequence::getId() const {
	return m_id;
}

epicsStatus 
evgSequence::setDescription(const char* desc) {
	m_desc.assign(desc);
	return OK; 
}

const char* 
evgSequence::getDescription() {
	return m_desc.c_str();
}

epicsStatus
evgSequence::setEventCode(epicsUInt8* eventCode, epicsUInt32 size) {
	if(size < 0 || size > 2048) {
		errlogPrintf("ERROR: Number of events is too large.");
		return ERROR;
	}
		
	m_eventCode.assign(eventCode, eventCode + size);
	
	return OK;
}

std::vector<epicsUInt8>
evgSequence::getEventCode() {
	return m_eventCode;
}


epicsStatus
evgSequence::setTimeStamp(epicsUInt32* timeStamp, epicsUInt32 size) {
	if(size < 0 || size > 2048) {
		errlogPrintf("ERROR: Number of event is too large.");
		return ERROR;
	}
	
	m_timeStamp.assign(timeStamp, timeStamp + size);
	return OK;
}

std::vector<epicsUInt32>
evgSequence::getTimeStamp() {
	return m_timeStamp;
}


epicsStatus 
evgSequence::setTrigSrc(epicsUInt32 trigSrc) {
	if(trigSrc > 18 || (trigSrc < 16 && trigSrc > 7)) {
		errlogPrintf("ERROR: EVG Sequencer Trigger Src %d is not valid\n", trigSrc);
		return ERROR;
	}
	m_trigSrc = trigSrc;
	return OK;
}

epicsUInt32 
evgSequence::getTrigSrc() {
	return m_trigSrc;
}


epicsStatus 
evgSequence::setRunMode(SeqRunMode runMode) {
	m_runMode = runMode;
	return OK;
}
	
SeqRunMode 
evgSequence::getRunMode() {
	return m_runMode;
}


epicsStatus 
evgSequence::setSeqRam(evgSeqRam* seqRam) {
	m_seqRam = seqRam;
	return OK;
}

evgSeqRam* 
evgSequence::getSeqRam() {
	return m_seqRam;
}