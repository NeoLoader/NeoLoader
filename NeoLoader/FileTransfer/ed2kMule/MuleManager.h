#pragma once

#include "../../../Framework/ObjectEx.h"
#include "MuleTags.h"
#include "../Transfer.h"
#include "MuleClient.h"
#include "../../../Framework/Scope.h"
#include "../../../Framework/Cryptography/AsymmetricKey.h"
#include "../FWWatch.h"

class CFileHash;
class CFile;
class CMuleKad;
class CFileHash;
class CMuleServer;
class CServerList;
struct SMuleSource;


class CMuleManager: public QObjectEx
{
	Q_OBJECT

public:
	CMuleManager(QObject* qObject = NULL);
	~CMuleManager();

	void							Process(UINT Tick);

	void							AddClient(CMuleClient* pClient)				{m_Clients.append(pClient);}
    void							RemoveClient(CMuleClient* pClient)			{m_Clients.removeOne(pClient);}
	const QList<CMuleClient*>&		GetClients()								{return m_Clients;}
	int								GetConnectionCount();
	CMuleServer*					GetServer()									{return m_Server;}

	bool							GrantSX(CFile* pFile);
	void							ConfirmSX(CFile* pFile);

	bool							GrantIPRequest();

	bool							DispatchClient(CMuleClient*& pClient);
	CMuleSource*					AttacheToFile(const CFileHash* pHash, uint64 uSize, CMuleClient* pClient);
	void							AddToFile(CFile* pFile, const SMuleSource& Mule, EFoundBy FoundBy);
	CMuleClient*					GetClient(const SMuleSource& Mule, bool* bAdded = NULL);

	void							CallbackRequested(const SMuleSource& Mule, bool bUTP = false);

	const TEd2kID&					GetUserHash() const							{return m_UserHash;}

	bool							IsFirewalled(CAddress::EAF eAF, bool bUTP = false, bool bIgnoreKad = false) const;
	CAddress						GetAddress(CAddress::EAF eAF, bool bIgnoreKad = false) const;
	int								GetIPv6Support() const;

	CMuleKad*						GetKad()									{return m_Kademlia;}
	CServerList*					GetServerList()								{return m_ServerList;}

	bool							SupportsCryptLayer() const					{return m_MyInfo.MiscOptions2.Fields.SupportsCryptLayer;}
	bool							RequestsCryptLayer() const					{return m_MyInfo.MiscOptions2.Fields.RequestsCryptLayer;}
	bool							RequiresCryptLayer() const					{return m_MyInfo.MiscOptions2.Fields.RequiresCryptLayer;}
	bool							DirectUDPCallback() const					{return m_MyInfo.MiscOptions2.Fields.DirectUDPCallback;}
	bool							SupportsNatTraversal() const				{return m_MyInfo.MiscOptionsN.Fields.SupportsNatTraversal;}

	struct SMyInfo
	{
		UMuleVer	MuleVersion;
		UMuleMisc1	MiscOptions1;
		UMuleMisc2	MiscOptions2;
		UMuleMiscN	MiscOptionsN;
		UMuleMiscNL	MiscOptionsNL;
		UMuleConOpt	MuleConOpts;
	};
	const SMyInfo*					GetMyInfo()									{return &m_MyInfo;}

	const QString&					GetVersion() const							{return m_Version;}

	bool							IsEnabled() const							{return m_bEnabled;}

	void							SendUDPPacket(CBuffer& Packet, uint8 Prot, const SMuleSource& Mule);
	void							ProcessUDPPacket(CBuffer& Packet, quint8 Prot, const CAddress& Address, quint16 uUDPPort);

	void							SetLastCallbacksMustWait(uint64 uWait)		{m_LastCallbacksMustWait = GetCurTick() + uWait;}
	bool							IsLastCallbacksMustWait()					{return m_LastCallbacksMustWait < GetCurTick();}

	CFWWatch*						FW()										{return &m_FWWatch;}

	// AICH Stuff

	void							RequestAICHData(CFile* pFile, uint64 uBegin, uint64 uEnd);

	void							AddAICHRequest(CMuleClient* pClient, CFile* pFile, uint16 uPart);
	void							DropAICHRequests(CMuleClient* pClient, CFile* pFile);
	bool							CheckAICHRequest(CMuleClient* pClient, CFile* pFile, uint16 uPart);
	bool							IsAICHRequestes(CFile* pFile, uint16 uPart);

	uint64							GrabCollection(const QByteArray& FileData, const QString& FileName);
	CFile*							ImportDownload(const QString& FilePath, bool bDeleteSource);
	
	CPrivateKey*					GetPrivateKey()								{return m_PrivateKey;}
	CPublicKey*						GetPublicKey()								{return m_PublicKey;}

	const STransferStats&			GetStats()									{return m_TransferStats;}

private slots:
	void							OnConnection(CStreamSocket* pSocket);

	void							ProcessUDPPacket(QByteArray Packet, quint8 Prot, CAddress Address, quint16 uUDPPort);

	void							OnHelloRecived();
	void							OnSocketClosed();

public slots:
	void							OnBytesWritten(qint64 Bytes);
    void 							OnBytesReceived(qint64 Bytes);

signals:
	void							SendUDPPacket(QByteArray Packet, quint8 Prot, CAddress Address, quint16 uUDPPort, QByteArray Hash);

protected:
	CMuleClient*					FindClient(const SMuleSource& Mule, CMuleClient* pNot = NULL);
	CMuleClient*					FindClient(const CAddress& Address, uint16 uUDPPort);

	void							ManageConnections();

	void							UpdateCache();

	TEd2kID							m_UserHash;

	CMuleServer*					m_Server;
	QList<CMuleClient*>				m_Clients;

	CMuleKad*						m_Kademlia;
	CServerList*					m_ServerList;

	struct SSXTiming
	{
		SSXTiming()
		 : NextReset(0), SXCount(0) {}
		uint64	NextReset;
		uint16	SXCount;
		QList<uint64> Pending;
	};
	QMap<uint64, SSXTiming>			m_SXTiming;

	uint64							m_NextIPRequest;

	SMyInfo							m_MyInfo;
	QString							m_Version;

	bool							m_bEnabled;

	uint64							m_LastCallbacksMustWait;

	CFWWatch						m_FWWatch;

	// AICH Stuff
	struct SAICHReq
	{
		SAICHReq(uint64 TimeOut = 0) {ASSERT(TimeOut); uTimeOut = TimeOut;}
		QList<uint16>	Parts;
		uint64			uTimeOut;
	};
	QMap<uint64, QMap<CMuleClient*, SAICHReq> >	m_AICHQueue;

	CScoped<CPrivateKey>			m_PrivateKey;
	CScoped<CPublicKey>				m_PublicKey;

	STransferStats					m_TransferStats;
};

bool PackPacket(CBuffer& Packet);
void UnpackPacket(CBuffer& Buffer);