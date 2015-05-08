#pragma once
//#include "GlobalHeader.h"

#include "../P2PClient.h"
#include "../../../Framework/Buffer.h"
#include "TorrentSocket.h"
#include "TorrentPeer.h"

class CTorrentClient : public CP2PClient
{
    Q_OBJECT

public:
	CTorrentClient(CTorrentPeer* pPeer, QObject* qObject = 0);
	CTorrentClient(CTorrentSocket* pSocket, QObject* qObject = 0);
	~CTorrentClient();

	virtual void		Process(UINT Tick);

	virtual QString		GetUrl();

	virtual QString		GetConnectionStr();

	virtual CFile*		GetFile();

	const TTorrentID&	GetClientID()	{return m_Peer.ID;}
	const QByteArray&	GetInfoHash()	{return m_InfoHash;}

	void				SendHandShake();

	void				KeepAlive();
    void				ChokePeer();
    void				UnchokePeer();
    void				SendInterested();
    void				SendNotInterested();
    void				SendPieceList(const QBitArray &BitField);
    void				RequestBlock(uint32 Index, uint32 Offset, uint32 Length);
	void				CancelRequest(uint32 Index, uint32 Offset, uint32 Length);
	void 				SendHaveAll();
	void 				SendHaveNone();
	void 				SuggestPiece(uint32 Index);
	void 				AllowedFast(uint32 Index);
	void 				RejectRequest(uint32 Index, uint32 Offset, uint32 Length);

	bool				HasSentHandShake()			{return m_SentHandShake;}
	bool				HasReceivedHandShake()		{return m_ReceivedHandShake;}
	bool				IsIdleTimeOut()				{return m_uIdleTimeOut < GetCurTick();}
	uint64				GetIdleTimeOut()			{return m_uIdleTimeOut;}
	bool				SupportsEncryption()		{return m_Peer.ConOpts.Fields.SupportsEncryption;}
	bool				SupportsMetadataExchange()	{return m_ut_metadata_Extension;}
	bool				SupportsHolepunch()			{return m_ut_holepunch_Extension;}
	bool				SupportsFAST()				{return m_Supports_FAST;}
	void				RequestMetadata(uint32 Index);
	void				SendMetadata(uint32 Index);
	void				SendHolepunch(int Type, const CAddress& Address, quint16 Port, int Error = 0);
	bool				SupportsHostCache()			{return m_Supports_HostCache;}
	//UINT				NeoKadVersion()				{return m_Version_NeoKad;}

	struct SRequestedBlock
	{
		SRequestedBlock(uint32 index, uint32 offset, uint32 length)
		 : Index(index), Offset(offset), Length(length) { }
		inline bool operator==(const SRequestedBlock &Other) const { return Index == Other.Index && Offset == Other.Offset && Length == Other.Length; } // for remove all
	    
		uint32			Index;
		uint32			Offset;
		uint32			Length;
	};
	//struct SPendingBlock
	//{
	//	uint32			Index;
	//	uint32			Offset;
	//	uint32			Length;
	//};

	QList<SRequestedBlock>& GetIncomingBlocks()		{return m_IncomingBlocks;}
	int					GetRequestCount()			{return m_IncomingBlocks.size();}
	int					GetRequestLimit()			{return m_RequestLimit;}

	uint64				GetSentPieceSize();
	uint64				GetRecivedPieceSize();
	//uint64				GetRequestedSize();

	bool				Connect();
	bool				IsConnected() const					{return m_Socket->GetState() == CTorrentSocket::eConnected && m_ReceivedHandShake;}
	bool				IsDisconnected() const				{return m_uConnectTimeOut == -1 && m_uIdleTimeOut == -1;}
	void				Disconnect(const QString& Error = "");
	const STorrentPeer&	GetPeer() const						{return m_Peer;}
	uint16				GetPort() const						{return m_Peer.Port;}
	
	void				SetTorrentPeer(CTorrentPeer* pPeer)	{m_pPeer = pPeer;}
	CTorrentPeer*		GetTorrentPeer()					{return m_pPeer;}

	CTorrentSocket*		GetSocket()							{return m_Socket;}

	virtual void		AddLogLine(time_t uStamp, uint32 uFlag, const CLogMsg& Line);

	QString				IdentifySoftware();

public slots:
	void				OnHavePiece(int Index);

private slots:
	void				OnConnected();
	void				OnDisconnected(int Error);

	void				OnReceivedHandshake(QByteArray Handshake);
	void				OnReceivedPacket(QByteArray Packet);

	void				OnDataRead(uint64 Offset, uint64 Length, const QByteArray& Data, bool bOk, void* Aux);

protected:
	friend class CTorrentManager;

	void				Init();

	void				SendExtensions();
	void				SendPeerList();
	void				SendDHTPort();

	bool				ProcessHandShake(CBuffer& Packet);
	void				ProcessPieceList(CBuffer& Packet);
	void				ProcessBlockRequest(CBuffer& Packet);
	void				ProcessCancelRequest(CBuffer& Packet);
	void				ProcessBlock(CBuffer& Packet);
	void				ProcessRejectRequest(CBuffer& Packet);

	void				ProcessExtensions(QByteArray Data);
	void				ProcessPeerList(QByteArray Data);
	void				ProcessMetadata(QByteArray Data);
	void				ProcessHolepunch(CBuffer& Packet);

	void				ProcessStream();
	void				ProcessPacket(CBuffer& Packet);
	void				SendPacket(CBuffer& Packet);

	void				TryRequestMetadata();

    enum EPacketType 
	{
        eChoke			= 0,
        eUnchoke		= 1,
        eInterested		= 2,
        eNotInterested	= 3,
        eHave			= 4,
        eBitField		= 5,
        eRequest		= 6,
        ePiece			= 7,
        eCancel			= 8,

		eDHTPort		= 9,

		eUnused1		= 10,
		eUnused2		= 11,
		eUnused3		= 12,

		// FAST Extension
		eSuggestPiece	= 13,
		eHaveAll		= 14,
		eHaveNone		= 15,
		eRejectRequest	= 16,
		eAllowedFast	= 17,

		eUnused4		= 18,
		eUnused5		= 19,

		eExtended		= 20,
    };

	enum EHolepunch
	{
		// msg_types
		eHpRendezvous = 0,
		eHpConnect = 1,
		eHpFailed = 2,
	};

	enum EHoleError
	{
		// error codes
		eHpNoSuchPeer = 1,
		eHpNotConnected = 2,
		eHpNoSupport = 3,
		eHpNoSelf = 4
	};

	bool				m_SentHandShake;
	bool				m_ReceivedHandShake;
	int					m_HolepunchTry;
	uint64				m_uConnectTimeOut;
	uint64				m_uIdleTimeOut;
	uint64				m_uNextKeepAlive;
	uint64				m_uRequestTimeOut;
	int					m_RequestLimit;
	uint64				m_SentPieceSize;
	uint64				m_RecivedPieceSize;
	//uint64				m_TempPieceSize;

	CTorrentPeer*		m_pPeer;
	STorrentPeer		m_Peer;
	
	QByteArray			m_InfoHash;

	bool				m_Supports_Extension;
	//bool				m_Supports_Merkle;
	bool				m_Supports_DHT;
	bool				m_Supports_FAST;
	uint8				m_ut_pex_Extension;
	uint8				m_ut_metadata_Extension;
	uint8				m_ut_holepunch_Extension;
	uint8				m_Tr_hashpiece_Extension;
	bool				m_Supports_HostCache;
	//UINT				m_Version_NeoKad;

	uint64				m_NextMessagePEX;

	enum EExtensionType 
	{
		eExtension			= 0,
		eExtension_ut_pex	= 1,
		eExtension_ut_metadata	= 2,
		eExtension_ut_holepunch	= 3,
		eExtension_Tr_hashpiece = 250,
	};

	enum EMetadataType 
	{
		eRequestMetadata	= 0,
		eMetadataPayload	= 1,
		eMetadataReject		= 2,
	};


	//QList<SPendingBlock>	m_OutgoingBlocks;
    QList<SRequestedBlock>	m_IncomingBlocks;

	QSet<STorrentPeer>	m_PeerList;

	CTorrentSocket*		m_Socket;
};
