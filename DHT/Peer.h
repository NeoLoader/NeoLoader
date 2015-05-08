#pragma once

#include "../Framework/Address.h"
#include <QList>

struct SPeer
{
	SPeer() : Port(0), Flags(0xFF) {}
	SPeer(const CAddress& addr, quint16 port, quint8 flags = 0xFF) 
	 : Address(addr), Port(port), Flags(flags) {}
	CAddress Address;
	quint16 Port;
	quint8 Flags;
};

typedef QList<SPeer> TPeerList;