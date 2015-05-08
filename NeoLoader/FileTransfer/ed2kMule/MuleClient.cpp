#include "GlobalHeader.h"
#include "MuleClient.h"
#include "../../NeoCore.h"
#include "MuleManager.h"
#include "MuleServer.h"
#include "MuleKad.h"
#include "../../../Framework/qzlib.h"
#include "../../FileList/File.h"
#include "../../FileList/FileStats.h"
#include "../../FileList/Hashing/FileHashSet.h"
#include "../../FileList/Hashing/FileHashTree.h"
#include "../../FileList/Hashing/HashingThread.h"
#include "../../FileList/FileManager.h"
#include "../../FileList/IOManager.h"
#include "../../../Framework/Scope.h"
#include "../../../Framework/Exception.h"
#include "../PeerWatch.h"
#include "../../Networking/BandwidthControl/BandwidthLimit.h"
#include "../../Networking/BandwidthControl/BandwidthLimiter.h"
#include "../NeoShare/NeoManager.h"
#include "../NeoShare/NeoKad.h"
#include "../HashInspector.h"
#include "./ServerClient/ServerList.h"
#include "./ServerClient/Ed2kServer.h"
#include "./ServerClient/ServerClient.h"
#include "../UploadManager.h"

CMuleClient::CMuleClient(const SMuleSource& Mule, QObject* qObject)
 : CP2PClient(qObject)
{
	m_Mule = Mule;
	m_Socket = NULL;

	Init();
}

CMuleClient::CMuleClient(CMuleSocket* pSocket, QObject* qObject)
: CP2PClient(qObject)
{
	m_Socket = pSocket;
	
	m_Socket->AddUpLimit(m_UpLimit);
	m_Socket->AddDownLimit(m_DownLimit);

	connect(m_Socket, SIGNAL(ReceivedPacket(QByteArray, quint8)), this, SLOT(OnReceivedPacket(QByteArray, quint8)), Qt::QueuedConnection);
	connect(m_Socket, SIGNAL(Disconnected(int)), this, SLOT(OnDisconnected(int)));
	connect(m_Socket, SIGNAL(NextPacketSend()), this, SLOT(OnPacketSend()));

	m_Socket->AcceptConnect(); // enable packet reading

	if(pSocket->GetState() == CStreamSocket::eConnected)
	{
		m_Mule.SetIP(pSocket->GetAddress());
	}

	Init();
	LogLine(LOG_DEBUG, tr("connected socket (Incoming)"));
}

void CMuleClient::Init()
{
	m_uTimeOut = GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("Ed2kMule/IdleTimeout"));
	m_bExpectConnection = false;
	m_bHelloRecived = false;
	m_HolepunchTry = 0;

	m_Ed2kVersion = 0;
	m_MuleProtocol = false;
	m_MuleVersion.Bits = 0;
	m_MiscOptions1.Bits = 0;
	m_MiscOptions2.Bits = 0;
	m_MiscOptionsN.Bits = 0;
	m_MiscOptionsNL.Bits = 0;
	m_ProtocolRevision = 0;

	m_Pending.Bits = 0;

	m_UpSource = NULL;
	m_DownSource = NULL;

	m_SentPartSize = 0;
	m_RecivedPartSize = 0;
	//m_TempPartSize = 0;

	m_HordeState = eNone;
	m_HordeTimeout = -1;

	m_uRndChallenge = 0; // SUI
}

CMuleClient::~CMuleClient()
{
	ASSERT(m_UpSource == NULL && m_DownSource == NULL && m_AllSources.isEmpty());

	ClearUpload();
	ClearDownload();

	if(m_Socket)
	{
		disconnect(m_Socket, 0, 0, 0);
		theCore->m_MuleManager->GetServer()->FreeSocket(m_Socket);
	}
}

void CMuleClient::Process(UINT Tick)
{
	if(m_uTimeOut < GetCurTick())
		Disconnect();

	if(m_HordeTimeout < GetCurTick())
	{
		m_HordeTimeout = -1;
		if(m_HordeState == eRejected) // if the request was rejected and we are still in upload after the set 3 minutes timeout, just reset and retry
			m_HordeState = eNone;
		else // if the remote client did not answered, go to permanent reject state
		{
			ASSERT(m_HordeState = eRequested);
			m_HordeState = eRejected;
		}
	}
}

QString CMuleClient::GetUrl()
{
	return QString("ed2k://|source,%1:%2|").arg(m_Mule.GetIP().ToQString(true)).arg(m_Mule.TCPPort);
}

QString CMuleClient::GetConnectionStr()
{
	if(IsDisconnected())
		return "Disconnected";
	QString Status;
	if(IsConnected())
		Status = "Connected";
	else if(!IsDisconnected())
	{
		if(IsExpectingCallback())
			Status = "Expecting Callback";
		else
			Status = "Connecting";
	}
	if(m_Socket)
	{
		if(m_Socket->GetSocketType() == SOCK_UTP)
			Status += " UTP";
		if(m_Socket->IsEncrypted())
			Status += " Obfuscated";
	}
	return Status;
}

CFile* CMuleClient::GetFile()
{
	if(m_DownSource)
		return m_DownSource->GetFile();
	if(m_UpSource)
		return m_UpSource->GetFile();
	return NULL;
}

void CMuleClient::OnPacketSend()
{
	m_uTimeOut = GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("Ed2kMule/IdleTimeout"));
}

void CMuleClient::SendHelloPacket(bool bAnswer)
{
	LogLine(LOG_DEBUG, tr("SendHelloPacket"));

	CBuffer Packet;

	if(!bAnswer)
	{
		Packet.WriteValue<uint8>(OP_HELLO);
		Packet.WriteValue<uint8>(16); // size of userhash is only sent in hello, not in hello answer, and its always 16
	}
	else
		Packet.WriteValue<uint8>(OP_HELLOANSWER);


	// Note: The ed2k Client ID for a high ID user is his IPv4 in network order, a low ID is a value < 16777216 (it would resolve to an invalid IP that  looks like *.*.*.0)
	//		 The eMule Client ID for a high ID user is his IPv4 in host order
	//			There is now LowIDv6 for LowID IPv6 uers no a normal 32 bit LowID is used
	//			If thereis no IPv4 but the IPv6 is not firewalled the clients ID is 0xFFFFFFFF

	CAddress IPv4 = theCore->m_MuleManager->GetAddress(CAddress::IPv4);
	CAddress IPv6 = theCore->m_MuleManager->GetAddress(CAddress::IPv6);

	CEd2kServer* pServer = theCore->m_MuleManager->GetServerList()->GetConnectedServer();

	uint32 ClientID = 0;
	if(theCore->m_MuleManager->IsFirewalled(CAddress::IPv4))
	{
		if(pServer)
			ClientID = pServer->GetClient()->GetClientID();
		else
			ClientID = 1; // note: we dont have a server ID to return as we dont supprot servers, 1 is the kad low ID id
	}
	else
		ClientID = IPv4.ToIPv4(); // client IP in host order is an ed2k high ID
	if(ClientID == 0 && !IPv6.IsNull())
		ClientID = 0xFFFFFFFF;
	if(!SMuleSource::IsLowID(ClientID))
		m_Mule.ClientID = _ntohl(ClientID);

	Packet.WriteData(theCore->m_MuleManager->GetUserHash().Data, 16);			// userhash
	Packet.WriteValue<uint32>(ClientID);										// client ID (IP)
	Packet.WriteValue<uint16>(theCore->m_MuleManager->GetServer()->GetPort());	// client Port

	QVariantMap Tags;
	Tags[TO_NUM(CT_NAME)]					= theCore->Cfg()->GetString("Ed2kMule/NickName");
	Tags[TO_NUM(CT_VERSION)]				= EDONKEYVERSION;
	Tags[TO_NUM(CT_MOD_VERSION)]			= theCore->m_MuleManager->GetVersion();
	Tags[TO_NUM(CT_EMULE_VERSION)]			= theCore->m_MuleManager->GetMyInfo()->MuleVersion.Bits;
	Tags[TO_NUM(CT_EMULE_MISCOPTIONS1)]		= theCore->m_MuleManager->GetMyInfo()->MiscOptions1.Bits;
	Tags[TO_NUM(CT_EMULE_MISCOPTIONS2)]		= theCore->m_MuleManager->GetMyInfo()->MiscOptions2.Bits;
	Tags[TO_NUM(CT_NEOMULE_MISCOPTIONS)]	= theCore->m_MuleManager->GetMyInfo()->MiscOptionsN.Bits;
	Tags[CT_NEOLOADER_MISCOPTIONS]			= theCore->m_MuleManager->GetMyInfo()->MiscOptionsNL.Bits;
	Tags[CT_PROTOCOL_REVISION]				= CT_REVISION_NETFWARP;
	Tags[TO_NUM(CT_EMULE_UDPPORTS)]			= ((uint32)theCore->m_MuleManager->GetKad()->GetKadPort() << 16) 
											| ((uint32)theCore->m_MuleManager->GetServer()->GetUTPPort() << 0);

	if(!IPv6.IsNull() && !theCore->m_MuleManager->IsFirewalled(CAddress::IPv6)) // we are only allowed to send the IP if we are not firewalled on it
		Tags[TO_NUM(CT_NEOMULE_IP_V6)]		= QVariant(CMuleHash::GetVariantType(), IPv6.Data());

	const CAddress& IP = m_Mule.GetIP();
	if(IP.Type() == CAddress::IPv4)
		Tags[TO_NUM(CT_NEOMULE_YOUR_IP)]	= _ntohl(IP.ToIPv4());
	else if(IP.Type() == CAddress::IPv6)
		Tags[TO_NUM(CT_NEOMULE_YOUR_IP)]	= QVariant(CMuleHash::GetVariantType(), IP.Data());

	if(CMuleClient* pBuddy = theCore->m_MuleManager->GetKad()->GetBuddy()) // note: it only returns a High budy meaning we are firewalled
	{
		Tags[TO_NUM(CT_EMULE_BUDDYIP)]		= _ntohl(pBuddy->GetMule().IPv4.ToIPv4());
		Tags[TO_NUM(CT_EMULE_BUDDYUDP)]		= pBuddy->GetKadPort();
		Tags[TO_NUM(CT_EMULE_BUDDYID)]		= QVariant(CMuleHash::GetVariantType(), pBuddy->GetBuddyID().data());
	}
	
	if(pServer && !pServer->GetAddress().IPv6.IsNull())
		Tags[TO_NUM(CT_NEOMULE_SVR_IP_V6)]	= QByteArray((char*)pServer->GetAddress().IPv6.Data(), 16);

	CMuleTags::WriteTags(Tags, &Packet);

	// we don't supprot servers but the data is mandatory
	Packet.WriteValue<uint32>(pServer ? _ntohl(pServer->GetAddress().IPv4.ToIPv4()) : 0);
	Packet.WriteValue<uint16>(pServer ? pServer->GetPort() : 0);

	SendPacket(Packet, OP_EDONKEYPROT);
}

void CMuleClient::SetUserHash(const QByteArray& UserHash)
{
	m_Mule.UserHash = UserHash;
	//LogLine(LOG_DEBUG, tr("client settgin hash to %1").arg(QString::fromLatin1(m_Mule.UserHash.toHex())));
}

void CMuleClient::ProcessHelloHead(const CBuffer& Packet)
{
	LogLine(LOG_DEBUG, tr("ProcessHelloPacket"));

	m_Mule.UserHash.Set(Packet.ReadData(16));
	//if(!m_Mule.UserHash.isEmpty() && m_Mule.UserHash != UserHash)
	//	LogLine(LOG_DEBUG, tr("client changed hash from %1 to %2").arg(QString::fromLatin1(m_Mule.UserHash.toHex())).arg(QString::fromLatin1(UserHash.toHex())));
	m_Mule.ClientID = Packet.ReadValue<uint32>();
	if(!SMuleSource::IsLowID(m_Mule.ClientID))
		m_Mule.ClientID = _ntohl(m_Mule.ClientID);
	m_Mule.TCPPort = Packet.ReadValue<uint16>();

	if(m_Mule.HasV4())
	{
		// Comment copyed form eMule (Fair Use):
		//(a)If this is a highID user, store the ID in the Hybrid format.
		//(b)Some older clients will not send a ID, these client are HighID users that are not connected to a server.
		//(c)Kad users with a *.*.*.0 IPs will look like a lowID user they are actually a highID user.. They can be detected easily
		//		because they will send a ID that is the same as their IP..
		if(!m_Mule.HasLowID() || m_Mule.ClientID == 0 || m_Mule.ClientID == _ntohl(m_Mule.IPv4.ToIPv4()))
			m_Mule.ClientID = m_Mule.IPv4.ToIPv4();
	}

	theCore->m_PeerWatch->PeerConnected(m_Mule.GetIP(), m_Mule.TCPPort);

	m_bHelloRecived = true;
}

void CMuleClient::ReadHelloInfo(const CBuffer& Packet)
{
	QString ModString;

	QVariantMap Tags = CMuleTags::ReadTags(&Packet);
	for(QVariantMap::iterator I = Tags.begin(); I != Tags.end(); ++I)
	{
		const QVariant& Value = I.value();
		// Note: Mule does here always a type test of the tags, 
		//			as variants dont fail on any conversions we dont need to do that
		//			if an invlid conversion is performed the value remains 0 or empty.
		switch(FROM_NUM(I.key()))
		{
			case CT_NAME:				m_NickName = Value.toString();								break;
			case CT_VERSION:			m_Ed2kVersion = Value.toUInt();								break;
			case CT_EMULE_VERSION:		m_MuleProtocol = true; // even if version is 0 enable the protocol mule support
										m_MuleVersion.Bits = Value.toUInt();						break;
			case CT_EMULE_MISCOPTIONS1:	m_MiscOptions1.Bits = Value.toUInt();						break;
			case CT_EMULE_MISCOPTIONS2:	m_MiscOptions2.Bits = Value.toUInt();
										m_Mule.SetConOpts(m_MiscOptions2);							break;
			case CT_EMULECOMPAT_OPTIONS:															break; // aMule
			case CT_NEOMULE_MISCOPTIONS:m_MiscOptionsN.Bits = Value.toUInt();
										m_Mule.SetConOptsEx(m_MiscOptionsN);						break;
			case CT_EMULE_UDPPORTS:		m_Mule.UDPPort = (uint16)(Value.toUInt() >> 0);
										m_Mule.KadPort = (uint16)(Value.toUInt() >> 16);			break;
			case CT_MOD_VERSION:		ModString = Value.toString();								break;

			case CT_NEOMULE_IP_V6:
			{
				CAddress IPv6;
				if(IPv6.FromArray(Value.value<CMuleHash>().ToByteArray()) == CAddress::IPv6)
				{
					m_Mule.SetIP(IPv6);
					m_Mule.OpenIPv6 = true;
				}
				break;
			}

			case CT_NEOMULE_YOUR_IP:
			{
				CAddress IP;
				if(Value.canConvert(QVariant::UInt))
					IP = _ntohl(Value.toUInt());
				else
					IP.FromArray(Value.value<CMuleHash>().ToByteArray());
				if(!IP.IsNull())
					theCore->m_MuleManager->GetServer()->AddAddress(IP);
				break;
			}

			case CT_EMULE_BUDDYIP:		m_Mule.BuddyAddress = CAddress(_ntohl(Value.toUInt()));	break;
			case CT_EMULE_BUDDYUDP:		m_Mule.BuddyPort = Value.toUInt();						break;
			case CT_EMULE_BUDDYID:		m_Mule.BuddyID = Value.value<CMuleHash>().ToByteArray();break;

			case CT_NEOMULE_SVR_IP_V6:	m_Mule.ServerAddress.FromArray(Value.toByteArray());	break;
			default:
				if(I.key() == CT_NEOLOADER_MISCOPTIONS)		m_MiscOptionsNL.Bits = Value.toUInt();
				// Comment copyed form eMule (Fair Use):
				// Since eDonkeyHybrid 1.3 is no longer sending the additional Int32 at the end of the Hello packet,
				// we use the "pr=1" tag to determine them.	
				else if(I.key() == CT_PROTOCOL_REVISION)	m_ProtocolRevision = Value.toInt();
				else
					LogLine(LOG_DEBUG, tr("Recived unknwon Ed2kMule hello tag: %1").arg(I.key()));
		}
	}

	if(uint32 ServerAddress = _ntohl(Packet.ReadValue<uint32>()))
		m_Mule.ServerAddress = ServerAddress;
	m_Mule.ServerPort = Packet.ReadValue<uint16>();

	bool bIsML = false;
	// Comment copyed form eMule (Fair Use):
	// Check for additional data in Hello packet to determine client's software version.
	//
	// *) eDonkeyHybrid 0.40 - 1.2 sends an additional Int32. (Since 1.3 they don't send it any longer.)
	// *) MLdonkey sends an additional Int32
	//
	if (Packet.GetSizeLeft() == sizeof(uint32))
	{
		if (Packet.ReadValue<uint32>() == 'KDLM')
			bIsML = true;
		else
			m_ProtocolRevision = -1;
	}

	if(!ModString.isEmpty())
		m_Software = ModString;
	else
		m_Software = IdentifySoftware(bIsML);

	// atempt to bootstrap from clients we encounter
	if(m_Mule.KadPort && KadVersion() > 1 && theCore->m_MuleManager->GetKad()->IsEnabled() && m_Mule.HasV4())
		theCore->m_MuleManager->GetKad()->AddNode(CAddress(m_Mule.IPv4.ToIPv4()), m_Mule.KadPort);

	// Notify the source object(s) that we are connected
	emit HelloRecived();
}

void CMuleClient::SendPublicIPRequest()
{
	LogLine(LOG_DEBUG, tr("SendPublicIPRequest"));

	m_Pending.Ops.PublicIPReq = true;

	CBuffer Packet(1);
	Packet.WriteValue<uint8>(OP_PUBLICIP_REQ);
	SendPacket(Packet, OP_EMULEPROT);
}

void CMuleClient::SendBuddyPing()
{
	LogLine(LOG_DEBUG, tr("SendBuddyPing"));

	CBuffer Packet(1);
	Packet.WriteValue<uint8>(OP_BUDDYPING);
	SendPacket(Packet, OP_EMULEPROT);
}

bool CMuleClient::RequestCallback()
{
	ASSERT(m_bExpectConnection == false);

	if(!DirectUDPCallback())
	{
		CEd2kServer* pServer = theCore->m_MuleManager->GetServerList()->FindServer(m_Mule.ServerAddress, m_Mule.Prot);
		if(pServer && pServer->GetClient())
		{
			pServer->GetClient()->RequestCallback(m_Mule.ClientID);
		}
		else if(!m_Mule.BuddyID.isEmpty()) // kad Callback
		{
			if(m_Mule.Prot != CAddress::IPv4)
			{
				m_Error = "Only IPv4 supports MuleKad Callback";
				return false; // EM-ToDo: add IPv6 callback using new server or NeoKad?
			}

			if(theCore->m_MuleManager->IsLastCallbacksMustWait())
				return false;

			if(!m_DownSource && !m_UpSource)
				return false; // for kad callback a file is needed

			if(!theCore->m_MuleManager->GetKad()->RequestKadCallback(this, m_DownSource ? m_DownSource->GetFile() : m_UpSource->GetFile()))
			{
				theCore->m_MuleManager->SetLastCallbacksMustWait(SEC2MS(10));
				return false; // retry to many attempts, say all went ok but dont do anything
			}
		}
		else
		{
			m_Error = "No Callback Available";
			return false;
		}

		// this callbacks are slow
		m_uTimeOut = GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("Ed2kMule/ConnectTimeout")) * 3;
		m_bExpectConnection = true;
		return true;
	}
	else
	{
		// Note: DirectCallback is IPv6 Compatible
		LogLine(LOG_DEBUG, tr("RequestDirectCallback"));

		CBuffer Packet;

		Packet.WriteValue<uint8>(OP_DIRECTCALLBACKREQ);
		Packet.WriteValue<uint16>(theCore->m_MuleManager->GetServer()->GetPort());
		Packet.WriteData(theCore->m_MuleManager->GetUserHash().Data, 16);
		Packet.WriteValue<uint8>(theCore->m_MuleManager->GetMyInfo()->MuleConOpts.Bits);

		theCore->m_MuleManager->SendUDPPacket(Packet, OP_EMULEPROT, m_Mule);

		ExpectConnection();
		return true;
	}
}

void CMuleClient::RelayKadCallback(const CAddress& Address, uint16 uPort, const QByteArray& BuddyID, const QByteArray& FileID)
{
	LogLine(LOG_DEBUG, tr("RelayKadCallback"));

	CBuffer Packet(1 + 16 + 16 + 4 + 2);
	Packet.WriteValue<uint8>(OP_CALLBACK);
	CMuleTags::WriteUInt128(BuddyID, &Packet);
	CMuleTags::WriteUInt128(FileID, &Packet);
	Packet.WriteValue<uint32>(_ntohl(Address.ToIPv4()));
	Packet.WriteValue<uint16>(uPort);
	SendPacket(Packet, OP_EMULEPROT);
}

void CMuleClient::RequestFWCheckUDP(uint16 IntPort, uint16 ExtPort, uint32 UDPKey)
{
	LogLine(LOG_DEBUG, tr("RequestFWCheckUDP"));

	CBuffer Packet(1 + 2 + 2 + 4);
	Packet.WriteValue<uint8>(OP_FWCHECKUDPREQ);
	Packet.WriteValue<uint16>(IntPort);
	Packet.WriteValue<uint16>(ExtPort);
	Packet.WriteValue<uint32>(UDPKey);
	SendPacket(Packet, OP_EMULEPROT);
}

void CMuleClient::SendFWCheckACK()
{
	LogLine(LOG_DEBUG, tr("SendFWCheckACK"));

	CBuffer Packet(1);
	Packet.WriteValue<uint8>(OP_KAD_FWTCPCHECK_ACK);
	SendPacket(Packet, OP_EMULEPROT);
}

void CMuleClient::SendFileRequest(CFile* pFile)
{
	CFileHash* pHash = pFile->GetHash(HashEd2k);

	if (MultiPacket() || ExtMultiPacket() || SupportsFileIdent()) // Note: SupportsFileIdent implicates the use of ExtMultiPacket
	{
		LogLine(LOG_DEBUG, tr("SendMultiPacket"));

		CBuffer Packet;
		if(SupportsFileIdent())
		{
			Packet.WriteValue<uint8>(OP_MULTIPACKET_EXT2);
			CMuleTags::WriteIdentifier(pFile, &Packet);
		}
		else if(ExtMultiPacket())
		{
			Packet.WriteValue<uint8>(OP_MULTIPACKET_EXT);
			Packet.WriteQData(pFile->GetHash(HashEd2k)->GetHash());
			Packet.WriteValue<uint64>(pFile->GetFileSize());
		}
		else
		{
			Packet.WriteQData(pFile->GetHash(HashEd2k)->GetHash());
			Packet.WriteValue<uint8>(OP_MULTIPACKET);
		}

		Packet.WriteValue<uint8>(OP_REQUESTFILENAME);
		if (ExtendedRequestsVer() > 0)
			WriteFileStatus(Packet, pFile);
		if (ExtendedRequestsVer() > 1)
			Packet.WriteValue<uint16>(pFile->GetStats()->GetAvailStats());

		if(pFile->GetFileSize() > (ProtocolRevision() > 0 ? ED2K_CRUMBSIZE : ED2K_PARTSIZE))
			Packet.WriteValue<uint8>(OP_REQUESTFILESTATUS);

		if (SupportsAICH() && !SupportsFileIdent()) // deprecated with fileidentifiers
			Packet.WriteValue<uint8>(OP_AICHFILEHASHREQ); // request AICH File hash

		if (SupportsSourceEx2() && theCore->m_MuleManager->GrantSX(pFile))
		{
			Packet.WriteValue<uint8>(OP_REQUESTSOURCES2);
			Packet.WriteValue<uint8>(ExtendedSourceEx() ? SOURCEEXCHANGEEXT_VERSION : SOURCEEXCHANGE2_VERSION);
			const uint16 nOptions = 0; // 16 ... Reserved
			Packet.WriteValue<uint16>(nOptions);
		}

		SendPacket(Packet, OP_EMULEPROT);
	}
	else
	{
		LogLine(LOG_DEBUG, tr("SendFileNameRequest"));

		CBuffer Packet;
		Packet.WriteValue<uint8>(OP_REQUESTFILENAME);
		Packet.WriteQData(pHash->GetHash());
		if (ExtendedRequestsVer() > 0)
			WriteFileStatus(Packet, pFile);
		if (ExtendedRequestsVer() > 1)
			Packet.WriteValue<uint16>(pFile->GetStats()->GetAvailStats());
		SendPacket(Packet, OP_EDONKEYPROT);

		if(pFile->GetFileSize() > (ProtocolRevision() > 0 ? ED2K_CRUMBSIZE : ED2K_PARTSIZE))
		{
			LogLine(LOG_DEBUG, tr("SendFileStatusRequest"));

			CBuffer AuxPacket(1 + 16);
			AuxPacket.WriteValue<uint8>(OP_REQUESTFILESTATUS);
			AuxPacket.WriteQData(pHash->GetHash());
			SendPacket(AuxPacket, OP_EDONKEYPROT);
		}

		if (SupportsAICH())
		{
			LogLine(LOG_DEBUG, tr("SendAICHHashRequest"));

			CBuffer AuxPacket(1 + 16);
			AuxPacket.WriteValue<uint8>(OP_AICHFILEHASHREQ);
			AuxPacket.WriteQData(pHash->GetHash());
			SendPacket(AuxPacket, OP_EMULEPROT);
		}

		if (SupportsSourceEx2() && theCore->m_MuleManager->GrantSX(pFile))
		{
			LogLine(LOG_DEBUG, tr("SendSourceRequest"));

			CBuffer AuxPacket(1 + 1 + 2 + 16);
			AuxPacket.WriteValue<uint8>(OP_REQUESTSOURCES2);
			AuxPacket.WriteValue<uint8>(ExtendedSourceEx() ? SOURCEEXCHANGEEXT_VERSION : SOURCEEXCHANGE2_VERSION);
			uint16 uXSOptions = 0; // 16 ... Reserved
			AuxPacket.WriteValue<uint16>(uXSOptions);
			AuxPacket.WriteQData(pHash->GetHash());
			SendPacket(AuxPacket, OP_EMULEPROT);
		}
	}
}

void CMuleClient::SendUploadRequest()
{
	LogLine(LOG_DEBUG, tr("SendUploadRequest"));

	CFileHash* pHash = m_DownSource->GetFile()->GetHash(HashEd2k);
	ASSERT(pHash);

	CBuffer Packet(1+16);
	Packet.WriteValue<uint8>(OP_STARTUPLOADREQ);
	Packet.WriteQData(pHash->GetHash());
	SendPacket(Packet, OP_EDONKEYPROT);
}

void CMuleClient::SendHashSetRequest(CFile* pFile)
{
	LogLine(LOG_DEBUG, tr("SendHashSetRequest"));

	if (SupportsFileIdent())
	{
		CFileHashSet* pHashSet = qobject_cast<CFileHashSet*>(pFile->GetHash(HashEd2k));
		CFileHashTree* pHashTree = qobject_cast<CFileHashTree*>(pFile->GetHash(HashMule));

		UMuleIdentReq IdentReq;
		IdentReq.Fields.uRequestingMD4 = pHashSet && pHashSet->IsValid() && !pHashSet->CanHashParts();
		IdentReq.Fields.uRequestingAICH = pHashTree && pHashTree->IsValid() && !pHashTree->CanHashParts();
		IdentReq.Fields.uOptions = 0;
		ASSERT(IdentReq.Bits != 0);

		CBuffer Packet;
		Packet.WriteValue<uint8>(OP_HASHSETREQUEST2);
		CMuleTags::WriteIdentifier(pFile, &Packet);
		Packet.WriteValue<uint8>(IdentReq.Bits);
		SendPacket(Packet, OP_EMULEPROT);
	}
	else
	{
		CFileHash* pHash = pFile->GetHash(HashEd2k);

		CBuffer Packet(1+16);
		Packet.WriteValue<uint8>(OP_HASHSETREQUEST);
		Packet.WriteQData(pHash->GetHash());
		SendPacket(Packet, OP_EDONKEYPROT);
	}
}

void CMuleClient::SendAICHRequest(CFile* pFile, uint16 uPart)
{
	LogLine(LOG_DEBUG, tr("SendAICHRequest, part %1").arg(uPart));

	//theCore->m_MuleManager->AddAICHRequest(this, pFile, uPart); // Done by manager

	CBuffer Packet(1 + 16 + 2 + 20);
	Packet.WriteValue<uint8>(OP_AICHREQUEST);
	Packet.WriteQData(pFile->GetHash(HashEd2k)->GetHash());
	Packet.WriteValue<uint16>(uPart);
	Packet.WriteQData(pFile->GetHash(HashMule)->GetHash());
	SendPacket(Packet, OP_EMULEPROT);
}

void CMuleClient::WriteFileStatus(CBuffer& Packet, CFile* pFile)
{
	ASSERT(pFile);

	/*if(SupportsRangeMap())
	{
		CShareMap RangeMap(pFile->GetFileSize());
		if(CPartMap* pParts = pFile->GetPartMap())
		{
			RangeMap.From(pParts);
		}
		else if(pFile->IsComplete())
		{
			CShareMap::ValueType uNewState = CShareMap::eAvailable;
			if(pFile->GetHash(HashNeo))
				uNewState |= CShareMap::eNeoVerified;
			if(pFile->GetHash(HashEd2k))
				uNewState |= CShareMap::eEd2kVerified;
			if(pFile->GetHash(HashMule))
				uNewState |= CShareMap::eMuleVerified;
			RangeMap.SetRange(0, -1, uNewState);
		}
		RangeMap.Write(&Packet, IsLargeEd2kMuleFile(pFile->GetFileSize()));
	}
	else*/

	CFileHashSet* pHashSet = qobject_cast<CFileHashSet*>(pFile->GetHash(HashEd2k));
	ASSERT(pHashSet);

	if(ProtocolRevision())
	{
		if(pFile->IsComplete())
		{
			/*if(ProtocolRevision() > CT_REVISION_NETFWARP) // this is not used by neft yet
			{
				Packet.WriteValue<uint16>(1);
				Packet.WriteValue<uint8>(0);
			}
			else*/
				Packet.WriteValue<uint16>(0);
			return;
		}

		uint64 uFileSize = pFile->GetFileSize();
		uint32 uPartCount = DivUp(uFileSize, ED2K_PARTSIZE);

		QBitArray BitField;
		CFileHashTree* pHashTree = qobject_cast<CFileHashTree*>(pFile->GetHash(HashMule));
		if (ProtocolRevision() >= CT_REVISION_NETFWARP && pHashTree) // AICH
		{
			uint64 PartSize = pHashSet->GetPartSize();
			int JoinBlocks = 1;

			ASSERT(PartSize == pHashTree->GetPartSize());
			ASSERT(pHashTree->GetPartSize() == ED2K_PARTSIZE);
			ASSERT(pHashTree->GetBlockSize() == ED2K_BLOCKSIZE);

			uint16 uWholeParts = uFileSize / ED2K_PARTSIZE;
			uint64 uLastPart = uFileSize % ED2K_PARTSIZE;

			int uMultis[6] = {1, 2, 4, 8, 16, 32}; // (1^i) - count of blocks to put into one bit
			int uDivs[6] = {53, 27, 14, 7, 4, 2};
			uint32 uTotalBits;
			for(int i=0; i < 6; i++)
			{
				uTotalBits = (uWholeParts * uDivs[i]) + DivUp(uLastPart, ED2K_BLOCKSIZE * uMultis[i]);
				if (uTotalBits <= 8000)
				{
					JoinBlocks = uMultis[i];
					break;
				}
			}

			QBitArray StatusMap = pHashTree->GetStatusMap();
			if(!StatusMap.isEmpty())
			{
				if(JoinBlocks == 1)
				{
					ASSERT(StatusMap.size() == uTotalBits);
					BitField = StatusMap;
				}
				else
				{
					BitField.resize(uTotalBits);
					int Index = 0;
					for(uint32 i = 0; i < StatusMap.size(); i += 53) // for each block in part steps
					{
						for(uint32 j=0; j < 53 && Index < uTotalBits; j += JoinBlocks) // for each block in part
							BitField.setBit(Index++, testBits(StatusMap, QPair<uint32, uint32>(i + j, Min(Min(i + j + JoinBlocks, i + 53), StatusMap.size()))));
					}
					ASSERT(Index = uTotalBits);
				}
			}
			else 
				BitField = QBitArray(uTotalBits, 0);
		}
		else if (uFileSize <= (8000 * ED2K_CRUMBSIZE)) // hybrid
		{
			BitField.resize(DivUp(uFileSize, ED2K_CRUMBSIZE));
			for(uint32 i = 0; i < BitField.size(); i++)
			{
				uint64 uPos = ED2K_CRUMBSIZE * i;
				if(pHashSet->GetResult(uPos, Min(uPos + ED2K_CRUMBSIZE, uFileSize)))
					BitField.setBit(i);
			}
		}
		else // ed2k
		{
			BitField = pHashSet->GetStatusMap();
			BitField.resize(uPartCount); // the map can be empty if nothing was verifyed yet, or it misses the last empty part that
		}
		QByteArray Bits = CShareMap::Bits2Bytes(BitField, true);
		Packet.WriteValue<uint16>(BitField.size());
		Packet.WriteData(Bits.data(), Bits.size());
	}
	else
	{	
		if(pFile->IsComplete())
		{
			Packet.WriteValue<uint16>(0);
			return;
		}

		// the partcount is right as long the file is not exact a multiple of ED2K_PARTSIZE that its one to much
		// this is an old ed2k bug that emule never fixed
		uint16 Ed2kPartCount = (pFile->GetFileSize() / ED2K_PARTSIZE + 1);
		Packet.WriteValue<uint16>(Ed2kPartCount);
		
		QBitArray BitField = pHashSet->GetStatusMap();
		BitField.resize(Ed2kPartCount); // the map can be empty if nothing was verifyed yet, or it misses the last empty part that

		QByteArray Bits = CShareMap::Bits2Bytes(BitField, true);
		Packet.WriteData(Bits.data(), Bits.size());
	}
}

void CMuleClient::ReadFileStatus(const CBuffer& Packet, CMuleSource* pSource)
{
	CFile* pFile = pSource->GetFile();

	/*if(SupportsRangeMap())
	{
		CShareMap RangeMap(pFile->GetFileSize());
		if(!RangeMap.Read(&Packet))
			throw CException(LOG_ERROR | LOG_DEBUG, L"Invalid part map Recived");
		pSource->RangesAvailable(&RangeMap);
	}
	else*/

	if(ProtocolRevision())
	{
		QBitArray BitField;

		uint64 uFileSize = pFile->GetFileSize();
		uint64 uSubChunkSize = 0;
		uint16 uSubChunkCount = 1;

		uint16 uSCTCount = Packet.ReadValue<uint16>();
		if(!uSCTCount)
		{
			BitField.resize(DivUp(uFileSize,ED2K_PARTSIZE));
			BitField.fill(true);
		}
		else if(uSCTCount == 1)
		{
			BitField.resize(DivUp(uFileSize,ED2K_PARTSIZE));
			BitField.fill(Packet.ReadValue<uint8>());
		}
		else
		{
			uint16 uWholeParts = uFileSize / ED2K_PARTSIZE;
			uint64 uLastPart = uFileSize % ED2K_PARTSIZE;

			if (uSCTCount == DivUp(uFileSize, ED2K_PARTSIZE)) // standard (old) chunks
			{
				uSubChunkSize = ED2K_PARTSIZE;
				uSubChunkCount = 1;
			}
			else if (uSCTCount == DivUp(uFileSize, ED2K_CRUMBSIZE)) // crumbs
			{
				uSubChunkSize = ED2K_CRUMBSIZE;
				uSubChunkCount = ED2K_PARTSIZE / ED2K_CRUMBSIZE; // 20;
			}
			else // new (aich) sub chunks
			{
				int uMultis[6] = {1, 2, 4, 8, 16, 32}; // (1^i)
				int uDivs[6] = {53, 27, 14, 7, 4, 2};
				for(int i=0; i < 6; i++)
				{
					if (uSCTCount == (uWholeParts * uDivs[i]) + DivUp(uLastPart, ED2K_BLOCKSIZE * uMultis[i]))
					{
						uSubChunkSize = ED2K_BLOCKSIZE * uMultis[i];
						uSubChunkCount = uDivs[i];
						break;
					}
				}
			}

			if(uSubChunkSize == 0 || uSCTCount != (uSubChunkCount * uWholeParts) + DivUp(uLastPart, uSubChunkSize))
				throw CException(LOG_ERROR | LOG_DEBUG, L"Invalid SCT Part Vector");

			BitField = CShareMap::Bytes2Bits(Packet.ReadQData(DivUp(uSCTCount, 8)), uSCTCount, true);
		}

		pSource->PartsAvailable(BitField, ED2K_PARTSIZE, uSubChunkSize);
	}
	else
	{
		QBitArray BitField;
		uint16 PartCount = DivUp(pFile->GetFileSize(),ED2K_PARTSIZE);
		uint16 Ed2kPartCount = Packet.ReadValue<uint16>();
		if(!Ed2kPartCount)
		{
			BitField.resize(PartCount);
			BitField.fill(true);
		}
		else
		{
			BitField = CShareMap::Bytes2Bits(Packet.ReadQData(DivUp(Ed2kPartCount, 8)), Ed2kPartCount, true);
			BitField.truncate(PartCount);
		}
		pSource->PartsAvailable(BitField, ED2K_PARTSIZE);
	}
}

void CMuleClient::SendCrumbComplete(CFile* pFile, uint32 Index)
{
	LogLine(LOG_DEBUG, tr("SendCrumbComplete"));

	CFileHash* pHash = pFile->GetHash(HashEd2k);
	ASSERT(pHash);

	CBuffer Packet(1+16+4);
	Packet.WriteValue<uint8>(OP_CRUMBCOMPLETE);
	Packet.WriteQData(pHash->GetHash());
	Packet.WriteValue<uint32>(Index);
	SendPacket(Packet, OP_EDONKEYPROT);
}

void CMuleClient::SendSXAnswer(CFile* pFile, uint8 uXSVersion, uint16 uXSOptions)
{
	if(!ExtendedSourceEx() && uXSVersion < SOURCEEXCHANGE2_VERSION)
	{
		ASSERT(0);
		return;
	}

	LogLine(LOG_DEBUG, tr("SendSXAnswer"));

	CBuffer Packet;
	Packet.WriteValue<uint8>(OP_ANSWERSOURCES2);
	Packet.WriteValue<uint8>(uXSVersion); // Note: if we are going to send an older version than requested, change it here

	Packet.WriteQData(pFile->GetHash(HashEd2k)->GetHash());
	uint16 Count = 0;
	size_t CountPos = Packet.GetPosition();
	Packet.WriteValue<uint16>(Count);

	foreach (CTransfer* pTransfer, pFile->GetTransfers())
	{
		if(CMuleSource* pSource = qobject_cast<CMuleSource*>(pTransfer))
		{
			const SMuleSource& Mule = pSource->GetClient()->GetMule();
			CMuleClient* pClient = pSource->GetClient();
			if(!pClient || !pClient->WasConnected())
				continue; 
			// EM-ToDo-Now: <<<<<<<< check IP connectivity 
			// EM-ToDo-Now: send only sources we know the asking clinet will need

			Packet.WriteValue<uint32>(Mule.ClientID); // for high ID's this is thair IP in host order just as we like it
			Packet.WriteValue<uint16>(Mule.TCPPort);
			if(ExtendedSourceEx())
			{
				QVariantMap Tags;
				if(Mule.HasLowID())
				{
					if(Mule.HasV4()) // Note: since the ID is not the IP in this case we need to send the address separatly
						Tags[TO_NUM(CT_EMULE_ADDRESS)]	= _ntohl(Mule.IPv4.ToIPv4());

					if(Mule.ConOpts.Fields.DirectUDPCallback || Mule.ConOpts.Fields.SupportsNatTraversal)
					{
						Tags[TO_NUM(CT_EMULE_UDPPORTS)]	= ((uint32)Mule.KadPort << 16) 
														| ((uint32)Mule.UDPPort << 0);
					}

					if(!Mule.BuddyID.isEmpty())
					{
						Tags[TO_NUM(CT_EMULE_BUDDYIP)]	= _ntohl(Mule.BuddyAddress.ToIPv4());
						Tags[TO_NUM(CT_EMULE_BUDDYUDP)]	= Mule.BuddyPort;
						CMuleHash BuddyID((byte*)Mule.BuddyID.data());
						Tags[TO_NUM(CT_EMULE_BUDDYID)]	= QVariant(CMuleHash::GetVariantType(), &BuddyID);
					}

					if(!Mule.ServerAddress.IsNull())
					{
						if(Mule.ServerAddress.Type() == CAddress::IPv4)
							Tags[TO_NUM(CT_EMULE_SERVERIP)]	= _ntohl(Mule.ServerAddress.ToIPv4());
						else
							Tags[TO_NUM(CT_EMULE_SERVERIP)]	= QByteArray((char*)Mule.ServerAddress.Data(), 16);
						Tags[TO_NUM(CT_EMULE_SERVERTCP)]= Mule.ServerPort;
					}
				}

				if(Mule.HasV6() && !Mule.ClosedIPv6())
					Tags[TO_NUM(CT_NEOMULE_IP_V6)]	= QVariant(CMuleHash::GetVariantType(), Mule.IPv6.Data());

				CMuleTags::WriteTags(Tags, &Packet, true, true);
			}
			else
			{
				Packet.WriteValue<uint32>(_ntohl(Mule.ServerAddress.ToIPv4()));
				Packet.WriteValue<uint16>(Mule.ServerPort);
			}
			Packet.WriteData(pSource->GetUserHash().Data, 16);
			Packet.WriteValue<uint8>(Mule.ConOpts.Bits);

			Count++;
		}
    }

	Packet.SetPosition(CountPos);
	Packet.WriteValue<uint16>(Count);
	Packet.SetPosition(-1); // goto end

	SendPacket(Packet, OP_EMULEPROT);
}

void CMuleClient::ProcessSXAnswer(const CBuffer& Packet)
{
	LogLine(LOG_DEBUG, tr("ProcessSXAnswer"));

	uint8 uXSVersion = Packet.ReadValue<uint8>();
	if(!ExtendedSourceEx() && uXSVersion != SOURCEEXCHANGE2_VERSION)
	{
		ASSERT(0);
		return;
	}

	CFileHash Hash(HashEd2k);
	Hash.SetHash(Packet.ReadQData(Hash.GetSize()));
	CFile* pFile = theCore->m_FileManager->GetFileByHash(&Hash);
	if(!pFile)
	{
		LogLine(LOG_DEBUG, tr("Resived sources for an unknown file"));
		return;
	}

	theCore->m_MuleManager->ConfirmSX(pFile);

	uint16 Count = Packet.ReadValue<uint16>();
	for(uint16 i = 0; i < Count; i++)
	{
		SMuleSource Mule;
		
		Mule.ClientID			= Packet.ReadValue<uint32>();
		if(!Mule.HasLowID() && Mule.ClientID != 0xFFFFFFFF) // 0xFFFFFFFF means HighID with IPv6 only
			Mule.SetIP(Mule.ClientID);
		Mule.TCPPort			= Packet.ReadValue<uint16>();
		if(ExtendedSourceEx())
		{
			QVariantMap Tags = CMuleTags::ReadTags(&Packet, true);
			for(QVariantMap::iterator I = Tags.begin(); I != Tags.end(); ++I)
			{
				const QVariant& Value = I.value();
				switch(FROM_NUM(I.key()))
				{
					case CT_EMULE_ADDRESS:		Mule.SetIP(_ntohl(Value.toUInt()));							break;
					case CT_NEOMULE_IP_V6:		Mule.SetIP(Value.value<CMuleHash>().GetData());	
												Mule.OpenIPv6 = true;										break;
					case CT_EMULE_UDPPORTS:		Mule.UDPPort = (uint16)(Value.toUInt() >> 0);
												Mule.KadPort = (uint16)(Value.toUInt() >> 16);				break;

					case CT_EMULE_BUDDYIP:		Mule.BuddyAddress = CAddress(_ntohl(Value.toUInt()));		break;
					case CT_EMULE_BUDDYUDP:		Mule.BuddyPort = Value.toUInt();							break;
					case CT_EMULE_BUDDYID:		Mule.BuddyID = Value.value<CMuleHash>().ToByteArray();		break;

					case CT_EMULE_SERVERIP:		if(Value.canConvert(QVariant::UInt))
													Mule.ServerAddress = CAddress(_ntohl(Value.toUInt()));
												else 
													Mule.ServerAddress.FromArray(Value.toByteArray());		break;
					case CT_EMULE_SERVERTCP:	Mule.ServerPort = Value.toUInt();							break;

					default:
						LogLine(LOG_DEBUG, tr("Recived unknwon extended Source Exchange tag"));
				}
			}
		}
		else
		{
			Mule.ServerAddress = _ntohl(Packet.ReadValue<uint32>());
			Mule.ServerPort = Packet.ReadValue<uint16>();
		}
		Mule.UserHash.Set(Packet.ReadData(16));
		Mule.ConOpts.Bits	= Packet.ReadValue<uint8>();

		theCore->m_MuleManager->AddToFile(pFile, Mule, eXS);
	}
}

void CMuleClient::SendFNF(const CFileHash* pHash)
{
	LogLine(LOG_DEBUG | LOG_ERROR, tr("Ed2kMule client requested a file we dont have, or don't share"));

	if(!m_Socket)
		return;

	LogLine(LOG_DEBUG, tr("SendFNF"));

	CBuffer Packet(1 + 16);
	Packet.WriteValue<uint8>(OP_FILEREQANSNOFIL);
	Packet.WriteQData(pHash->GetHash());
	SendPacket(Packet, OP_EDONKEYPROT);
}

void CMuleClient::SendRankingInfo(uint16 uRank)
{
	LogLine(LOG_DEBUG, tr("SendRankingInfo"));

	if (MuleProtSupport())
	{
		CBuffer Packet(1 + 2 + 10);
		Packet.WriteValue<uint8>(OP_QUEUERANKING);
		Packet.WriteValue<uint16>(uRank);
		Packet.WriteData(NULL,10); // emule requirers 10 NULL bytes what a waist
		SendPacket(Packet, OP_EMULEPROT);
	}
	else
	{
		CBuffer Packet(1 + 4);
		Packet.WriteValue<uint8>(OP_QUEUERANK);
		Packet.WriteValue<uint16>(uRank);
		SendPacket(Packet, OP_EDONKEYPROT);
	}
}

void CMuleClient::SendAcceptUpload()
{
	LogLine(LOG_DEBUG, tr("SendAcceptUpload"));

	m_Socket->SetUpload(true);

	m_SentPartSize = 0; // reset upload counter
	m_RecivedPartSize = 0;
	//m_TempPartSize = 0;
	m_HordeState = eNone;
	m_HordeTimeout = -1;

	CBuffer Packet(1);
	Packet.WriteValue<uint8>(OP_ACCEPTUPLOADREQ);
	SendPacket(Packet, OP_EDONKEYPROT);
}

void CMuleClient::SendOutOfPartReqs()
{
	LogLine(LOG_DEBUG, tr("SendOutOfPartReqs"));

	CBuffer Packet(1);
	Packet.WriteValue<uint8>(OP_OUTOFPARTREQS);
	SendPacket(Packet, OP_EDONKEYPROT);

	m_Socket->ClearQueue();
	m_Socket->SetUpload(false);
}

void CMuleClient::SendComment(CFile* pFile, QString Description, uint8 Rating)
{
	LogLine(LOG_DEBUG, tr("SendComment"));

	CFileHash* pHash = pFile->GetHash(HashEd2k);
	ASSERT(pHash);

	CBuffer Packet;
	Packet.WriteValue<uint8>(OP_FILEDESC);
	if(SupportsExtendedComments())
		Packet.WriteQData(pHash->GetHash());
	Packet.WriteValue<uint8>(Rating);
	Packet.WriteString(Description.toStdWString(), UnicodeSupport() ? CBuffer::eUtf8 : CBuffer::eAscii, CBuffer::e32Bit);
	SendPacket(Packet, OP_EMULEPROT);
}

void CMuleClient::SendBlockRequest(CFile* pFile, uint64 Begins[3], uint64 Ends[3])
{
	int iDebug = theCore->Cfg()->GetInt("Log/Level");
	if(iDebug == 3)
		LogLine(LOG_DEBUG, tr("SendBlockRequest"));

	CFileHash* pHash = pFile->GetHash(HashEd2k);

	bool bI64 = SupportsLargeFiles();

	CBuffer Packet(1 + 16 + (bI64 ? (3*8) + (3*8) : (3*4) + (3*4)));
	Packet.WriteValue<uint8>(bI64 ? OP_REQUESTPARTS_I64 : OP_REQUESTPARTS);
	Packet.WriteQData(pHash->GetHash());
	for(int i=0; i<3; i++)
	{
		if(bI64)
			Packet.WriteValue<uint64>(Begins[i]);
		else
			Packet.WriteValue<uint32>(Begins[i]);
	}
	for(int i=0; i<3; i++)
	{
		if(bI64)
			Packet.WriteValue<uint64>(Ends[i]);
		else
			Packet.WriteValue<uint32>(Ends[i]);
	}
	SendPacket(Packet, bI64 ? OP_EMULEPROT : OP_EDONKEYPROT);


	for(int i=0; i<3; i++)
	{
		if(Ends[i] > Begins[i])
		{
			if(iDebug == 3)
				LogLine(LOG_DEBUG, tr("-> Request %1 - %2").arg(Begins[i]).arg(Ends[i]));
			SRequestedBlock* pBlock = new SRequestedBlock();
			pBlock->uBegin = Begins[i];
			pBlock->uEnd = Ends[i];
			pBlock->pFile = pFile;
			m_IncomingBlocks.append(pBlock);
		}
	}
}

void CMuleClient::ProcessBlockRequest(const CBuffer& Packet, bool bI64)
{
	int iDebug = theCore->Cfg()->GetInt("Log/Level");
	if(iDebug == 3)
		LogLine(LOG_DEBUG, tr("ProcessBlockRequest"));

	CFileHash Hash(HashEd2k);
	Hash.SetHash(Packet.ReadQData(Hash.GetSize()));
	CFile* pFile = theCore->m_FileManager->GetFileByHash(&Hash);
	if(!pFile)
	{
		SendFNF(&Hash);
		return;
	}

	if(!m_UpSource || !m_UpSource->IsActiveUpload())
		return;

	if(m_ProtocolRevision != 0 && m_HordeState == eNone && m_DownSource && theCore->m_UploadManager->OfferHorde(m_UpSource))
		SendHordeRequest(pFile);

	uint64 Begins[3];
	uint64 Ends[3];
	for(int i=0; i<3; i++)
		Begins[i] = bI64 ? Packet.ReadValue<uint64>() : Packet.ReadValue<uint32>();
	for(int i=0; i<3; i++)
		Ends[i] = bI64 ? Packet.ReadValue<uint64>() : Packet.ReadValue<uint32>();

	CPartMap* pMap = pFile->IsIncomplete() ? pFile->GetPartMap() : NULL;

	for(int i=0; i<3; i++)
	{
		if(Ends[i] <= Begins[i])
			continue;

		bool bRedundant = false;
		for(int j=0; j < m_OutgoingBlocks.count(); j++)
		{
			SPendingBlock* pBlock = m_OutgoingBlocks[j];
			if(bRedundant = (pBlock->uBegin == Begins[i] && pBlock->uEnd == Ends[i]))
			{
				m_OutgoingBlocks.append(m_OutgoingBlocks.takeAt(j)); // move block down so we keep track of it
				break;
			}
		}
		if(bRedundant)
			continue;

		SPendingBlock* pBlock = new SPendingBlock();
		pBlock->uBegin = Begins[i];
		pBlock->uEnd = Ends[i];
		memcpy(pBlock->Hash, Hash.GetHash().data(), 16);

		m_OutgoingBlocks.append(pBlock);

		uint64 Length = Ends[i] - Begins[i];

		if(pMap && (pMap->GetRange(pBlock->uBegin, pBlock->uBegin + Length) & Part::Available) == 0)
		{
			LogLine(LOG_ERROR | LOG_DEBUG, tr("Client requested a incomplete part"));
			return;
		}

		//m_TempPartSize += Length;
		theCore->m_IOManager->ReadData(this, pFile->GetFileID(), pBlock->uBegin, Length, pBlock);
	}	
}

//uint64 CMuleClient::GetRequestedSize()
//{
//	if(!m_Socket)
//		return 0;
//	return m_TempPartSize + m_Socket->QueueSize();
//}

void CMuleClient::OnDataRead(uint64 Offset, uint64 Length, const QByteArray& Data, bool bOk, void* Aux)
{
	if(!m_UpSource)
		return;

	SPendingBlock* pBlock = (SPendingBlock*)Aux;
	if(!m_OutgoingBlocks.contains(pBlock))
		return; // its ok, the upload go canceled in the mean time probably

	if(!m_Socket)
		return;

	if(Data.size() != pBlock->uEnd - pBlock->uBegin)
	{
		LogLine(LOG_DEBUG | LOG_ERROR, tr("Read Error when processing emule block request"));
		CancelUpload();
		return;
	}

	int iDebug = theCore->Cfg()->GetInt("Log/Level");
	if(iDebug == 3)
		LogLine(LOG_DEBUG, tr("QueueBlock"));

	//m_TempPartSize -= Data.size();

	QByteArray PackedData;
	if(DataCompVer() == 1 && theCore->m_MuleManager->GetServer()->GetUpLimit()->GetRate() < KB2B(128)) // C-ToDo: customize
		PackedData = Pack(Data);

	bool bI64 = SupportsLargeFiles();

	if(!PackedData.isEmpty() && PackedData.size() < Data.size())
	{
		m_SentPartSize += PackedData.size();
		m_UpSource->OnBytesWritten(PackedData.size());
		theCore->m_MuleManager->OnBytesWritten(PackedData.size());

		for(uint64 Pos = 0; Pos < PackedData.size(); )
		{
			uint64 Size = Min(KB2B(10), PackedData.size() - Pos);

			CBuffer Packet(1 + 16 + (bI64 ? 8 : 4) + 4 + Size);
			Packet.WriteValue<uint8>(bI64 ? OP_COMPRESSEDPART_I64 : OP_COMPRESSEDPART);
			Packet.WriteData(pBlock->Hash,16);
			if(bI64)
				Packet.WriteValue<uint64>(pBlock->uBegin);
			else
				Packet.WriteValue<uint32>(pBlock->uBegin);
			Packet.WriteValue<uint32>(PackedData.size());
			Packet.WriteData((byte*)PackedData.data() + Pos, Size);

			m_Socket->QueuePacket((uint64)Aux, Packet.ToByteArray(), OP_EMULEPROT);

			Pos += Size;
		}
	}		
	else
	{
		m_SentPartSize += Data.size();
		m_UpSource->OnBytesWritten(Data.size());
		theCore->m_MuleManager->OnBytesWritten(Data.size());

		for(uint64 Pos = 0; Pos < Data.size(); )
		{
			uint64 Size = Min(KB2B(10), Data.size() - Pos);
			uint64 uBegin = pBlock->uBegin + Pos;
			uint64 uEnd = uBegin + Size;

			CBuffer Packet(1 + 16 + (bI64 ? 8 + 8 : 4 + 4) + Size);
			Packet.WriteValue<uint8>(bI64 ? OP_SENDINGPART_I64 : OP_SENDINGPART);
			Packet.WriteData(pBlock->Hash,16);
			if(bI64)
			{
				Packet.WriteValue<uint64>(uBegin);
				Packet.WriteValue<uint64>(uEnd);
			}
			else
			{
				Packet.WriteValue<uint32>(uBegin);
				Packet.WriteValue<uint32>(uEnd);
			}
			Packet.WriteData((byte*)Data.data() + Pos, Size);

			m_Socket->QueuePacket((uint64)Aux, Packet.ToByteArray(), bI64 ? OP_EMULEPROT : OP_EDONKEYPROT);

			Pos += Size;
		}
	}

	// we must only keep track of the last 3 blocks
	while(m_OutgoingBlocks.size() > 3)
	{
		ASSERT(pBlock != m_OutgoingBlocks.first());
		delete m_OutgoingBlocks.takeFirst();
	}
}

void CMuleClient::CancelUpload()
{
	ClearUpload();
	SetUpSource(NULL);
}

void CMuleClient::ProcessBlock(const CBuffer& Packet, bool bPacked, bool bI64)
{
	int iDebug = theCore->Cfg()->GetInt("Log/Level");
	if(iDebug == 3)
		LogLine(LOG_DEBUG, tr("ProcessBlock"));

	CFileHash Hash(HashEd2k);
	Hash.SetHash(Packet.ReadQData(Hash.GetSize()));

	if(!m_DownSource)
	{
		LogLine(LOG_DEBUG, tr("recived packet for a downlaod operaion, without started one"));
		return;
	}
	if(!Hash.Compare(m_DownSource->GetFile()->GetHash(HashEd2k)))
	{
		LogLine(LOG_DEBUG, tr("recived packet for a downlaod operaion, for a different hash"));
		return;
	}
	if(!m_DownSource->GetFile()->IsIncomplete())
		return; // the file was complete in the mean time weare not interested anymore

	CFile* pFile = m_DownSource->GetFile();

	SRequestedBlock* pBlock = NULL;
	if(bPacked)
	{
		uint64 uBegin = bI64 ? Packet.ReadValue<uint64>() : Packet.ReadValue<uint32>();
		uint64 uNewSize = Packet.ReadValue<uint32>();
		QByteArray PackedData = Packet.ReadQData();

		m_RecivedPartSize += PackedData.size();
		m_DownSource->OnBytesReceived(PackedData.size());
		theCore->m_MuleManager->OnBytesReceived(PackedData.size());

		int i = 0;
		for (; i < m_IncomingBlocks.size(); i++) 
		{
			SRequestedBlock* pCurBlock = m_IncomingBlocks.at(i);
			if (pCurBlock->uBegin == uBegin)
			{
				pBlock = pCurBlock;

				QByteArray Data = ungzip_arr(pBlock->zStream, PackedData, false);
				if(Data.size() > 0)
				{
					//LogLine(LOG_INFO, tr("-> Recived %1-%2").arg(uBegin + pBlock->uWriten).arg(uBegin + pBlock->uWriten + Data.size()));

					uint64 uOffset = uBegin + pBlock->uWriten;

					pBlock->uWriten += Data.length();
					if(pBlock->uEnd - pBlock->uBegin <= pBlock->uWriten)
					{
						ASSERT(pBlock->uEnd - pBlock->uBegin == pBlock->uWriten);
						clear_z(pBlock->zStream);
						m_IncomingBlocks.removeAt(i--);
						delete pBlock;
					}

					m_DownSource->RangeReceived(uOffset, uOffset + Data.length(), Data);
				}

				break;
			}
		}
		//ASSERT(i < m_IncomingBlocks.size()); // Note: this can happen on fast start stop of file
	}
	else
	{
		uint64 uBegin = bI64 ? Packet.ReadValue<uint64>() : Packet.ReadValue<uint32>();
		uint64 uEnd = bI64 ? Packet.ReadValue<uint64>() : Packet.ReadValue<uint32>();
		QByteArray Data = Packet.ReadQData();

		m_RecivedPartSize += Data.size();
		m_DownSource->OnBytesReceived(Data.size());
		theCore->m_MuleManager->OnBytesReceived(Data.size());

		int i = 0;
		for (; i < m_IncomingBlocks.size(); i++) 
		{
			SRequestedBlock* pCurBlock = m_IncomingBlocks.at(i);
			if (uBegin >= pCurBlock->uBegin && uEnd <= pCurBlock->uEnd)
			{
				pBlock = pCurBlock;

				uint64 uLength = uEnd - uBegin;
				ASSERT(uLength == Data.size());
				//LogLine(LOG_INFO, tr("-> Recived %1-%2").arg(uBegin).arg(uEnd));

				pBlock->uWriten += Data.length();
				if(pBlock->uEnd - pBlock->uBegin <= pBlock->uWriten)
				{
					ASSERT(pBlock->uEnd - pBlock->uBegin == pBlock->uWriten);
					m_IncomingBlocks.removeAt(i--);
					delete pBlock;
				}
				if(uLength == 0)
					break;

				m_DownSource->RangeReceived(uBegin, uEnd, Data);
				break;
			}
		}
		//ASSERT(i < m_IncomingBlocks.size()); // Note: this can happen on fast start stop of file
	}

	// request more
	m_DownSource->RequestBlocks();
}

void CMuleClient::CancelDownload()
{
	if(m_Socket)
	{
		LogLine(LOG_DEBUG, tr("SendCancelTransfer"));

		CBuffer Packet(1);
		Packet.WriteValue<uint8>(OP_CANCELTRANSFER);
		SendPacket(Packet, OP_EDONKEYPROT);
	}

	ClearDownload();
	SetDownSource(NULL);
}

void CMuleClient::SendHordeRequest(CFile* pFile)
{
	m_HordeState = eRequested;
	m_HordeTimeout = GetCurTick() + SEC2MS(10);

	LogLine(LOG_DEBUG, tr("SendHordeRequest"));

	CFileHash* pHash = pFile->GetHash(HashEd2k);
	ASSERT(pHash);

	CBuffer Packet(1+16);
	Packet.WriteValue<uint8>(OP_HORDESLOTREQ);
	Packet.WriteQData(pHash->GetHash());
	SendPacket(Packet, OP_EDONKEYPROT);
}

void CMuleClient::AttacheSource(CMuleSource* pSource)
{
	if(!m_AllSources.contains(pSource->GetFile()->GetHash(HashEd2k)->GetHash(), pSource))
		m_AllSources.insert(pSource->GetFile()->GetHash(HashEd2k)->GetHash(), pSource);
}

void CMuleClient::DettacheSource(CMuleSource* pSource)
{
	QByteArray Hash = m_AllSources.key(pSource);
	m_AllSources.remove(Hash, pSource); 

	if(pSource == m_UpSource) 
	{
		if(m_Socket)
			m_Socket->RemoveUpLimit(m_UpSource->GetFile()->GetUpLimit());
		m_UpSource = NULL; 
	}

	if(pSource == m_DownSource) 
	{
		if(m_Socket)
			m_Socket->RemoveDownLimit(m_DownSource->GetFile()->GetDownLimit());
		m_DownSource = NULL;
	}
}

CMuleSource* CMuleClient::GetSource(const CFileHash* pHash, uint64 uSize)
{
	if(!pHash)
		return NULL;
	CMuleSource* pSource = m_AllSources.value(pHash->GetHash());
	if(!pSource)
		pSource = theCore->m_MuleManager->AttacheToFile(pHash, uSize, this);
	if(!pSource || !pSource->GetFile()->IsStarted() || !pSource->GetFile()->IsEd2kShared()) // the requested file couldn't be found or is not active
		return NULL; 
	return pSource;
}

void CMuleClient::SetUpSource(CMuleSource* pSource)
{
	if(m_UpSource == pSource)
		return;

	if(m_UpSource)
		m_UpSource->CancelUpload();

	if(m_Socket && m_UpSource)
		m_Socket->RemoveUpLimit(m_UpSource->GetFile()->GetUpLimit());

	m_UpSource = pSource;

	if(m_Socket && m_UpSource)
		m_Socket->AddUpLimit(m_UpSource->GetFile()->GetUpLimit());
}

void CMuleClient::SetDownSource(CMuleSource* pSource)	
{
	ASSERT(m_DownSource != pSource);

	if(m_DownSource)
		m_DownSource->EndDownload();

	if(m_Socket && m_DownSource)
		m_Socket->RemoveDownLimit(m_DownSource->GetFile()->GetDownLimit());

	ClearDownload(); // shoud not be needed, but in case

	m_DownSource = pSource;

	if(m_Socket && m_DownSource)
		m_Socket->AddDownLimit(m_DownSource->GetFile()->GetDownLimit());
}

void CMuleClient::ProcessPacket(CBuffer& Packet, uint8 Prot)
{
	uint8 uOpcode = Packet.ReadValue<uint8>();
	switch(Prot)
	{
		case OP_EDONKEYPROT:
		{
			switch(uOpcode)
			{
				case OP_HELLO:
				{
					// Make sure we drop all unencrypted connections if we require encryption
					if(!m_Socket || (theCore->m_MuleManager->RequiresCryptLayer() && !m_Socket->IsEncrypted()))
					{
						Disconnect();
						return;
					}

					if(Packet.ReadValue<uint8>() != 16) // it must be 16
						throw CException(LOG_ERROR | LOG_DEBUG, L"Recived invalid user hash length");

					ProcessHelloHead(Packet);

					// Note: We must make sure the socket knows the remote user hash, so it can encrypto for a first try decrypt and not for backup decrypt
					if(m_Socket->GetSocketType() == SOCK_UTP)
					{
						CMuleServer* pServer = (CMuleServer*)m_Socket->GetServer();
						if(pServer->HasCrypto(m_Socket->GetAddress(), m_Socket->GetPort()))
							pServer->SetupCrypto(m_Socket->GetAddress(), m_Socket->GetPort(), m_Mule.UserHash.ToArray(true));
					}

					CMuleClient* Client = this;
					if(!theCore->m_MuleManager->DispatchClient(Client)) // note: this will change the pointer if the client is already known and swap the m_Socket
					{
						Disconnect();
						return;
					}

					Client->ReadHelloInfo(Packet);

					Client->SendHelloPacket(true); // send answer

					if(Client->MuleProtSupport() && theCore->m_MuleManager->GrantIPRequest())
						Client->SendPublicIPRequest();

					if(m_Socket)
						theCore->m_MuleManager->FW()->Incoming(m_Socket->GetAddress().Type(), m_Socket->GetSocketType() == SOCK_UTP ? CFWWatch::eUTP : CFWWatch::eTCP);
					break;
				}
				case OP_HELLOANSWER:
				{
					ProcessHelloHead(Packet);

					ReadHelloInfo(Packet);

					if(m_Socket)
						theCore->m_MuleManager->FW()->Outgoing(m_Socket->GetAddress().Type(), m_Socket->GetSocketType() == SOCK_UTP ? CFWWatch::eUTP : CFWWatch::eTCP);
					break;
				}

				//case OP_CHANGE_CLIENT_ID: // used by Overnet

				case OP_REQUESTFILENAME:
				{
					LogLine(LOG_DEBUG, tr("ProcessFileNameRequest"));

					CFileHash Hash(HashEd2k);
					Hash.SetHash(Packet.ReadQData(Hash.GetSize()));
					CMuleSource* pSource = GetSource(&Hash);
					if(!pSource)
					{
						SendFNF(&Hash);
						return;
					}

					if (ExtendedRequestsVer() > 0)
						ReadFileStatus(Packet, pSource);
					if (ExtendedRequestsVer() > 1)
						pSource->SetRemoteAvailability(Packet.ReadValue<uint16>());

					LogLine(LOG_DEBUG, tr("SendFileNameAnswer"));

					CBuffer Answer;
					Answer.WriteValue<uint8>(OP_REQFILENAMEANSWER);
					Answer.WriteQData(Hash.GetHash());
					Answer.WriteString(pSource->GetFile()->GetFileName().toStdWString(), UnicodeSupport() ? CBuffer::eUtf8 : CBuffer::eAscii, CBuffer::e16Bit);
					SendPacket(Answer, OP_EDONKEYPROT);
					break;
				}
				case OP_REQUESTFILESTATUS:
				{
					LogLine(LOG_DEBUG, tr("ProcessFileStatusRequest"));

					CFileHash Hash(HashEd2k);
					Hash.SetHash(Packet.ReadQData(Hash.GetSize()));
					CMuleSource* pSource = GetSource(&Hash);
					if(!pSource)
					{
						SendFNF(&Hash);
						return;
					}

					LogLine(LOG_DEBUG, tr("SendFileStatusAnswer"));
					CBuffer Answer;
					Answer.WriteValue<uint8>(OP_FILESTATUS);
					Answer.WriteQData(Hash.GetHash());
					WriteFileStatus(Answer, pSource->GetFile());
					SendPacket(Answer, OP_EDONKEYPROT);
					break;
				}
				case OP_FILEREQANSNOFIL:
				{
					LogLine(LOG_DEBUG, tr("ProcessFNF"));

					CFileHash Hash(HashEd2k);
					Hash.SetHash(Packet.ReadQData(Hash.GetSize()));
					CMuleSource* pSource = GetSource(&Hash);
					if(!pSource)
						return;

					LogLine(LOG_DEBUG | LOG_WARNING, tr("Recived a file not found"));
					pSource->GetFile()->RemoveTransfer(pSource);
					break;
				}
				case OP_REQFILENAMEANSWER:
				{
					LogLine(LOG_DEBUG, tr("ProcessFileNameAnswer"));

					CFileHash Hash(HashEd2k);
					Hash.SetHash(Packet.ReadQData(Hash.GetSize()));
					CMuleSource* pSource = GetSource(&Hash);
					if(!pSource)
						return;

					QString FileName = QString::fromStdWString(Packet.ReadString(UnicodeSupport() ? CBuffer::eUtf8 : CBuffer::eAscii, CBuffer::e16Bit));
					pSource->SetFileName(FileName);
					CFile* pFile = pSource->GetFile();
					CFileHashSet* pHashSet = qobject_cast<CFileHashSet*>(pFile->GetHash(HashEd2k));

					// Comment copyed form eMule (Fair Use):
					// 26-Jul-2003: removed requesting the file status for files <= PARTSIZE for better compatibility with ed2k protocol (eDonkeyHybrid).
					// if the remote client answers the OP_REQUESTFILENAME with OP_REQFILENAMEANSWER the file is shared by the remote client. if we
					// know that the file is shared, we know also that the file is complete and don't need to request the file status.
					uint16 Ed2kPartCount = (pFile->GetFileSize() / ED2K_PARTSIZE + 1);
					if(Ed2kPartCount == 1)
					{
						pSource->PartsAvailable(QBitArray(1, true), ED2K_PARTSIZE);

						CFileHashSet* pHashSet = qobject_cast<CFileHashSet*>(pFile->GetHash(HashEd2k));
						if(!pHashSet->CanHashParts())
						{
							QList<QByteArray> HashSet;
							HashSet.append(pHashSet->GetHash());
							pHashSet->SetHashSet(HashSet);
						}
					}

					if(!pHashSet->CanHashParts())
						SendHashSetRequest(pFile);

					if(pSource == m_DownSource && pHashSet->GetPartCount() == 1 && m_DownSource->IsInteresting())
						SendUploadRequest();
					break;
				}
				case OP_FILESTATUS:
				{
					LogLine(LOG_DEBUG, tr("ProcessFileStatusAnswer"));

					CFileHash Hash(HashEd2k);
					Hash.SetHash(Packet.ReadQData(Hash.GetSize()));
					CMuleSource* pSource = GetSource(&Hash);
					if(!pSource)
						return;

					ReadFileStatus(Packet, pSource);
					
					if(pSource == m_DownSource && m_DownSource->IsInteresting())
						SendUploadRequest();
					break;
				}

				case OP_STARTUPLOADREQ:
				{
					LogLine(LOG_DEBUG, tr("ProcessUploadRequest"));

					CFileHash Hash(HashEd2k);
					Hash.SetHash(Packet.ReadQData(Hash.GetSize()));
					CMuleSource* pSource = GetSource(&Hash);
					if(!pSource)
					{
						// EM-ToDo: trace invalid requests and drop client on to many fials like emule does
						return;
					}
					SetUpSource(pSource);
					pSource->RequestUpload();
					break;
				}
				case OP_QUEUERANK:		// used by ed2k not by emule
				{
					LogLine(LOG_DEBUG, tr("ProcessRankingInfo"));

					if(!m_DownSource)
					{
						LogLine(LOG_DEBUG, tr("recived packet for a downlaod operaion, without started one"));
						return;
					}
					uint32 QueueRank = Packet.ReadValue<uint32>();
					m_DownSource->SetQueueRank(QueueRank);
					break;
				}
				case OP_ACCEPTUPLOADREQ:
				{
					LogLine(LOG_DEBUG, tr("ProcessAcceptUpload"));

					if(m_Socket)
						m_Socket->SetDownload(true);

					if(!m_DownSource)
					{
						LogLine(LOG_DEBUG, tr("recived packet for a downlaod operaion, without started one"));
						return;
					}
					m_RecivedPartSize = 0;
					m_DownSource->OnDownloadStart();
					break;
				}

				case OP_REQUESTPARTS:
					ProcessBlockRequest(Packet);
					break;
				case OP_SENDINGPART:
					ProcessBlock(Packet);
					break;

				case OP_END_OF_DOWNLOAD: // used by ed2k not by emule
				case OP_CANCELTRANSFER:
					LogLine(LOG_DEBUG, tr("ProcessCancelTransfer"));
					CancelUpload();
					break;

				case OP_OUTOFPARTREQS:
				{
					LogLine(LOG_DEBUG, tr("ProcessOutOfPartReqs"));

					if(m_Socket)
						m_Socket->SetDownload(false);

					if(!m_DownSource)
					{
						LogLine(LOG_DEBUG, tr("recived packet for a downlaod operaion, without started one"));
						return;
					}
					ClearDownload();
					m_DownSource->OnDownloadStop();
					break;
				}

				case OP_HASHSETREQUEST:
				{
					LogLine(LOG_DEBUG, tr("ProcessHashSetRequest"));

					CFileHash Hash(HashEd2k);
					Hash.SetHash(Packet.ReadQData(Hash.GetSize()));
					CFile* pFile = theCore->m_FileManager->GetFileByHash(&Hash);
					if(!pFile)
					{
						LogLine(LOG_DEBUG, tr("Requested a hashset for a not existing file"));
						return;
					}
					CFileHashSet* pHashSet = qobject_cast<CFileHashSet*>(pFile->GetHash(HashEd2k));
					if(!pHashSet)
					{
						LogLine(LOG_DEBUG, tr("Requested a hashset for a file that does not have one"));
						return;
					}

					LogLine(LOG_DEBUG, tr("SendHashSetAnswer"));

					CBuffer Answer;
					Answer.WriteValue<uint8>(OP_HASHSETANSWER);
					CMuleTags::WriteHashSet(pHashSet, &Answer);
					SendPacket(Answer, OP_EDONKEYPROT);
					break;
				}
				case OP_HASHSETANSWER:
				{
					LogLine(LOG_DEBUG, tr("ProcessHashSetAnswer"));

					// EM-ToDo-Now: fail on unsolicited answers, like emule does					

					CFileHash Hash(HashEd2k);
					Hash.SetHash(Packet.ReadQData(Hash.GetSize()));
					CMuleSource* pSource = GetSource(&Hash);
					if(!pSource)
						return;

					CFile* pFile = pSource->GetFile();
					if(pFile->IsComplete())
						return;

					CFileHashSet* pHashSet = qobject_cast<CFileHashSet*>(pFile->GetHash(HashEd2k));
					if(!CMuleTags::ReadHashSet(&Packet, pHashSet, Hash.GetHash()))
						LogLine(LOG_DEBUG, tr("Got an invalid hashset"));
					else
						theCore->m_Hashing->SaveHash(pHashSet);
					break;
				}

				//case OP_CHANGE_SLOT:		// sometimes sent by Hybrid

				// don't support messages, just log them
				case OP_MESSAGE:
				{
					QString Message = QString::fromStdWString(Packet.ReadString(UnicodeSupport() ? CBuffer::eUtf8 : CBuffer::eAscii));
					LogLine(LOG_DEBUG, tr("Recived ed2k message %1").arg(Message));
					break;
				}

				// not supported
				case OP_ASKSHAREDDIRS:
				case OP_ASKSHAREDFILESDIR:
				{
					CBuffer Answer(1);
					Answer.WriteValue<uint8>(OP_ASKSHAREDDENIEDANS);
					SendPacket(Answer, OP_EDONKEYPROT);
				}
				case OP_ASKSHAREDDIRSANS:
				case OP_ASKSHAREDFILESDIRANS:
				case OP_ASKSHAREDDENIEDANS:
					return;

				// eDonkey Hybrid Horde
				case OP_HORDESLOTREQ:
				{
					LogLine(LOG_DEBUG, tr("ProcessHordeSlotRequest"));

					QByteArray FileHash = Packet.ReadQData(16);

					CBuffer Answer(1 + 16);
					if(m_UpSource && m_DownSource && theCore->m_UploadManager->AcceptHorde(m_UpSource))
					{
						m_HordeState = eAccepted;

						LogLine(LOG_DEBUG, tr("SendHordeSlotAccept"));
						Answer.WriteValue<uint8>(OP_HORDESLOTANS);
					}
					else
					{
						LogLine(LOG_DEBUG, tr("SendHordeSlotReject"));
						Answer.WriteValue<uint8>(OP_HORDESLOTREJ);
					}
					Answer.WriteQData(FileHash);
					SendPacket(Answer, OP_EDONKEYPROT);
					break;
				}
				case OP_HORDESLOTANS:
				{
					if(m_HordeState == eRequested)
					{
						m_HordeTimeout = -1;
						m_HordeState = eAccepted;
					}
					LogLine(LOG_DEBUG, tr("ProcessHordeSlotAnswer"));
					break;
				}
				case OP_HORDESLOTREJ:
				{
					if(m_HordeState == eRequested)
					{
						m_HordeTimeout = GetCurTick() + MIN2MS(3);
						m_HordeState = eRejected;
					}
					LogLine(LOG_DEBUG, tr("ProcessHordeSlotReject"));
					break;
				}

				// eDonkey Hybrid Crumbs and Mule SCT
				case OP_CRUMBSETREQ:
				{
					LogLine(LOG_DEBUG, tr("ProcessCrumbSetRequest"));

					CFileHash Hash(HashEd2k);
					Hash.SetHash(Packet.ReadQData(Hash.GetSize()));
					CFile* pFile = theCore->m_FileManager->GetFileByHash(&Hash);
					if(!pFile)
					{
						LogLine(LOG_DEBUG, tr("Requested a chrumbset for a not existing file"));
						return;
					}

					LogLine(LOG_DEBUG, tr("SendCrumbSetAnswer"));

					CBuffer Answer;
					Answer.WriteValue<uint8>(OP_CRUMBSETANS);
					Answer.WriteQData(Hash.GetHash());
					if(pFile->GetFileSize() > ED2K_PARTSIZE)
					{
						if(CFileHashSet* pHashSet = qobject_cast<CFileHashSet*>(pFile->GetHash(HashEd2k)))
						{
							Answer.WriteValue<uint8>(1);
							QList<QByteArray> HashSet = pHashSet->GetHashSet();
							for (uint32 i = 0; i < HashSet.size(); i++)
								Answer.WriteQData(HashSet.at(i));
						}
						else
							Answer.WriteValue<uint8>(0);
					}

					// EM-ToDo-Now: add crumb hash set XXXXXXXXX
					Answer.WriteValue<uint8>(0); // no crumb set

					SendPacket(Answer, OP_EDONKEYPROT);
					break;
				}
				case OP_CRUMBSETANS:
				{
					LogLine(LOG_DEBUG, tr("ProcessCrumbSetAnswer"));

					// EM-ToDo-Now: fail on unsolicited answers, like emule does					

					CFileHash Hash(HashEd2k);
					Hash.SetHash(Packet.ReadQData(Hash.GetSize()));
					CMuleSource* pSource = GetSource(&Hash);
					if(!pSource)
						return;

					CFile* pFile = pSource->GetFile();
					if(pFile->IsComplete())
						return;

					if(pFile->GetFileSize() > ED2K_PARTSIZE)
					{
						if(Packet.ReadValue<uint8>())
						{
							CFileHashSet* pHashSet = qobject_cast<CFileHashSet*>(pFile->GetHash(HashEd2k));
							if(!CMuleTags::ReadHashSet(&Packet, pHashSet, Hash.GetHash()))
								LogLine(LOG_DEBUG, tr("Got an invalid hashset"));
							else
								theCore->m_Hashing->SaveHash(pHashSet);
						}
					}

					// EM-ToDo-Now: add crumb hash set XXXXXXXXX
					if(Packet.ReadValue<uint8>())
					{
						uint32 uHashCount = Packet.ReadValue<uint16>();
						for (uint32 i = 0; i < uHashCount; i++)
							Packet.ReadQData(16);
					}

					break;
				}

				case OP_CRUMBCOMPLETE:
				{
					LogLine(LOG_DEBUG, tr("ProcessCrumbComplete"));

					CFileHash Hash(HashEd2k);
					Hash.SetHash(Packet.ReadQData(Hash.GetSize()));
					CMuleSource* pSource = GetSource(&Hash);
					if(!pSource)
						return;

					uint32 Index = Packet.ReadValue<uint32>();
					pSource->OnCrumbComplete(Index);
					break;
				}

				//case OP_PUBLICIPNOTIFY:

				default:
					LogLine(LOG_DEBUG, tr("Recived unknwon Ed2kMule packet Opcode: %1").arg(uOpcode));
			}
			break;
		}
		case OP_EMULEPROT:
		{
			switch(uOpcode)
			{
				case OP_MULTIPACKET:
				case OP_MULTIPACKET_EXT:
				case OP_MULTIPACKET_EXT2: // EXT2 is with file identifyers
				{
					LogLine(LOG_DEBUG, tr("ProcessMultiPacket"));

					uint64 uFileSize = 0;
					QMap<EFileHashType,CFileHashPtr> HashMap;
					if(uOpcode == OP_MULTIPACKET_EXT2)
					{
						HashMap = CMuleTags::ReadIdentifier(&Packet, uFileSize);
						if(!HashMap.contains(HashEd2k)) // Note: md4 hash is mandatory
						{
							LogLine(LOG_ERROR, tr("Recived invalid File ident packet, MD4 hash is mandatory"));
							return;
						}
					}
					else
					{
						CScoped<CFileHash> pEd2kHash = new CFileHash(HashEd2k); // a read error will throw, don't loose memory
						pEd2kHash->SetHash(Packet.ReadQData(pEd2kHash->GetSize()));
						HashMap.insert(HashEd2k, CFileHashPtr(pEd2kHash.Detache()));
						if(uOpcode == OP_MULTIPACKET_EXT)
							uFileSize = Packet.ReadValue<uint64>();
					}

					CMuleSource* pSource = GetSource(HashMap.value(HashMule).data(), uFileSize);
					if(!pSource)
						pSource = GetSource(HashMap.value(HashEd2k).data(), uFileSize);
					if(!pSource) // first try AICH hash then ed2k
					{
						SendFNF(HashMap.value(HashEd2k).data());
						return;
					}

					LogLine(LOG_DEBUG, tr("SendMultiPacketAnswer"));

					CBuffer Answer;
					if(uOpcode == OP_MULTIPACKET_EXT2)
					{
						Answer.WriteValue<uint8>(OP_MULTIPACKETANSWER_EXT2);
						CMuleTags::WriteIdentifier(pSource->GetFile(), &Answer);
					}
					else
					{
						Answer.WriteValue<uint8>(OP_MULTIPACKETANSWER);
						Answer.WriteQData(HashMap.value(HashEd2k)->GetHash());
					}

					CFile* pFile = pSource->GetFile();

					bool bSendComment = false;
					while(Packet.GetSizeLeft())
					{
						switch(Packet.ReadValue<uint8>())
						{
							case OP_REQUESTFILENAME:
							{
								if (ExtendedRequestsVer() > 0)
									ReadFileStatus(Packet, pSource);
								if (ExtendedRequestsVer() > 1)
									pSource->SetRemoteAvailability(Packet.ReadValue<uint16>());

								Answer.WriteValue<uint8>(OP_REQFILENAMEANSWER);
								Answer.WriteString(pFile->GetFileName().toStdWString(), UnicodeSupport() ? CBuffer::eUtf8 : CBuffer::eAscii, CBuffer::e16Bit);
								bSendComment = SupportsExtendedComments();
								break;
							}
							case OP_REQUESTFILESTATUS:
							{
								Answer.WriteValue<uint8>(OP_FILESTATUS);
								WriteFileStatus(Answer, pFile);
								break;
							}

							case OP_AICHFILEHASHREQ:
							{
								if(CFileHash* pAICHHash = pFile->GetHash(HashMule))
								{
									Answer.WriteValue<uint8>(OP_AICHFILEHASHANS);
									Answer.WriteQData(pAICHHash->GetHash());
								}
								break;
							}

							//We still send the source packet seperately.. 
							//case OP_REQUESTSOURCES: // deprecated
							case OP_REQUESTSOURCES2:
							{
								uint8 uXSVersion = Packet.ReadValue<uint8>(); 
								uint16 uXSOptions = Packet.ReadValue<uint16>(); 
								SendSXAnswer(pFile, uXSVersion, uXSOptions);
								break;
							}

							default:
								LogLine(LOG_DEBUG, tr("Recived unknwon Ed2kMule packet extended multipacket Opcode"));
								return; // after this error we can not continue reaing this packet
						}
					}

					SendPacket(Answer, OP_EMULEPROT);

					if(bSendComment)
						pSource->SendCommendIfNeeded();
					break;
				}

				case OP_MULTIPACKETANSWER:
				case OP_MULTIPACKETANSWER_EXT2: // EXT2 is with file identifyers
				{
					LogLine(LOG_DEBUG, tr("ProcessMultiPacketAnswer"));

					uint64 uFileSize = 0;
					QMap<EFileHashType,CFileHashPtr> HashMap;
					if(uOpcode == OP_MULTIPACKETANSWER_EXT2)
					{
						HashMap = CMuleTags::ReadIdentifier(&Packet, uFileSize);
						if(!HashMap.contains(HashEd2k)) // Note: md4 hash is mandatory
						{
							LogLine(LOG_ERROR, tr("Recived invalid File ident packet, MD4 hash is mandatory"));
							return;
						}
					}
					else
					{
						CScoped<CFileHash> pEd2kHash = new CFileHash(HashEd2k); // a read error will throw, don't loose memory
						pEd2kHash->SetHash(Packet.ReadQData(pEd2kHash->GetSize()));
						HashMap.insert(HashEd2k, CFileHashPtr(pEd2kHash.Detache()));
					}

					CMuleSource* pSource = GetSource(HashMap.value(HashEd2k).data());
					if(!pSource)
						return;

					CFile* pFile = pSource->GetFile();
					if(HashMap.contains(HashMule) && pFile->IsIncomplete())
						pFile->GetInspector()->AddUntrustedHash(HashMap.value(HashMule), m_Mule.GetIP());

					uint16 Ed2kPartCount = (pFile->GetFileSize() / ED2K_PARTSIZE + 1);

					while(Packet.GetSizeLeft())
					{
						switch(Packet.ReadValue<uint8>())
						{
							case OP_REQFILENAMEANSWER:
							{
								QString FileName = QString::fromStdWString(Packet.ReadString(UnicodeSupport() ? CBuffer::eUtf8 : CBuffer::eAscii, CBuffer::e16Bit));
								pSource->SetFileName(FileName);

								// Comment copyed form eMule (Fair Use):
								// 26-Jul-2003: removed requesting the file status for files <= PARTSIZE for better compatibility with ed2k protocol (eDonkeyHybrid).
								// if the remote client answers the OP_REQUESTFILENAME with OP_REQFILENAMEANSWER the file is shared by the remote client. if we
								// know that the file is shared, we know also that the file is complete and don't need to request the file status.
								if(Ed2kPartCount == 1)
									pSource->PartsAvailable(QBitArray(1, true), ED2K_PARTSIZE);
								break;
							}
							case OP_FILESTATUS:
							{
								ReadFileStatus(Packet, pSource);
								break;
							}

							case OP_AICHFILEHASHANS:
							{
								CFileHashPtr AICHHash = CFileHashPtr(new CFileHash(HashMule));
								AICHHash->SetHash(Packet.ReadQData(AICHHash->GetSize()));
								if(pFile->IsIncomplete())
									pFile->GetInspector()->AddUntrustedHash(AICHHash, m_Mule.GetIP());
								break;
							}

							default:
								LogLine(LOG_DEBUG, tr("Recived unknwon Ed2kMule packet extended multipacket Opcode"));
								return; // after this error we can not continue reaing this packet
						}
					}

					CFileHashSet* pHashSet = qobject_cast<CFileHashSet*>(pFile->GetHash(HashEd2k));
					if(Ed2kPartCount == 1 && !pHashSet->CanHashParts())
					{
						QList<QByteArray> HashSet;
						HashSet.append(pHashSet->GetHash());
						pHashSet->SetHashSet(HashSet);
					}

					CFileHashTree* pHashTree = qobject_cast<CFileHashTree*>(pFile->GetHash(HashMule));
					if(!pHashSet->CanHashParts()
					 || (SupportsFileIdent() && HashMap.contains(HashMule) && pHashTree && !pHashTree->CanHashParts()))
						SendHashSetRequest(pFile);
					
					if(pSource == m_DownSource && m_DownSource->IsInteresting())
						SendUploadRequest();
					break;
				}

				//case OP_EMULEINFO: // deprecated
				//case OP_EMULEINFOANSWER: // deprecated

				case OP_QUEUERANKING:
				{
					LogLine(LOG_DEBUG, tr("ProcessRankingInfo"));

					if(!m_DownSource)
					{
						LogLine(LOG_DEBUG, tr("recived packet for a downlaod operaion, without started one"));
						return;
					}
					uint32 QueueRank = Packet.ReadValue<uint16>();
					m_DownSource->SetQueueRank(QueueRank);

					// emule requirers 10 NULL bytes what a waist
					if(Packet.GetSizeLeft() == 10)
						Packet.ReadData(10); 
					break;
				}

				//case OP_REQUESTSOURCES: // deprecated
				case OP_REQUESTSOURCES2:
				{
					LogLine(LOG_DEBUG, tr("ProcessSourceRequst"));

					uint8 uXSVersion = Packet.ReadValue<uint8>(); 
					uint16 uXSOptions = Packet.ReadValue<uint16>(); 
					CFileHash Hash(HashEd2k);
					Hash.SetHash(Packet.ReadQData(Hash.GetSize()));
					CFile* pFile = theCore->m_FileManager->GetFileByHash(&Hash);
					if(!pFile)
					{
						LogLine(LOG_DEBUG, tr("Requested a XS request for an non existing file"));
						return;
					}
					SendSXAnswer(pFile, uXSVersion, uXSOptions);
					break;
				}
				//case OP_ANSWERSOURCES: // deprecated
				case OP_ANSWERSOURCES2:
					ProcessSXAnswer(Packet);
					break;

				// SUI
				case OP_SECIDENTSTATE:
				{
					LogLine(LOG_DEBUG, tr("ProcessSecIdentState"));

					uint8 uState = Packet.ReadValue<uint8>(); 
					switch(uState)
					{
						case IS_UNAVAILABLE:
							break;
						case IS_KEYANDSIGNEEDED:
						{
							if(CPublicKey* pPubKey = theCore->m_MuleManager->GetPublicKey())
							{
								CBuffer Answer;
								Answer.WriteValue<uint8>(OP_PUBLICKEY);
								size_t uSize = pPubKey->GetSize();
								Answer.WriteValue<uint8>(uSize);
								Answer.WriteData(pPubKey->GetKey(), uSize);
								SendPacket(Answer, OP_EMULEPROT);
							}
						}
						case IS_SIGNATURENEEDED:
						{
							m_uRndChallenge = Packet.ReadValue<uint32>(); 
							if(m_SUIKey.isEmpty()) // we need the key
							{
								CBuffer Request;
								Request.WriteValue<uint8>(OP_SECIDENTSTATE);
								Request.WriteValue<uint8>(IS_KEYANDSIGNEEDED);
								Request.WriteValue<uint32>(rand() + 1);
								SendPacket(Request, OP_EMULEPROT);
							}
							else
								SendSignature();
						}
					}
					break;
				}
				case OP_PUBLICKEY:
				{
					LogLine(LOG_DEBUG, tr("ProcessPublicKey"));

					size_t uSize = Packet.ReadValue<uint8>(); 
					m_SUIKey = Packet.ReadQData(uSize);
					if(m_uRndChallenge)
						SendSignature();
					break;
				}
  				case OP_SIGNATURE: 
				{
					LogLine(LOG_DEBUG, tr("ProcessSignature"));
					size_t uSize = Packet.ReadValue<uint8>(); 
					Packet.ReadQData(uSize);
					// we dont give a damn
					break;
				}
				//

				case OP_FILEDESC:
				{
					LogLine(LOG_DEBUG, tr("ProcessComment"));

					// Note: eMule sends this only in contect of a upload request
					CMuleSource* pSource = m_DownSource;
					if(SupportsExtendedComments())
					{
						CFileHash Hash(HashEd2k);
						Hash.SetHash(Packet.ReadQData(Hash.GetSize()));
						pSource = GetSource(&Hash);
					}
					if(!pSource)
						return;
					
					uint8 Rating = Packet.ReadValue<uint8>();
					QString Description = QString::fromStdWString(Packet.ReadString(UnicodeSupport() ? CBuffer::eUtf8 : CBuffer::eAscii, CBuffer::e32Bit));
					m_DownSource->HandleComment(Description,Rating);
					break;
				}

				case OP_REQUESTPARTS_I64:
					ProcessBlockRequest(Packet, true);
					break;
				case OP_COMPRESSEDPART:
					ProcessBlock(Packet, true);
					break;
				case OP_SENDINGPART_I64:
					ProcessBlock(Packet, false, true);
					break;
				case OP_COMPRESSEDPART_I64:
					ProcessBlock(Packet, true, true);
					break;

				case OP_AICHFILEHASHREQ:
				{
					LogLine(LOG_DEBUG, tr("ProcessAICHHashRequest"));

					CFileHash Hash(HashEd2k);
					Hash.SetHash(Packet.ReadQData(Hash.GetSize()));
					CFile* pFile = theCore->m_FileManager->GetFileByHash(&Hash);
					if(!pFile)
					{
						LogLine(LOG_DEBUG, tr("Requested a aich hash for a not existing file"));
						return;
					}
					if(CFileHash* pAICHHash = pFile->GetHash(HashMule))
					{
						LogLine(LOG_DEBUG, tr("SendAICHHashAnswer"));

						CBuffer Answer(1 + 16 + 20);
						Answer.WriteValue<uint8>(OP_AICHFILEHASHANS);
						Answer.WriteQData(Hash.GetHash());
						Answer.WriteQData(pAICHHash->GetHash());
						SendPacket(Answer, OP_EMULEPROT);
					}
					break;
				}
				case OP_AICHFILEHASHANS:
				{
					LogLine(LOG_DEBUG, tr("ProcessAICHHashAnswer"));

					CFileHash Hash(HashEd2k);
					Hash.SetHash(Packet.ReadQData(Hash.GetSize()));

					CFile* pFile = theCore->m_FileManager->GetFileByHash(&Hash);
					if(!pFile)
					{
						LogLine(LOG_DEBUG, tr("Requested a aich hash for a not existing file"));
						return;
					}
					
					CFileHashPtr AICHHash = CFileHashPtr(new CFileHash(HashMule));
					AICHHash->SetHash(Packet.ReadQData(AICHHash->GetSize()));
					if(pFile->IsIncomplete())
						pFile->GetInspector()->AddUntrustedHash(AICHHash, m_Mule.GetIP());
					break;
				}

				case OP_AICHREQUEST:
				{
					LogLine(LOG_DEBUG, tr("ProcessAICHRequest"));

					LogLine(LOG_DEBUG, tr("SendAICHRespone"));

					CFileHash Hash(HashEd2k);
					Hash.SetHash(Packet.ReadQData(Hash.GetSize()));
					uint16 uPart = Packet.ReadValue<uint16>(); 
					CFileHash AICHHash(HashMule);
					AICHHash.SetHash(Packet.ReadQData(AICHHash.GetSize()));

					CBuffer Answer;
					Answer.WriteValue<uint8>(OP_AICHANSWER);
					Answer.WriteQData(Hash.GetHash());
					if(CFile* pFile = theCore->m_FileManager->GetFileByHash(&Hash))
					{
						CFileHashTree* pHashTree = qobject_cast<CFileHashTree*>(pFile->GetHash(HashMule));
						if(AICHHash.Compare(pHashTree))
						{
							uint64 uBegin = uPart*ED2K_PARTSIZE;
							uint64 uEnd = Min(uBegin + ED2K_PARTSIZE, pFile->GetFileSize());
							if(pHashTree->IsFullyResolved(uBegin, uEnd))
							{
								if(CScoped<CFileHashTree> pBranche = pHashTree->GetLeafs(uBegin, uEnd))
								{
									Answer.WriteValue<uint16>(uPart);
									Answer.WriteQData(pHashTree->GetHash());
									CMuleTags::WriteAICHRecovery(pBranche, &Answer);
								}
								else
									LogLine(LOG_DEBUG, tr("Failed to create AICH recivery branche"));
							}
						}
						else
							LogLine(LOG_DEBUG, tr("Recivec a aich resuest for a wrong master hash"));
					}
					else
						LogLine(LOG_DEBUG, tr("Recivec a aich resuest for a not existing file"));
					SendPacket(Answer, OP_EMULEPROT);
					break;
				}
				case OP_AICHANSWER:
				{
					LogLine(LOG_DEBUG, tr("ProcessAICHRespone"));

					CFileHash Hash(HashEd2k);
					Hash.SetHash(Packet.ReadQData(Hash.GetSize()));
					CFile* pFile = theCore->m_FileManager->GetFileByHash(&Hash);
					if(!pFile)
					{
						LogLine(LOG_DEBUG, tr("Recivec a aich response for a not existing file"));
						return;
					}

					if(Packet.GetSizeLeft() == 0)
					{
						theCore->m_MuleManager->DropAICHRequests(this, pFile);
						break;
					}
					uint16 uPart = Packet.ReadValue<uint16>(); 
					CFileHash AICHHash(HashMule);
					AICHHash.SetHash(Packet.ReadQData(AICHHash.GetSize()));

					if(!theCore->m_MuleManager->CheckAICHRequest(this, pFile, uPart))
					{
						LogLine(LOG_DEBUG, tr("Recivec unsolicited aich response, part %1").arg(uPart));
						return;
					}
					if(pFile->IsComplete())
						return;

					CFileHashTree* pHashTree = qobject_cast<CFileHashTree*>(pFile->GetHash(HashMule));
					if(AICHHash.Compare(pHashTree))
					{
						CScoped<CFileHashTree> pBranche = new CFileHashTree(HashMule,pHashTree->GetTotalSize());
						CMuleTags::ReadAICHRecovery(&Packet, pBranche);
						if(!pHashTree->AddLeafs(pBranche))
							LogLine(LOG_DEBUG, tr("Recivec an invalid AICH resposne"));
						else
						{
							// EM-ToDo: store it at once ono unload if there are changes
							//theCore->m_Hashing->SaveHash(pHashTree);
							pFile->GetInspector()->OnRecoveryData();
						}
					}
					else
						LogLine(LOG_DEBUG, tr("Recivec a aich response for a wrong master hash"));
					break;
				}

				case OP_HASHSETREQUEST2:
				{
					LogLine(LOG_DEBUG, tr("ProcessHashSetRequest"));

					uint64 uFileSize = 0;
					QMap<EFileHashType,CFileHashPtr> HashMap = CMuleTags::ReadIdentifier(&Packet, uFileSize);
					CFile* pFile = HashMap.contains(HashMule) ? theCore->m_FileManager->GetFileByHash(HashMap.value(HashMule).data()) : NULL;
					if(!pFile)
						pFile = HashMap.contains(HashEd2k) ? theCore->m_FileManager->GetFileByHash(HashMap.value(HashEd2k).data()) : NULL;

					if(!pFile)
					{
						LogLine(LOG_DEBUG, tr("Requested a hashset for a not existing file"));
						return;
					}

					LogLine(LOG_DEBUG, tr("SendHashSetAnswer"));

					UMuleIdentReq IdentReq;
					IdentReq.Bits = Packet.ReadValue<uint8>();

					CFileHashSet* pHashSet = NULL;
					if(IdentReq.Fields.uRequestingMD4)
					{
						pHashSet = qobject_cast<CFileHashSet*>(pFile->GetHash(HashEd2k));
						if(!pHashSet || !pHashSet->CanHashParts())
							IdentReq.Fields.uRequestingMD4 = false;
					}

					CFileHashTree* pHashTree = NULL;
					if(IdentReq.Fields.uRequestingAICH)
					{
						pHashTree = qobject_cast<CFileHashTree*>(pFile->GetHash(HashMule));
						if(!pHashTree || !pHashTree->CanHashParts())
							IdentReq.Fields.uRequestingAICH = false;
					}

					CBuffer Answer;
					Answer.WriteValue<uint8>(OP_HASHSETANSWER2);
					CMuleTags::WriteIdentifier(pFile, &Answer);
					Answer.WriteValue<uint8>(IdentReq.Bits);
					if(IdentReq.Fields.uRequestingMD4)
						CMuleTags::WriteHashSet(pHashSet, &Answer);
					if(IdentReq.Fields.uRequestingAICH)
						CMuleTags::WriteHashSet(pHashTree, &Answer);
					SendPacket(Answer, OP_EMULEPROT);
					break;
				}
				case OP_HASHSETANSWER2:
				{
					LogLine(LOG_DEBUG, tr("ProcessHashSetAnswer"));

					// EM-ToDo-Now: fail on unsolicited answers, like emule does

					uint64 uFileSize = 0;
					QMap<EFileHashType,CFileHashPtr> HashMap = CMuleTags::ReadIdentifier(&Packet, uFileSize);
					CFile* pFile = HashMap.contains(HashMule) ? theCore->m_FileManager->GetFileByHash(HashMap.value(HashMule).data()) : NULL;
					if(!pFile)
						pFile = HashMap.contains(HashEd2k) ? theCore->m_FileManager->GetFileByHash(HashMap.value(HashEd2k).data()) : NULL;

					if(!pFile)
					{
						LogLine(LOG_DEBUG, tr("Resived a hashset for a not existing file"));
						return;
					}

					if(pFile->IsComplete())
						return;

					UMuleIdentReq IdentReq;
					IdentReq.Bits = Packet.ReadValue<uint8>();

					if(IdentReq.Fields.uRequestingMD4)
					{
						CFileHashSet* pHashSet = qobject_cast<CFileHashSet*>(pFile->GetHash(HashEd2k));
						if(!CMuleTags::ReadHashSet(&Packet, pHashSet))
							LogLine(LOG_DEBUG, tr("Got an invalid MD4 hashset"));
						else
							theCore->m_Hashing->SaveHash(pHashSet);
					}
					
					if(IdentReq.Fields.uRequestingAICH)
					{
						CFileHashTree* pHashTree = qobject_cast<CFileHashTree*>(pFile->GetHash(HashMule));
						if(!CMuleTags::ReadHashSet(&Packet, pHashTree))
							LogLine(LOG_DEBUG, tr("Got an invalid AICH hashset"));
						else
							theCore->m_Hashing->SaveHash(pHashTree);
					}
					break;
				}

				case OP_CALLBACK:
				{
					LogLine(LOG_DEBUG, tr("ProcessKadCallback"));

					CMuleClient* pBuddy = theCore->m_MuleManager->GetKad()->GetBuddy();
					if(this != pBuddy)
						return;

					QByteArray BuddyID = CMuleTags::ReadUInt128(&Packet);
					if(pBuddy->GetBuddyID() != BuddyID)
					{
						QByteArray FileID = CMuleTags::ReadUInt128(&Packet);
						
						SMuleSource Mule;
						Mule.SetIP(_ntohl(Packet.ReadValue<uint32>()));
						Mule.TCPPort = Packet.ReadValue<uint16>();
						theCore->m_MuleManager->CallbackRequested(Mule);
					}
					break;
				}
				case OP_BUDDYPING:
				{
					LogLine(LOG_DEBUG, tr("ProcessBuddyPing"));

					CMuleClient* pBuddy = theCore->m_MuleManager->GetKad()->GetBuddy(true);
					if(this != pBuddy)
						return;

					LogLine(LOG_DEBUG, tr("SendBuddyPong"));

					CBuffer Answer(1);
					Answer.WriteValue<uint8>(OP_BUDDYPONG);
					SendPacket(Answer, OP_EMULEPROT);

					theCore->m_MuleManager->GetKad()->RecivedBuddyPing();
					break;
				}
				case OP_BUDDYPONG:
				{
					LogLine(LOG_DEBUG, tr("ProcessBuddyPong"));

					CMuleClient* pBuddy = theCore->m_MuleManager->GetKad()->GetBuddy(true);
					if(this != pBuddy)
						return;

					theCore->m_MuleManager->GetKad()->RecivedBuddyPing();
					break;
				}

				case OP_REASKCALLBACKTCP:
				{
					LogLine(LOG_DEBUG, tr("ProcessReaskCallback"));

					if(this != theCore->m_MuleManager->GetKad()->GetBuddy())
						return;

					CAddress Address;
					uint32 uIP = _ntohl(Packet.ReadValue<uint32>());
					if(uIP == -1)
						Address = CAddress(Packet.ReadData(16));
					else
						Address = CAddress(uIP);
					uint16 uUDPPort = Packet.ReadValue<uint16>();

					CBuffer AuxPacket;
					QByteArray FileHash = Packet.ReadQData(16);
					if(FileHash != QByteArray(16, '\0'))
					{	
						AuxPacket.WriteValue<uint8>(OP_REASKFILEPING);
						AuxPacket.WriteQData(FileHash);
						AuxPacket.WriteData(Packet.GetData(0), Packet.GetSizeLeft());
					}
					else // Dummy Hash followed by a nested packet
						AuxPacket.WriteData(Packet.GetData(0), Packet.GetSizeLeft());
					theCore->m_MuleManager->ProcessUDPPacket(AuxPacket, OP_EMULEPROT, Address, uUDPPort);
					break;
				}

				case OP_FWCHECKUDPREQ:
				{
					LogLine(LOG_DEBUG, tr("ProcessFWCheckUDP"));

					uint16 IntPort = Packet.ReadValue<uint16>();
					uint16 ExtPort = Packet.ReadValue<uint16>();
					uint32 UDPKey = Packet.ReadValue<uint32>();
					theCore->m_MuleManager->GetKad()->FWCheckUDPRequested(this, IntPort, ExtPort, UDPKey);
					break;
				}
				case OP_KAD_FWTCPCHECK_ACK:
				{
					LogLine(LOG_DEBUG, tr("ProcessFWCheckACK"));

					theCore->m_MuleManager->GetKad()->FWCheckACKRecived(this);
					break;
				}

				case OP_PUBLICIP_REQ:
				{
					LogLine(LOG_DEBUG, tr("ProcessPublicIPRequest"));

					LogLine(LOG_DEBUG, tr("SendPublicIPAnswer"));
					CBuffer Answer(1 + 4);
					Answer.WriteValue<uint8>(OP_PUBLICIP_ANSWER);
					CAddress IP = m_Mule.GetIP();
					if(IP.Type() == CAddress::IPv4)
						Answer.WriteValue<uint32>(_ntohl(IP.ToIPv4()));
					else if(IP.Type() == CAddress::IPv6)
					{
						Answer.WriteValue<uint32>(-1);
						Answer.WriteData(IP.Data(), 16);
					}
					SendPacket(Answer, OP_EMULEPROT);
					break;
				}
				case OP_PUBLICIP_ANSWER:
				{
					LogLine(LOG_DEBUG, tr("ProcessPublicIPAnswer"));

					if(m_Pending.Ops.PublicIPReq == true)
					{
						m_Pending.Ops.PublicIPReq = false;

						uint32 uIP = _ntohl(Packet.ReadValue<uint32>());
						if(uIP == -1)
							theCore->m_MuleManager->GetServer()->AddAddress(CAddress(Packet.ReadData(16)));
						else if(uIP)
							theCore->m_MuleManager->GetServer()->AddAddress(CAddress(uIP));
					}
					break;
				}

				// not supported
				//case OP_PEERCACHE_QUERY: 
				//case OP_PEERCACHE_ANSWER:
				//case OP_PEERCACHE_ACK:

				// not supported
				//case OP_CHATCAPTCHAREQ:
				//case OP_CHATCAPTCHARES:

				// not supported
				//case OP_REQUESTPREVIEW:
				//case OP_PREVIEWANSWER:

				default:
					LogLine(LOG_DEBUG, tr("Recived unknwon Ed2kMule packet extended Opcode: %1").arg(uOpcode));
			}
			break;
		}

		default:
			LogLine(LOG_DEBUG, tr("Recived unknwon Ed2kMule packet protocol: %1").arg(Prot));
	}

	if(Packet.GetSizeLeft())
		LogLine(LOG_WARNING | LOG_DEBUG, tr("Recived excess data, protocol: %1, opcode: %2, data left: %3").arg(Prot).arg(uOpcode).arg(Packet.GetSizeLeft()));
}

void CMuleClient::SendSignature()
{
	ASSERT(!m_SUIKey.isEmpty());
	if(CPrivateKey* pPrivKey = theCore->m_MuleManager->GetPrivateKey())
	{						
		uint8 byChaIPKind = 0;
		CAddress ChallengeIP;
		if ((m_MiscOptions1.Fields.SupportSecIdent & 0x01) == 0) // use V1 by default and V2 only if its the only supported
		{
			ChallengeIP = theCore->m_MuleManager->GetAddress(m_Mule.Prot);
			if(ChallengeIP.IsNull())
			{
				ChallengeIP = m_Mule.GetIP(); // we dont know our own IP so we use the remote one
				byChaIPKind = CRYPT_CIP_REMOTECLIENT;
			}
			else
				byChaIPKind = CRYPT_CIP_LOCALCLIENT;
		}
	
		CBuffer Challenge;
		Challenge.WriteQData(m_SUIKey);
		Challenge.WriteValue<uint32>(m_uRndChallenge);
		if(byChaIPKind)
		{
			if(ChallengeIP.Type() == CAddress::IPv6)
				Challenge.WriteData(ChallengeIP.Data(), 16);
			else
				Challenge.WriteValue<uint32>(_ntohl(ChallengeIP.ToIPv4()));
			Challenge.WriteValue<uint8>(byChaIPKind);
		}

		CBuffer Signature;
		pPrivKey->Sign(&Challenge, &Signature);
		size_t uSize = Signature.GetSize();

		CBuffer Packet(1 + 1 + uSize);
		Packet.WriteValue<uint8>(OP_SIGNATURE);
		Packet.WriteValue<uint8>(uSize);
		Packet.WriteData(Signature.GetBuffer(), uSize);
		SendPacket(Packet, OP_EMULEPROT);
	}
	m_uRndChallenge = 0;
}

bool CMuleClient::SendUDPPingRequest()
{
	m_Pending.Ops.UDPPingReq = true;

	if(UDPPingVer() == 0)
		return false;

	CAddress IP = m_Mule.GetIP();
	bool bCallBack = false;
	if(IsFirewalled(IP.Type()))
	{
		bCallBack = true;

		if(IP.Type() != CAddress::IPv4)
			return false; // EM-ToDo: add IPv6 callback using new server or NeoKad?

		if(m_Mule.BuddyID.isEmpty())
			return false;
	}

	CBuffer Packet;
	if(bCallBack)
	{
		Packet.WriteValue<uint8>(OP_REASKCALLBACKUDP);
		Packet.WriteQData(m_Mule.BuddyID);
	}
	else
		Packet.WriteValue<uint8>(OP_REASKFILEPING);

	CFile* pFile = m_DownSource->GetFile();
	CFileHash* pHash = pFile->GetHash(HashEd2k);
	ASSERT(pHash);

	Packet.WriteQData(pHash->GetHash());

	if (UDPPingVer() > 3)
		WriteFileStatus(Packet, pFile);
	if (UDPPingVer() > 2)
		Packet.WriteValue<uint16>(pFile->GetStats()->GetAvailStats());

	if(bCallBack)
	{
		SMuleSource Mule;
		Mule.SetIP(m_Mule.BuddyAddress);
		Mule.UDPPort = m_Mule.BuddyPort;
		theCore->m_MuleManager->SendUDPPacket(Packet, OP_EMULEPROT, Mule);
	}
	else
		theCore->m_MuleManager->SendUDPPacket(Packet, OP_EMULEPROT, m_Mule);
	return true;
}

void CMuleClient::ProcessUDPPacket(CBuffer& Packet, uint8 Prot)
{
	uint8 uOpcode = Packet.ReadValue<uint8>();
	switch(Prot)
	{
		case OP_EMULEPROT:
		{
			switch(uOpcode)
			{
				case OP_REASKFILEPING:
				{
					CFileHash Hash(HashEd2k);
					Hash.SetHash(Packet.ReadQData(Hash.GetSize()));
					CMuleSource* pSource = GetSource(&Hash);
					if(!pSource)
					{
						CBuffer Answer(1);
						Answer.WriteValue<uint8>(OP_FILENOTFOUND);
						theCore->m_MuleManager->SendUDPPacket(Answer, OP_EMULEPROT, m_Mule);
						return;
					}

					if (UDPPingVer() > 3)
						ReadFileStatus(Packet, pSource);
					if (UDPPingVer() > 2)
						pSource->SetRemoteAvailability(Packet.ReadValue<uint16>());

					CBuffer Answer;
					if(uint16 uRank = pSource->GetQueueRank(true))
					{
						Answer.WriteValue<uint8>(OP_REASKACK);
						if (UDPPingVer() > 3)
							WriteFileStatus(Answer,pSource->GetFile());
						Answer.WriteValue<uint16>(uRank);
					}
					else
					{
						Packet.WriteValue<uint8>(OP_QUEUEFULL);
					}
					theCore->m_MuleManager->SendUDPPacket(Packet, OP_EMULEPROT, m_Mule);
					break;
				}
				case OP_REASKACK:
				{
					if(!m_DownSource)
						return;
					
					if(m_Pending.Ops.UDPPingReq != true)
						return;
					m_Pending.Ops.UDPPingReq = false;

					if (UDPPingVer() > 3)
						ReadFileStatus(Packet, m_DownSource);

					uint32 QueueRank = Packet.ReadValue<uint16>();
					m_DownSource->SetQueueRank(QueueRank, true);
					break;
				}
				case OP_QUEUEFULL:
				{
					if(!m_DownSource)
					{
						LogLine(LOG_DEBUG, tr("recived packet for a downlaod operaion, without started one"));
						return;
					}
					if(m_Pending.Ops.UDPPingReq != true)
						return;
					m_Pending.Ops.UDPPingReq = false;

					m_DownSource->SetQueueRank(0, true);
					break;
				}
				case OP_FILENOTFOUND:
				{
					if(!m_DownSource)
					{
						LogLine(LOG_DEBUG, tr("recived packet for a downlaod operaion, without started one"));
						return;
					}
					if(m_Pending.Ops.UDPPingReq != true)
						return;
					m_Pending.Ops.UDPPingReq = false;

					LogLine(LOG_DEBUG | LOG_WARNING, tr("Recived a file not found"));
					m_DownSource->GetFile()->RemoveTransfer(m_DownSource);
					break;
				}

				default:
					LogLine(LOG_DEBUG, tr("Recived unknwon Ed2kMule packet extended Opcode"));
			}
			break;
		}

		default:
			LogLine(LOG_DEBUG, tr("Recived unknwon Ed2kMule packet protocol"));
	}

	if(Packet.GetSizeLeft())
		LogLine(LOG_WARNING | LOG_DEBUG, tr("Recived excess udp data, protocol: %1, opcode: %2, data left: %3").arg(Prot).arg(uOpcode).arg(Packet.GetSizeLeft()));
}

bool CMuleClient::IsFirewalled(CAddress::EAF eAF) const 
{
	if(eAF == CAddress::IPv4)
		return m_Mule.HasLowID();
	else if(eAF == CAddress::IPv6)
		return m_Mule.ClosedIPv6();
	return true;
}

bool CMuleClient::Connect(bool bUTP)
{
	if(!m_Mule.SelectIP())
	{
		m_Error = "Unavailable TransportLayer";
		return false;
	}

	m_HolepunchTry = 0;

	if(!bUTP && IsFirewalled(m_Mule.Prot))
	{
		if(SupportsNatTraversal() && theCore->m_MuleManager->SupportsNatTraversal())
		{
			if(MakeRendezvous()) // may fail due to missing udp port or kad buddy
				return true;
		}
		if(theCore->m_MuleManager->IsFirewalled(CAddress::IPv4))
		{
			m_Error = "LowID2LowID with no NAT-T";
			return false;
		}
		return RequestCallback(); // request callback using the kad buddy or direct callback request
	}
	return ConnectSocket(bUTP);
}

bool CMuleClient::ConnectSocket(bool bUTP)
{
	if(m_Socket)
	{
		ASSERT(0);
		return false;
	}

	m_uTimeOut = GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("Ed2kMule/ConnectTimeout"));
	m_bExpectConnection = false;
	m_bHelloRecived = false;

	if(bUTP)
		m_Socket = (CMuleSocket*)theCore->m_MuleManager->GetServer()->AllocUTPSocket();
	else
		m_Socket = (CMuleSocket*)theCore->m_MuleManager->GetServer()->AllocSocket();
	
	m_Socket->AddUpLimit(m_UpLimit);
	if(m_UpSource)
		m_Socket->AddUpLimit(m_UpSource->GetFile()->GetUpLimit());
	m_Socket->AddDownLimit(m_DownLimit);
	if(m_DownSource)
		m_Socket->AddDownLimit(m_DownSource->GetFile()->GetDownLimit());

	connect(m_Socket, SIGNAL(Connected()), this, SLOT(OnConnected()));
	connect(m_Socket, SIGNAL(ReceivedPacket(QByteArray, quint8)), this, SLOT(OnReceivedPacket(QByteArray, quint8)), Qt::QueuedConnection);
	connect(m_Socket, SIGNAL(Disconnected(int)), this, SLOT(OnDisconnected(int)));
	connect(m_Socket, SIGNAL(NextPacketSend()), this, SLOT(OnPacketSend()));

	CAddress IP = m_Mule.GetIP();

	if(!theCore->m_MuleManager->SupportsCryptLayer() || !SupportsCryptLayer() || m_Mule.UserHash.IsEmpty()) // not supported or not available
	{
		if(theCore->m_MuleManager->RequiresCryptLayer() || RequiresCryptLayer())
		{
			m_Error = "Crypto Not Available";
			return false;
		}
	}
	else if(theCore->m_MuleManager->RequestsCryptLayer() || RequestsCryptLayer())
	{
		if(bUTP)
		{
			CMuleServer* pServer = (CMuleServer*)m_Socket->GetServer();
			pServer->SetupCrypto(IP, m_Mule.UDPPort, m_Mule.UserHash.ToArray(true));
		}
		else
			m_Socket->InitCrypto(m_Mule.UserHash.ToArray(true));
	}

	m_Socket->ConnectToHost(IP, m_Mule.GetPort(bUTP));
	return true;
}

void CMuleClient::OnConnected()
{
	LogLine(LOG_DEBUG, tr("connected socket (outgoing)"));
	// Check inneded if doing this in punch callback mode
	//if(m_HolepunchTry < 2) // on bidirectional punch the remote side sends the hello, its kinf of like a callback
	SendHelloPacket();
}

void CMuleClient::ExpectConnection()
{
	m_uTimeOut = GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("Ed2kMule/ConnectTimeout"));
	m_bExpectConnection = true;
}

bool CMuleClient::MakeRendezvous()
{
	if(!m_Mule.UDPPort && !m_Mule.KadPort)
		return false;

	if(!m_Mule.SelectIP())
	{
		m_Error = "Unavailable TransportLayer";
		return false;
	}

	if(DirectUDPCallback() && m_HolepunchTry == 0)
	{
		m_HolepunchTry = 1; // first try only UTPconnection without callback
		ConnectSocket(true);
	}
	else
	{
		// Note: this is like a callback request, we expect the remote side to open a connection to us, 
		//			to facilitate that we send a OP_HOLEPUNCH and wait for an incomming connection

		CEd2kServer* pServer = theCore->m_MuleManager->GetServerList()->FindServer(m_Mule.ServerAddress, m_Mule.Prot);
		if(pServer && pServer->GetClient() && pServer->SupportsNatTraversal()) 
		{
			pServer->GetClient()->RequestNatCallback(m_Mule.ClientID);

			// this callbacks are slow
			m_uTimeOut = GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("Ed2kMule/ConnectTimeout")) * 3;
			m_bExpectConnection = true;
		}
		else if(!m_Mule.BuddyAddress.IsNull() && m_Mule.BuddyPort) // kad Callback
		{
			CBuffer Packet;
			Packet.WriteValue<uint8>(OP_REASKCALLBACKUDP);
			Packet.WriteQData(m_Mule.BuddyID);
			Packet.WriteQData(QByteArray(16, '\0'));

			Packet.WriteValue<uint8>(OP_RENDEZVOUS);
			Packet.WriteData(theCore->m_MuleManager->GetUserHash().Data, 16);
			Packet.WriteValue<uint8>(theCore->m_MuleManager->GetMyInfo()->MuleConOpts.Bits);

			SMuleSource Mule;
			Mule.SetIP(m_Mule.BuddyAddress);
			Mule.KadPort = m_Mule.BuddyPort;
			theCore->m_MuleManager->SendUDPPacket(Packet, OP_EMULEPROT, Mule);

			ExpectConnection();
		}
		else
		{
			m_Error = "No NAT Callback Available";
			return false;
		}

		m_HolepunchTry = 2; // bidirectional punch
		//ConnectSocket(true);

		CBuffer Dummy;
		Dummy.WriteValue<uint8>(OP_HOLEPUNCH);
		theCore->m_MuleManager->SendUDPPacket(Dummy, OP_EMULEPROT, m_Mule);
	}
	return true;
}

void CMuleClient::Disconnect(const QString& Error)
{
	m_uTimeOut = -1;

	if(!Error.isEmpty()) 
		m_Error = Error; 

	if(m_UpSource && m_UpSource->IsActiveUpload())
	{
		m_UpSource->LogLine(LOG_DEBUG | LOG_INFO, tr("Stopping upload of %1 to %2 after %3 kb disconnecting due to timeout")
			.arg(m_UpSource->GetFile()->GetFileName()).arg(m_UpSource->GetDisplayUrl()).arg((double)m_SentPartSize/1024.0));
		m_UpSource->StopUpload(true);
	}

	if(m_Socket) 
		m_Socket->DisconnectFromHost();

	if(m_bExpectConnection)
	{
		m_bExpectConnection = false;

		if(m_Error.isEmpty() && m_bHelloRecived == false)
			m_Error = "Not Calledback";
		
		emit SocketClosed();
	}
}

void CMuleClient::OnDisconnected(int Error)
{
	//ASSERT(m_Socket); // X-ToDo-Now: FixMe

	if(m_UpSource && m_UpSource->IsActiveUpload())
	{
		m_UpSource->LogLine(LOG_DEBUG | LOG_INFO, tr("Stopping upload of %1 to %2 after %3 kb socket s, reason: %4")
			.arg(m_UpSource->GetFile()->GetFileName()).arg(m_UpSource->GetDisplayUrl()).arg((double)m_SentPartSize/1024.0).arg(CStreamSocket::GetErrorStr(Error)));
		m_UpSource->StopUpload(true);
	}

	// clear block queues
	ClearUpload();
	ClearDownload();

	bool bConnectionFailed = false;
	if(m_Socket)
	{
		CStreamSocket::EState State = m_Socket->GetState();
		bConnectionFailed = (State == CStreamSocket::eConnectFailed);

		m_Socket->RemoveUpLimit(m_UpLimit);
		if(m_UpSource)
			m_Socket->RemoveUpLimit(m_UpSource->GetFile()->GetUpLimit());
		m_Socket->RemoveDownLimit(m_DownLimit);
		if(m_DownSource)
			m_Socket->RemoveDownLimit(m_DownSource->GetFile()->GetDownLimit());

		//m_Socket->disconnect(this);
		disconnect(m_Socket, 0, 0, 0);
		theCore->m_MuleManager->GetServer()->FreeSocket(m_Socket);
		m_Socket = NULL;
		m_uTimeOut = -1;
	}

	if(m_Error.isEmpty() && m_bHelloRecived == false)
	{
		if(m_Mule.Prot == CAddress::IPv6)
		{
			// try IPv4
			m_Mule.IPv6 = CAddress(CAddress::IPv6);
			m_Mule.Prot = CAddress::IPv4;
			return;
		}
		else if(SupportsNatTraversal() && m_HolepunchTry < 2 && theCore->m_MuleManager->SupportsNatTraversal())
		{
			// try hole punch
			if(MakeRendezvous())
				return;
		}
		else
			m_Error = "Not Connected";
	}

	if(bConnectionFailed && !m_bHelloRecived)
	{
		LogLine(LOG_DEBUG, tr("connection to ed2k cleint failed - dead"));
		theCore->m_PeerWatch->PeerFailed(m_Mule.GetIP(), m_Mule.TCPPort);
	}

	emit SocketClosed();
}

void CMuleClient::ClearUpload()
{
	while(!m_OutgoingBlocks.isEmpty())
		delete m_OutgoingBlocks.takeFirst();

	if(m_Socket) // clear all queues payload packets
		m_Socket->ClearQueue();
}

void CMuleClient::ClearDownload()
{
	while(!m_IncomingBlocks.isEmpty())
	{
		SRequestedBlock* pBlock = m_IncomingBlocks.takeFirst();
		if(pBlock->zStream)
			clear_z(pBlock->zStream);
		delete pBlock;
	}
}

void CMuleClient::SendPacket(CBuffer& Packet, uint8 Prot)
{
	ASSERT(m_Socket);
	if(!m_Socket)
		return;

	if(Prot == OP_PACKEDPROT)
		Prot = PackPacket(Packet) ? OP_PACKEDPROT : OP_EMULEPROT;

	m_Socket->SendPacket(Packet.ToByteArray(), Prot);
}

void CMuleClient::SwapSocket(CMuleClient* pClient)
{
	//if(m_Socket)
	//{
	//	m_Socket->RemoveUpLimit(m_UpLimit);
	//	if(m_UpSource)
	//		m_Socket->RemoveUpLimit(m_UpSource->GetFile()->GetUpLimit());
	//	m_Socket->RemoveDownLimit(m_DownLimit);
	//	if(m_DownSource)
	//		m_Socket->RemoveDownLimit(m_DownSource->GetFile()->GetDownLimit());
	//}

	ASSERT(pClient->m_Socket);

	if(m_Socket)
	{
		//m_Socket->disconnect(this);
		disconnect(m_Socket, 0, 0, 0);
		theCore->m_MuleManager->GetServer()->FreeSocket(m_Socket);
		m_Socket = NULL;
	}

	pClient->disconnect(pClient->m_Socket, 0, 0, 0);
	m_Socket = pClient->m_Socket;
	pClient->m_Socket = NULL;

	connect(m_Socket, SIGNAL(ReceivedPacket(QByteArray, quint8)), this, SLOT(OnReceivedPacket(QByteArray, quint8)), Qt::QueuedConnection);
	connect(m_Socket, SIGNAL(Disconnected(int)), this, SLOT(OnDisconnected(int)));
	connect(m_Socket, SIGNAL(NextPacketSend()), this, SLOT(OnPacketSend()));

	m_Socket->AddUpLimit(m_UpLimit);
	if(m_UpSource)
		m_Socket->AddUpLimit(m_UpSource->GetFile()->GetUpLimit());
	m_Socket->AddDownLimit(m_DownLimit);
	if(m_DownSource)
		m_Socket->AddDownLimit(m_DownSource->GetFile()->GetDownLimit());

	if(m_Mule.UserHash.IsEmpty())
		m_Mule.UserHash = pClient->m_Mule.UserHash;
	else if(m_Mule.UserHash != pClient->m_Mule.UserHash)
	{
		LogLine(LOG_WARNING, tr("Ed2kMule client changed ist hash!"));
		m_Mule.UserHash = pClient->m_Mule.UserHash;
	}

	// Get the infos from the hello header
	m_Mule.ClientID = pClient->m_Mule.ClientID;
	m_Mule.SetIP(pClient->m_Mule);
	m_Mule.TCPPort = pClient->m_Mule.TCPPort;

	m_bHelloRecived = true;

	m_uTimeOut = GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("Ed2kMule/IdleTimeout"));
	m_bExpectConnection = false;

	emit HelloRecived();
}

void CMuleClient::OnReceivedPacket(QByteArray Packet, quint8 Prot)
{
	if(m_uTimeOut != -1) // for buddys timeout gets disabled
		m_uTimeOut = GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("Ed2kMule/IdleTimeout"));

	try
	{
		CBuffer Buffer(Packet, true);
		if(Prot == OP_PACKEDPROT)
		{
			UnpackPacket(Buffer);
			Prot = OP_EMULEPROT;
		}
		ProcessPacket(Buffer, Prot);
	}
	catch(const CException& Exception)
	{
		LogLine(Exception.GetFlag(), tr("recived malformated packet; %1").arg(QString::fromStdWString(Exception.GetLine())));
	}
}

void CMuleClient::AddLogLine(time_t uStamp, uint32 uFlag, const CLogMsg& Line)
{
	Line.AddMark((uint64)this);

	int iDebug = theCore->Cfg()->GetInt("Log/Level");
	if(iDebug >= 2 || (iDebug == 1 && (uFlag & LOG_DEBUG) == 0))
	{
		Line.Prefix(QString("MuleClient - %1:%2").arg(m_Mule.GetIP().ToQString()).arg(m_Mule.TCPPort));
		QObjectEx::AddLogLine(uStamp, uFlag | LOG_MOD('m'), Line);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// ID code

QString CMuleClient::IdentifySoftware(bool bIsML)
{
	enum ECompatible
	{
		SO_EMULE			= 0,
		SO_CDONKEY			= 1,
		SO_XMULE			= 2,
		SO_AMULE			= 3,
		SO_SHAREAZA			= 4,
		SO_EMULEPLUS		= 5,
		SO_HYDRANODE		= 6,
		SO_MLDONKEY			= 10,
		SO_LPHANT			= 20,
		SO_SHAREAZA2		= 28,
		SO_TRUSTYFILES		= 30,
		SO_SHAREAZA3		= 40,		
		SO_EDONKEYHYBRID	= 50,
		SO_MLDONKEY2		= 52,
		SO_SHAREAZA4		= 68,
		SO_NEOLOADER		= 78,	// 0-255 (N == 78)
		SO_MLDONKEY3		= 152,
	};

	if (m_MuleProtocol || (m_Mule.UserHash.Data[5] == 14 && m_Mule.UserHash.Data[14] == 111))
	{
		QString Software;
		switch(m_MuleVersion.Fields.Compatible)
		{
			case SO_NEOLOADER:	Software = "NeoLoader";		break;
			case SO_CDONKEY:	Software = "cDonkey";		break;
			case SO_XMULE:		Software = "xMule";			break;
			case SO_AMULE:		Software = "aMule";			break;
			case SO_SHAREAZA:
			case SO_SHAREAZA2:
			case SO_SHAREAZA3:	
			case SO_SHAREAZA4:	Software = "Shareaza";		break;
			case SO_LPHANT:		Software = "lphant";		break;
			case SO_EMULEPLUS:	Software = "eMulePlus";		break;
			case SO_HYDRANODE:	Software = "Hydranode";		break;
			case SO_TRUSTYFILES:Software = "TrustyFiles";	break;
			case SO_MLDONKEY:
			case SO_MLDONKEY3:	Software = "MLdonkey";		break;
			default:
				if (bIsML)
				{
					m_MuleVersion.Fields.Compatible = SO_MLDONKEY;
					Software = "MLdonkey";
				}
				if (m_MuleVersion.Fields.Compatible != 0)
					Software = "eMule Compat";
				else
					Software = "eMule";
		}

		if(m_MuleVersion.Fields.Compatible == SO_NEOLOADER)
		{
			Software += " v" + QString::number(m_MuleVersion.Fields.VersionMjr) + "." + QString::number(m_MuleVersion.Fields.VersionMin).rightJustified(2, '0');
			if(m_MuleVersion.Fields.VersionUpd)
				Software.append('a' + m_MuleVersion.Fields.VersionUpd - 1);
		}
		else if (m_MuleVersion.Fields.Compatible == SO_EMULE)
			Software += QString(" v%1.%2%3").arg(m_MuleVersion.Fields.VersionMjr).arg(m_MuleVersion.Fields.VersionMin).arg(QString(_T('a') + m_MuleVersion.Fields.VersionUpd));
		else if (m_MuleVersion.Fields.Compatible == SO_AMULE || m_MuleVersion.Fields.VersionUpd != 0)
			Software += QString(" v%1.%2.%3").arg(m_MuleVersion.Fields.VersionMjr).arg(m_MuleVersion.Fields.VersionMin).arg(m_MuleVersion.Fields.VersionUpd);
		else if (m_MuleVersion.Fields.Compatible == SO_LPHANT)
			Software += " v" + QString::number(m_MuleVersion.Fields.VersionMjr-1) + "." + QString::number(m_MuleVersion.Fields.VersionMin).rightJustified(2, '0');
		else
			Software += QString(" v%1.%2").arg(m_MuleVersion.Fields.VersionMjr).arg(m_MuleVersion.Fields.VersionMin);
		return Software;
	}

	if (m_ProtocolRevision != 0)
	{
		m_MuleVersion.Fields.Compatible = SO_EDONKEYHYBRID;

		if (m_Ed2kVersion > 100000){
			UINT uMaj = m_Ed2kVersion/100000;
			m_MuleVersion.Fields.VersionMjr = uMaj - 1;
			m_MuleVersion.Fields.VersionMin = (m_Ed2kVersion - uMaj*100000) / 100;
			m_MuleVersion.Fields.VersionUpd = m_Ed2kVersion % 100;
		}
		else if (m_Ed2kVersion >= 10100 && m_Ed2kVersion <= 10309){
			UINT uMaj = m_Ed2kVersion/10000;
			m_MuleVersion.Fields.VersionMjr = uMaj;
			m_MuleVersion.Fields.VersionMin = (m_Ed2kVersion - uMaj*10000) / 100;
			m_MuleVersion.Fields.VersionUpd = m_Ed2kVersion % 10;
		}
		else if (m_Ed2kVersion > 10000){
			UINT uMaj = m_Ed2kVersion/10000;
			m_MuleVersion.Fields.VersionMjr = uMaj - 1;
			m_MuleVersion.Fields.VersionMin = (m_Ed2kVersion - uMaj*10000) / 10;
			m_MuleVersion.Fields.VersionUpd = m_Ed2kVersion % 10;
		}
		else if (m_Ed2kVersion >= 1000 && m_Ed2kVersion < 1020){
			UINT uMaj = m_Ed2kVersion/1000;
			m_MuleVersion.Fields.VersionMjr = uMaj;
			m_MuleVersion.Fields.VersionMin = (m_Ed2kVersion - uMaj*1000) / 10;
			m_MuleVersion.Fields.VersionUpd = m_Ed2kVersion % 10;
		}
		else if (m_Ed2kVersion > 1000){
			UINT uMaj = m_Ed2kVersion/1000;
			m_MuleVersion.Fields.VersionMjr = uMaj - 1;
			m_MuleVersion.Fields.VersionMin = m_Ed2kVersion - uMaj*1000;
			m_MuleVersion.Fields.VersionUpd = 0;
		}
		else if (m_Ed2kVersion > 100){
			UINT uMin = m_Ed2kVersion/10;
			m_MuleVersion.Fields.VersionMjr = 0;
			m_MuleVersion.Fields.VersionMin = uMin;
			m_MuleVersion.Fields.VersionUpd = m_Ed2kVersion - uMin*10;
		}
		else{
			m_MuleVersion.Fields.VersionMjr = 0;
			m_MuleVersion.Fields.VersionMin = m_Ed2kVersion;
			m_MuleVersion.Fields.VersionUpd = 0;
		}
		return QString("eDonkeyHybrid v%1.%2.%3").arg(m_MuleVersion.Fields.VersionMjr).arg(m_MuleVersion.Fields.VersionMin).arg(m_MuleVersion.Fields.VersionUpd);
	}

	m_MuleVersion.Fields.VersionMin = m_MuleVersion.Bits & 0x00ffffff;
	if (bIsML || (m_Mule.UserHash.Data[5] == 'M' && m_Mule.UserHash.Data[14] == 'L'))
		return QString("MLdonkey v0.%u").arg(m_Ed2kVersion);
	else if (m_Mule.UserHash.Data[5] == 13 && m_Mule.UserHash.Data[14] == 110)
		return QString("Old eMule v0.%u").arg(m_Ed2kVersion);
	else
		return QString("eDonkey v0.%u").arg(m_Ed2kVersion);
}
