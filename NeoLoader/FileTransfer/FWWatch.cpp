#include "GlobalHeader.h"
#include "FWWatch.h"
#include "../NeoCore.h"

CFWWatch::CFWWatch(QObject* pObject)
 : QObjectEx(pObject)
{
}

void CFWWatch::Incoming(CAddress::EAF eAF, EProt Prot)
{
	SFWWatch& Watch = m_Map[eAF | (Prot << 16)];
	Watch.Incoming++;
}

void CFWWatch::Outgoing(CAddress::EAF eAF, EProt Prot)
{
	SFWWatch& Watch = m_Map[eAF | (Prot << 16)];
	Watch.Outgoing++;
}

bool CFWWatch::IsFirewalled(CAddress::EAF eAF, EProt Prot) const
{
	const SFWWatch& Watch = m_Map[eAF | (Prot << 16)];
	if(Watch.Outgoing > 10 && Watch.Incoming < 3)
		return true;
	return false;
}