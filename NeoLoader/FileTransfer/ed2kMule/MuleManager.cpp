#include "GlobalHeader.h"
#include "MuleManager.h"
#include "MuleKad.h"
#include "MuleServer.h"
#include "../../FileList/FileManager.h"
#include "../../FileList/Hashing/FileHashTree.h"
#include "../../FileList/Hashing/FileHashSet.h"
#include "../../NeoCore.h"
#include "../../NeoVersion.h"
#include "../Framework/Cryptography/AbstractKey.h"
#include "../Framework/Exception.h"
#include "../PeerWatch.h"
#include "../../Networking/SocketThread.h"
#include "../../Networking/BandwidthControl/BandwidthLimit.h"
#include "../../Interface/InterfaceManager.h"
#include "../../FileList/FileStats.h"
#include "../../../MiniUPnP/MiniUPnP.h"
#include "../FileGrabber.h"
#include "MuleCollection.h"
#include "../../FileList/Hashing/HashingJobs.h"
#include "../../FileList/Hashing/HashingThread.h"
#include "./ServerClient/ServerList.h"
#include "../../Common/KeygenRSA.h"

CMuleManager::CMuleManager(QObject* qObject)
: QObjectEx(qObject)
{
	QString HashMode = theCore->Cfg()->GetString("Ed2kMule/HashMode");
	bool bRandom = HashMode.contains("Random", Qt::CaseInsensitive);
	bool bSecure = HashMode.contains("Secure", Qt::CaseInsensitive);

	if(QDateTime::currentDateTime().toTime_t() - theCore->Cfg()->GetUInt64("Ed2kMule/HashAge") < HR2S(6))
		bRandom = false; // do not change hash to often
	
	if(bSecure)
	{
		m_PrivateKey = new CPrivateKey();
		QByteArray Key = theCore->Cfg()->GetBlob("Ed2kMule/SUIKey");
		if(bRandom || Key.isEmpty() || !m_PrivateKey->SetKey(Key))
		{
			m_PrivateKey->SetAlgorithm(CPrivateKey::eRSA);
			byte PrivKeyArr[1024]; // Thats a shit, the FIPS compliance test fails on 384 short keys, we have to make a key manually
			size_t uSize = MakeKeyRSA(PrivKeyArr, ARRSIZE(PrivKeyArr), 384); 
			m_PrivateKey->SetKey(PrivKeyArr, uSize);
			theCore->Cfg()->SetBlob("Ed2kMule/SUIKey", m_PrivateKey->ToByteArray());
		}
		m_PublicKey = m_PrivateKey->PublicKey();

		m_UserHash.Empty = false;
		CAbstractKey::Fold(m_PublicKey->GetKey(), m_PublicKey->GetSize(), m_UserHash.Data, 16);
		// set emule marker bytes (shareaza does this to)
		m_UserHash.Data[5] = 14;
		m_UserHash.Data[14] = 111;
	}
	else
	{
		m_UserHash = theCore->Cfg()->GetBlob("Ed2kMule/UserHash");
		if(bRandom || m_UserHash.IsEmpty())
		{
			m_UserHash.Set(CAbstractKey(16,true).GetKey());
			// set emule marker bytes (shareaza does this to)
			m_UserHash.Data[5] = 14;
			m_UserHash.Data[14] = 111;
			theCore->Cfg()->SetBlob("Ed2kMule/UserHash",m_UserHash.ToArray());
		}
	}

	// setup listner
	m_Server = new CMuleServer(m_UserHash.ToArray(true));
	connect(m_Server, SIGNAL(Connection(CStreamSocket*)), this, SLOT(OnConnection(CStreamSocket*)));

	connect(m_Server, SIGNAL(ProcessUDPPacket(QByteArray, quint8, CAddress, quint16)), this, SLOT(ProcessUDPPacket(QByteArray, quint8, CAddress, quint16)));
	connect(this, SIGNAL(SendUDPPacket(QByteArray, quint8, CAddress, quint16, QByteArray)), m_Server, SLOT(SendUDPPacket(QByteArray, quint8, CAddress, quint16, QByteArray)));

	theCore->m_Network->AddServer(m_Server);

	m_Server->SetPorts(theCore->Cfg()->GetInt("Ed2kMule/TCPPort"), theCore->Cfg()->GetInt("Ed2kMule/UDPPort"));

	m_Kademlia = new CMuleKad(m_Server, this);

	m_ServerList = new CServerList(m_Server, this);

	m_NextIPRequest = GetCurTick();

	m_LastCallbacksMustWait = 0;

	m_TransferStats.UploadedTotal = theCore->Stats()->value("Ed2kMule/Uploaded").toULongLong();
	m_TransferStats.DownloadedTotal = theCore->Stats()->value("Ed2kMule/Downloaded").toULongLong();

	// Fill My Info
	ASSERT(NEO_VERSION_MJR < 128);
	ASSERT(NEO_VERSION_MIN < 128);
	m_MyInfo.MuleVersion.Fields.VersionBld = 0; // 0-127 (emule does not use it)
//#if NEO_VERSION_UPD <= 7
//	m_MyInfo.MuleVersion.Fields.VersionUpd = NEO_VERSION_UPD; // 0-7
//#else
	m_MyInfo.MuleVersion.Fields.VersionUpd = 0;
//#endif
	m_MyInfo.MuleVersion.Fields.VersionMin = NEO_VERSION_MIN; // 0-127
	m_MyInfo.MuleVersion.Fields.VersionMjr = NEO_VERSION_MJR; // 0-127
	//m_MyInfo.MuleVersion.Fields.Compatible = 'n'; // 0-255 (n == 110)
	m_MyInfo.MuleVersion.Fields.Compatible = 'N'; // 0-255 (N == 78)
	
	m_Version = GetNeoVersion();

	m_MyInfo.MiscOptions1.Fields.SupportsPreview		= 0; // not supported
	m_MyInfo.MiscOptions1.Fields.MultiPacket			= 1;
	m_MyInfo.MiscOptions1.Fields.NoViewSharedFiles		= 1;
	m_MyInfo.MiscOptions1.Fields.PeerCache				= 0; // not supported
	m_MyInfo.MiscOptions1.Fields.AcceptCommentVer		= 1;
	m_MyInfo.MiscOptions1.Fields.ExtendedRequestsVer	= 2;
	m_MyInfo.MiscOptions1.Fields.SourceExchange1Ver		= 0; // deprecated
	m_MyInfo.MiscOptions1.Fields.SupportSecIdent		= m_PrivateKey ? 3 : 0; // SUI
	m_MyInfo.MiscOptions1.Fields.DataCompVer			= 1;
	m_MyInfo.MiscOptions1.Fields.UDPPingVer				= 4;
	m_MyInfo.MiscOptions1.Fields.UnicodeSupport			= 1;
	m_MyInfo.MiscOptions1.Fields.SupportsAICH			= 1;

	m_MyInfo.MiscOptions2.Fields.KadVersion				= KADEMLIA_VERSION;
	m_MyInfo.MiscOptions2.Fields.SupportsLargeFiles		= 1;
	m_MyInfo.MiscOptions2.Fields.ExtMultiPacket			= 1;
	m_MyInfo.MiscOptions2.Fields.ModBit					= 0; // we ain't a mod
	//m_MyInfo.MiscOptions2.Fields.SupportsCryptLayer		= 
	//m_MyInfo.MiscOptions2.Fields.RequestsCryptLayer		= 
	//m_MyInfo.MiscOptions2.Fields.RequiresCryptLayer		= 
	m_MyInfo.MiscOptions2.Fields.SupportsSourceEx2		= 1;
	m_MyInfo.MiscOptions2.Fields.SupportsCaptcha		= 0; // not supported
	//m_MyInfo.MiscOptions2.Fields.DirectUDPCallback		= 
	m_MyInfo.MiscOptions2.Fields.SupportsFileIdent		= 1;
	m_MyInfo.MiscOptions2.Fields.Reserved = 0;

	m_MyInfo.MiscOptionsN.Fields.ExtendedSourceEx		= 1;
	//m_MyInfo.MiscOptionsN.Fields.SupportsNatTraversal	= 
	m_MyInfo.MiscOptionsN.Fields.SupportsIPv6			= 1;
	m_MyInfo.MiscOptionsN.Fields.ExtendedComments		= 1;
	m_MyInfo.MiscOptionsN.Fields.Reserved = 0;

	m_MyInfo.MiscOptionsNL.Fields.HostCache				= 1;
	m_MyInfo.MiscOptionsNL.Fields.Reserved	= 0;


	//m_MyInfo.MuleConOpts.Fields.SupportsCryptLayer		= m_MyInfo.MiscOptions2.Fields.SupportsCryptLayer;
	//m_MyInfo.MuleConOpts.Fields.RequestsCryptLayer		= m_MyInfo.MiscOptions2.Fields.RequestsCryptLayer;
	//m_MyInfo.MuleConOpts.Fields.RequiresCryptLayer		= m_MyInfo.MiscOptions2.Fields.RequiresCryptLayer;
	m_MyInfo.MuleConOpts.Fields.DirectUDPCallback		= 0;
	m_MyInfo.MuleConOpts.Fields.Reserved				= 0;
	//m_MyInfo.MuleConOpts.Fields.SupportsNatTraversal	= m_MyInfo.MiscOptionsN.Fields.SupportsNatTraversal;

	UpdateCache();

	m_bEnabled = false;
}

CMuleManager::~CMuleManager()
{
	foreach(CMuleClient* pClient, m_Clients)
		pClient->Disconnect();
}

void CMuleManager::UpdateCache()
{
	// update ym connection info
	m_MyInfo.MiscOptions2.Fields.SupportsCryptLayer		= theCore->Cfg()->GetString("Ed2kMule/Obfuscation") != "Disable";
	m_MyInfo.MiscOptions2.Fields.RequestsCryptLayer		= theCore->Cfg()->GetString("Ed2kMule/Obfuscation") == "Request" || theCore->Cfg()->GetString("Ed2kMule/Obfuscation") == "Require";
	m_MyInfo.MiscOptions2.Fields.RequiresCryptLayer		= theCore->Cfg()->GetString("Ed2kMule/Obfuscation") == "Require";
	m_MyInfo.MiscOptions2.Fields.DirectUDPCallback		= IsFirewalled(CAddress::IPv4) && m_Kademlia->IsUDPOpen(); // if our TCP port is firewalled but out UDP port is not we support direct callbacks

	m_MyInfo.MiscOptionsN.Fields.SupportsNatTraversal	= theCore->Cfg()->GetBool("ed2kMule/NatTraversal");

	m_MyInfo.MuleConOpts.Fields.SupportsCryptLayer		= m_MyInfo.MiscOptions2.Fields.SupportsCryptLayer;
	m_MyInfo.MuleConOpts.Fields.RequestsCryptLayer		= m_MyInfo.MiscOptions2.Fields.RequestsCryptLayer;
	m_MyInfo.MuleConOpts.Fields.RequiresCryptLayer		= m_MyInfo.MiscOptions2.Fields.RequiresCryptLayer;
	m_MyInfo.MuleConOpts.Fields.DirectUDPCallback		= 0; // EM-ToDo
	m_MyInfo.MuleConOpts.Fields.SupportsNatTraversal	= m_MyInfo.MiscOptionsN.Fields.SupportsNatTraversal;

	// Note: changing crypto support features on runtime is not recomended
}

void CMuleManager::Process(UINT Tick)
{
	foreach(CMuleClient* pClient, m_Clients)
		pClient->Process(Tick);

	if(m_Kademlia->IsEnabled())
		m_Kademlia->Process(Tick);

	if((Tick & EPerSec) == 0)
		return;

	if(m_bEnabled != theCore->Cfg()->GetBool("Ed2kMule/Enable"))
	{
		m_bEnabled = theCore->Cfg()->GetBool("Ed2kMule/Enable");
		if(m_bEnabled)
			theCore->m_Interfaces->EstablishInterface("MuleKad", m_Kademlia);
		else
		{
			theCore->m_Interfaces->TerminateInterface("MuleKad");

			foreach(CFile* pFile, CFileList::GetAllFiles())
			{
				if(pFile->IsMultiFile())
					continue;

				foreach (CTransfer* pTransfer, pFile->GetTransfers()) 
				{
					if(CMuleSource* pSource = qobject_cast<CMuleSource*>(pTransfer))
						pFile->RemoveTransfer(pSource);
				}
			}
		}
	}

	if(!m_bEnabled)
		return;

	if(!m_Clients.isEmpty()) // note when we used the current hash for the last time
		theCore->Cfg()->SetSetting("Ed2kMule/HashAge", QDateTime::currentDateTime().toTime_t());

	UpdateCache();
	int iPriority = theCore->Cfg()->GetInt("Ed2kMule/Priority");
	m_Server->GetUpLimit()->SetLimit(theCore->Cfg()->GetInt("Ed2kMule/Upload"));
	//m_Server->GetUpLimit()->SetPriority(iPriority);
	m_Server->GetDownLimit()->SetLimit(theCore->Cfg()->GetInt("Ed2kMule/Download"));
	m_Server->GetDownLimit()->SetPriority(iPriority);

	if ((Tick & E100PerSec) == 0)
	{
		theCore->Stats()->setValue("Ed2kMule/Uploaded", m_TransferStats.UploadedTotal);
		theCore->Stats()->setValue("Ed2kMule/Downloaded", m_TransferStats.DownloadedTotal);
	}

	if(m_Server->GetPort() != theCore->Cfg()->GetInt("Ed2kMule/TCPPort") || m_Server->GetUTPPort() != theCore->Cfg()->GetInt("Ed2kMule/UDPPort")
		|| m_Server->GetIPv4() != theCore->m_Network->GetIPv4() || m_Server->GetIPv6() != theCore->m_Network->GetIPv6())
	{
		m_Server->SetIPs(theCore->m_Network->GetIPv4(), theCore->m_Network->GetIPv6());
		m_Server->SetPorts(theCore->Cfg()->GetInt("Ed2kMule/TCPPort"), theCore->Cfg()->GetInt("Ed2kMule/UDPPort"));

		m_Server->UpdateSockets();
		m_Kademlia->StopKad();
		m_Kademlia->StartKad();

		if(theCore->Cfg()->GetBool("Bandwidth/UseUPnP"))
		{
			int TCPPort = 0;
			if(theCore->m_MiniUPnP->GetStaus("MuleTCP", &TCPPort) == -1 || TCPPort != m_Server->GetPort())
				theCore->m_MiniUPnP->StartForwarding("MuleTCP", m_Server->GetPort(), "TCP");

			int UDPPort = 0;
			if(theCore->m_MiniUPnP->GetStaus("MuleUDP", &UDPPort) == -1 || UDPPort != m_Server->GetUTPPort())
				theCore->m_MiniUPnP->StartForwarding("MuleUDP", m_Server->GetUTPPort(), "UDP");
		}
		else
		{
			if(theCore->m_MiniUPnP->GetStaus("MuleTCP") != -1)
				theCore->m_MiniUPnP->StopForwarding("MuleTCP");

			if(theCore->m_MiniUPnP->GetStaus("MuleUDP") != -1)
				theCore->m_MiniUPnP->StopForwarding("MuleUDP");
		}
	}

	ManageConnections();

	foreach(uint64 FileID, m_SXTiming.keys())
	{
		SSXTiming& SXTiming = m_SXTiming[FileID];
		while(!SXTiming.Pending.isEmpty() && SXTiming.Pending.first() < GetCurTick())
			SXTiming.Pending.removeFirst();
		if(SXTiming.NextReset < GetCurTick())
		{
			SXTiming.NextReset += SEC2MS(theCore->Cfg()->GetInt("Ed2kMule/SXInterval"));
			SXTiming.SXCount--;
			if(SXTiming.SXCount <= 0)
				m_SXTiming.remove(FileID);
		}
	}

	m_ServerList->Process(Tick);
}

void CMuleManager::ManageConnections()
{
	int DefaultInterval = SEC2MS(theCore->Cfg()->GetInt("Ed2kMule/ReaskInterval"));
	
	int MaxSources = theCore->Cfg()->GetInt("Ed2kMule/MaxSources");

	foreach(CMuleClient* pClient, m_Clients)
	{
		CMuleSource* pCurSource = pClient->GetDownSource();
		CFile* pCurFile = pCurSource ? pCurSource->GetFile() : NULL; // if there is no file we must switch, and if it is complete we must as well

		// Drop Handling
		if (pCurFile && pCurSource->IsChecked() && !pCurSource->IsActiveDownload()
		 && pCurFile->GetStats()->GetTransferCount(eEd2kMule) >= (MaxSources * 10 / 8))
		{
			if (!pCurSource->IsInteresting() // NNP
			 || !pCurSource->IsWaitingDownload()) // FullQ
				pCurFile->RemoveTransfer(pClient->GetDownSource()); // drop source to make place for new once
		}

		// A4AF Handling
		if(!pCurFile || !pCurFile->IsIncomplete () || (pClient->GetDownSource()->IsChecked() && !pClient->GetDownSource()->IsInteresting())) // we could switch, the source is NNP
		{
			QMap<int, CMuleSource*> Sources; // this is ordered smalest first
			foreach(CMuleSource* pTransfer, pClient->GetAllSources())
			{
				CFile* pFile = pTransfer->GetFile();
				if(!pFile->IsStarted() || !pFile->IsEd2kShared())
					continue;
				if(pFile->IsIncomplete() && pFile != pCurFile) // dont switch to it self
				{
					CMuleSource* pSource = qobject_cast<CMuleSource*>(pTransfer);
					if(pSource->GetNextRequest() < GetCurTick() || !pCurFile) // make sure we will not ask to oftem
						Sources.insert(pSource->GetFile()->GetQueuePos(), pSource);
				}
			}
			if(!Sources.isEmpty())
				Sources.values().first()->BeginDownload(); // This resets checked
		}

		CMuleSource* pDownSource = pClient->GetDownSource();
		if(pDownSource && pDownSource->GetFile()->IsIncomplete())
		{
			if(pDownSource->GetNextRequest() < GetCurTick() && (!pDownSource->GetFile()->IsPaused() || !pDownSource->IsChecked()))
			{
				if(pClient->IsConnected())
					pDownSource->ReaskForDownload();
				else if(pClient->IsDisconnected() && theCore->m_Network->CanConnect())
				{
					if(pClient->Connect()) // this returns false not only on error but also if callback is not available
						theCore->m_Network->CountConnect();
					// drop source that couldnt be called in a reasonable tiem span
					else if(!pDownSource->IsChecked() && (GetCurTick() - pDownSource->GetFirstSeen() > DefaultInterval))
						pDownSource->SetError("Expired");
				}
			}
			else if(pDownSource->IsChecked() && pDownSource->GetNextRequest() - MIN2MS(2) < GetCurTick() && pClient->WasConnected() && !pClient->IsUDPPingRequestPending())
				pClient->SendUDPPingRequest();
		}

		// if the client is needer used for uplaod not for downlaod anymore, remove it as we wont connect to it anymore anyways
		if(pClient->GetUpSource() == NULL && pDownSource == NULL && pClient->IsDisconnected())
		{
			m_Kademlia->RemoveClient(pClient);

			foreach(CMuleSource* pSource, pClient->GetAllSources())
				pSource->GetFile()->RemoveTransfer(pSource);

			RemoveClient(pClient);
			delete pClient;
		}
	}
}

void CMuleManager::OnConnection(CStreamSocket* pSocket)
{
	if(!m_bEnabled 
	 || (pSocket->GetAddress().Type() == CAddress::IPv6 && GetIPv6Support() == 0) 
	 || !theCore->m_PeerWatch->CheckPeer(pSocket->GetAddress(), 0, true)
	 || pSocket->GetState() != CStreamSocket::eIncoming)
	{
		m_Server->FreeSocket(pSocket);
		return;
	}
	CMuleClient* pClient = new CMuleClient(((CMuleSocket*)pSocket), this);
	AddClient(pClient);
}

void CMuleManager::SendUDPPacket(CBuffer& Packet, uint8 Prot, const SMuleSource& Mule)
{
	QByteArray UserHash;
	if(RequestsCryptLayer() || Mule.ConOpts.Fields.RequestsCryptLayer)
	{
		if(!(SupportsCryptLayer() && Mule.ConOpts.Fields.SupportsCryptLayer) && (RequiresCryptLayer() || Mule.ConOpts.Fields.RequiresCryptLayer))
			return;
		UserHash = Mule.UserHash.ToArray(true);
	}

	if(Prot == OP_PACKEDPROT)
		Prot = PackPacket(Packet) ? OP_PACKEDPROT : OP_EMULEPROT;

	emit SendUDPPacket(Packet.ToByteArray(), Prot, Mule.GetIP(), Mule.GetPort(true), UserHash);
}

void CMuleManager::ProcessUDPPacket(QByteArray PacketArr, quint8 Prot, CAddress Address, quint16 uUDPPort)
{
	try
	{
		CBuffer Packet(PacketArr, true);
		if(Prot == OP_PACKEDPROT)
		{
			UnpackPacket(Packet);
			Prot = OP_EMULEPROT;
		}
		ProcessUDPPacket(Packet, Prot, Address, uUDPPort);
	}
	catch(const CException& Exception)
	{
		LogLine(Exception.GetFlag(), tr("recived malformated packet form mule client udp; %1").arg(QString::fromStdWString(Exception.GetLine())));
	}
}

void CMuleManager::ProcessUDPPacket(CBuffer& Packet, quint8 Prot, const CAddress& Address, quint16 uUDPPort)
{
	if(!theCore->m_PeerWatch->CheckPeer(Address, 0, true))
		return;

	// Note: some packets have to be handled without having a cleint assotiated with filter this packets here
	uint8 uOpcode = Packet.ReadValue<uint8>();
	switch(Prot)
	{
		case OP_EMULEPROT:
		{
			switch(uOpcode)
			{
				case OP_REASKCALLBACKUDP:
				{
					m_Kademlia->RelayUDPPacket(Address, uUDPPort, Packet);
					break;
				}
				case OP_DIRECTCALLBACKREQ: 
				{
					SMuleSource Mule;
					Mule.SetIP(Address);
					Mule.TCPPort = Packet.ReadValue<uint16>();
					Mule.UserHash.Set(Packet.ReadData(16));
					Mule.ConOpts.Bits = Packet.ReadValue<uint8>();
					CallbackRequested(Mule);
					break;
				}
				case OP_RENDEZVOUS:
				{
					if(!SupportsNatTraversal())
						break;

					// Note: we dont get here form the socket directly but from our boddy, the IP and port argumetns are the one seen by the buddy
					SMuleSource Mule;
					Mule.SetIP(Address);
					Mule.UDPPort = uUDPPort;
					Mule.UserHash.Set(Packet.ReadData(16));
					Mule.ConOpts.Bits = Packet.ReadValue<uint8>();
					CallbackRequested(Mule, true);
					break;
				}
				case OP_HOLEPUNCH:
					break;

				default:
					if(CMuleClient* pClient = FindClient(Address, uUDPPort))
						pClient->ProcessUDPPacket(Packet,Prot);
			}
			break;
		}

		default:
			LogLine(LOG_DEBUG, tr("Recived unknwon Ed2kMule UDP packet protocol"));
	}
}

int CMuleManager::GetConnectionCount()
{
	return m_Server->GetSocketCount();
}

bool CMuleManager::GrantSX(CFile* pFile)
{
	if(pFile->GetStats()->GetTransferCount(eEd2kMule) >= theCore->Cfg()->GetInt("Ed2kMule/MaxSources"))
		return false;

	SSXTiming& SXTiming = m_SXTiming[pFile->GetFileID()];
	if(SXTiming.SXCount + SXTiming.Pending.size() < theCore->Cfg()->GetInt("Ed2kMule/SXVolume"))
	{
		if(SXTiming.NextReset == 0)
			SXTiming.NextReset = GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("Ed2kMule/SXInterval"));
		SXTiming.Pending.append(GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("Ed2kMule/ConnectTimeout")));
		return true;
	}
	return false;
}

void CMuleManager::ConfirmSX(CFile* pFile)
{
	SSXTiming& SXTiming = m_SXTiming[pFile->GetFileID()];
	if(SXTiming.Pending.isEmpty())
		return; // unsilicitated SX packet ?!
	SXTiming.Pending.removeFirst();
	SXTiming.SXCount++;
}

bool CMuleManager::GrantIPRequest()
{
	if(m_NextIPRequest <= GetCurTick())
	{
		m_NextIPRequest = GetCurTick() + SEC2MS(1);
		return true;
	}
	return false;
}

CMuleClient* CMuleManager::FindClient(const SMuleSource& Mule, CMuleClient* pNot)
{
	CMuleClient* pFoundClient = NULL;
	foreach (CMuleClient* pCurClient, m_Clients)
	{
		if(pFoundClient == NULL && Mule.TCPPort && pCurClient->GetMule() == Mule && pCurClient != pNot)
			pFoundClient = pCurClient;
		if(!Mule.UserHash.IsEmpty() && pCurClient->GetUserHash() == Mule.UserHash && pCurClient != pNot)
		{
			pFoundClient = pCurClient;
			break;
		}
	}
	return pFoundClient;
}

CMuleClient* CMuleManager::FindClient(const CAddress& Address, uint16 uUDPPort)
{
	CMuleClient* pFoundClient = NULL;
	foreach (CMuleClient* pCurClient, m_Clients)
	{
		if(pCurClient->GetMule().CompareTo(Address))
		{
			pFoundClient = pCurClient;
			if(pCurClient->GetUDPPort() == uUDPPort)
				break;
		}
	}
	return pFoundClient;
}

bool CMuleManager::DispatchClient(CMuleClient*& pClient)
{
	CMuleClient* pFoundClient = FindClient(pClient->GetMule(), pClient);
	if(!pFoundClient)
		return true; // nothing to do the client is new
	
	if(pFoundClient->IsConnected())
		LogLine(LOG_DEBUG | LOG_WARNING, tr("A Ed2kmule Client connected to us while we ware still connected to it"));

	// swap socket to the existing client and let the pending connection be deleted, as source and socket free
	pFoundClient->SwapSocket(pClient);
	pClient = pFoundClient;

	return true;
}

CMuleSource* CMuleManager::AttacheToFile(const CFileHash* pHash, uint64 uSize, CMuleClient* pClient)
{
	CFile* pFile = theCore->m_FileManager->GetFileByHash(pHash);
	if(!pFile)
		return NULL;

	if(uSize && pFile->GetFileSize() != uSize)
	{
		LogLine(LOG_ERROR, tr("Size Mismatch on requested ed2k file"));
		return NULL;
	}

	CMuleSource* pFoundSource = NULL;
    foreach (CTransfer* pTransfer, pFile->GetTransfers())
	{
		if(CMuleSource* pSource = qobject_cast<CMuleSource*>(pTransfer))
		{
			if(pSource->GetClient() == pClient)
			{
				pFoundSource = pSource;
				break;
			}
			else if(pSource->GetUserHash() == pClient->GetUserHash())
				pFoundSource = pSource;
		}
	}

	if(!pFoundSource)
	{
		pFoundSource = new CMuleSource();
		pFoundSource->SetFoundBy(eSelf);
		pFile->AddTransfer(pFoundSource);
	}

	if(!pFoundSource->GetClient())
		pFoundSource->AttacheClient(pClient);
	else if(pClient != pFoundSource->GetClient())
		LogLine(LOG_DEBUG | LOG_ERROR, tr("client source assitiation error (client might be present twice!)"));

	return pFoundSource;
}

void CMuleManager::AddToFile(CFile* pFile, const SMuleSource& Mule, EFoundBy FoundBy)
{
	if(!pFile->IsStarted() || !pFile->IsEd2kShared())
		return;

	if(m_UserHash == Mule.UserHash || (m_Server->GetPort() == Mule.TCPPort && Mule.CompareTo(GetAddress(Mule.Prot))))
		return; // dont connect to ourselves

	if(!theCore->m_PeerWatch->CheckPeer(Mule.GetIP(), Mule.TCPPort))
		return;

    foreach (CTransfer* pTransfer, pFile->GetTransfers())
	{
		if(CMuleSource* pSource = qobject_cast<CMuleSource*>(pTransfer))
		{
			if(pSource->GetMule() == Mule)
				return; // we already know this source

			if(!Mule.UserHash.IsEmpty() && pSource->GetUserHash() == Mule.UserHash)
				return; // we already know this source
		}
	}

	CMuleSource* pNewSource = new CMuleSource(Mule);
	pNewSource->SetFoundBy(FoundBy);
	pFile->AddTransfer(pNewSource);
	pNewSource->AttacheClient();
}

CMuleClient* CMuleManager::GetClient(const SMuleSource& Mule, bool* bAdded)
{
	CMuleClient* pClient = FindClient(Mule);
	if(!pClient)
	{
		pClient = new CMuleClient(Mule, this);
		AddClient(pClient);

		if(bAdded)
			*bAdded = true;
	}
	return pClient;
}

void CMuleManager::CallbackRequested(const SMuleSource& Mule, bool bUTP)
{
	if(!theCore->m_PeerWatch->CheckPeer(Mule.GetIP(), Mule.TCPPort))
		return;

	CMuleClient* pClient = GetClient(Mule);
	
	if(!bUTP && pClient->IsFirewalled(Mule.Prot))
	{
		LogLine(LOG_ERROR, tr("Firewalled emule client requested callback (impossible)"));
		return;
	}

	if(pClient->IsDisconnected())
		pClient->Connect(bUTP);
}

bool CMuleManager::IsFirewalled(CAddress::EAF eAF, bool bUTP, bool bIgnoreKad) const
{
	// EM-ToDo: once we have proepr IPv6 servers or an other method return proepr firewalled state
	if(eAF == CAddress::IPv6)
		return m_FWWatch.IsFirewalled(CAddress::IPv6, bUTP ? CFWWatch::eUTP : CFWWatch::eTCP);
	else if(bUTP)
		return !m_Kademlia->IsUDPOpen();

	// Here we test IPv4 and TCP only
	if(!bIgnoreKad && m_Kademlia->IsEnabled())
	{
		if(!m_Kademlia->IsFirewalled())
			return false;
	}
	// EM-ToDo: servers may in futire support UTP and ore IPv6
	if(!m_ServerList->IsFirewalled(eAF))
		return false;
	return true;
}

CAddress CMuleManager::GetAddress(CAddress::EAF eAF, bool bIgnoreKad) const
{
	if(eAF == CAddress::IPv4 && !bIgnoreKad && m_Kademlia->IsEnabled())
	{
		const CAddress& KadAddress = m_Kademlia->GetAddress();
		if(!KadAddress.IsNull())
			return KadAddress;
	}
	CAddress Address = m_ServerList->GetAddress(eAF);
	if(Address.IsNull())
		Address = m_Server->GetAddress(eAF);

	if(eAF == CAddress::IPv6 && Address.IsNull())
		Address = theCore->m_Network->GetAddress(CAddress::IPv6);
	return Address;
}

int CMuleManager::GetIPv6Support() const
{
	if(!m_Server->HasIPv6())
		return 0; // No Support
	if(!GetAddress(CAddress::IPv6).IsNull())
		return 1; // Confirmed Support
	return 2; // we dont know
}

// AICH Stuff
void CMuleManager::RequestAICHData(CFile* pFile, uint64 uBegin, uint64 uEnd)
{
	uint16 FirstPart = uBegin / ED2K_PARTSIZE;
	uint16 LastPart = DivUp(uEnd, ED2K_PARTSIZE);

	QList<uint16> Parts;
	for(uint16 uPart = FirstPart; uPart <= LastPart; uPart++)
	{
		if(IsAICHRequestes(pFile, uPart))
			continue;
		Parts.append(uPart);
	}
	if(Parts.isEmpty())
		return;

	CMuleClient* pFoundClient = NULL;
	foreach (CTransfer* pTransfer, pFile->GetTransfers()) 
	{
		if(CMuleSource* pSource = qobject_cast<CMuleSource*>(pTransfer))
		{
			CMuleClient* pClient = pSource->GetClient();
			if(!pClient || !pClient->SupportsAICH())
				continue;

			if(!pSource->IsComplete() && pClient->ProtocolRevision() == 0)
				continue; // Note: old mules only have AICH data if thay are compelte

			pFoundClient = pClient;
			if(pClient->GetSocket())
				break;
		}
	}
	if(!pFoundClient)
		return;

	foreach(uint16 uPart, Parts)
	{
		AddAICHRequest(pFoundClient, pFile, uPart);
		if(pFoundClient->IsConnected())
			pFoundClient->SendAICHRequest(pFile, uPart);
		else
		{
			connect(pFoundClient, SIGNAL(HelloRecived()), this, SLOT(OnHelloRecived()));
			if(!pFoundClient->GetSocket())
				pFoundClient->Connect();
		}
		connect(pFoundClient, SIGNAL(SocketClosed()), this, SLOT(OnSocketClosed()));
	}
}

void CMuleManager::OnHelloRecived()
{
	CMuleClient* pClient = (CMuleClient*)sender();

	foreach(uint64 FileID, m_AICHQueue.keys())
	{
		CFile* pFile = CFileManager::GetFile(FileID);
		if(!pFile || pFile->IsRemoved() || pFile->GetHash(HashMule) == NULL)
		{
			m_AICHQueue.remove(FileID);
			continue;
		}

		QMap<CMuleClient*, SAICHReq>& Temp = m_AICHQueue[FileID];
		if(Temp.contains(pClient))
		{
			foreach(uint16 uPart, Temp[pClient].Parts)
				pClient->SendAICHRequest(pFile, uPart);
		}
	}
}

void CMuleManager::OnSocketClosed()
{
	CMuleClient* pClient = (CMuleClient*)sender();

	foreach(uint64 FileID, m_AICHQueue.keys())
	{
		QMap<CMuleClient*, SAICHReq>& Temp = m_AICHQueue[FileID];
		Temp.remove(pClient);
		if(Temp.isEmpty())
			m_AICHQueue.remove(FileID);
	}

	disconnect(pClient, SIGNAL(SocketClosed()), this, SLOT(OnSocketClosed()));
}

void CMuleManager::AddAICHRequest(CMuleClient* pClient, CFile* pFile, uint16 uPart)
{
	if(!m_AICHQueue[pFile->GetFileID()].contains(pClient))
		m_AICHQueue[pFile->GetFileID()].insert(pClient, SAICHReq(GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("Ed2kMule/IdleTimeout"))));
	if(!m_AICHQueue[pFile->GetFileID()][pClient].Parts.contains(uPart))
		m_AICHQueue[pFile->GetFileID()][pClient].Parts.append(uPart);
}

void CMuleManager::DropAICHRequests(CMuleClient* pClient, CFile* pFile)
{
	QMap<CMuleClient*, SAICHReq>& Temp = m_AICHQueue[pFile->GetFileID()];
	Temp.remove(pClient);
	if(Temp.isEmpty())
		m_AICHQueue.remove(pFile->GetFileID());
}

bool CMuleManager::CheckAICHRequest(CMuleClient* pClient, CFile* pFile, uint16 uPart)
{
	if(!m_AICHQueue.contains(pFile->GetFileID()))
		return false;
	if(!m_AICHQueue[pFile->GetFileID()].contains(pClient))
		return false;
	SAICHReq& AICHReq = m_AICHQueue[pFile->GetFileID()][pClient];
	if(!AICHReq.Parts.contains(uPart))
		return false;
	AICHReq.Parts.removeOne(uPart);
	if(AICHReq.Parts.isEmpty())
	{
		m_AICHQueue[pFile->GetFileID()].remove(pClient);
		if(m_AICHQueue[pFile->GetFileID()].isEmpty())
			m_AICHQueue.remove(pFile->GetFileID());
	}
	else 
		AICHReq.uTimeOut += GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("Ed2kMule/IdleTimeout"));
	return true;	
}

bool CMuleManager::IsAICHRequestes(CFile* pFile, uint16 uPart)
{
	if(!m_AICHQueue.contains(pFile->GetFileID()))
		return false;
	foreach(CMuleClient* pClient, m_AICHQueue[pFile->GetFileID()].keys())
	{
		SAICHReq& AICHReq = m_AICHQueue[pFile->GetFileID()][pClient];

		if(AICHReq.uTimeOut < GetCurTick())
			m_AICHQueue[pFile->GetFileID()].remove(pClient);
		else if(AICHReq.Parts.contains(uPart))
			return true;	
	}
	return false;
}


uint64 CMuleManager::GrabCollection(const QByteArray& FileData, const QString& FileName)
{
	CMuleCollection Collection;
	if(Collection.LoadFromData(FileData, FileName))
	{
		uint64 GrabberID = theCore->m_FileGrabber->GrabUris(QStringList("file://" + FileName));
		ASSERT(GrabberID);

		CFile* pFile = new CFile();
		pFile->SetPending();

		theCore->m_FileGrabber->AddFile(pFile, GrabberID); // this must not fail
		Collection.Populate(pFile);

		pFile->Start();

		return GrabberID;
	}
	else
		LogLine(LOG_ERROR, tr("The collection file %1 cannot not be parsed.").arg(FileName));
	return 0;
}

CFile* ImportMuleDownload(CBuffer& MetBuffer)
{
	uint8 uVersion = MetBuffer.ReadValue<uint8>();
	if (uVersion != PARTFILE_VERSION && uVersion != PARTFILE_SPLITTEDVERSION && uVersion != PARTFILE_VERSION_LARGEFILE)
		return NULL;

	bool bNewStyle = (uVersion == PARTFILE_SPLITTEDVERSION);
	if(!bNewStyle)
	{
		MetBuffer.SetPosition(24);
		uint8 Test[4];
		for(int i=0; i < ARRSIZE(Test); i++)
			Test[i] = MetBuffer.ReadValue<uint8>();
		MetBuffer.SetPosition(1);
		if (Test[0]==0 && Test[1]==0 && Test[2]==2 && Test[3]==1)
			bNewStyle = true;
	}

	time_t uLastModified = 0;
	QByteArray Hash;
	if(bNewStyle)
	{
		uint32 Temp = MetBuffer.ReadValue<uint32>();
		if (Temp != 0) // 0.48
		{
			MetBuffer.SetPosition(2);
			uLastModified = MetBuffer.ReadValue<uint32>();

			Hash = MetBuffer.ReadQData(16);
		}
	}
	else 
		uLastModified = MetBuffer.ReadValue<uint32>();

	QList<QByteArray> HashSet;
	if(Hash.isEmpty())
	{
		Hash = MetBuffer.ReadQData(16);
		uint32 uHashCount = MetBuffer.ReadValue<uint16>();
		for (uint32 i = 0; i < uHashCount; i++)
			HashSet.append(MetBuffer.ReadQData(16));
	}

	QString FileName;
	uint64 uFileSize;
	//QMap<int, QPair<uint64, uint64> > Gaps;
	QVariantMap Tags = CMuleTags::ReadTags(&MetBuffer);
	for(QVariantMap::iterator I = Tags.begin(); I != Tags.end(); ++I)
	{
		const QVariant& Value = I.value();
		switch(FROM_NUM(I.key()))
		{
			case FT_FILENAME:	FileName = Value.toString();		break;
			case FT_FILESIZE:	uFileSize = Value.toULongLong();	break;
			/*default:
			{	
				bool bEnd = false;
				if(!I.key().isEmpty() && (I.key().at(0) == FT_GAPSTART || (bEnd = I.key().at(0) == FT_GAPEND)))
				{
					int GapKey = I.key().mid(1).toInt();
					if(bEnd)
						Gaps[GapKey].second = Value.toULongLong();
					else
						Gaps[GapKey].first = Value.toULongLong();
				}
			}*/
		}
	}

	if(uFileSize == 0 || FileName.isEmpty())
		return NULL;

	CFileHashPtr pHashSet = CFileHashPtr(new CFileHashSet(HashEd2k, uFileSize));
	pHashSet->SetHash(Hash);

	if (bNewStyle && MetBuffer.GetSizeLeft() > 0 && HashSet.isEmpty()) 
	{
		uint8 Temp = MetBuffer.ReadValue<uint8>();
			
		uint32 uHashCount = ((CFileHashSet*)pHashSet.data())->GetPartCount();
		for (uint32 i = 0; i < uHashCount && MetBuffer.GetSizeLeft() >= 16; i++)
			HashSet.append(MetBuffer.ReadQData(16));
	}

	if(!((CFileHashSet*)pHashSet.data())->SetHashSet(HashSet))
		return NULL;

	CFile* pFile = new CFile(theCore->m_FileManager);
	pFile->AddHash(pHashSet);
	pFile->AddEmpty(HashEd2k, FileName, uFileSize, false);
	return pFile;
}

CFile* CMuleManager::ImportDownload(const QString& FilePath, bool bDeleteSource)
{
	QFile MetFile(FilePath);
	if(!MetFile.open(QFile::ReadOnly))
		return NULL;
	CBuffer MetBuffer(MetFile.readAll());

	StrPair FileEx = Split2(FilePath, ".", true); // set path to 001.part, from path to 001.part.met

	CFile* pFile = NULL;
	try
	{
		pFile = ImportMuleDownload(MetBuffer);
	}
	catch(CException&)
	{
		return NULL;
	}

	if(!pFile)
	{
		LogLine(LOG_ERROR, tr("Import of %1 failed").arg(FilePath));
		return NULL;
	}

	CFile* pKnownFile = NULL;
	if(!theCore->m_FileManager->AddUniqueFile(pFile, false, &pKnownFile))
	{
		ASSERT(pKnownFile);

		if(!pKnownFile->IsStarted())
			pKnownFile->Pause();

		CFileHashSet* pHash = qobject_cast<CFileHashSet*>(pKnownFile->GetHash(HashEd2k));
		if(!pHash->IsComplete())
		{
			CFileHashSet* pHashSet = qobject_cast<CFileHashSet*>(pFile->GetHash(HashEd2k));
			pHash->SetHashSet(pHashSet->GetHashSet());
		}

		delete pFile;
		pFile = pKnownFile;
	}
	else if(!(pFile->IsPending() || pFile->MetaDataMissing()))
		pFile->Resume(); // OpenIO

	
	QList<CFileHashPtr> List;
	List.append(pFile->GetHashPtr(HashEd2k));
	CHashingJobPtr pHashingJob = CHashingJobPtr(new CImportPartsJob(pFile->GetFileID(), List, pFile->GetPartMapPtr(), FileEx.first, bDeleteSource));
	theCore->m_Hashing->AddHashingJob(pHashingJob);

	MetFile.close();
	if(bDeleteSource)
		MetFile.remove();

	return pFile;
}

void CMuleManager::OnBytesWritten(qint64 Bytes)
{
	m_TransferStats.AddUpload(Bytes);
	theCore->m_Network->OnBytesWritten(Bytes);
}

void CMuleManager::OnBytesReceived(qint64 Bytes)
{
	m_TransferStats.AddDownload(Bytes);
	theCore->m_Network->OnBytesReceived(Bytes);
}

bool PackPacket(CBuffer& Buffer)
{
	Buffer.SetPosition(0);
	uint8 uOpcode = Buffer.ReadValue<uint8>();
	CBuffer TempPacket(Buffer.ReadData(0), Buffer.GetSizeLeft(), true);
	if(!TempPacket.Pack())
		return false;

	Buffer.AllocBuffer(1 + TempPacket.GetSize());
	Buffer.WriteValue<uint8>(uOpcode);
	Buffer.WriteData(TempPacket.GetBuffer(), TempPacket.GetSize());
	return true;
}

void UnpackPacket(CBuffer& Buffer)
{
	uint8 uOpcode = Buffer.ReadValue<uint8>();
	CBuffer TempPacket(Buffer.ReadData(0), Buffer.GetSizeLeft(), true);
	if(!TempPacket.Unpack())
		throw CException(LOG_DEBUG, L"Packet Unpack Error");

	Buffer.AllocBuffer(1 + TempPacket.GetSize(), true);
	Buffer.WriteValue<uint8>(uOpcode);
	Buffer.WriteData(TempPacket.GetBuffer(), TempPacket.GetSize());
	Buffer.SetPosition(0);
}
