#include "GlobalHeader.h"
#include "ServerClient.h"
#include "Ed2kServer.h"
#include "ServerList.h"
#include "../../../NeoCore.h"
#include "../MuleManager.h"
#include "../MuleServer.h"
#include "../../../../Framework/qzlib.h"
#include "../../../../Framework/Exception.h"
#include "../../../FileList/File.h"
#include "../../../FileSearch/Search.h"

CServerClient::CServerClient(CEd2kServer* pServer)
 : QObjectEx(pServer)
{
	m_Socket = NULL;

	m_LastPublish = GetCurTick();

	m_ClientID = 0;
}

CServerClient::~CServerClient()
{
	if(m_Socket)
	{
		disconnect(m_Socket, 0, 0, 0);
		theCore->m_MuleManager->GetServer()->FreeSocket(m_Socket);
	}
}

void CServerClient::Process(UINT Tick)
{
	// Comment copyed form eMule (Fair Use):
	// "Ping" the server if the TCP connection was not used for the specified interval with 
	// an empty publish files packet -> recommended by lugdunummaster himself!
	if(m_ClientID != 0 && GetCurTick() - m_LastPublish > MIN2MS(5))
		PublishFiles(QList<CFile*>());
}

bool CServerClient::Connect()
{
	ASSERT(m_Socket == NULL);
	m_Socket = (CMuleSocket*)theCore->m_MuleManager->GetServer()->AllocSocket();

	connect(m_Socket, SIGNAL(Connected()), this, SLOT(OnConnected()));
	connect(m_Socket, SIGNAL(ReceivedPacket(QByteArray, quint8)), this, SLOT(OnReceivedPacket(QByteArray, quint8)), Qt::QueuedConnection);
	connect(m_Socket, SIGNAL(Disconnected(int)), this, SLOT(OnDisconnected(int)));

	CEd2kServer* pServer = GetServer();

	// Note: we must setup the key even if its empty so that we wil proeprly separate mule packets form server packets on one socket
	theCore->m_MuleManager->GetServer()->SetServerKey(pServer->GetIP(), pServer->GetUDPKey());

	if(!theCore->m_MuleManager->SupportsCryptLayer() || !pServer->SupportsObfuscationTCP())
	{
		if(theCore->m_MuleManager->RequiresCryptLayer())
			return false;
	}
	else if(theCore->m_MuleManager->RequestsCryptLayer())
	{
		m_Socket->InitCrypto();
		m_Socket->ConnectToHost(pServer->GetIP(), pServer->GetCryptoPort());
		return true;
	}

	m_Socket->ConnectToHost(pServer->GetIP(), pServer->GetPort());
	return true;
}

void CServerClient::Disconnect()
{
	if(m_Socket)
		m_Socket->DisconnectFromHost();
}

void CServerClient::OnConnected()
{
	SendLoginRequest();
}

void CServerClient::OnDisconnected(int Error)
{
	ASSERT(m_Socket);
	
	CEd2kServer* pServer = GetServer();
	theCore->m_MuleManager->GetServerList()->FinishSearch(pServer);

	disconnect(m_Socket, 0, 0, 0);
	theCore->m_MuleManager->GetServer()->FreeSocket(m_Socket);
	m_Socket = NULL;

	emit Disconnected();
}

void CServerClient::SendLoginRequest()
{
	CAddress IPv6 = theCore->m_MuleManager->GetAddress(CAddress::IPv6);
	CAddress IPv4 = theCore->m_MuleManager->GetAddress(CAddress::IPv6);

	CBuffer Packet;
	Packet.WriteValue<uint8>(OP_LOGINREQUEST);

	Packet.WriteQData(theCore->m_MuleManager->GetUserHash().ToArray());
	Packet.WriteValue<uint32>(_ntohl(IPv4.ToIPv4()));							// client ID (IP)
	Packet.WriteValue<uint16>(theCore->m_MuleManager->GetServer()->GetPort());	// client Port

	QVariantMap Tags;
	Tags[TO_NUM(CT_NAME)]					= theCore->Cfg()->GetString("Ed2kMule/NickName");
	Tags[TO_NUM(CT_VERSION)]				= EDONKEYVERSION;
	USvrFlags Flags;
	Flags.Bits = 0;
	Flags.Fields.uCompression	= 1;
	Flags.Fields.uNewTags		= 1;
	Flags.Fields.uUnicode		= 1;
	Flags.Fields.uIPinLogin		= 0;
	Flags.Fields.uAuxPort		= 0;
	Flags.Fields.uLargeFiles	= 1;
	Flags.Fields.uSupportCrypto = theCore->m_MuleManager->SupportsCryptLayer() ? 1 : 0;
	Flags.Fields.uRequireCrypto = theCore->m_MuleManager->RequestsCryptLayer() ? 1 : 0;
	Flags.Fields.uRequestCrypto = theCore->m_MuleManager->RequiresCryptLayer() ? 1 : 0;
	Flags.Fields.uNatTraversal	= theCore->m_MuleManager->SupportsNatTraversal() ? 1 : 0;
	Flags.Fields.uIPv6			= theCore->m_MuleManager->GetIPv6Support() ? 1 : 0;
	Tags[TO_NUM(CT_SERVER_FLAGS)]			= Flags.Bits;
	Tags[TO_NUM(CT_EMULE_VERSION)]			= theCore->m_MuleManager->GetMyInfo()->MuleVersion.Bits;
	if(!IPv6.IsNull())
		Tags[TO_NUM(CT_NEOMULE_IP_V6)]		= QByteArray((char*)IPv6.Data(), 16);

	CMuleTags::WriteTags(Tags, &Packet);

	SendPacket(Packet, OP_EDONKEYPROT);
}

void CServerClient::RequestSources(CFile* pFile)
{
	CEd2kServer* pServer = GetServer();
	CFileHash* pHash = pFile->GetHash(HashEd2k);
	ASSERT(pHash);

	CBuffer Packet;
	Packet.WriteValue<uint8>((theCore->m_MuleManager->SupportsCryptLayer() && pServer->SupportsObfuscationTCP()) ? OP_GETSOURCES_OBFU : OP_GETSOURCES);
	Packet.WriteQData(pHash->GetHash());
	if(!IsLargeEd2kMuleFile(pFile->GetFileSize()))
		Packet.WriteValue<uint32>(pFile->GetFileSize());
	else
	{
		ASSERT(pServer->SupportsLargeFiles());
		Packet.WriteValue<uint32>(0);
		Packet.WriteValue<uint64>(pFile->GetFileSize());
	}

	SendPacket(Packet, OP_EDONKEYPROT);
}

void CServerClient::FindFiles(const SSearchRoot& SearchRoot)
{
	CEd2kServer* pServer = GetServer();

	CBuffer Packet;
	Packet.WriteValue<uint8>(OP_SEARCHREQUEST);
	WriteSearchTree(Packet, SearchRoot, pServer->SupportsLargeFiles());

	SendPacket(Packet, OP_EDONKEYPROT);
}

void CServerClient::FindMore()
{
	CBuffer Packet(1);
	Packet.WriteValue<uint8>(OP_QUERY_MORE_RESULT);
	
	SendPacket(Packet, OP_EDONKEYPROT);
}

void CServerClient::PublishFiles(QList<CFile*> Files)
{
	CEd2kServer* pServer = GetServer();

	m_LastPublish = GetCurTick();

	CBuffer Packet;
	Packet.WriteValue<uint8>(OP_OFFERFILES);
	
	ASSERT(pServer->GetSoftLimit() >= Files.size());
	Packet.WriteValue<uint32>(Files.size());

	foreach(CFile* pFile, Files)
		pServer->WriteFile(pFile, Packet);

	SendPacket(Packet, pServer->SupportsCompression() ? OP_PACKEDPROT : OP_EDONKEYPROT);
}

void CServerClient::RequestCallback(uint32 ClientID)
{
	CBuffer Packet(1 + 4);
	Packet.WriteValue<uint8>(OP_CALLBACKREQUEST);
	Packet.WriteValue<uint32>(ClientID);

	SendPacket(Packet, OP_EDONKEYPROT);
}

void CServerClient::RequestNatCallback(uint32 ClientID)
{
	CEd2kServer* pServer = GetServer();
	ASSERT(pServer->SupportsNatTraversal());

	CBuffer Packet(1 + 4 + 4);
	Packet.WriteValue<uint8>(OP_NAT_CALLBACKREQUEST);
	Packet.WriteValue<uint32>(ClientID); // targetID
	Packet.WriteValue<uint32>(m_ClientID); // ourID
	
	// Note: this request must be sent over UDP so that teh server detects our NAT Port
	theCore->m_MuleManager->GetServerList()->SendSvrPacket(Packet.ToByteArray(), OP_EDONKEYPROT, m_Socket->GetAddress(), pServer->GetUDPPort(), pServer->GetUDPKey());
}

/*void CServerClient::SendGetServerList()
{
	CBuffer Packet(1);
	Packet.WriteValue<uint8>(OP_GETSERVERLIST);

	SendPacket(Packet, OP_EDONKEYPROT);
}*/

void CServerClient::SendPacket(CBuffer& Packet, uint8 Prot)
{
	ASSERT(m_Socket);
	if(!m_Socket)
		return;

	if(Prot == OP_PACKEDPROT)
		Prot = PackPacket(Packet) ? OP_PACKEDPROT : OP_EDONKEYPROT;

	m_Socket->SendPacket(Packet.ToByteArray(), Prot);
}

void CServerClient::OnReceivedPacket(QByteArray Packet, quint8 Prot)
{
	try
	{
		CBuffer Buffer(Packet, true);
		if(Prot == OP_PACKEDPROT)
		{
			UnpackPacket(Buffer);
			Prot = OP_EDONKEYPROT;
		}
		ProcessPacket(Buffer, Prot);
	}
	catch(const CException& Exception)
	{
		LogLine(Exception.GetFlag(), tr("recived malformated packet; %1").arg(QString::fromStdWString(Exception.GetLine())));
	}
}

void CServerClient::ProcessPacket(CBuffer& Packet, uint8 Prot)
{
	CEd2kServer* pServer = GetServer();

	uint8 uOpcode = Packet.ReadValue<uint8>();
	switch(Prot)
	{
		case OP_EDONKEYPROT:
		{
			switch(uOpcode)
			{
				case OP_SERVERMESSAGE:
				{
					QString Message = QString::fromStdWString(Packet.ReadString(CBuffer::eAscii));
					LogLine(LOG_DEBUG | LOG_INFO, tr("Recived ed2k Server message %1").arg(Message));
					break;
				}
				case OP_IDCHANGE:
				{
					uint32 uClientID = Packet.ReadValue<uint32>(); // ID is IP in network order
					if(SMuleSource::IsLowID(uClientID))
						m_ClientID = uClientID;
					else
						m_ClientID = _ntohl(uClientID); // switch to eMules hybrid format

					uint32 uFlags = Packet.GetSizeLeft() >= 4 ? Packet.ReadValue<uint32>() : 0;
					uint32 AuxPort = Packet.GetSizeLeft() >= 4 ? Packet.ReadValue<uint32>() : 0;
					CAddress Address;
					if(Packet.GetSizeLeft() >= 4)
					{
						uint32 uIP = _ntohl(Packet.ReadValue<uint32>());
						if(uIP == -1)
							Address = CAddress(Packet.ReadData(16));
						else
							Address = CAddress(uIP);
					}
					uint32 CryptoPort = Packet.GetSizeLeft() >= 4 ? Packet.ReadValue<uint32>() : 0;
					uint32 YourPort = Packet.GetSizeLeft() >= 4 ? Packet.ReadValue<uint32>() : 0; // for TCP NatT

					ASSERT(SMuleSource::IsLowID(m_ClientID) || Address.IsNull() || (Address.Type() == CAddress::IPv4 ? m_ClientID == Address.ToIPv4() : m_ClientID == 0xFFFFFFFF));

					// Ipv6 ext
					if(Packet.GetSizeLeft() > 4)
					{
						QVariantMap Tags = CMuleTags::ReadTags(&Packet);
						for(QVariantMap::iterator I = Tags.begin(); I != Tags.end(); ++I)
						{
							const QVariant& Value = I.value();
							switch(FROM_NUM(I.key()))
							{
							}
						}
					}

					pServer->SetFlagsTCP(uFlags);
					pServer->SetCryptoPort(CryptoPort);
					
					if(!Address.IsNull())
						theCore->m_MuleManager->GetServerList()->SetAddress(Address, SMuleSource::IsLowID(m_ClientID));

					theCore->m_MuleManager->GetServerList()->BeginSearch(pServer);

					emit Connected();
					break;
				}
				case OP_SEARCHRESULT:
				{
					uint32 Count = Packet.ReadValue<uint32>();
					for(;Count > 0;Count--)
					{
						if(CFile* pFile = pServer->ReadFile(Packet))
							theCore->m_MuleManager->GetServerList()->AddFoundFile(pFile, pServer);
					}
					uint8 uMore = Packet.GetSizeLeft() >= 1 ? Packet.ReadValue<uint8>() : 0;
					theCore->m_MuleManager->GetServerList()->FinishSearch(pServer, uMore == 0x01);
					break;
				}
				case OP_FOUNDSOURCES_OBFU:
				case OP_FOUNDSOURCES:
				{
					pServer->AddSources(Packet, uOpcode == OP_FOUNDSOURCES_OBFU);
					break;
				}
				case OP_SERVERSTATUS:
				{
					uint32 UserCount = Packet.ReadValue<uint32>();
					uint32 FileCount = Packet.ReadValue<uint32>();
					pServer->SetUserCount(UserCount);
					pServer->SetFileCount(FileCount);
					break;
				}
				case OP_SERVERLIST: 
				{
					/*int Count = Packet.ReadValue<uint8>();
					for(;Count > 0;Count--)
					{
						uint32 IP = _ntohl(Packet.ReadValue<uint32>());
						uint16 Port = Packet.ReadValue<uint16>();
					}*/
					break;
				}
				case OP_SERVERIDENT: // Note: this is sent only if we send a OP_GETSERVERLIST
				{
					/*
					QByteArray Hash = Packet.ReadQData(16);
					uint32 IP = _ntohl(Packet.ReadValue<uint32>());
					uint16 Port = Packet.ReadValue<uint16>();
					QVariantMap Tags = CMuleTags::ReadTags(&Packet);
					for(QVariantMap::iterator I = Tags.begin(); I != Tags.end(); ++I)
					{
						const QVariant& Value = I.value();
						switch(FROM_NUM(I.key()))
						{
							case ST_SERVERNAME:
								Value.toString(); break;
							case ST_DESCRIPTION:
								Value.toString(); break;
							default:
								LogLine(LOG_DEBUG, tr("Recived unknwon Ed2k Server Info tag: %1").arg(I.key()));
						}
					}*/
					break;
				}
				case OP_CALLBACKREQUESTED:
				{
					SMuleSource Mule;
					CAddress Address;
					uint32 uIP = _ntohl(Packet.ReadValue<uint32>());
					if(uIP == -1)
						Address = CAddress(Packet.ReadData(16));
					else
						Address = CAddress(uIP);
					Mule.SetIP(Address);
					Mule.TCPPort = Packet.ReadValue<uint16>(); 
					if(Packet.GetSizeLeft() >= 1 + 16)
					{
						Mule.ConOpts.Bits = Packet.ReadValue<uint8>();
						Mule.UserHash.Set(Packet.ReadData(16));
					}
					theCore->m_MuleManager->CallbackRequested(Mule);
					break;
				}
				case OP_NAT_CALLBACKREQUESTED: 
				case OP_NAT_CALLBACKREQUESTED_UDP: // this came through the UDP SPcket
				{
					if(!theCore->m_MuleManager->SupportsNatTraversal())
						break;

					SMuleSource Mule;
					CAddress Address;
					uint32 uIP = _ntohl(Packet.ReadValue<uint32>());
					if(uIP == -1)
						Address = CAddress(Packet.ReadData(16));
					else
						Address = CAddress(uIP);
					Mule.SetIP(Address);
					Mule.UDPPort = Packet.ReadValue<uint16>(); 
					Mule.ClientID = Packet.ReadValue<uint32>(); 
					if(Packet.GetSizeLeft() >= 1 + 16)
					{
						Mule.UserHash.Set(Packet.ReadData(16));
						Mule.ConOpts.Bits = Packet.ReadValue<uint8>();
					}
					theCore->m_MuleManager->CallbackRequested(Mule, true);
					break;
				}
				case OP_CALLBACK_FAIL:
					LogLine(LOG_DEBUG, tr("ed2k Server Callback Failed"));
					break;
				case OP_REJECT:
					LogLine(LOG_DEBUG, tr("ed2k Server Rejected last Request ???"));
					break;

				default:
					LogLine(LOG_DEBUG, tr("Recived unknwon EServer packet Opcode: %1").arg(uOpcode));
			}
			break;
		}

		default:
			LogLine(LOG_DEBUG, tr("Recived unknwon EServer packet protocol: %1").arg(Prot));
	}
}


void CServerClient::AddLogLine(time_t uStamp, uint32 uFlag, const CLogMsg& Line)
{
	int iDebug = theCore->Cfg()->GetInt("Log/Level");
	if(iDebug >= 2 || (iDebug == 1 && (uFlag & LOG_DEBUG) == 0))
	{
		if(CEd2kServer* pServer = GetServer())
			Line.Prefix(QString("Ed2kServer - %1:%2").arg(pServer->GetIP().ToQString()).arg(pServer->GetPort()));
		QObjectEx::AddLogLine(uStamp, uFlag | LOG_MOD('m'), Line);
	}
}
