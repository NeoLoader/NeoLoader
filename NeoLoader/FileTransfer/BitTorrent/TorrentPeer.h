#pragma once

#include "../Transfer.h"

#include <QBitArray>

typedef SUID<20> TTorrentID;

struct STorrentPeer: SAddressCombo
{
	STorrentPeer()
	 : Port(0){
		 ConOpts.Bits = 0; 
		 // always try encryption and utp if not explicitly stated otherwice
		 ConOpts.Fields.SupportsEncryption = 1;
		 ConOpts.Fields.SupportsUTP = 1;
	}
	
	bool operator==(const STorrentPeer &Other) const 
	{
		if(Port && Other.Port && Port != Other.Port)
			return false;
		return CompareTo(Other);
	}

	bool					SelectIP();

	quint16			Port;
	TTorrentID		ID;
	union UPeerConOpt
	{
		uint8	Bits;
		struct UConOpt
		{
			uint8
			SupportsEncryption	: 1,
			IsSeed				: 1,
			SupportsUTP			: 1,
			SupportsHolepunch	: 1,
			Reserved			: 4; // 4 Reserved (!)
		}		Fields;
	}				ConOpts;
};

class CTorrentClient;

class CTorrentPeer: public CP2PTransfer
{
	Q_OBJECT

public:
	CTorrentPeer(CTorrent* pTorrent);
	CTorrentPeer(CTorrent* pTorrent, const STorrentPeer& Peer);
	~CTorrentPeer();

	virtual bool					Process(UINT Tick);

	virtual	QString					GetUrl() const;

	virtual	bool 					IsUpload() const		{return m_Status.Flags.PeerIsInterested;}
	virtual	bool 					IsWaitingUpload() const	{return IsUpload() && !m_Status.Flags.PeerIsUnchoked;}
	virtual	bool 					IsActiveUpload() const	{return IsUpload() && m_Status.Flags.PeerIsUnchoked;}

	virtual	bool 					IsDownload() const		{return true;}
	virtual	bool 					IsInteresting() const	{return m_Interesting;}
	virtual	bool 					IsWaitingDownload() const {return IsInteresting() && !m_Status.Flags.UnchokedByPeer;}
	virtual	bool 					IsActiveDownload() const{return IsInteresting() && m_Status.Flags.UnchokedByPeer;}

	virtual bool					IsConnected() const		{return m_pClient != NULL;}
	virtual CTorrentClient*			GetClient() const		{return m_pClient;}
	virtual QString					GetConnectionStr() const;

	virtual ETransferType			GetType()				{return eBitTorrent;}

	virtual CFileHashPtr			GetHash();

	virtual void					Hold(bool bDelete);

	virtual void					AttacheClient(CTorrentClient* pClient);
	virtual bool					Connect();
	virtual void					Disconnect();
	virtual void					DetacheClient();

	virtual CBandwidthLimit*		GetUpLimit();
	virtual CBandwidthLimit*		GetDownLimit();

	virtual CTorrent*				GetTorrent()						{ASSERT(m_pTorrent); return m_pTorrent;}

	//virtual void					ResetNextConnect();

	virtual bool					StartUpload();
	virtual void					StopUpload(bool bStalled = false);

	virtual const TTorrentID&		GetClientID()						{return m_Peer.ID;}
	const STorrentPeer&				GetPeer() const						{return m_Peer;}
	virtual uint16					GetPort()							{return m_Peer.Port;}

	virtual	void					CancelRequest(uint64 uBegin, uint64 uEnd);

	virtual qint64					PendingBytes() const;
	virtual qint64					PendingBlocks() const;
	virtual qint64					LastUploadedBytes() const;
	virtual bool					IsBlocking() const;
	virtual qint64					RequestedBlocks() const;
	virtual qint64					LastDownloadedBytes() const;

	virtual bool					IsSeed() const						{if(!m_Parts) return false; return m_Peer.ConOpts.Fields.IsSeed;}
	
	virtual bool					IsChecked() const 					{return m_LastConnect != 0;}
	virtual uint64					GetLastConnect() const				{return m_LastConnect;}

	virtual bool					SupportsHostCache() const;
	virtual QPair<const byte*, size_t> GetID() const;

	virtual QString					GetSoftware() const;

	// Load/Store
	virtual QVariantMap				Store();
	virtual bool					Load(const QVariantMap& Transfer);

public slots:
	virtual	void					OnMetadataLoaded();

protected:
	friend class CTorrentClient;

	virtual void 					OnHandShakeRecived();
	virtual void 					OnExtensionsRecived();
	
	virtual void 					OnInterested()						{m_Status.Flags.PeerIsInterested = true;}
	virtual void 					OnNotInterested()					{m_Status.Flags.PeerIsInterested = false;}
	virtual void 					OnChoked();
	virtual void 					OnUnchoked();

	virtual void 					OnPieceAvailable(uint32 Index);
	virtual void 					OnPiecesAvailable(const QBitArray &FullPieces);
	virtual void 					OnHaveAll();
	virtual void 					OnHaveNone();
	virtual void 					OnPieceSuggested(uint32 Index);

	virtual void					Init();

	virtual bool					CheckInterest();

	virtual void					RequestBlocks(bool bRetry = false);

	CTorrent*						m_pTorrent;
	STorrentPeer					m_Peer;
	
	QString							m_Software;

	uint64							m_LastConnect;

	union UStatus
	{
		UINT	Bits;
		struct SFlags
		{
			UINT	
			// Upload
			PeerIsInterested	: 1,
			PeerIsUnchoked		: 1,
			//Download
			InterestedInPeer	: 1,
			UnchokedByPeer		: 1;
		}
		Flags;
	}								m_Status;		// this status is lost on disconenct
	bool							m_Interesting;	// cache this vlaue 
	int								m_RequestVolume;

	CTorrentClient*					m_pClient;

	struct SCachedMap
	{
		enum EType
		{
			eAll,
			eNone,
			eBitField
		}			Type;
		QBitArray	BitField;
	}*								m_CachedMap;
};
