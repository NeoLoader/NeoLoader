#include "GlobalHeader.h"
#include "ServerList.h"
#include "Ed2kServer.h"
#include "ServerClient.h"
#include "../../../NeoCore.h"
#include "../../../Networking/SocketThread.h"
#include "../MuleManager.h"
#include "../MuleServer.h"
#include "../../../../Framework/qzlib.h"
#include "../../../../Framework/Exception.h"
#include "../../../../Framework/Strings.h"
#include "../../../FileSearch/Search.h"
#include "../../../../Framework/Xml.h"

#define GLOBAL_SERVER ((CEd2kServer*)-1)

CServerList::CServerList(CMuleServer* pServer, QObject* qObject)
 : QObjectEx(qObject)
{
	m_SearchIDCounter = 0;
	m_GlobalSearchTimeOut = -1;

	Load();

	m_Changed = false;

	connect(pServer, SIGNAL(ProcessSvrPacket(QByteArray, quint8, CAddress, quint16)), this, SLOT(ProcessSvrPacket(QByteArray, quint8, CAddress, quint16)));
	connect(this, SIGNAL(SendSvrPacket(QByteArray, quint8, CAddress, quint16, quint32)), pServer, SLOT(SendSvrPacket(QByteArray, quint8, CAddress, quint16, quint32)));
}

void CServerList::Process(UINT Tick)
{
	if(!theCore->m_Network->IsConnectable())
		return; // VPN is down deny connecting

	QMultiMap<CEd2kServer*, CFile*> DynamicServers;

	if(theCore->Cfg()->GetString("Ed2kMule/UseServers") != "None") // None, Custom, Static, One, Booth
	{
		QStringList List = theCore->Cfg()->GetStringList("Ed2kMule/StaticServers");

		// Assemble Dynamic Server List and File Lists
		bool bNoCustom = theCore->Cfg()->GetString("Ed2kMule/UseServers") != "Static";
		foreach(CFile* pFile, CFileList::GetAllFiles())
		{
			CFileHash* pHash = pFile->GetHash(HashEd2k);
			if(pFile->IsRemoved() || !pHash || !pHash->IsValid())
				continue;

			if(pFile->IsDuplicate())
				continue;

			if(!pFile->IsStarted() || !pFile->IsEd2kShared())
				continue;

			QStringList Servers = pFile->GetProperty("Ed2kServers").toStringList();
			if(!bNoCustom || Servers.isEmpty())
				DynamicServers.insert(GLOBAL_SERVER, pFile);
			else 
			{
				foreach(const QString& Url, Servers)
				{
					CAddress Address;
					uint16 Port = 0;
					if(CMuleSource::ParseUrl(Url, Address, Port, "server"))
					{
						CEd2kServer* pServer = FindServer(Address, Port);
						if(!pServer)
						{
							pServer = new CEd2kServer(this);
							pServer->SetAddress(Address, Port);
							m_Servers.append(pServer);
						}
						DynamicServers.insert(pServer, pFile);
					}
				}
			}
		}

		// Update Dynamis Server File Lists
		foreach(CEd2kServer* pServer, DynamicServers.uniqueKeys())
		{
			if(pServer == GLOBAL_SERVER)
				continue;

			QList<CFile*> OnServer = pServer->GetFiles();
			foreach(CFile* pFile, DynamicServers.values(pServer))
			{
				if(!OnServer.removeOne(pFile))
					pServer->AddFile(pFile);
			}
			foreach(CFile* pFile, OnServer)
				pServer->RemoveFile(pFile);
		}
		// Now we know all files on dynamic servers are consistent

		int Index = 0;
		QList<CFile*> GlobalFiles = theCore->Cfg()->GetString("Ed2kMule/UseServers") == "One" ? DynamicServers.values(GLOBAL_SERVER) : DynamicServers.values();
		foreach(CEd2kServer* pServer, m_Servers)
		{
			if(!pServer->IsStatic())
				continue;

			// Prevent fast reconnect 
			if(pServer->GetLastDisconnected() && GetCurTick() - pServer->GetLastDisconnected() < MIN2MS(5))
				continue;
		
			QList<CFile*> OnServer = pServer->GetFiles();
			uint32 Limit = pServer->GetSoftLimit();
			if(!Limit)
				Limit = 50;
			for(uint32 Count = 0; Index < GlobalFiles.size() && Count < Limit; Count++)
			{
				CFile* pFile = GlobalFiles.at(Index++);
				if(!OnServer.removeOne(pFile))
					pServer->AddFile(pFile);
			}
			foreach(CFile* pFile, OnServer)
				pServer->RemoveFile(pFile);
		}
	}
	else // Note: we must ensure the servers file list is valid or empty
	{
		foreach(CEd2kServer* pServer, m_Servers)
			pServer->RemoveAllFiles();
	}

	// ping all servers
	foreach(CEd2kServer* pServer, m_Servers)
	{
		pServer->Process(Tick);

		// remove all unused dynamic servers
		if(!pServer->IsStatic() && !pServer->GetClient())
		{
			RemoveServer(pServer);
			continue;
		}

		if(!pServer->GetLastPingAnswer() || GetCurTick() - pServer->GetLastPingAnswer() > HR2MS(4))
		{
			if(!pServer->GetLastPingAttempt() || (GetCurTick() - pServer->GetLastPingAttempt() > SEC2MS(20) * pServer->GetPingAttempts()))
			{
				if(pServer->GetPingAttempts() < 4)
					PingServer(pServer);
				else // 1 crypto ping and 3 normal ping failed
				{
					pServer->SetLastPingAnswer(); // retry in 4 hours again
					pServer->SetChallenge(0);
				}
			}
		}
	}


	if(m_GlobalSearchTimeOut < GetCurTick())
	{
		m_GlobalSearchTimeOut = -1;
		FinishSearch(GLOBAL_SERVER);
	}

	if(m_Changed)
		Store();
}

CEd2kServer* CServerList::FindServer(const CAddress& Address, uint16 uPort, bool bAdd)
{
	foreach(CEd2kServer* pServer, m_Servers)
	{
		if(pServer->GetAddress().CompareTo(Address) && pServer->GetPort() == uPort)
			return pServer;
	}
	if(bAdd)
	{
		CEd2kServer* pServer = new CEd2kServer(this);
		pServer->SetAddress(Address, uPort);
		m_Servers.append(pServer);
		m_Changed = true;
		return pServer;
	}
	return NULL;
}

CEd2kServer* CServerList::FindServer(const QString& Url, bool bAdd)
{
	CAddress Address;
	uint16 Port = 0;
	if(!CMuleSource::ParseUrl(Url, Address, Port, "server"))
		return NULL;
	return FindServer(Address, Port, bAdd);
}

void CServerList::RemoveServer(CEd2kServer* pServer)
{
	if(m_Servers.removeOne(pServer))
	{
		m_Changed = true;
		delete pServer;
	}
}

CEd2kServer* CServerList::GetConnectedServer()
{
	foreach(CEd2kServer* pServer, m_Servers)
	{
		if(CServerClient* pClient = pServer->GetClient())
		{
			if(pClient->GetClientID())
				return pServer;
		}
	}
	return NULL;
}

QString	CServerList::GetServerStatus(CFile* pFile, const QString& Url, uint64* pNext)
{
	CAddress Address;
	uint16 Port = 0;
	if(!CMuleSource::ParseUrl(Url, Address, Port, "server"))
		return "Error: Invalid URL";

	CEd2kServer* pServer = CServerList::FindServer(Address, Port);
	if(!pServer)
		return "No Used";

	CEd2kServer::SFile* File = pServer->GetFile(pFile);
	if(!File)
		return "No Assigned";
	
	CServerClient* pClient = pServer->GetClient();
	if(!pClient)
	{
		if(pServer->GetConnectionFails() > 0)
		{
			if(pNext)
			{
				uint64 uNow = GetCurTick();
				uint64 uWait = -1;
				if(pServer->GetConnectionFails() < 5)
					uWait = SEC2MS(10);
				else if(pServer->GetConnectionFails() < 10)
					uWait = MIN2MS(30);
				uint64 uTimePassed = pServer->GetLastConnectAttempt() ? (uNow - pServer->GetLastConnectAttempt()) : 0;
				*pNext = (uWait != -1 && uTimePassed < uWait) ? uWait - uTimePassed : 0;
			}
			return "ERROR: Connection Failed";
		}
		else
			return "Disconnected";
	}

	if(pClient->GetClientID() == 0)
		return "Connecting";

	if(pNext)
	{
		uint64 uNow = GetCurTick();
		if(File->uNextReask > uNow)
			*pNext = File->uNextReask - uNow;
		else if(pServer->GetNextRequestFrame() > uNow)
			*pNext = pServer->GetNextRequestFrame() - uNow;
	}

	QString Info = "Connected";
	//(!pFile->IsComplete() && File->uNextReask == 0)
	//Info += ", Waiting";
	if(File->bPulished)
		Info += ", Published";
	return Info;
}

bool CServerList::FindSources(CFile* pFile)
{
	if(!pFile->IsEd2kShared())
		return true; // finished without error
	
	int Count = 0;
	foreach(CEd2kServer* pServer, m_Servers)
	{
		if(pServer->GetFile(pFile) && pServer->GetClient() && pServer->GetClient()->GetClientID())
		{
			pServer->GetClient()->RequestSources(pFile);
			Count++;
		}
	}
	return Count > 0;
}

void CServerList::PingServer(CEd2kServer* pServer)
{
	uint32 uChallenge;
	//if(pServer->GetChallenge() == 0 || (pServer->GetChallenge() & 0xFFFF0000) == 0x55AA0000) // if last ping failed and was crypto dont try crypto again
	if(pServer->GetChallenge() == 0) // last ping was ok, try clrypto
	{
		size_t Pading = rand() % 16;
		CBuffer Ping(4 + Pading);
		uChallenge = (rand() << 17) | (rand() << 2) | (rand() & 0x03);
		if(uChallenge == 0)
			uChallenge = 1;
		Ping.WriteValue<uint32>(uChallenge);
		if(Pading)
			Ping.WriteData(CAbstractKey(Pading, true).GetKey(), Pading);

		emit SendSvrPacket(Ping.ToByteArray(), 0, pServer->GetIP(), pServer->GetPort() + 12, uChallenge);
	}
	else
	{
		CBuffer Ping(1 + 4);
		Ping.WriteValue<uint8>(OP_GLOBSERVSTATREQ);
		uChallenge = 0x55AA0000 + (rand() & 0x0000FFFF);
		Ping.WriteValue<uint32>(uChallenge);
		
		emit SendSvrPacket(Ping.ToByteArray(), OP_EDONKEYPROT, pServer->GetIP(), pServer->GetPort() + 4, 0);
	}
	pServer->SetChallenge(uChallenge);
	pServer->SetLastPingAttempt();
}

void CServerList::SendSvrPacket(CBuffer& Packet, uint8 Prot, CEd2kServer* pServer)
{
	ASSERT(Prot);
	ASSERT(Prot != OP_PACKEDPROT);
	emit SendSvrPacket(Packet.ToByteArray(), Prot, pServer->GetIP(), pServer->GetUDPPort(), pServer->GetUDPKey());
}

void CServerList::ProcessSvrPacket(QByteArray PacketArr, quint8 Prot, CAddress Address, quint16 uUDPPort)
{
	try
	{
		CBuffer Buffer(PacketArr, true);
		ASSERT(Prot != OP_PACKEDPROT);
		ProcessSvrPacket(Buffer, Prot, Address, uUDPPort);
	}
	catch(const CException& Exception)
	{
		LogLine(Exception.GetFlag(), tr("recived malformated packet form ed2m server udp; %1").arg(QString::fromStdWString(Exception.GetLine())));
	}
}

CEd2kServer* CServerList::GetServer(const CAddress& Address, uint16 uUDPPort)
{
	foreach(CEd2kServer* pServer, m_Servers)
	{
		if(pServer->GetAddress().CompareTo(Address) && (pServer->GetPort() + 4 == uUDPPort || pServer->GetPort() + 12 == uUDPPort || pServer->GetUDPPort() == uUDPPort))
			return pServer;
	}
	return NULL;
}

void CServerList::ProcessSvrPacket(CBuffer& Packet, quint8 Prot, const CAddress& Address, quint16 uUDPPort)
{
	CEd2kServer* pServer = GetServer(Address, uUDPPort);

	uint8 uOpcode = Packet.ReadValue<uint8>();
	switch(Prot)
	{
		case OP_EDONKEYPROT:
		{
			switch(uOpcode)
			{
				case OP_GLOBSEARCHRES:
				{
					do // now how sick is that?
					{
						if(CFile* pFile = pServer->ReadFile(Packet))
							AddFoundFile(pFile, GLOBAL_SERVER);
					}
					while(Packet.GetSizeLeft() > 2 && Packet.ReadValue<uint8>() == OP_EDONKEYPROT && Packet.ReadValue<uint8>() == OP_GLOBSEARCHRES);
					break;
				}
				case OP_GLOBFOUNDSOURCES:
				{
					do // now how sick is that?
					{
						pServer->AddSources(Packet, false);
					}
					while(Packet.GetSizeLeft() > 2 && Packet.ReadValue<uint8>() == OP_EDONKEYPROT && Packet.ReadValue<uint8>() == OP_GLOBFOUNDSOURCES);
					break;
				}
				case OP_GLOBSERVSTATRES:
				{
					uint32 uChallenge = Packet.ReadValue<uint32>();
					if(pServer->GetChallenge() != uChallenge)
						throw CException(LOG_DEBUG, L"Server Challenge invalid");
					pServer->SetChallenge(0);
					pServer->SetLastPingAnswer();

					uint32 UserCount	= Packet.ReadValue<uint32>();
					uint32 FileCount	= Packet.ReadValue<uint32>();
					uint32 UserLimit	= Packet.GetSizeLeft() >= 4 ? Packet.ReadValue<uint32>() : 0;
					uint32 SoftLimit	= Packet.GetSizeLeft() >= 4 ? Packet.ReadValue<uint32>() : 0;
					uint32 HardLimit	= Packet.GetSizeLeft() >= 4 ? Packet.ReadValue<uint32>() : 0;
					uint32 uFlags		= Packet.GetSizeLeft() >= 4 ? Packet.ReadValue<uint32>() : 0;
					uint32 LowIDCount	= Packet.GetSizeLeft() >= 4 ? Packet.ReadValue<uint32>() : 0;
					uint16 UDPPort		= Packet.GetSizeLeft() >= 2 ? Packet.ReadValue<uint16>() : 0;
					uint16 CryptoPort	= Packet.GetSizeLeft() >= 2 ? Packet.ReadValue<uint16>() : 0;
					uint32 UDPKey		= Packet.GetSizeLeft() >= 4 ? Packet.ReadValue<uint32>() : 0;

					pServer->SetFlagsUDP(uFlags);
					pServer->SetCryptoPort(CryptoPort);
					pServer->SetUDPPort(UDPPort, UDPKey);
					pServer->SetLimits(SoftLimit, HardLimit);
					pServer->SetFileCount(FileCount);
					pServer->SetUserLimit(UserLimit);
					pServer->SetUserCount(UserCount, LowIDCount);

					// Comment copyed form eMule (Fair Use):
					//	eserver 16.45+ supports a new OP_SERVER_DESC_RES answer, if the OP_SERVER_DESC_REQ contains a uint32
					//	challenge, the server returns additional info with OP_SERVER_DESC_RES. To properly distinguish the
					//	old and new OP_SERVER_DESC_RES answer, the challenge has to be selected carefully. The first 2 bytes 
					//	of the challenge (in network byte order) MUST NOT be a valid string-len-int16!

					CBuffer Request(1 + 4);
					Request.WriteValue<uint8>(OP_SERVER_DESC_REQ);
					uint32 uDescChallenge = ((uint32)rand() << 16) + 0xF0FF; // 0xF0FF = an 'invalid' string length.
					Request.WriteValue<uint32>(uDescChallenge);
					SendSvrPacket(Request, OP_EDONKEYPROT, pServer);
					break;
				}
				case OP_SERVER_DESC_RES:
				{
					// Comment copyed form eMule (Fair Use):
					// old packet: <name_len 2><name name_len><desc_len 2 desc_en>
					// new packet: <challenge 4><taglist>
					//
					// NOTE: To properly distinguish between the two packets which are both useing the same opcode...
					// the first two bytes of <challenge> (in network byte order) have to be an invalid <name_len> at least.

					QString Name;
					QString Description;
					QString Version;

					uint32 uDescChallenge = Packet.ReadValue<uint32>();
					if((uDescChallenge & 0xFFFF) == 0xF0FF) // new format
					{
						QVariantMap Tags = CMuleTags::ReadTags(&Packet);
						for(QVariantMap::iterator I = Tags.begin(); I != Tags.end(); ++I)
						{
							const QVariant& Value = I.value();
							switch(FROM_NUM(I.key()))
							{
								case ST_SERVERNAME:
									Name = Value.toString(); break;
								case ST_DESCRIPTION:
									Description = Value.toString(); break;
								case ST_VERSION:
									if(Value.type() == QVariant::String)
										Version = Value.toString();
									else
										Version = QString("%1.%2").arg(Value.toUInt() >> 16).arg(Value.toUInt() & 0xFFFF);
								case ST_DYNIP:
									//Value.toString(); // host name
									break;
								case ST_AUXPORTSLIST:
									// <string> = <port> [, <port>...]
									break; 
								default:
									LogLine(LOG_DEBUG, tr("Recived unknwon Ed2k Server Info tag: %1").arg(I.key()));
							}
						}
					}
					else // old format
					{
						Packet.SetPosition(1);
						Name = QString::fromStdWString(Packet.ReadString(CBuffer::eAscii));
						Description = QString::fromStdWString(Packet.ReadString(CBuffer::eAscii));
					}

					pServer->SetName(Name);
					pServer->SetVersion(Version);
					pServer->SetDescription(Description);

					break;
				}
				case OP_NAT_CALLBACKREQUESTED_UDP:
				{
					if(CServerClient* pClient = pServer->GetClient())
					{
						Packet.SetPosition(0);
						pClient->ProcessPacket(Packet, Prot);
					}
					break;
				}
				
				default:
					LogLine(LOG_DEBUG, tr("Recived unknwon EServer UDP packet Opcode: %1").arg(uOpcode));
			}
			break;
		}

		default:
			LogLine(LOG_DEBUG, tr("Recived unknwon EServer UDP packet protocol: %1").arg(Prot));
	}
}

void CServerList::RequestSources(QList<CFile*> Files, CEd2kServer* pServer)
{
	CBuffer Packet;
	int Count = 0;
	foreach(CFile* pFile, Files)
	{
		if(Packet.GetSize() == 0)
			Packet.WriteValue<uint8>(pServer->SupportsExtGetSources2() ? OP_GLOBGETSOURCES2 : OP_GLOBGETSOURCES);
		Count++;
		CFileHash* pHash = pFile->GetHash(HashEd2k);
		ASSERT(pHash);
		Packet.WriteQData(pHash->GetHash());
		if(pServer->SupportsExtGetSources2())
		{
			if(!IsLargeEd2kMuleFile(pFile->GetFileSize()))
				Packet.WriteValue<uint32>(pFile->GetFileSize());
			else
			{
				ASSERT(pServer->SupportsLargeFiles());
				Packet.WriteValue<uint32>(0);
				Packet.WriteValue<uint64>(pFile->GetFileSize());
			}
		}
		// Check if the packet is full and if so send it
		if(!pServer->SupportsExtGetSources() || Packet.GetSize() >= MAX_UDP_PACKET_DATA + 1 || Count >= MAX_REQUESTS_PER_SERVER)
		{
			SendSvrPacket(Packet, OP_PACKEDPROT, pServer);
			Packet.SetBuffer(NULL, 0); // rese Buffer
			Count = 0;
		}
	}
	if(Packet.GetSize() > 0)
		SendSvrPacket(Packet, OP_PACKEDPROT, pServer);
}

void CServerList::FindFiles(const SSearchRoot& SearchRoot, CEd2kServer* pServer)
{
	CBuffer Packet;
	if(pServer->SupportsLargeFiles() && pServer->SupportsExtGetFiles())
	{
		Packet.WriteValue<uint8>(OP_GLOBSEARCHREQ3);

		QVariantMap Tags;
		Tags[TO_NUM(CT_SERVER_UDPSEARCH_FLAGS)]	= SRVCAP_UDP_NEWTAGS_LARGEFILES | SRVCAP_UDP_NEWTAGS_IPv6;
		CMuleTags::WriteTags(Tags, &Packet);

		WriteSearchTree(Packet, SearchRoot, true);
	}
	else 
	{
		Packet.WriteValue<uint8>(pServer->SupportsExtGetFiles() ? OP_GLOBSEARCHREQ2 : OP_GLOBSEARCHREQ);

		WriteSearchTree(Packet, SearchRoot, false);
	}
	SendSvrPacket(Packet, OP_EDONKEYPROT, pServer);
}

SSearchRoot::SSearchRoot(CAbstractSearch* pSearch)
{
	pSearchTree		= CSearchManager::MakeSearchTree(pSearch->GetExpression());
	typeText		= pSearch->GetCriteria("Ed2kFileType").toString().toStdWString();
	extension		= pSearch->GetCriteria("FileExt").toString().toStdWString();
	minSize			= pSearch->GetCriteria("MinSize").toULongLong();
	maxSize			= pSearch->GetCriteria("MaxSize").toULongLong();
	availability	= pSearch->GetCriteria("Availability").toUInt();
}

void CServerList::StartSearch(CAbstractSearch* pSearch)
{
	ASSERT(!pSearch->IsRunning());
	
	if(pSearch->GetExpression().isEmpty())
		return;

	SSearchRoot SearchRoot(pSearch);
	if(!SearchRoot.pSearchTree)
	{
		pSearch->SetError("Invalie Search Query");
		LogLine(LOG_ERROR, tr("Invalie Search Query: %1").arg(pSearch->GetExpression()));
	}

	m_SearchIDCounter++;
	pSearch->SetStarted(m_SearchIDCounter);

	foreach(CEd2kServer* pServer, m_Servers)
	{
		if(pServer->GetClient() && pServer->GetClient()->GetClientID())
		{
			pServer->GetClient()->FindFiles(SearchRoot);
			m_RunningSearches.insert(pSearch, pServer);
		}
	}

	if(theCore->Cfg()->GetString("Ed2kMule/UseServers") != "None")
	{
		// if we are not connected to any server start a connection to one
		if(!m_RunningSearches.contains(pSearch))
		{
			foreach(CEd2kServer* pServer, m_Servers)
			{
				if(!pServer->GetLastPingAnswer())
					continue;
				QTimer::singleShot(0, pServer, SLOT(Connect()));
				m_RunningSearches.insert(pSearch, pServer);
				break;
			}
		}
	}

	// if no global search is still pending, start it
	if(m_GlobalSearchTimeOut == -1)
	{
		m_GlobalSearchTimeOut = GetCurTick() + SEC2MS(5);
		foreach(CEd2kServer* pServer, m_Servers)
		{
			if(!pServer->GetClient())
				FindFiles(SearchRoot, pServer);
		}
		m_RunningSearches.insert(pSearch, GLOBAL_SERVER);
	}
}

void CServerList::BeginSearch(CEd2kServer* pServer)
{
	foreach(CAbstractSearch* pSearch, m_RunningSearches.keys(pServer))
	{
		SSearchRoot SearchRoot(pSearch);
		pServer->GetClient()->FindFiles(SearchRoot);
	}
}

void CServerList::AddFoundFile(CFile* pFile, CEd2kServer* pServer)
{
	if(CAbstractSearch* pSearch = m_RunningSearches.key(pServer))
		pSearch->AddFoundFile(pFile);
	else
	{
		LogLine(LOG_ERROR | LOG_DEBUG, tr("Got result for a unknown ed2k search"));
		delete pFile;
	}
}

void CServerList::FinishSearch(CEd2kServer* pServer, bool bMore)
{
	foreach(CAbstractSearch* pSearch, m_RunningSearches.keys(pServer))
	{
		if(pServer != GLOBAL_SERVER && bMore && pSearch->GetResultCount() < pSearch->GetCriteria("MinResults").toInt())
			pServer->GetClient()->FindMore();
		else
		{
			m_RunningSearches.remove(pSearch, pServer);
			if(!m_RunningSearches.contains(pSearch))
				pSearch->SetStopped();
		}
	}
}

void CServerList::StopSearch(CAbstractSearch* pSearch)
{
	ASSERT(pSearch->IsRunning());

	pSearch->SetStopped();
	m_RunningSearches.remove(pSearch);
}

void CServerList::Store()
{
	QStringList Servers;
	foreach(CEd2kServer* pServer, m_Servers)
	{
		if(!pServer->IsStatic())
			continue;
		Servers.append(pServer->GetUrl());
	}

	theCore->Cfg()->setValue("Ed2kMule/StaticServers", Servers);

	m_Changed = false;
}

void CServerList::Load()
{
	foreach(const QString& Url, theCore->Cfg()->GetStringList("Ed2kMule/StaticServers"))
	{
		CAddress Address;
		uint16 Port = 0;
		if(!CMuleSource::ParseUrl(Url, Address, Port, "server"))
			continue;
		
		CEd2kServer* pServer = new CEd2kServer(this);
		pServer->SetAddress(Address, Port);
		m_Servers.append(pServer);
		pServer->SetStatic(true);
	}
}


////////////////////////////////////////////////
//

class CSearchTreeWriter
{
public:
	CSearchTreeWriter(CBuffer* pBuffer, bool bSupports64bit, bool bSupportsUnicode)
		: m_pBuffer(pBuffer),
		  m_bSupports64bit(bSupports64bit),
		  m_bSupportsUnicode(bSupportsUnicode)
	{
	}

	void WriteAND()
	{
		m_pBuffer->WriteValue<uint8>(0);						// boolean operator parameter type
		m_pBuffer->WriteValue<uint8>(0x00);						// "AND"
	}

	void WriteOR()
	{
		m_pBuffer->WriteValue<uint8>(0);						// boolean operator parameter type
		m_pBuffer->WriteValue<uint8>(0x01);						// "OR"
	}

	void WriteNOT()
	{
		m_pBuffer->WriteValue<uint8>(0);						// boolean operator parameter type
		m_pBuffer->WriteValue<uint8>(0x02);						// "NOT"
	}

	void WriteString(const wstring& Value)
	{
		m_pBuffer->WriteValue<uint8>(1);						// string parameter type
		m_pBuffer->WriteString(Value,							// string value
			m_bSupportsUnicode ? CBuffer::eUtf8 : CBuffer::eAscii);
	}

	void WriteParam(uint8 TagID, const wstring& Value, bool bForceAscii = false)
	{
		m_pBuffer->WriteValue<uint8>(2);						// string parameter type
		m_pBuffer->WriteString(Value,							// string value
			!bForceAscii && m_bSupportsUnicode ? CBuffer::eUtf8 : CBuffer::eAscii);	
		m_pBuffer->WriteValue<uint16>(sizeof(uint8));			// meta tag ID length
		m_pBuffer->WriteValue<uint8>(TagID);					// meta tag ID name
	}
	
	//void WriteParam(const wstring& TagID, const wstring& Value)
	//{
	//	m_pBuffer->WriteValue<uint8>(2);						// string parameter type
	//	m_pBuffer->WriteString(Value, m_eStrEncode);			// string value
	//	m_pBuffer->WriteString(TagID);							// meta tag ID
	//}

	void WriteParam(uint8 TagID, uint8 uOperator, uint64 Value)
	{
		bool Need64bit = Value > 0xFFFFFFFF;
		if (Need64bit && m_bSupports64bit) 
		{
			m_pBuffer->WriteValue<uint8>(8);					// numeric parameter type (int64)
			m_pBuffer->WriteValue<uint64>(Value);				// numeric value
		} 
		else 
		{
			if (Need64bit)
				Value = 0xFFFFFFFF;
			m_pBuffer->WriteValue<uint8>(3);					// numeric parameter type (int32)
			m_pBuffer->WriteValue<uint32>(Value);				// numeric value
		}
		m_pBuffer->WriteValue<uint8>(uOperator);				// comparison operator
		m_pBuffer->WriteValue<uint16>(sizeof(uint8));			// meta tag ID length
		m_pBuffer->WriteValue<uint8>(TagID);					// meta tag ID name
	}

	//void WriteParam(const wstring& TagID, uint8 uOperator, uint64 Value)
	//{
	//	bool Need64bit = Value > 0xFFFFFFFF;
	//	if (Need64bit && m_bSupports64bit) 
	//	{
	//		m_pBuffer->WriteValue<uint8>(8);					// numeric parameter type (int64)
	//		m_pBuffer->WriteValue<uint64>(Value);				// numeric value
	//	} 
	//	else 
	//	{
	//		if (Need64bit)
	//			Value = 0xFFFFFFFF;
	//		m_pBuffer->WriteValue<uint8>(3);					// numeric parameter type (int32)
	//		m_pBuffer->WriteValue<uint32>(Value);				// numeric value
	//	}
	//	m_pBuffer->WriteValue<uint8>(uOperator);				// comparison operator
	//	m_pBuffer->WriteString(TagID);							// meta tag ID
	//}

	void WriteStrings(vector<wstring> Value)
	{
		wstring String;
		for(int i = 0; i < Value.size(); i++)
		{
			if(i > 0)
				String += L" ";
			String += Value.at(i);
		}
		WriteString(String);
	}

	void WriteTree(SSearchTree* pSearchTree)
	{
		if(!pSearchTree)
			return;
		if(pSearchTree->Type == SSearchTree::String)
		{
			/*for(int i = 0; i < pSearchTree->Strings.size(); i++)
			{
				if(i > 0)
					WriteAND();
				WriteParam(pSearchTree->Strings.at(i));
			}*/
			WriteStrings(pSearchTree->Strings);
		}
		else
		{
			if(pSearchTree->Type == SSearchTree::AND)
				WriteAND();
			else if(pSearchTree->Type == SSearchTree::OR)
				WriteOR();
			else if(pSearchTree->Type == SSearchTree::NAND)
				WriteNOT();
			WriteTree(pSearchTree->Left);
			WriteTree(pSearchTree->Right);
		}
	}

protected:
	CBuffer*			m_pBuffer;
	bool				m_bSupportsUnicode;
	bool				m_bSupports64bit;
};

void WriteSearchTree(CBuffer& Packet, const SSearchRoot& SearchRoot, bool bSupports64bit, bool bSupportsUnicode)
{
	int iParameterTotal = 0;
	if ( !SearchRoot.typeText.empty() )		iParameterTotal++;
	if ( SearchRoot.minSize > 0 )			iParameterTotal++;
	if ( SearchRoot.maxSize > 0 )			iParameterTotal++;
	if ( SearchRoot.availability > 0 )		iParameterTotal++;
	if ( !SearchRoot.extension.empty() )	iParameterTotal++;

	wstring typeText = SearchRoot.typeText;
	if (typeText == ED2KFTSTR_ARCHIVE){
		// Comment copyed form eMule (Fair Use):
		// eDonkeyHybrid 0.48 uses type "Pro" for archives files
		// www.filedonkey.com uses type "Pro" for archives files
		typeText = ED2KFTSTR_PROGRAM;
	} else if (typeText == ED2KFTSTR_CDIMAGE){
		// Comment copyed form eMule (Fair Use):
		// eDonkeyHybrid 0.48 uses *no* type for iso/nrg/cue/img files
		// www.filedonkey.com uses type "Pro" for CD-image files
		typeText = ED2KFTSTR_PROGRAM;
	}
	
	CSearchTreeWriter SearchTree(&Packet, bSupports64bit, bSupportsUnicode);

	int iParameterCount = 0;
	if (SearchRoot.pSearchTree->Type == SSearchTree::String) // && SearchRoot.pSearchTree->Strings.size() <= 1)
	{
		// Comment copyed form eMule (Fair Use):
		// lugdunummaster requested that searchs without OR or NOT operators,
		// and hence with no more expressions than the string itself, be sent
		// using a series of ANDed terms, intersecting the ANDs on the terms 
		// (but prepending them) instead of putting the boolean tree at the start 
		// like other searches. This type of search is supposed to take less load 
		// on servers. Go figure.
		//
		// input:      "a" AND min=1 AND max=2
		// instead of: AND AND "a" min=1 max=2
		// we use:     AND "a" AND min=1 max=2

		if(SearchRoot.pSearchTree->Strings.size() > 0) 
		{
			iParameterTotal += 1;

			if (++iParameterCount < iParameterTotal)
				SearchTree.WriteAND();
			SearchTree.WriteStrings(SearchRoot.pSearchTree->Strings); //SearchTree.WriteParam(SearchRoot.pSearchTree->Strings.at(0));
		}

		if (!typeText.empty()) 
		{
			if (++iParameterCount < iParameterTotal)
				SearchTree.WriteAND();
			SearchTree.WriteParam(FT_FILETYPE, typeText, true);
		}
		
		if (SearchRoot.minSize > 0) 
		{
			if (++iParameterCount < iParameterTotal)
				SearchTree.WriteAND();
			SearchTree.WriteParam(FT_FILESIZE, 0x01, SearchRoot.minSize);
		}

		if (SearchRoot.maxSize > 0)
		{
			if (++iParameterCount < iParameterTotal) 
				SearchTree.WriteAND();
			SearchTree.WriteParam(FT_FILESIZE, 0x02, SearchRoot.maxSize);
		}
		
		if (SearchRoot.availability > 0)
		{
			if (++iParameterCount < iParameterTotal)
				SearchTree.WriteAND();
			SearchTree.WriteParam(FT_SOURCES, 0x01, SearchRoot.availability);
		}

		if (!SearchRoot.extension.empty())
		{
			if (++iParameterCount < iParameterTotal)
				SearchTree.WriteAND();
			SearchTree.WriteParam(FT_FILEFORMAT, SearchRoot.extension);
		}
		
		// ...

		// If this assert fails... we're seriously fucked up 
		
		ASSERT( iParameterCount == iParameterTotal );
		
	} 
	else 
	{
		if (!SearchRoot.extension.empty() && ++iParameterCount < iParameterTotal)
			SearchTree.WriteAND();

		if (SearchRoot.availability > 0 && ++iParameterCount < iParameterTotal)
			SearchTree.WriteAND();
	  
		if (SearchRoot.maxSize > 0 && ++iParameterCount < iParameterTotal)
			SearchTree.WriteAND();
        
		if (SearchRoot.minSize > 0 && ++iParameterCount < iParameterTotal)
			SearchTree.WriteAND();
        
		if (!typeText.empty() && ++iParameterCount < iParameterTotal)
			SearchTree.WriteAND();
 
		// ...

		// As above, if this fails, we're seriously fucked up.
		ASSERT( iParameterCount == iParameterTotal );

		SearchTree.WriteTree(SearchRoot.pSearchTree);

		if (!SearchRoot.typeText.empty())
			SearchTree.WriteParam(FT_FILETYPE, SearchRoot.typeText, true);

		if (SearchRoot.minSize > 0)
			SearchTree.WriteParam(FT_FILESIZE, 0x01, SearchRoot.minSize);

		if (SearchRoot.maxSize > 0)
			SearchTree.WriteParam(FT_FILESIZE, 0x02, SearchRoot.maxSize);

		if (SearchRoot.availability > 0)
			SearchTree.WriteParam(FT_SOURCES, 0x01, SearchRoot.availability);

		if (!SearchRoot.extension.empty())
			SearchTree.WriteParam(FT_FILEFORMAT, SearchRoot.extension);

		// ...
	}
}

/*#define EMPTY_SEARCHTREE (SSearchTree*)(-1)

SSearchTree* ReadSearchTree(CBuffer& Packet, SSearchRoot* pSearchRoot, bool bUnicode)
{
	uint8 Token = Packet.ReadValue<uint8>();
	switch(Token)
	{
		case 0x00: // node
		{
			SSearchTree* pSearchTerm = new SSearchTree;
			uint8 Operator = Packet.ReadValue<uint8>();
			switch(Operator)
			{
				case 0x00:  // AND
					pSearchTerm->Type = SSearchTree::AND;
					break;
				case 0x01: // OR
					pSearchTerm->Type = SSearchTree::OR;
					break;
				case 0x02: // NAND
					pSearchTerm->Type = SSearchTree::NAND;
					break;
				default:
					delete pSearchTerm;
					return NULL;
			}

			if ((pSearchTerm->Left = ReadSearchTree(Packet, pSearchRoot, bUnicode)) == NULL){
				delete pSearchTerm;
				return NULL;
			}

			if ((pSearchTerm->Right = ReadSearchTree(Packet, pSearchRoot, bUnicode)) == NULL){
				delete pSearchTerm;
				return NULL;
			}

			if(pSearchTerm->Left == EMPTY_SEARCHTREE && pSearchTerm->Right != EMPTY_SEARCHTREE)
			{
				SSearchTree* Temp = pSearchTerm;
				pSearchTerm = pSearchTerm->Right;
				delete Temp;
			}
			else if(pSearchTerm->Left != EMPTY_SEARCHTREE && pSearchTerm->Right == EMPTY_SEARCHTREE)
			{
				SSearchTree* Temp = pSearchTerm;
				pSearchTerm = pSearchTerm->Left;
				delete Temp;
			}
			else if(pSearchTerm->Left == EMPTY_SEARCHTREE && pSearchTerm->Right == EMPTY_SEARCHTREE)
			{
				delete pSearchTerm;
				pSearchTerm = EMPTY_SEARCHTREE;
			}

			return pSearchTerm;
		}
		case 0x01: // String
		{
			wstring str = Packet.ReadString(bUnicode ? CBuffer::eUtf8 : CBuffer::eAscii);

			SSearchTree* pSearchTerm = new SSearchTree;
			pSearchTerm->Type = SSearchTree::String;
			pSearchTerm->Strings = SplitStr(str, L" ()[]{}<>,._-!?:;\\/\"", false, true);
			return pSearchTerm;
		}
		case 0x02: // Meta tag
		{
			wstring strValue = Packet.ReadString(bUnicode ? CBuffer::eUtf8 : CBuffer::eAscii);

			uint16 Len = Packet.ReadValue<uint16>();
			byte* Name = Packet.ReadData(Len);

			switch(Name[0])
			{
				case FT_FILEFORMAT:
					pSearchRoot->extension = strValue;
					break;
				case FT_FILETYPE:
					pSearchRoot->typeText = strValue;
					break;
			}

			return EMPTY_SEARCHTREE;
		}
		case 0x03:
		case 0x08: // Numeric Relation (0x03=32-bit or 0x08=64-bit)
		{
			uint64 uValue = (Token == 0x03) ? Packet.ReadValue<uint32>() : Packet.ReadValue<uint64>();

			uint8 Operator = Packet.ReadValue<uint8>();
			if (Operator >= 0x06)
				return NULL;

			uint16 Len = Packet.ReadValue<uint16>();
			byte* Name = Packet.ReadData(Len);

			switch((BYTE)Name[0])
			{
				case FT_SOURCES:
					pSearchRoot->availability = (UINT)uValue;
					break;
				case FT_FILESIZE:
					switch(Operator)
					{
						case 0x00: //SSearchTree::OpEqual:
							pSearchRoot->minSize = uValue;
							pSearchRoot->maxSize = uValue;
							break;
						case 0x01: //SSearchTree::OpGreaterEqual:
							pSearchRoot->minSize = uValue;
							break;
						case 0x02: //SSearchTree::OpLessEqual:
							pSearchRoot->maxSize = uValue;
							break;
						case 0x03: //SSearchTree::OpGreater:
							pSearchRoot->minSize = uValue+1;
							break;
						case 0x04: //SSearchTree::OpLess:
							pSearchRoot->maxSize = uValue-1;
							break;
						case 0x05: //SSearchTree::OpNotEqual:
							break;
					}
					break;
			}
			return EMPTY_SEARCHTREE;
		}
		default:
			return NULL;
	}
}*/