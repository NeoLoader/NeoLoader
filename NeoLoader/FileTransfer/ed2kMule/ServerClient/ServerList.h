#pragma once
//#include "GlobalHeader.h"

#include "../../../../Framework/ObjectEx.h"
#include "../../../../Framework/Address.h"
#include "../MuleTags.h"
#include "../../../FileSearch/SearchManager.h"
class CMuleServer;
class CEd2kServer;
class CAbstractSearch;
struct SSearchRoot;

class CServerList : public QObjectEx
{
    Q_OBJECT

public:
	CServerList(CMuleServer* pServer, QObject* qObject = 0);

	void							Process(UINT Tick);

	bool							IsFirewalled(CAddress::EAF eAF) const					{return m_LastFirewalled.value(eAF, true);}
	CAddress						GetAddress(CAddress::EAF eAF) const						{return m_LastAddress.value(eAF);}
	void							SetAddress(const CAddress& Address, bool bFirewalled)	{m_LastAddress[Address.Type()] = Address; m_LastFirewalled[Address.Type()] = bFirewalled;}

	CEd2kServer*					GetServer(const CAddress& Address, uint16 uPort);
	CEd2kServer*					FindServer(const CAddress& Address, uint16 uPort, bool bAdd = false);
	CEd2kServer*					FindServer(const QString& Url, bool bAdd = false);
	
	void							RemoveServer(CEd2kServer* pServer);

	const QList<CEd2kServer*>&		GetServers()											{return m_Servers;}

	CEd2kServer*					GetConnectedServer();

	QString							GetServerStatus(CFile* pFile, const QString& Url, uint64* pNext = NULL);

	bool							FindSources(CFile* pFile);
	
	void							SendSvrPacket(CBuffer& Packet, uint8 Prot, CEd2kServer* pServer);
	void							ProcessSvrPacket(CBuffer& Packet, quint8 Prot, const CAddress& Address, quint16 uUDPPort);

	void							PingServer(CEd2kServer* pServer);

	void							RequestSources(QList<CFile*> Files, CEd2kServer* pServer);
	void							FindFiles(const SSearchRoot& SearchRoot, CEd2kServer* pServer);

	void							StartSearch(CAbstractSearch* pSearch);
	void							StopSearch(CAbstractSearch* pSearch);

	void							AddFoundFile(CFile* pFile, CEd2kServer* pServer);
	void							FinishSearch(CEd2kServer* pServer, bool bMore = false);
	void							BeginSearch(CEd2kServer* pServer);

	void							Store();
	void							Load();

private slots:
	void							ProcessSvrPacket(QByteArray Packet, quint8 Prot, CAddress Address, quint16 uUDPPort);

signals:
	void							SendSvrPacket(QByteArray Packet, quint8 Prot, CAddress Address, quint16 uSvrPort, quint32 UDPKey);

protected:
	QList<CEd2kServer*>				m_Servers;
	bool							m_Changed;

	QMap<CAddress::EAF,bool>		m_LastFirewalled;
	QMap<CAddress::EAF,CAddress>	m_LastAddress;

	QMultiMap<CAbstractSearch*, CEd2kServer*> m_RunningSearches;
	uint32							m_SearchIDCounter;

	uint64							m_GlobalSearchTimeOut;
};

struct SSearchRoot
{
	SSearchRoot(CAbstractSearch* pSearch);
	~SSearchRoot()
	{
		delete pSearchTree;
	}

	SSearchTree* pSearchTree;

	wstring typeText;
	wstring extension;
	uint64 minSize;
	uint64 maxSize;
	uint32 availability;
};

void WriteSearchTree(CBuffer& Packet, const SSearchRoot& SearchRoot, bool bSupports64bit = true, bool bSupportsUnicode = true);
//SSearchTree* ReadSearchTree(CBuffer& Packet, SSearchRoot* pSearchRoot, bool bUnicode)