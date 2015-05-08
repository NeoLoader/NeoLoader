#pragma once

#include "../../Framework/ObjectEx.h"
#include "../../Framework/Address.h"

struct SIPFilter
{
	uint32		uStart;
	uint32		uEnd;
	UINT		uLevel;
};

class CPeerWatch: public QObjectEx
{
	Q_OBJECT

public:
	CPeerWatch(QObject* qObject = NULL);

	void		Process(UINT Tick);

	int			AliveCount()	{return m_AliveList.size();}
	int			DeadCount()		{return m_DeadList.size();}
	int			BannedCount()	{return m_BanList.size();}

	bool		CheckPeer(const CAddress& Address, uint16 uPort, bool bIncoming = false);

	void		PeerFailed(const CAddress& Address, uint16 uPort);
	void		PeerConnected(const CAddress& Address, uint16 uPort);

	void		BanPeer(const CAddress& Address);


	/*bool		LoadIPFilter();
	bool		ImportIPFilter(const QString& FileName);*/

private slots:
	//void		OnRequestFinished();

protected:
	struct SPeer
	{
		SPeer()	{ uPort = 0; }
		SPeer(const CAddress& Address, uint16 Port)	{ uAddress = Address; uPort = Port; }
		inline bool operator==	(const SPeer &Other) const { return uAddress == Other.uAddress && (uPort == Other.uPort || !uPort || !Other.uPort); }
		inline bool operator<	(const SPeer &Other) const { return uAddress < Other.uAddress || (uAddress == Other.uAddress && uPort < Other.uPort); }
		CAddress	uAddress;
		uint16		uPort;
	};
	
	map<SPeer, uint64>	m_AliveList;		// Peer, ResetTime
	map<SPeer, uint64>	m_DeadList;			// Peer, ResetTime

	map<CAddress, uint64>	m_BanList;		// Ip, Duration

	vector<SIPFilter>	m_FilterList;
	time_t		m_IpFilterDate;

	uint64		m_NextCleanUp;
};
