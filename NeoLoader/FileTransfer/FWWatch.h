#pragma once

#include "../../Framework/ObjectEx.h"
#include "../../Framework/Address.h"

class CFWWatch: public QObjectEx
{
	Q_OBJECT
public:
	CFWWatch(QObject* pObject = 0);

	enum EProt
	{
		eTCP = 0,
		eUTP
	};

	void		Incoming(CAddress::EAF eAF, EProt Prot = eTCP);
	void		Outgoing(CAddress::EAF eAF, EProt Prot = eTCP);

	bool		IsFirewalled(CAddress::EAF eAF, EProt Prot = eTCP) const;

protected:
	struct SFWWatch
	{
		SFWWatch() : Incoming(0), Outgoing(0) {}
		int Incoming;
		int Outgoing;
	};

	QMap<UINT, SFWWatch>	m_Map;
};