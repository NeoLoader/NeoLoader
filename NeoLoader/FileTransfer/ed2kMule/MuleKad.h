#pragma once

#include "../../../Framework/ObjectEx.h"
#include "../../../Framework/Address.h"
class QNetworkReply;
class CMuleClient;
class CFile;
class CBuffer;
class CAbstractSearch;
class CMuleServer;

class CMuleKad: public QObjectEx
{
	Q_OBJECT

public:
	CMuleKad(CMuleServer* pServer, QObject* qObject = NULL);

	void							Process(UINT Tick);

	void							StartKad();
	void							StopKad();
	void							AddNode(const CAddress& Address = CAddress(), uint16 uKadPort = 0);

	bool							RequestKadCallback(CMuleClient* pClient, CFile* pFile);
	void							RelayUDPPacket(const CAddress& Address, uint16 uUDPPort, const CBuffer& Packet);

	void							RecivedBuddyPing();

	bool							IsEnabled() const;
	bool							IsFirewalled() const						{return m_Firewalled;}
	const CAddress&					GetAddress() const							{return m_Address;}
	//uint16							GetKadPort(bool bSocket = false) const		{return bSocket ? m_UDPPort : m_KadPort;}
	uint16							GetKadPort() const							{return m_KadPort;}
	const QByteArray&				GetKadID() const							{return m_KadID;}
	bool							IsUDPOpen() const							{return !m_KadFirewalled;}
	enum EStatus
	{
		eDisconnected,
		eConnecting,
		eConnected
	};
	bool							IsConnected() const							{return m_KadStatus == eConnected;}
	bool							IsDisconnected() const						{return m_KadStatus == eDisconnected;}
	const QVariantMap&				GetKadStats() const							{return m_KadStats;}

	bool							FindSources(CFile* pFile);
	bool							ResetPub(CFile* pFile);

	void							CheckFWState();

	void							FWCheckUDPRequested(CMuleClient* pClient, uint16 IntPort, uint16 ExtPort, uint32 UDPKey);
	void							FWCheckACKRecived(CMuleClient* pClient);
	void							SendFWCheckACK(CMuleClient* pClient);
	void							SetUDPFWCheckResult(CMuleClient* pClient, bool bCanceled);

	void							RemoveClient(CMuleClient* pClient);

	CMuleClient*					GetBuddy(bool Booth = false);

	void							StartSearch(CAbstractSearch* pSearch);
	void							SyncSearch(CAbstractSearch* pSearch);
	void							StopSearch(CAbstractSearch* pSearch);

	uint32							FindNotes(const QByteArray& Hash, uint64 Size, const QString& Name);
	bool							GetFoundNotes(uint32 SearchID, QVariantList& Notes);

	bool							FindNotes(CFile* pFile);
	bool							IsFindingNotes(CFile* pFile);

	QString							GetStatus(CFile* pFile = NULL, uint64* pNext = NULL) const;

private slots:
	void							OnHelloRecived();
	void							OnSocketClosed();

	void							OnNotificationRecived(const QString& Command, const QVariant& Parameters);

	void							ProcessKadPacket(QByteArray Packet, quint8 Prot, CAddress Address, quint16 uKadPort, quint32 UDPKey, bool bValidKey);

signals:
	void							SendKadPacket(QByteArray Packet, quint8 Prot, CAddress Address, quint16 uKadPort, QByteArray NodeID, quint32 UDPKey);

protected:
	void							SyncFiles();
	void							SyncLog();
	void							SyncNotes();

	CAddress						m_Address;
	uint16							m_KadPort;
	QByteArray						m_KadID;
	//uint16							m_UDPPort;
	bool							m_Firewalled;	// does KAD think we are firewalled
	bool							m_KadFirewalled;
	EStatus							m_KadStatus;
	QVariantMap						m_KadStats;

	CMuleClient*					m_MyBuddy;
	QList<CMuleClient*>				m_PendingBuddys;
	uint64							m_NextBuddyPing;
	uint64							m_NextConnectionAttempt;

	uint64							m_uLastLog;

	struct SFWCheck
	{
		SFWCheck() 
		{bTestUDP = false; uIntPort = uExtPort = uUDPKey = 0;}
		SFWCheck(uint16 IntPort, uint16 ExtPort, uint32 UDPKey) 
		{bTestUDP = true; uIntPort = IntPort; uExtPort = ExtPort; uUDPKey = UDPKey;}
		bool bTestUDP;
		uint16 uIntPort;
		uint16 uExtPort;
		uint32 uUDPKey;
	};
	QMap<CMuleClient*, SFWCheck>	m_QueuedFWChecks;

	struct SSrcLookup
	{
		SSrcLookup()
		{
			uNextLookup = 0;
			uPendingLookup = 0;
		}
		uint64	uNextLookup;
		uint32	uPendingLookup;
	};
	QMap<CFile*, SSrcLookup>		m_SourceLookup;
	int								m_LookupCount;

	QMap<uint64, uint32>			m_NoteLookup;

	QSet<CAbstractSearch*>			m_RunningSearches;

	uint64							m_uNextKadFirewallRecheck;
};
