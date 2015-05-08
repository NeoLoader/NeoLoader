#include "GlobalHeader.h"
#include "TorrentClient.h"
#include "TorrentServer.h"
#include "../../NeoCore.h"
#include "TorrentManager.h"
#include "../../../qbencode/lib/bencode.h"
#include "TorrentInfo.h"
#include "Torrent.h"
#include "../../FileList/IOManager.h"
#include "../../FileList/FileStats.h"
#include "../../FileList/Hashing/FileHashTree.h"
#include "../../FileList/Hashing/HashingThread.h"
#include "../PeerWatch.h"
#include "../../../Framework/Exception.h"
#include "../../../Framework/Scope.h"
#include "../../Networking/BandwidthControl/BandwidthLimit.h"
#include "../../Networking/BandwidthControl/BandwidthLimiter.h"
#include "../NeoShare/NeoManager.h"
#include "../NeoShare/NeoKad.h"

CTorrentClient::CTorrentClient(CTorrentPeer* pPeer, QObject* qObject)
: CP2PClient(qObject)
{
	m_Peer = pPeer->GetPeer();
	m_pPeer = pPeer;
	m_InfoHash = m_pPeer->GetTorrent()->GetInfoHash();
	if(m_Peer.ConOpts.Fields.SupportsUTP && theCore->Cfg()->GetBool("BitTorrent/uTP"))
		m_Socket = (CTorrentSocket*)theCore->m_TorrentManager->GetServer()->AllocUTPSocket();
	else
		m_Socket = (CTorrentSocket*)theCore->m_TorrentManager->GetServer()->AllocSocket();
	m_Socket->AddUpLimit(m_UpLimit);
	m_Socket->AddDownLimit(m_DownLimit);
	m_uConnectTimeOut = -1; // connect call wil start timeout

	Init();
}

CTorrentClient::CTorrentClient(CTorrentSocket* pSocket, QObject* qObject)
: CP2PClient(qObject)
{
	m_pPeer = NULL;
	m_Socket = pSocket;
	m_Socket->AddUpLimit(m_UpLimit);
	m_Socket->AddDownLimit(m_DownLimit);
	m_uConnectTimeOut = GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("BitTorrent/ConnectTimeout"));

	connect(m_Socket, SIGNAL(ReceivedHandshake(QByteArray)), this, SLOT(OnReceivedHandshake(QByteArray)), Qt::QueuedConnection);
	connect(m_Socket, SIGNAL(ReceivedPacket(QByteArray)), this, SLOT(OnReceivedPacket(QByteArray)), Qt::QueuedConnection);
	connect(m_Socket, SIGNAL(Disconnected(int)), this, SLOT(OnDisconnected(int)));

	m_Socket->AcceptConnect(); // enable packet reading

	if(pSocket->GetState() == CStreamSocket::eConnected)
	{
		m_Peer.SetIP(pSocket->GetAddress());
		//if(m_Socket->GetSocketType() == SOCK_UTP) // this fucks up the client recognition
		//	m_Peer.Port = pSocket->GetPort(); // Note: TCP ports are random, UTP Ports are not
		m_Peer.ConOpts.Fields.SupportsEncryption = m_Socket->SupportsEncryption();
		m_Peer.ConOpts.Fields.SupportsUTP = (m_Socket->GetSocketType() == SOCK_UTP);
	}

	Init();
	LogLine(LOG_DEBUG, tr("connected socket (Incoming)"));
}

void CTorrentClient::Init()
{
	m_SentHandShake = false;
	m_ReceivedHandShake = false;
	m_HolepunchTry = 0;
	m_uIdleTimeOut = -1;
	m_uNextKeepAlive = -1;
	m_uRequestTimeOut = -1;
	m_RequestLimit = theCore->Cfg()->GetInt("BitTorrent/RequestLimit");
	m_SentPieceSize = 0;
	m_RecivedPieceSize = 0;
	//m_TempPieceSize = 0;

	m_Supports_Extension = false;
	//m_Supports_Merkle = false;
	m_Supports_DHT = false;
	m_Supports_FAST = false;
	m_ut_pex_Extension = 0;
	m_ut_metadata_Extension = 0;
	m_ut_holepunch_Extension = 0;
	m_Tr_hashpiece_Extension = 0;
	m_Supports_HostCache = false;
	//m_Version_NeoKad = 0;

	m_NextMessagePEX = GetCurTick() + (qrand()%MIN2MS(10)); // BT-ToDo: customize
}

CTorrentClient::~CTorrentClient()
{
	ASSERT(m_pPeer == NULL);
	disconnect(m_Socket, 0, 0, 0);
	theCore->m_TorrentManager->GetServer()->FreeSocket(m_Socket);
}

void CTorrentClient::Process(UINT Tick)
{
	uint64 uNow = GetCurTick();
	if(m_uConnectTimeOut < uNow) //|| (!m_ReceivedHandShake && (m_Socket->GetLastActivity() + SEC2MS(theCore->Cfg()->GetInt("BitTorrent/ConnectTimeout")) < uNow)))
		Disconnect("TimeOut");
	else if(m_uRequestTimeOut < uNow)
		Disconnect("ReqTimeOut");
	else if(m_uNextKeepAlive < uNow)
		KeepAlive();
	else if(m_pPeer) // Note: pocess is called form the manager also for not yet fully handshaked cleints!
	{
		if(m_ut_pex_Extension != 0 && m_NextMessagePEX < uNow)
			SendPeerList();
	}
}

QString CTorrentClient::GetUrl()
{
	return QString("bt://%1:%2/").arg(m_Peer.GetIP().ToQString(true)).arg(m_Peer.Port);
}

QString CTorrentClient::GetConnectionStr()
{
	if(IsDisconnected())
		return "Disconnected";
	QString Status;
	if(IsConnected())
		Status = "Connected";
	else if(!IsDisconnected())
		Status = "Connecting";
	if(m_Socket)
	{
		if(m_Socket->GetSocketType() == SOCK_UTP)
			Status += " UTP";
		if(m_Socket->IsEncrypted())
			Status += " Encrypted";
	}
	return Status;
}

CFile* CTorrentClient::GetFile()
{
	if(m_pPeer)
		return m_pPeer->GetFile();
	return NULL;
}

// Sends the handshake to the peer.
void CTorrentClient::SendHandShake()
{
	LogLine(LOG_DEBUG, tr("SendHandShake"));
	
    m_SentHandShake = true;

    // Restart the timeout
	CBuffer Packet(HAND_SHAKE_SIZE);

	ASSERT(strlen(PROTOCOL_ID) == 19);
	Packet.WriteValue<uint8>(19, true);									// ProtocolIdSize
	Packet.WriteData(PROTOCOL_ID, 19);									// ProtocolId
	uint64 Reserved = 0;
	((char*)&Reserved)[4] |= 0x01;	// HostCache
	((char*)&Reserved)[5] |= 0x10;	// Extensions
	((char*)&Reserved)[5] |= 0x08;	// Merkle torrents
	((char*)&Reserved)[7] |= 0x01;	// DHT
	((char*)&Reserved)[7] |= 0x04;	// FAST
	Packet.WriteValue<uint64>(Reserved);								// reserved
	Packet.WriteData(m_InfoHash.data(),20);								// infoHash
	Packet.WriteData(theCore->m_TorrentManager->GetClientID().Data, 20);// peerIdString

	ASSERT(Packet.GetSize() == Packet.GetPosition());
	m_Socket->SendHandshake(Packet.ToByteArray());
}

bool CTorrentClient::ProcessHandShake(CBuffer& Packet)
{
	LogLine(LOG_DEBUG, tr("ProcessHandShake"));

	m_ReceivedHandShake = true;

	// Sanity check the protocol ID
	if (Packet.ReadValue<uint8>(true) != 19 || memcmp(Packet.ReadData(19),PROTOCOL_ID, 19))
	{
		LogLine(LOG_DEBUG | LOG_ERROR, tr("ProcessHandShake Error - Invalid protocol ID"));
		Disconnect("HandShakeError");
		return false;
	}

	uint64 Reserved = Packet.ReadValue<uint64>();
	m_Supports_HostCache	= ((((char*)&Reserved)[4] & 0x01) != 0);
	m_Supports_Extension	= ((((char*)&Reserved)[5] & 0x10) != 0);
	//m_Supports_Merkle		= ((((char*)&Reserved)[5] & 0x08) != 0); // Merkle torrents
	m_Supports_DHT			= ((((char*)&Reserved)[7] & 0x01) != 0);
	m_Supports_FAST			= ((((char*)&Reserved)[7] & 0x04) != 0);

	// Read infoHash
	m_InfoHash = QByteArray((char*)Packet.ReadData(20),20);

	// We connected to ourself
	m_Peer.ID.Set(Packet.ReadData(20));
	if (m_Peer.ID == theCore->m_TorrentManager->GetClientID())
	{
		Disconnect("Loopback");
		return false;
	}

	return true;
}

// Sends a "keep-alive" message to prevent the peer from closing
// the connection when there's no activity
void CTorrentClient::KeepAlive()
{
	CBuffer Buffer(0);
    SendPacket(Buffer);

	// schedule next keep alive
	m_uNextKeepAlive = GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("BitTorrent/KeepAlive"));
}

// Sends a "choke" message, asking the peer to stop requesting blocks.
void CTorrentClient::ChokePeer()
{
	LogLine(LOG_DEBUG, tr("SendChokePeer"));

	CBuffer Packet(1);
	Packet.WriteValue<uint8>(eChoke, true);
    SendPacket(Packet);

	//m_OutgoingBlocks.clear();

	// After receiving a choke message, the peer will assume all
    // pending requests are lost.
	m_Socket->ClearQueue();
	m_Socket->SetUpload(false);
}

// Sends an "unchoke" message, allowing the peer to start/resume
// requesting blocks.
void CTorrentClient::UnchokePeer()
{
	LogLine(LOG_DEBUG, tr("SendUnchokePeer"));

	m_Socket->SetUpload(true);

	m_Socket->ClearQueue();
	m_SentPieceSize = 0; // reset upload counter
	//m_TempPieceSize = 0;

	CBuffer Packet(1);
	Packet.WriteValue<uint8>(eUnchoke, true);
    SendPacket(Packet);
}

// Sends an "interested" message, informing the peer that it has got
// pieces that we'd like to download.
void CTorrentClient::SendInterested()
{
	LogLine(LOG_DEBUG, tr("SendInterested"));

	CBuffer Packet(1);
	Packet.WriteValue<uint8>(eInterested, true);
    SendPacket(Packet);

    // After telling the peer that we're interested, we expect to get
    // unchoked within a certain timeframe; otherwise we'll drop the
    // connection.
    //m_uRequestTimeOut = GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("BitTorrent/RequestTimeout"));
}

// Sends a "not interested" message, informing the peer that it does
// not have any pieces that we'd like to download.
void CTorrentClient::SendNotInterested()
{
	LogLine(LOG_DEBUG, tr("SendNotInterested"));

	CBuffer Packet(1);
	Packet.WriteValue<uint8>(eNotInterested, true);
    SendPacket(Packet);
}

// Sends the complete list of pieces that we have downloaded.
void CTorrentClient::SendPieceList(const QBitArray &BitField)
{
	LogLine(LOG_DEBUG, tr("SendPieceList"));

    // The bitfield message may only be sent immediately after the
    // handshaking sequence is completed, and before any other
    // messages are sent.

	QByteArray Bits;
	if(m_pPeer->GetFile()->IsComplete())
	{
		CTorrentInfo* pTorrentInfo = m_pPeer->GetTorrent()->GetInfo();
		int Size = DivUp(pTorrentInfo->GetTotalLength(), pTorrentInfo->GetPieceLength());
		Bits.fill((char)0xFF, Size);
	}
    else
	{
		if (BitField.isEmpty() || BitField.count(true) == 0) // Don't send the bitfield if it's all zeros.
			return;

		Bits = CShareMap::Bits2Bytes(BitField);
	}

	CBuffer Packet(1 + Bits.size());
	Packet.WriteValue<uint8>(eBitField, true);
	Packet.WriteData(Bits.data(), Bits.size());
	SendPacket(Packet);
}

void CTorrentClient::ProcessPieceList(CBuffer& Packet)
{
	LogLine(LOG_DEBUG, tr("ProcessPieceList"));

	CTorrentInfo* pTorrentInfo = m_pPeer->GetTorrent()->GetInfo();

	// The peer has the following pieces available.
	size_t PieceCount;
	if(pTorrentInfo->IsEmpty()) // thats when we dont have the metainfo yet.
		PieceCount = Packet.GetSizeLeft() * 8;
	else
		PieceCount = DivUp(pTorrentInfo->GetTotalLength(), pTorrentInfo->GetPieceLength());
	
	QBitArray BitField = CShareMap::Bytes2Bits(Packet.ReadQData(DivUp(PieceCount, 8)), PieceCount);
	m_pPeer->OnPiecesAvailable(BitField);
}

// Sends a request for a block.
void CTorrentClient::RequestBlock(uint32 Index, uint32 Offset, uint32 Length)
{
	int iDebug = theCore->Cfg()->GetInt("Log/Level");
	if(iDebug == 3)
		LogLine(LOG_DEBUG, tr("SendRequestBlock"));

	CBuffer Packet(1 + 4 + 4 + 4);
	Packet.WriteValue<uint8>(eRequest, true);
	Packet.WriteValue<uint32>(Index, true);
	Packet.WriteValue<uint32>(Offset, true);
	Packet.WriteValue<uint32>(Length, true);
	SendPacket(Packet);

    m_IncomingBlocks.append(SRequestedBlock(Index, Offset, Length));
    // After requesting a block, we expect the block to be sent by the
    // other peer within a certain number of seconds. Otherwise, we
    // drop the connection.
    m_uRequestTimeOut = GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("BitTorrent/RequestTimeout"));
}

void CTorrentClient::ProcessBlockRequest(CBuffer& Packet)
{
	int iDebug = theCore->Cfg()->GetInt("Log/Level");
	if(iDebug == 3)
		LogLine(LOG_DEBUG, tr("ProcessBlockRequest"));

	// The peer requests a block.
	uint32 Index = Packet.ReadValue<uint32>(true);
	uint32 Offset = Packet.ReadValue<uint32>(true);
	uint32 Length = Packet.ReadValue<uint32>(true);


	if(!m_pPeer->IsActiveUpload())
	{
		if(SupportsFAST())
			RejectRequest(Index, Offset, Length);
		return; // Silently ignore requests from choked peers
	}

	//bool bRedundant = false;
	//for(int j=0; j < m_OutgoingBlocks.count(); j++)
	//{
	//	SPendingBlock& Block = m_OutgoingBlocks[j];
	//	if(bRedundant = (Block.Index == Index && Block.Offset == Offset && Block.Length == Length))
	//	{
	//		m_OutgoingBlocks.append(m_OutgoingBlocks.takeAt(j)); // move block down so we keep track of it
	//		break;
	//	}
	//}
	//if(bRedundant)
	//	return;
	//
	//SPendingBlock Block;
	//Block.Index = Index;
	//Block.Offset = Offset;
	//Block.Length = Length;
	//m_OutgoingBlocks.append(Block);

	//m_TempPieceSize += Length;

	uint64 uBegin = (Index * m_pPeer->GetTorrent()->GetInfo()->GetPieceLength()) + Offset;
	theCore->m_IOManager->ReadData(this, m_pPeer->GetFile()->GetFileID(), uBegin, Length);
}

void CTorrentClient::OnDataRead(uint64 Offset, uint64 Length, const QByteArray& Data, bool bOk, void* Aux)
{
	if(!m_pPeer)
		return;

	CTorrent* pTorrent = m_pPeer->GetTorrent();
	CTorrentInfo* pTorrentInfo = pTorrent->GetInfo();

	m_pPeer->OnBytesWritten(Data.size());
	theCore->m_TorrentManager->OnBytesWritten(Data.size());

	uint64 uBegin = Offset;
	uint32 Index = 0;
	Index = uBegin / pTorrentInfo->GetPieceLength();
	Offset = uBegin % pTorrentInfo->GetPieceLength();

	if (!m_pPeer->IsActiveUpload())
		return;

	if(!bOk)
	{
		if(SupportsFAST())
			RejectRequest(Index, Offset, Length);
		return;
	}

	int iDebug = theCore->Cfg()->GetInt("Log/Level");
	if(iDebug == 3)
		LogLine(LOG_DEBUG, tr("QueueBlock"));

	//m_TempPieceSize -= Data.size();

	// Sends a block to the peer.
	CBuffer Packet; // (1 + 4 + 4 + Data.size());
	if(pTorrentInfo->IsMerkle())
	{
		if(m_Tr_hashpiece_Extension) // Tribbler Style
		{
			Packet.WriteValue<uint8>(eExtended, true);
			Packet.WriteValue<uint8>(m_Tr_hashpiece_Extension, true);
		}
		else // libTorrent Style
			Packet.WriteValue<uint8>(eExtension_Tr_hashpiece, true);
	}
	else
		Packet.WriteValue<uint8>(ePiece, true);

	Packet.WriteValue<uint32>(Index, true);
	Packet.WriteValue<uint32>(Offset, true);
	
	if(pTorrentInfo->IsMerkle())
	{
		if(Offset == 0)
		{
			CFileHashTree* pHashTree = qobject_cast<CFileHashTree*>(pTorrent->GetHash().data());
			if(!pHashTree)
			{
				ASSERT(0);
				return;
			}
			uint64 uFrom = Index * pTorrentInfo->GetPieceLength();
			uint64 uTo = Min(uFrom + pTorrentInfo->GetPieceLength(), pTorrentInfo->GetTotalLength());
			CScoped<CFileHashTree> pBranche = pHashTree->GetLeafs(uFrom, uTo);
			if(!pBranche)
			{
				if(SupportsFAST())
					RejectRequest(Index, Offset, Length);
				return;
			}

			SHashTreeDump Leafs(CFileHash::GetSize(HashTorrent));
			pBranche->Save(Leafs);

			QVariantList List;
			for(int Index = 0; Index < Leafs.Count(); Index++)
			{
				QVariantList Entry;
				Entry.append(Neo2Merkle(Leafs.ID(Index)));
				Entry.append(QByteArray((const char*)Leafs.Hash(Index), Leafs.Size()));
				List.append((QVariant)Entry);
			}

			QByteArray Data = Bencoder::encode(List).buffer();
			Packet.WriteValue<uint32>(Data.size(), true);
			Packet.WriteQData(Data);
		}
		else
			Packet.WriteValue<uint32>(0, true);
	}

	Packet.WriteData(Data.data(), Data.size());

	m_SentPieceSize += 4 + Packet.GetSize();
	m_Socket->QueuePacket(uBegin, Packet.ToByteArray());
}

uint64 CTorrentClient::GetSentPieceSize()
{
	if(!m_Socket)
		return 0;
	int QueuedSize = m_Socket->QueueSize();
	ASSERT(m_SentPieceSize >= QueuedSize);
	return m_SentPieceSize - QueuedSize;
}

//uint64 CTorrentClient::GetRequestedSize()
//{
//	if(!m_Socket)
//		return 0;
//	return m_TempPieceSize + m_Socket->QueueSize();
//}

void CTorrentClient::ProcessBlock(CBuffer& Packet)
{
	int iDebug = theCore->Cfg()->GetInt("Log/Level");
	if(iDebug == 3)
		LogLine(LOG_DEBUG, tr("ProcessBlock"));

	CTorrent* pTorrent = m_pPeer->GetTorrent();
	CTorrentInfo* pTorrentInfo = pTorrent->GetInfo();

	uint32 Index = Packet.ReadValue<uint32>(true);
	uint32 Offset = Packet.ReadValue<uint32>(true);

	if(pTorrentInfo->IsMerkle())
	{
		uint32 uListSize = Packet.ReadValue<uint32>(true);
		if(uListSize > 0)
		{
			CFileHashTree* pHashTree = qobject_cast<CFileHashTree*>(pTorrent->GetHash().data());
			if(!pHashTree)
			{
				ASSERT(0);
				return;
			}

			Bdecoder Decoder(Packet.ReadQData(uListSize));
			QVariantList List;
			Decoder.read(&List);
			if(Decoder.error())
			{
				LogLine(LOG_DEBUG | LOG_ERROR, tr("ProcessBlock Error - parser error"));
				return;
			}

			SHashTreeDump Leafs(CFileHash::GetSize(HashTorrent), List.size());
			foreach(const QVariant& vEntry, List)
			{
				QVariantList Entry = vEntry.toList();
				if(Entry.count() != 2)
					return;
				Leafs.Add(Merkle2Neo(Entry[0].toUInt()), (byte*)Entry[1].toByteArray().data());
			}

			CScoped<CFileHashTree> pBranche = new CFileHashTree(HashTorrent,pHashTree->GetTotalSize());
			pBranche->Load(Leafs);
			if(!pHashTree->AddLeafs(pBranche))
				return;
			theCore->m_Hashing->SaveHash(pHashTree);
		}
	}

	if(m_IncomingBlocks.removeAll(SRequestedBlock(Index, Offset, (uint32)Packet.GetSizeLeft())) == 0)
		return; // we havnt requested that block or the request was canceled

	if (Packet.GetSizeLeft() == 0) 
	{
        Disconnect("Invalid Piece packet");
        return;
    }

	// Kill the pending block timer.
	m_uRequestTimeOut = -1;

	size_t Length = Packet.GetSizeLeft();
	QByteArray Data = QByteArray((char*)Packet.GetData(0), (int)Length);

	m_pPeer->OnBytesReceived(Data.size());
	theCore->m_TorrentManager->OnBytesReceived(Data.size());

	// The peer sends a block.
	uint64 uBegin = (Index * pTorrentInfo->GetPieceLength()) + Offset;
	m_pPeer->RangeReceived(uBegin, uBegin + Length, Data);

	m_RecivedPieceSize += Length;

	// request more
	m_pPeer->RequestBlocks();
}

uint64 CTorrentClient::GetRecivedPieceSize()
{
	return m_RecivedPieceSize;
}

// Cancels a request for a block.
void CTorrentClient::CancelRequest(uint32 Index, uint32 Offset, uint32 Length)
{
	LogLine(LOG_DEBUG, tr("SendCancelRequest"));

	CBuffer Packet(1 + 4 + 4 + 4);
	Packet.WriteValue<uint8>(eCancel, true);
	Packet.WriteValue<uint32>(Index, true);
	Packet.WriteValue<uint32>(Offset, true);
	Packet.WriteValue<uint32>(Length, true);
    SendPacket(Packet);

    m_IncomingBlocks.removeAll(SRequestedBlock(Index, Offset, Length));
}

void CTorrentClient::ProcessCancelRequest(CBuffer& Packet)
{
	LogLine(LOG_DEBUG, tr("ProcessCancelRequest"));

	// The peer cancels a block request.
	uint32 Index = Packet.ReadValue<uint32>(true);
	uint32 Offset = Packet.ReadValue<uint32>(true);
	uint32 Length = Packet.ReadValue<uint32>(true);

	uint64 uBegin = (Index * m_pPeer->GetTorrent()->GetInfo()->GetPieceLength()) + Offset;
	m_Socket->CancelStream(uBegin);
}

void CTorrentClient::SendDHTPort()
{
	uint16 Port = theCore->m_TorrentManager->GetServer()->GetUTPPort();
	if(Port == 0)
		return;

	LogLine(LOG_DEBUG, tr("SendDHTPort"));

	CBuffer Packet(1 + 2);
	Packet.WriteValue<uint8>(eDHTPort, true);
	Packet.WriteValue<uint16>(Port, true);
    SendPacket(Packet);
}

// Sends a piece notification / a "have" message, informing the peer
// that we have just downloaded a new piece.
void CTorrentClient::OnHavePiece(int Index)
{
	LogLine(LOG_DEBUG, tr("SendHave"));

	CBuffer Packet(1 + 4);
	Packet.WriteValue<uint8>(eHave, true);
	Packet.WriteValue<uint32>(Index, true);
    SendPacket(Packet);
}

void CTorrentClient::SendHaveAll()
{
	LogLine(LOG_DEBUG, tr("SendHaveAll"));

	CBuffer Packet(1);
	Packet.WriteValue<uint8>(eHaveAll, true);
    SendPacket(Packet);
}

void CTorrentClient::SendHaveNone()
{
	LogLine(LOG_DEBUG, tr("SendHaveNone"));

	CBuffer Packet(1);
	Packet.WriteValue<uint8>(eHaveNone, true);
    SendPacket(Packet);
}

void CTorrentClient::SuggestPiece(uint32 Index)
{
	LogLine(LOG_DEBUG, tr("SendSuggestPiece"));

	CBuffer Packet(1 + 4);
	Packet.WriteValue<uint8>(eSuggestPiece, true);
	Packet.WriteValue<uint32>(Index, true);
    SendPacket(Packet);
	// BT-ToDo: use this function
}

void CTorrentClient::AllowedFast(uint32 Index)
{
	LogLine(LOG_DEBUG, tr("SendAllowedFast"));

	CBuffer Packet(1 + 4);
	Packet.WriteValue<uint8>(eAllowedFast, true);
	Packet.WriteValue<uint32>(Index, true);
    SendPacket(Packet);
	// BT-ToDo: use this function
}

void CTorrentClient::RejectRequest(uint32 Index, uint32 Offset, uint32 Length)
{
	LogLine(LOG_DEBUG, tr("SendRejectRequest"));

	CBuffer Packet(1 + 4 + 4 + 4);
	Packet.WriteValue<uint8>(eRejectRequest, true);
	Packet.WriteValue<uint32>(Index, true);
	Packet.WriteValue<uint32>(Offset, true);
	Packet.WriteValue<uint32>(Length, true);
    SendPacket(Packet);
}

void CTorrentClient::ProcessRejectRequest(CBuffer& Packet)
{
	LogLine(LOG_DEBUG, tr("ProcessRejectRequest"));

	uint32 Index = Packet.ReadValue<uint32>(true);
	uint32 Offset = Packet.ReadValue<uint32>(true);
	uint32 Length = Packet.ReadValue<uint32>(true);

	if(m_IncomingBlocks.removeAll(SRequestedBlock(Index, Offset, Length)) == 0)
		return; // was already canceled

	LogLine(LOG_DEBUG | LOG_ERROR, tr("Request Rejected %1 + %2 - %3").arg(Index).arg(Offset).arg(Length));

	uint64 uBegin = (Index * m_pPeer->GetTorrent()->GetInfo()->GetPieceLength()) + Offset;
	m_pPeer->ReleaseRange(uBegin, uBegin + Length);

	// BT-ToDo-Now: block this range for this client for this session XXXXXXX

	// request others
	m_pPeer->RequestBlocks();
}

void CTorrentClient::SendExtensions()
{
	LogLine(LOG_DEBUG, tr("SendExtensions"));

	QVariantMap m;
	m["ut_pex"] = eExtension_ut_pex;
	m["ut_metadata"] = eExtension_ut_metadata;
	m["ut_holepunch"] = eExtension_ut_holepunch;
	m["Tr_hashpiece"] = eExtension_Tr_hashpiece;

	QVariantMap Dict;
	Dict["m"] = m;
	Dict["p"] = theCore->m_TorrentManager->GetServer()->GetPort();
	Dict["v"] = theCore->m_TorrentManager->GetVersion();
	CAddress IP = m_Peer.GetIP();
	if(IP.Type() == CAddress::IPv6)
	{
		Dict["yourip"] = QByteArray((char*)IP.Data(), 16);

		CAddress MyAddress = theCore->m_TorrentManager->GetAddress(CAddress::IPv4);
		if(!MyAddress.IsNull())
		{
			uint32 uIP = _ntohl(MyAddress.ToIPv4());
			Dict["ipv4"] = QByteArray((char*)&uIP, 4);
		}
	}
	else if(IP.Type() == CAddress::IPv4)
	{
		uint32 uIP = _ntohl(IP.ToIPv4());
		Dict["yourip"] = QByteArray((char*)&uIP, 4);

		CAddress MyAddress = theCore->m_TorrentManager->GetAddress(CAddress::IPv6);
		if(!MyAddress.IsNull())
			Dict["ipv6"] = QByteArray((char*)MyAddress.Data(), 16);
	}
	Dict["reqq"] = theCore->Cfg()->GetInt("BitTorrent/RequestLimit");
	Dict["metadata_size"] = m_pPeer->GetTorrent()->GetInfo()->GetMetadataSize();
	//Dict["neo_kad"] = {port, id} // BT-ToDo // K-ToDo

	QByteArray Data = Bencoder::encode(Dict).buffer();

	CBuffer Packet(1 + 1 + Data.size());
	Packet.WriteValue<uint8>(eExtended, true);
	Packet.WriteValue<uint8>(eExtension, true);
	Packet.WriteData((byte*)Data.data(),Data.size());
    SendPacket(Packet);
}

void CTorrentClient::ProcessExtensions(QByteArray Data)
{
	LogLine(LOG_DEBUG, tr("ProcessExtensions"));

	Bdecoder Decoder(Data);
	QVariantMap Dict;
	Decoder.read(&Dict);
	if(Decoder.error())
	{
		LogLine(LOG_DEBUG | LOG_ERROR, tr("ProcessExtensions Error - parser error"));
		return;
	}

	QVariantMap Extensions = Dict["m"].toMap();
	m_ut_pex_Extension = Extensions["ut_pex"].toUInt();
	m_ut_metadata_Extension = Extensions["ut_metadata"].toUInt();
	m_ut_holepunch_Extension = Extensions["ut_holepunch"].toUInt();
	m_Tr_hashpiece_Extension = Extensions["Tr_hashpiece"].toUInt();

	if(m_ut_holepunch_Extension)
	{
		m_Peer.ConOpts.Fields.SupportsUTP = true;
		m_Peer.ConOpts.Fields.SupportsHolepunch = true;
	}

	//if(Dict.contains("neo_kad"))
	//	m_Version_NeoKad = Dict["neo_kad"].toUInt();
	
	if(Dict.contains("p"))
		m_Peer.Port = Dict["p"].toUInt();

	ASSERT(m_pPeer);
	if(Dict.contains("v"))
		m_Software = QString::fromUtf8(Dict["v"].toByteArray());
	
	if(Dict.contains("yourip"))
	{
		CAddress MyAddress;
		if(MyAddress.FromArray(Dict["yourip"].toByteArray()))
			theCore->m_TorrentManager->GetServer()->AddAddress(MyAddress);
	}

	CAddress IP = m_Peer.GetIP();
	if(Dict.contains("ipv4") && IP.Type() != CAddress::IPv4)
		m_Peer.AddIP(_ntohl(*((uint32*)Dict["ipv4"].toByteArray().data())));
	else if(Dict.contains("ipv6") && IP.Type() != CAddress::IPv6)
		m_Peer.AddIP((quint8*)Dict["ipv6"].toByteArray().data());

	if(Dict.contains("reqq"))
		m_RequestLimit = Dict["reqq"].toUInt();
	//uint64 metadata_size = Dict["metadata_size"].toULongLong();

	//if(m_Version_NeoKad && theCore->m_NeoManager->GetKad()->get // X-ToDo-X

	if(!theCore->m_TorrentManager->DispatchClient(this))
	{
		Disconnect();
		return;
	}

	m_pPeer->OnExtensionsRecived();

	if(m_pPeer->GetTorrent()->GetInfo()->IsEmpty() && SupportsMetadataExchange())
		TryRequestMetadata();
}

uint qHash(const STorrentPeer& Peer)
{
	uint val = Peer.Port;
	if(Peer.Prot == CAddress::IPv6)
	{
		uint* c = (uint*)Peer.IPv6.Data();
		for(int i=(16/sizeof(uint) - 1); i >= 0 ; --i)
			val |= c[i];
	}
	else
		val |= Peer.IPv4.ToIPv4();
	return val;
}

void CTorrentClient::SendPeerList()
{
	if(m_pPeer->GetTorrent()->GetInfo()->IsEmpty() || m_pPeer->GetTorrent()->GetInfo()->IsPrivate())
		return;

	LogLine(LOG_DEBUG, tr("SendPeerList"));

	m_NextMessagePEX = GetCurTick() + MIN2MS(10); // BT-ToDo: customize

	QSet<STorrentPeer> AddedList;
	QSet<STorrentPeer> DroppedList = m_PeerList; // what ever will be left will be whatw as dropped
	foreach(CTransfer* pTransfer, m_pPeer->GetFile()->GetTransfers())
	{
		if(pTransfer->IsChecked())
			continue;

		if(CTorrentPeer* pTorrentPeer = qobject_cast<CTorrentPeer*>(pTransfer))
		{
			if(!DroppedList.remove(pTorrentPeer->m_Peer))
				AddedList.insert(pTorrentPeer->m_Peer);
		}
	}
	
	CBuffer Added;
	CBuffer Added6;
	CBuffer Flags;
	CBuffer Flags6;
	int MaxAdded = 100; //BT-ToDo-Now: Customite
	foreach(const STorrentPeer& Peer, AddedList)
	{
		if(MaxAdded <= 0)
			break;
		MaxAdded--;

		m_PeerList.insert(Peer);
		if(Peer.HasV6())
		{
			Added6.WriteData(Peer.IPv6.Data(), 16);
			Added6.WriteValue<uint16>(Peer.Port, true);
			Flags6.WriteValue<uint8>(Peer.ConOpts.Bits, true);
		}
		else if(Peer.HasV4())
		{
			Added.WriteValue<uint32>(Peer.IPv4.ToIPv4(), true);
			Added.WriteValue<uint16>(Peer.Port, true);
			Flags.WriteValue<uint8>(Peer.ConOpts.Bits, true);
		}
	}

	CBuffer Dropped;
	CBuffer Dropped6;
	int MaxDropped = 100; //BT-ToDo-Now: Customite
	foreach(const STorrentPeer& Peer, DroppedList)
	{
		if(MaxDropped <= 0)
			break;
		MaxDropped--;

		m_PeerList.remove(Peer);
		if(Peer.HasV6())
		{
			Dropped6.WriteData(Peer.IPv6.Data(), 16);
			Dropped6.WriteValue<uint16>(Peer.Port, true);
		}
		else if(Peer.HasV4())
		{
			Dropped.WriteValue<uint32>(Peer.IPv4.ToIPv4(), true);
			Dropped.WriteValue<uint16>(Peer.Port, true);
		}
	}

	BencodedMap Dict;
	Dict.set("added", Added.ToByteArray());
	Dict.set("added6", Added6.ToByteArray());
	Dict.set("added.f", Flags.ToByteArray());
	Dict.set("added6.f", Flags6.ToByteArray());
	Dict.set("dropped.f",  Dropped.ToByteArray());
	Dict.set("dropped6.f",  Dropped6.ToByteArray());

	QByteArray Data = Bencoder::encode(Dict).buffer();

	CBuffer Packet(1 + 1 + Data.size());
	Packet.WriteValue<uint8>(eExtended, true);
	Packet.WriteValue<uint8>(m_ut_pex_Extension, true);
	Packet.WriteData((byte*)Data.data(),Data.size());
    SendPacket(Packet);
}

void CTorrentClient::ProcessPeerList(QByteArray Data)
{
	LogLine(LOG_DEBUG, tr("ProcessPeerList"));

	CTorrent* pTorrent = m_pPeer->GetTorrent();

	Bdecoder Decoder(Data);
	BencodedMap Dict;
	Decoder.read(&Dict);
	if(Decoder.error())
	{
		LogLine(LOG_DEBUG | LOG_ERROR, tr("ProcessPeerList Error - parser error"));
		return;
	}

	QByteArray AddedList = Dict.get("added").toByteArray();
	QByteArray AddedFlags = Dict.get("added.f").toByteArray();
	int AddedCount = AddedList.size() / 6;
	if (AddedList.size() % 6 == 0 && AddedFlags.size() == AddedCount)
	{
		CBuffer Added(AddedList.data(), AddedList.size(), true);
		CBuffer Flags(AddedFlags.data(), AddedFlags.size(), true);	
		for (int i = 0; i < AddedCount; ++i)
		{
			STorrentPeer Peer;
			Peer.SetIP(Added.ReadValue<uint32>(true));
			Peer.Port = Added.ReadValue<uint16>(true);
			Peer.ConOpts.Bits = Flags.ReadValue<uint8>(true);

			m_PeerList.insert(Peer);
			theCore->m_TorrentManager->AddToFile(pTorrent, Peer, ePEX);
		}
	}
	else
		LogLine(LOG_DEBUG | LOG_ERROR, tr("ProcessPeerList Error - added/added.f"));
	
	QByteArray AddedList6 = Dict.get("added6").toByteArray();
	QByteArray AddedFlags6 = Dict.get("added6.f").toByteArray();
	int AddedCount6 = AddedList6.size() / 18;
	if (AddedList6.size() % 18 == 0 && AddedFlags6.size() == AddedCount6)
	{
		CBuffer Added(AddedList6.data(), AddedList6.size(), true);
		CBuffer Flags(AddedFlags6.data(), AddedFlags6.size(), true);	
		for (int i = 0; i < AddedCount6; ++i)
		{
			STorrentPeer Peer;
			Peer.SetIP(Added.ReadData(16));
			Peer.Port = Added.ReadValue<uint16>(true);
			Peer.ConOpts.Bits = Flags.ReadValue<uint8>(true);

			m_PeerList.insert(Peer);
			theCore->m_TorrentManager->AddToFile(pTorrent, Peer, ePEX);
		}
	}
	else
		LogLine(LOG_DEBUG | LOG_ERROR, tr("ProcessPeerList Error - added6/added6.f"));

	QByteArray DroppedList = Dict.get("dropped").toByteArray();
	int DroppedCount = DroppedList.size() / 6;
	if (DroppedList.size() % 6 == 0)
	{
		CBuffer Dropped(DroppedList.data(), DroppedList.size(), true);
		for (int i = 0; i < DroppedCount; ++i)
		{
			STorrentPeer Peer;
			Peer.SetIP(Dropped.ReadValue<uint32>(true));
			Peer.Port = Dropped.ReadValue<uint16>(true);
			m_PeerList.remove(Peer);
		}
	}
	else
		LogLine(LOG_DEBUG | LOG_ERROR, tr("ProcessPeerList Error - dropped"));

	QByteArray DroppedList6 = Dict.get("dropped6").toByteArray();;
	int DroppedCount6 = DroppedList6.size() / 18;
	if (DroppedList6.size() % 18 == 0)
	{
		CBuffer Dropped(DroppedList6.data(), DroppedList6.size(), true);
		for (int i = 0; i < DroppedCount6; ++i)
		{
			STorrentPeer Peer;
			Peer.SetIP(Dropped.ReadData(16));
			Peer.Port = Dropped.ReadValue<uint16>(true);
			m_PeerList.remove(Peer);
		}
	}
	else
		LogLine(LOG_DEBUG | LOG_ERROR, tr("ProcessPeerList Error - dropped6"));
}

void CTorrentClient::TryRequestMetadata()
{
	uint32 Index = m_pPeer->GetTorrent()->NextMetadataBlock(m_Peer.ID.ToArray(true));
	if(Index != -1)
		RequestMetadata(Index);
}

void CTorrentClient::RequestMetadata(uint32 Index)
{
	LogLine(LOG_DEBUG, tr("SendRequestMetadata"));

	QVariantMap Dict;
	Dict["msg_type"] = eRequestMetadata;
	Dict["piece"] = Index;

	QByteArray Data = Bencoder::encode(Dict).buffer();

	CBuffer Packet(1 + 1 + Data.size());
	Packet.WriteValue<uint8>(eExtended, true);
	Packet.WriteValue<uint8>(m_ut_metadata_Extension, true);
	Packet.WriteData((byte*)Data.data(),Data.size());
    SendPacket(Packet);
}

void CTorrentClient::SendMetadata(uint32 Index)
{
	LogLine(LOG_DEBUG, tr("SendMetadata"));

	CTorrentInfo* pTorrentInfo = m_pPeer->GetTorrent()->GetInfo();

	QVariantMap Dict;
	Dict["msg_type"] = pTorrentInfo->IsEmpty() ? eMetadataReject : eMetadataPayload;
	Dict["piece"] = Index;
	Dict["total_size"] = pTorrentInfo->GetMetadataSize();

	QByteArray Data = Bencoder::encode(Dict).buffer();
	
	if(!pTorrentInfo->IsEmpty())
		Data.append(pTorrentInfo->GetMetadataBlock(Index));

	CBuffer Packet(1 + 1 + Data.size());
	Packet.WriteValue<uint8>(eExtended, true);
	Packet.WriteValue<uint8>(m_ut_metadata_Extension, true);
	Packet.WriteData((byte*)Data.data(),Data.size());
    SendPacket(Packet);
}

void CTorrentClient::ProcessMetadata(QByteArray Data)
{
	LogLine(LOG_DEBUG, tr("ProcessMetadata"));

	Bdecoder Decoder(Data);
	BencodedMap Dict;
	Decoder.read(&Dict);
	if(Decoder.error())
	{
		LogLine(LOG_DEBUG | LOG_ERROR, tr("ProcessMetadata Error - parser error"));
		return;
	}

	uint32 Index = Dict.get("piece").toUInt();
	switch(Dict.get("msg_type").toUInt())
	{
		case eRequestMetadata:
			SendMetadata(Index);
			break;
		case eMetadataPayload:
		{
			uint64 uTotalSize = Dict.get("total_size").toULongLong();
			QByteArray Paylaod = Data.mid(Decoder.pos());
			
			m_pPeer->GetTorrent()->AddMetadataBlock(Index, Paylaod, uTotalSize, m_Peer.ID.ToArray(true));

			TryRequestMetadata();
			break;
		}
		case eMetadataReject:
		{
			m_pPeer->GetTorrent()->ResetMetadataBlock(Index, m_Peer.ID.ToArray(true));
			break;
		}
	}
}

void CTorrentClient::SendHolepunch(int Type, const CAddress& Address, quint16 Port, int Error)
{	
	ASSERT(m_ut_holepunch_Extension);

	LogLine(LOG_DEBUG, tr("SendHolepunch"));

	CBuffer Packet;
	Packet.WriteValue<uint8>(eExtended, true);
	Packet.WriteValue<uint8>(m_ut_holepunch_Extension, true);

	Packet.WriteValue<uint8>(Type, true);
	if(Address.Type() != CAddress::IPv4)
	{
		Packet.WriteValue<uint8>(0, true);
		Packet.WriteValue<uint32>(Address.ToIPv4(), true);
	}
	else if(Address.Type() != CAddress::IPv6)
	{
		Packet.WriteValue<uint8>(1, true);
		Packet.WriteData(Address.Data(), 16);
	}
	else{
		ASSERT(0);
	}
	Packet.WriteValue<uint16>(Port, true);
	if(Type == eHpFailed)
		Packet.WriteValue<uint32>(Error, true);

    SendPacket(Packet);
}

void CTorrentClient::ProcessHolepunch(CBuffer& Packet)
{
	if(!m_ut_holepunch_Extension || !theCore->Cfg()->GetBool("BitTorrent/uTP"))
		return;

	LogLine(LOG_DEBUG, tr("ProcessHolepunch"));

	int Type = Packet.ReadValue<uint8>(true);
	int Prot = Packet.ReadValue<uint8>(true);
	CAddress Address;
	if(Prot == 0)
		Address = CAddress(Packet.ReadValue<uint32>(true));
	else if(Prot == 1)
		Address = CAddress((byte*)Packet.ReadData(16));
	else
		return;
	quint16 Port = Packet.ReadValue<uint16>(true);

	switch(Type)
	{
		case eHpRendezvous: // client wants us to Rendezvous him with an other one
		{
			CTorrentClient* pClient = theCore->m_TorrentManager->FindClient(Address, Port);
			if(!pClient)
				SendHolepunch(eHpFailed, Address, Port, eHpNotConnected);
			else if(!pClient->SupportsHolepunch())
				SendHolepunch(eHpFailed, Address, Port, eHpNoSupport);
			else if(pClient == this)
				SendHolepunch(eHpFailed, Address, Port, eHpNoSelf);
			else
			{
				SendHolepunch(eHpConnect, Address, Port);
				pClient->SendHolepunch(eHpConnect, m_Peer.GetIP(), m_Peer.Port);
			}
			break;
		}
		case eHpConnect: // we are Rendezvous'ing lets connect
		{
			CTorrentClient* pClient = theCore->m_TorrentManager->FindClient(Address, Port);
			if(!pClient)
			{
				ASSERT(!Address.IsNull());
				CTorrentSocket* pSocket = (CTorrentSocket*)theCore->m_TorrentManager->GetServer()->AllocUTPSocket();
				pClient = new CTorrentClient(pSocket, theCore->m_TorrentManager);
				pClient->m_Peer.SetIP(Address);
				pClient->m_Peer.Port = Port;
				theCore->m_TorrentManager->AddConnection(pClient, true);

				pClient->Connect();
			}
			else // if the client is already known setup new socket
			{
				if(pClient->m_Socket->GetState() == CStreamSocket::eNotConnected)
					pClient->Connect();
			}
			break;
		}
		case eHpFailed:
		{
			int Error = Packet.ReadValue<uint32>(true);
			CTorrentClient* pClient = theCore->m_TorrentManager->FindClient(Address, Port);
			if(pClient)
			{
				if(++pClient->m_HolepunchTry < theCore->Cfg()->GetInt("BitTorrent/MaxRendezvous"))
					theCore->m_TorrentManager->MakeRendezvous(pClient);
			}
			break;
		}
	}
}

void CTorrentClient::ProcessPacket(CBuffer& Packet)
{
	uint8 uOpcode = Packet.ReadValue<uint8>(true);
	switch (uOpcode) 
	{
		case eChoke:
			LogLine(LOG_DEBUG, tr("ProcessChokePeer"));
			// We have been choked.
			m_IncomingBlocks.clear();
			m_uRequestTimeOut = -1;
			m_Socket->SetDownload(false);
			m_pPeer->OnChoked();
			break;
		case eUnchoke:
			LogLine(LOG_DEBUG, tr("ProcessUnchokedPeer"));
			// We have been unchoked.
			m_RecivedPieceSize = 0;
			m_Socket->SetDownload(true);
			m_pPeer->OnUnchoked();
			break;
		case eInterested:
			LogLine(LOG_DEBUG, tr("ProcessInterested"));
			// The peer is interested in downloading.
			m_pPeer->OnInterested();
			break;
		case eNotInterested:
			LogLine(LOG_DEBUG, tr("ProcessNotInterested"));
			// The peer is not interested in downloading.
			m_pPeer->OnNotInterested();
			break;
		case eBitField:
			ProcessPieceList(Packet);
			break;
		case eRequest: 
			ProcessBlockRequest(Packet);
			break;
		case ePiece: 
		case eExtension_Tr_hashpiece:
			ProcessBlock(Packet);
			break;
		case eCancel: 
			ProcessCancelRequest(Packet);
			break;
		case eDHTPort:
		{
			LogLine(LOG_DEBUG, tr("ProcessDHTPort"));
			uint16 Port = Packet.ReadValue<uint16>(true);
			if(m_Peer.HasV4())
				theCore->m_TorrentManager->AddDHTNode(m_Peer.IPv4, Port);
			if(m_Peer.HasV6())
				theCore->m_TorrentManager->AddDHTNode(m_Peer.IPv6, Port);
			break;
		}
		case eHave: 
			LogLine(LOG_DEBUG, tr("ProcessHave"));
			// The peer notifyes us that is has completed a new peace
			m_pPeer->OnPieceAvailable(Packet.ReadValue<uint32>(true));
			break;
		case eHaveAll:
			LogLine(LOG_DEBUG, tr("ProcessHaveAll"));
			m_pPeer->OnHaveAll();
			break;
		case eHaveNone:
			LogLine(LOG_DEBUG, tr("ProcessHaveNone"));
			m_pPeer->OnHaveNone();
			break;
		case eSuggestPiece:
			LogLine(LOG_DEBUG, tr("ProcessSuggestPiece"));
			m_pPeer->OnPieceSuggested(Packet.ReadValue<uint32>(true));
			break;
		case eAllowedFast:
			LogLine(LOG_DEBUG, tr("ProcessAllowedFast"));
			//m_pPeer->OnPieceAllowed(Packet.ReadValue<uint32>(true));
			//BT-ToDo-Now: use it
			break;
		case eRejectRequest:
			ProcessRejectRequest(Packet);
			break;
		case eExtended: 
		{
			uint8 uExtension = Packet.ReadValue<uint8>(true);
			switch(uExtension)
			{
				case eExtension:
					ProcessExtensions(QByteArray::fromRawData((char*)Packet.ReadData(0), (int)Packet.GetSizeLeft()));
					break;
				case eExtension_ut_pex:
					if(m_pPeer->GetFile()->GetStats()->GetTransferCount(eBitTorrent) > theCore->Cfg()->GetInt("BitTorrent/MaxPeers") * 10)
						break;
					ProcessPeerList(QByteArray::fromRawData((char*)Packet.ReadData(0), (int)Packet.GetSizeLeft()));
					break;
				case eExtension_ut_metadata:
					ProcessMetadata(QByteArray::fromRawData((char*)Packet.ReadData(0), (int)Packet.GetSizeLeft()));
					break;
				case eExtension_ut_holepunch:
					ProcessHolepunch(Packet);
					break;
				case eExtension_Tr_hashpiece:
					ProcessBlock(Packet);
					break;
				default:
					LogLine(LOG_DEBUG | LOG_WARNING, tr("Recived unknwon extended torrent packet %1").arg(uExtension));
			}
			break;
		}
		default:
			LogLine(LOG_DEBUG | LOG_WARNING, tr("Recived unknwon torrent packet %1").arg(uOpcode));
	}
}

bool CTorrentClient::Connect()
{
	m_SentHandShake = false;

	m_uConnectTimeOut = GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("BitTorrent/ConnectTimeout"));

	if(!m_Peer.SelectIP())
	{
		m_Error = "Unavailable TransportLayer";
		return false;
	}

	if(theCore->m_TorrentManager->SupportsCryptLayer() && m_Peer.ConOpts.Fields.SupportsEncryption) // this flag is set to true is real value is unknown, on error we will retry unencrypted
	{
		SendHandShake(); // prepare handshake to be send as atomic transaction directly after crypto handshake
		m_Socket->InitCrypto(m_InfoHash);
	}
	else if(theCore->m_TorrentManager->RequiresCryptLayer())
	{
		m_Error = "Encryption Not Available";
		return false;
	}

	ASSERT(m_Socket->GetState() == CStreamSocket::eNotConnected);

	connect(m_Socket, SIGNAL(Connected()), this, SLOT(OnConnected()));
				
	connect(m_Socket, SIGNAL(ReceivedHandshake(QByteArray)), this, SLOT(OnReceivedHandshake(QByteArray)), Qt::QueuedConnection);
	connect(m_Socket, SIGNAL(ReceivedPacket(QByteArray)), this, SLOT(OnReceivedPacket(QByteArray)), Qt::QueuedConnection);

	connect(m_Socket, SIGNAL(Disconnected(int)), this, SLOT(OnDisconnected(int)));

	m_Socket->ConnectToHost(m_Peer.GetIP(), m_Peer.Port);
	return true;
}

void CTorrentClient::OnConnected()
{
	LogLine(LOG_DEBUG, tr("connected socket (outgoing)"));
	m_Peer.ConOpts.Fields.SupportsEncryption = m_Socket->SupportsEncryption();
	m_Peer.ConOpts.Fields.SupportsUTP = (m_Socket->GetSocketType() == SOCK_UTP);
	if(m_SentHandShake == false)
		SendHandShake();
}

void CTorrentClient::Disconnect(const QString& Error)
{
	if(m_pPeer && m_pPeer->IsActiveUpload())
	{
		m_pPeer->LogLine(LOG_DEBUG | LOG_INFO, tr("Stopping upload of %1 to %2 after %3 kb disconnecting due to timeout")
			.arg(m_pPeer->GetFile()->GetFileName()).arg(m_pPeer->GetDisplayUrl()).arg((double)m_SentPieceSize/1024.0));
		m_pPeer->StopUpload();
	}

	m_uConnectTimeOut = -1;
	m_uIdleTimeOut = -1;
	m_uRequestTimeOut = -1;

	if(!Error.isEmpty()) 
		m_Error = Error; 

	m_Socket->DisconnectFromHost();
}

void CTorrentClient::OnDisconnected(int Error)
{
	CStreamSocket::EState State = m_Socket->GetState();
	bool bConnectionFailed = (State == CStreamSocket::eConnectFailed);

	if(m_Error.isEmpty() && m_ReceivedHandShake == false && m_uConnectTimeOut != -1) // if we initialized the disconnect dont count that as error
	{
		// If the socket was a UTP socket and did not connet at all
		bool bUTPFailed = m_HolepunchTry == 0 && (m_Socket->GetSocketType() == SOCK_UTP) && State == CStreamSocket::eConnectFailed;
		// If the socket connected but did not finish crypto handshake
		bool bCryptoFailed = (State == CStreamSocket::eHalfConnected) && m_Socket->CryptoInProgress();
		// If the connection was ok the state is eDisconnected
		// If a Holepunch atempt was planed but never executed the state is not connected

		int iRetry = 0;
		if(bUTPFailed)
		{
			m_Peer.ConOpts.Fields.SupportsUTP = false;
			iRetry = 1;
		}
		else if(bCryptoFailed)
		{
			m_Peer.ConOpts.Fields.SupportsEncryption = false;
			if(!theCore->m_TorrentManager->RequiresCryptLayer())
				iRetry = 1;
		}
		else // plain raw tcp atempt failed
		{
			if(m_Peer.ConOpts.Fields.SupportsHolepunch && m_HolepunchTry == 0)
				iRetry = 2;
		}

		if(iRetry && m_pPeer)
		{
			CFile* pFile = m_pPeer->GetFile();

			//m_Socket->RemoveUpLimit(m_UpLimit);
			//m_Socket->RemoveDownLimit(m_DownLimit);

			//m_Socket->RemoveUpLimit(pFile->GetUpLimit());
			//m_Socket->RemoveDownLimit(pFile->GetDownLimit());

			//m_Socket->disconnect(this);
			disconnect(m_Socket, 0, 0, 0);
			theCore->m_TorrentManager->GetServer()->FreeSocket(m_Socket);
			if(m_Peer.ConOpts.Fields.SupportsUTP && theCore->Cfg()->GetBool("BitTorrent/uTP"))
				m_Socket = (CTorrentSocket*)theCore->m_TorrentManager->GetServer()->AllocUTPSocket();
			else
				m_Socket = (CTorrentSocket*)theCore->m_TorrentManager->GetServer()->AllocSocket();

			m_Socket->AddUpLimit(m_UpLimit);
			m_Socket->AddDownLimit(m_DownLimit);

			m_Socket->AddUpLimit(pFile->GetUpLimit());
			m_Socket->AddDownLimit(pFile->GetDownLimit());

			m_uConnectTimeOut = GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("BitTorrent/ConnectTimeout"));
			if(iRetry == 1)
				Connect();
			else if(iRetry == 2) // try a Nat hole punch
			{
				m_HolepunchTry = 1;
				if(m_Peer.SelectIP())
					theCore->m_TorrentManager->MakeRendezvous(this);
				else
					m_Error = "Unavailable TransportLayer";
			}
			return;
		}
		else
		{
			switch(Error)
			{
				case SOCK_ERR_NETWORK:		m_Error = "Sock Error"; break;
				case SOCK_ERR_REFUSED:		m_Error = "Con Refused"; break;
				case SOCK_ERR_RESET:		m_Error = "Con Reset"; break;
				case SOCK_ERR_CRYPTO:		m_Error = "Crypto Error"; break;
				case SOCK_ERR_OVERSIZED:	m_Error = "Sock ToBig"; break;
				case SOCK_ERR_NONE:			m_Error = "Sock Unknown"; break;
			}
		}
	}

	m_uConnectTimeOut = -1;

	if(m_pPeer && m_pPeer->IsActiveUpload())
	{
		m_pPeer->LogLine(LOG_DEBUG | LOG_INFO, tr("Stopping upload of %1 to %2 after %3 kb socket disconnected, reason: %4")
			.arg(m_pPeer->GetFile()->GetFileName()).arg(m_pPeer->GetDisplayUrl()).arg((double)m_SentPieceSize/1024.0).arg(CStreamSocket::GetErrorStr(Error)));
		m_pPeer->StopUpload();
	}

	if(HasError())
		LogLine(LOG_DEBUG, tr("disconnected due to an error: %1").arg(m_Error));
		
	if(bConnectionFailed && !m_ReceivedHandShake)
	{
		LogLine(LOG_DEBUG, tr("connection to torrent peer failed - dead"));
		theCore->m_PeerWatch->PeerFailed(m_Peer.GetIP(), m_Peer.Port);
	}
}

void CTorrentClient::SendPacket(CBuffer& Packet)
{
	ASSERT(m_SentHandShake);
	m_Socket->SendPacket(Packet.ToByteArray());
}

void CTorrentClient::OnReceivedHandshake(QByteArray Handshake)
{
	// Make sure we drop all unencrypted connections if we require encryption
	if(!m_Socket || (theCore->m_TorrentManager->RequiresCryptLayer() && !m_Socket->IsEncrypted()))
	{
		Disconnect("Encryption needed but not available");
		return;
	}

	m_uConnectTimeOut = -1;
	m_uIdleTimeOut = GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("BitTorrent/IdleTimeout"));

	try
	{
		CBuffer Buffer(Handshake, true);
		if(!ProcessHandShake(Buffer))
			return;
	}
	catch(const CException& Exception)
	{
		LogLine(Exception.GetFlag(), tr("recived malformated handshake; %1").arg(QString::fromStdWString(Exception.GetLine())));
		return;
	}

	theCore->m_PeerWatch->PeerConnected(m_Peer.GetIP(), m_Peer.Port);

	if(!m_SentHandShake || !m_Supports_Extension) // incomming connection, or outgoing not supporting extended info
	{
		ASSERT(m_SentHandShake || m_Peer.Port == 0);
		if(!theCore->m_TorrentManager->DispatchClient(this))
		{
			Disconnect();
			return;
		}
	}

	if(!m_SentHandShake)
		theCore->m_TorrentManager->FW()->Incoming(m_Socket->GetAddress().Type(), m_Socket->GetSocketType() == SOCK_UTP ? CFWWatch::eUTP : CFWWatch::eTCP);
	else
		theCore->m_TorrentManager->FW()->Outgoing(m_Socket->GetAddress().Type(), m_Socket->GetSocketType() == SOCK_UTP ? CFWWatch::eUTP : CFWWatch::eTCP);

	// is it an Incoming connection
	if (!m_SentHandShake)
	{
		// Send handshake
		SendHandShake();
	}

	if(!m_pPeer) // peer can be NULL if the client was dropped in the mean time
		return;

	// this must be sent immediately after the handshake.
	m_pPeer->OnHandShakeRecived();

	m_Software = IdentifySoftware();

	if(m_Supports_DHT)
		SendDHTPort();

	if(m_Supports_Extension)
		SendExtensions();

	// Initialize keep-alive timer
	m_uNextKeepAlive = GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("BitTorrent/KeepAlive"));
}

void CTorrentClient::OnReceivedPacket(QByteArray Packet)
{
	if(m_pPeer == NULL)
		return;

	m_uIdleTimeOut = GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("BitTorrent/IdleTimeout"));

	if(Packet.isEmpty())
		return; // KeepAlive

	try
	{
		CBuffer Buffer(Packet, true);
		ProcessPacket(Buffer);
	}
	catch(const CException& Exception)
	{
		LogLine(Exception.GetFlag(), tr("recived malformated packet; %1").arg(QString::fromStdWString(Exception.GetLine())));
	}
}

void CTorrentClient::AddLogLine(time_t uStamp, uint32 uFlag, const CLogMsg& Line)
{
	Line.AddMark((uint64)m_pPeer);

	int iDebug = theCore->Cfg()->GetInt("Log/Level");
	if(iDebug >= 2 || (iDebug == 1 && (uFlag & LOG_DEBUG) == 0))
	{
		Line.Prefix(QString("TorrentClient - %1:%2").arg(m_Peer.GetIP().ToQString()).arg(m_Peer.Port));
		QObjectEx::AddLogLine(uStamp, uFlag | LOG_MOD('t'), Line);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// ID code from libtorrent (BSD)

#include <cctype>

int decode_digit(char c)
{
	if (std::isdigit(c)) return c - '0';
	return unsigned(c) - 'A' + 10;
}

struct fingerprint
{
	fingerprint(const char* id_string, int major, int minor, int revision, int tag)
		: major_version(major)
		, minor_version(minor)
		, revision_version(revision)
		, tag_version(tag)
	{
		ASSERT(id_string);
		ASSERT(major >= 0);
		ASSERT(minor >= 0);
		ASSERT(revision >= 0);
		ASSERT(tag >= 0);
		ASSERT(std::strlen(id_string) == 2);
		name[0] = id_string[0];
		name[1] = id_string[1];
	}

	std::string to_string() const
	{
		std::stringstream s;
		s << "-" << name[0] << name[1]
			<< version_to_char(major_version)
			<< version_to_char(minor_version)
			<< version_to_char(revision_version)
			<< version_to_char(tag_version) << "-";
		return s.str();
	}

	char name[2];
	int major_version;
	int minor_version;
	int revision_version;
	int tag_version;

private:

	char version_to_char(int v) const
	{
		if (v >= 0 && v < 10) return '0' + v;
		else if (v >= 10) return 'A' + (v - 10);
		ASSERT(false);
		return '0';
	}

};

struct map_entry
{
	char const* id;
	char const* name;
};

// only support BitTorrentSpecification
// must be ordered alphabetically
map_entry name_map[] =
{
	{"A",  "ABC"}
	, {"AG",  "Ares"}
	, {"AR", "Arctic Torrent"}
	, {"AV", "Avicora"}
	, {"AX", "BitPump"}
	, {"AZ", "Azureus"}
	, {"A~",  "Ares"}
	, {"BB", "BitBuddy"}
	, {"BC", "BitComet"}
	, {"BF", "Bitflu"}
	, {"BG", "BTG"}
	, {"BR", "BitRocket"}
	, {"BS", "BTSlave"}
	, {"BX", "BittorrentX"}
	, {"CD", "Enhanced CTorrent"}
	, {"CT", "CTorrent"}
	, {"DE", "Deluge Torrent"}
	, {"EB", "EBit"}
	, {"ES", "electric sheep"}
	, {"HL", "Halite"}
	, {"HN", "Hydranode"}
	, {"KT", "KTorrent"}
	, {"LC", "LeechCraft"}
	, {"LK", "Linkage"}
	, {"LP", "lphant"}
	, {"LT", "libtorrent"}
	, {"M",  "Mainline"}
	, {"ML", "MLDonkey"}
	, {"MO", "Mono Torrent"}
	, {"MP", "MooPolice"}
	, {"MR", "Miro"}
	, {"MT", "Moonlight Torrent"}
	, {"NL", "NeoLoader"}
	, {"O",  "Osprey Permaseed"}
	, {"PD",  "Pando"}
	, {"Q", "BTQueue"}
	, {"QT", "Qt 4"}
	, {"R",  "Tribler"}
	, {"S",  "Shadow"}
	, {"SB", "Swiftbit"}
	, {"SN", "ShareNet"}
	, {"SS", "SwarmScope"}
	, {"ST", "SymTorrent"}
	, {"SZ", "Shareaza"}
	, {"S~",  "Shareaza (beta)"}
	, {"T",  "BitTornado"}
	, {"TN", "Torrent.NET"}
	, {"TR", "Transmission"}
	, {"TS", "TorrentStorm"}
	, {"TT", "TuoTu"}
	, {"U",  "UPnP"}
	, {"UL", "uLeecher"}
	, {"UT", "uTorrent"}
	, {"XL", "Xunlei"}
	, {"XT", "XanTorrent"}
	, {"XX", "Xtorrent"}
	, {"ZT", "ZipTorrent"}
	, {"lt", "rTorrent"}
	, {"pX", "pHoeniX"}
	, {"qB", "qBittorrent"}
	, {"st", "SharkTorrent"}
};

struct generic_map_entry
{
	int offset;
	char const* id;
	char const* name;
};
// non-standard names
generic_map_entry generic_mappings[] =
{
	{0, "Deadman Walking-", "Deadman"}
	, {5, "Azureus", "Azureus 2.0.3.2"}
	, {0, "DansClient", "XanTorrent"}
	, {4, "btfans", "SimpleBT"}
	, {0, "PRC.P---", "Bittorrent Plus! II"}
	, {0, "P87.P---", "Bittorrent Plus!"}
	, {0, "S587Plus", "Bittorrent Plus!"}
	, {0, "martini", "Martini Man"}
	, {0, "Plus---", "Bittorrent Plus"}
	, {0, "turbobt", "TurboBT"}
	, {0, "a00---0", "Swarmy"}
	, {0, "a02---0", "Swarmy"}
	, {0, "T00---0", "Teeweety"}
	, {0, "BTDWV-", "Deadman Walking"}
	, {2, "BS", "BitSpirit"}
	, {0, "Pando-", "Pando"}
	, {0, "LIME", "LimeWire"}
	, {0, "btuga", "BTugaXP"}
	, {0, "oernu", "BTugaXP"}
	, {0, "Mbrst", "Burst!"}
	, {0, "PEERAPP", "PeerApp"}
	, {0, "Plus", "Plus!"}
	, {0, "-Qt-", "Qt"}
	, {0, "exbc", "BitComet"}
	, {0, "DNA", "BitTorrent DNA"}
	, {0, "-G3", "G3 Torrent"}
	, {0, "-FG", "FlashGet"}
	, {0, "-ML", "MLdonkey"}
	, {0, "XBT", "XBT"}
	, {0, "OP", "Opera"}
	, {2, "RS", "Rufus"}
	, {0, "AZ2500BT", "BitTyrant"}
};

bool compare_id(map_entry const& lhs, map_entry const& rhs)
{
	return lhs.id[0] < rhs.id[0]
		|| ((lhs.id[0] == rhs.id[0]) && (lhs.id[1] < rhs.id[1]));
}

std::string lookup(fingerprint const& f)
{
	std::stringstream identity;

	const int size = sizeof(name_map)/sizeof(name_map[0]);
	map_entry tmp = {f.name, ""};
	map_entry* i =
		std::lower_bound(name_map, name_map + size
			, tmp, &compare_id);

	if (i < name_map + size && f.name[0] == i->id[0] && f.name[1] == i->id[1])
		identity << i->name;
	else
	{
		identity << f.name[0];
		if (f.name[1] != 0) identity << f.name[1];
	}

	identity << " " << (int)f.major_version
		<< "." << (int)f.minor_version
		<< "." << (int)f.revision_version;

	if (f.tag_version != 0)
		identity << "." << (int)f.tag_version;

	return identity.str();
}

std::string parse_az_style(const QByteArray& id)
{
	if (id[0] != '-' || !isprint(id[1]) || (id[2] < '0')
		|| (id[3] < '0') || (id[4] < '0')
		|| (id[5] < '0') || (id[6] < '0')
		|| id[7] != '-')
		return "";
	
	fingerprint ret("..", 0, 0, 0, 0);

	ret.name[0] = id[1];
	ret.name[1] = id[2];
	ret.major_version = decode_digit(id[3]);
	ret.minor_version = decode_digit(id[4]);
	ret.revision_version = decode_digit(id[5]);
	ret.tag_version = decode_digit(id[6]);

	return lookup(ret);
}

std::string parse_shadow_style(const QByteArray& id)
{
	if (id[0] > 0 && std::isalnum(id[0]))
		return "";
	
	fingerprint ret("..", 0, 0, 0, 0);

	if (id.mid(4,2) == "--")
	{
		if ((id[1] < '0') || (id[2] < '0') || (id[3] < '0'))
			return "";
		ret.major_version = decode_digit(id[1]);
		ret.minor_version = decode_digit(id[2]);
		ret.revision_version = decode_digit(id[3]);
	}
	else
	{
		if (id[8] != 0 || id[1] > 127 || id[2] > 127 || id[3] > 127)
			return "";
		ret.major_version = id[1];
		ret.minor_version = id[2];
		ret.revision_version = id[3];
	}

	ret.name[0] = id[0];
	ret.name[1] = 0;

	ret.tag_version = 0;
	return lookup(ret);
}

std::string parse_mainline_style(const QByteArray& id)
{
	char ids[21];
	std::copy(id.begin(), id.end(), ids);
	ids[20] = 0;
	fingerprint ret("..", 0, 0, 0, 0);
	ret.name[1] = 0;
	ret.tag_version = 0;
	if (sscanf(ids, "%c%d-%d-%d--", &ret.name[0], &ret.major_version, &ret.minor_version
		, &ret.revision_version) != 4
		|| !isprint(ret.name[0]))
		return "";

	return lookup(ret);
}

QString CTorrentClient::IdentifySoftware()
{
	QByteArray id = QByteArray::fromRawData((char*)m_Peer.ID.Data, 20);

	if(id == QByteArray(20, 0))
		return "Unknown";

	// ----------------------
	// non standard encodings
	// ----------------------

	int num_generic_mappings = sizeof(generic_mappings) / sizeof(generic_mappings[0]);

	for (int i = 0; i < num_generic_mappings; ++i)
	{
		generic_map_entry const& e = generic_mappings[i];
		if (id.indexOf(QByteArray(e.id), e.offset) == 0)
			return e.name;
	}

	if (id.indexOf("-BOW") == 0 && id[7] == '-')
		return "Bits on Wheels " + id.mid(4,3);

	if (id.indexOf("eX") == 0)
		return "eXeem ('" + id.mid(2,12) + "')"; 

	if (id.mid(0,13) == "\0\0\0\0\0\0\0\0\0\0\0\0\x97")
		return "Experimental 3.2.1b2";

	if (id.mid(0,13) == "\0\0\0\0\0\0\0\0\0\0\0\0\0")
		return "Experimental 3.1";

	// look for azureus style id
	std::string ret1 = parse_az_style(id);
	if(!ret1.empty())
		return QString::fromStdString(ret1);

	// look for shadow style id
	std::string ret2 = parse_shadow_style(id);
	if(!ret2.empty())
		return QString::fromStdString(ret2);

	// look for mainline style id
	std::string ret3 = parse_mainline_style(id);
	if(!ret3.empty())
		return QString::fromStdString(ret3);

	if (id.mid(0,12) == "\0\0\0\0\0\0\0\0\0\0\0\0")
		return "Generic";

	std::string unknown("Unknown [");
	for(int i=0; i < id.size(); i++)
	{
		char c = id[i];
		unknown += (c >= 32 && c < 127) ? c : '.';
	}
	unknown += "]";
	return QString::fromStdString(unknown);
}
