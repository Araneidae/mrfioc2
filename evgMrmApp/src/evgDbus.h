#ifndef EVG_DBUS_H
#define EVG_DBUS_H

#include <epicsTypes.h>

class evgDbus {
public:
	evgDbus(const epicsUInt32, volatile epicsUInt8* const);
	~evgDbus();
	
	epicsStatus setDbusMap(epicsUInt16);

private:
	const epicsUInt32 			m_id;
	volatile epicsUInt8* const	m_pReg;
	
};

#endif //EVG_DBUS_H
