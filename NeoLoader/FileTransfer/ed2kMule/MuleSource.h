#pragma once

#include "../Transfer.h"
#include "MuleTags.h"
class CMuleClient;

typedef SUID<16> TEd2kID;

struct SMuleSource: SAddressCombo
{
	SMuleSource()
		: ClientID(0), OpenIPv6(false)
		, TCPPort(0), UDPPort(0), KadPort(0)
		, ServerPort(0)
		, BuddyPort(0)
	{
		ConOpts.Bits = 0;
	}

	static bool		IsLowID(uint32 ClientID)			{return (ClientID && ClientID < 16777216);}

	bool operator==(const SMuleSource &Other) const 
	{
		if(TCPPort && Other.TCPPort && TCPPort != Other.TCPPort)
			return false;
		return CompareTo(Other);
	}

	bool			SelectIP();

	uint16			GetPort(bool bUDP) const			{return bUDP ? (KadPort ? KadPort : UDPPort) : TCPPort;}

	__inline bool	HasLowID() const					{return IsLowID(ClientID);}
	__inline bool	ClosedIPv6() const					{return !OpenIPv6;}

	void			SetConOpts(UMuleMisc2 MiscOptions2)
	{
		ConOpts.Fields.SupportsCryptLayer = MiscOptions2.Fields.SupportsCryptLayer;
		ConOpts.Fields.RequestsCryptLayer = MiscOptions2.Fields.RequestsCryptLayer;
		ConOpts.Fields.RequiresCryptLayer = MiscOptions2.Fields.RequiresCryptLayer;
		ConOpts.Fields.DirectUDPCallback = MiscOptions2.Fields.DirectUDPCallback;
	}

	void			SetConOptsEx(UMuleMiscN MiscOptionsN)
	{
		ConOpts.Fields.SupportsNatTraversal = MiscOptionsN.Fields.SupportsNatTraversal;
	}

	uint32			ClientID;
	bool			OpenIPv6;
	TEd2kID			UserHash;

	uint16			TCPPort;
	uint16			UDPPort;
	uint16			KadPort;
	UMuleConOpt		ConOpts;

	CAddress		ServerAddress;
	uint16			ServerPort;

	QByteArray		BuddyID;
	CAddress		BuddyAddress;
	uint16			BuddyPort;
};

class CMuleSource: public CP2PTransfer
{
	Q_OBJECT

public:
	CMuleSource();
	CMuleSource(const SMuleSource& Mule);
	~CMuleSource();

	virtual bool					Process(UINT Tick);

	virtual void					RequestUpload();
	virtual void					CancelUpload();
	virtual void					BeginDownload();
	virtual void					EndDownload();

	virtual	QString					GetUrl() const;
	virtual QString 				GetDisplayUrl() const;

	virtual void					Hold(bool bDelete);

	virtual	bool 					IsUpload() const		{return m_Status.Flags.RequestedUpload;}
	virtual	bool 					IsWaitingUpload() const {return m_Status.Flags.OnLocalQueue;}
	virtual	bool 					IsActiveUpload() const	{return m_Status.Flags.Uploading;}

	virtual	bool 					IsDownload() const		{return true;}
	virtual	bool 					IsInteresting() const	{return m_Status.Flags.HasNeededParts;}
	virtual	bool 					IsWaitingDownload() const {return m_Status.Flags.OnRemoteQueue;}
	virtual	bool 					IsActiveDownload() const{return m_Status.Flags.Downloading;}

	virtual bool					IsConnected() const;
	virtual bool					Connect();
	virtual void					Disconnect();
	virtual QString					GetConnectionStr() const;

	virtual ETransferType			GetType()				{return eEd2kMule;}

	virtual CFileHashPtr			GetHash();

	virtual void					AttacheClient(CMuleClient* pClient = NULL);

	virtual CBandwidthLimit*		GetUpLimit();
	virtual CBandwidthLimit*		GetDownLimit();

	virtual bool					StartUpload();
	virtual void					StopUpload(bool bStalled = false);

	virtual const TEd2kID&			GetUserHash()						{return m_Mule.UserHash;}
	virtual const SMuleSource&		GetMule()							{return m_Mule;}
	virtual uint16					GetPort()							{return m_Mule.TCPPort;}

	virtual CMuleClient*			GetClient()							{return m_pClient;}

	virtual QString					GetFileName() const;

	virtual bool					IsComplete() const;

	virtual qint64					PendingBytes() const;
	virtual qint64					PendingBlocks() const;
	virtual qint64					LastUploadedBytes() const;
	virtual bool					IsBlocking() const;
	virtual qint64					RequestedBlocks() const;
	virtual qint64					LastDownloadedBytes() const;

	virtual bool					IsSeed() const;

	virtual void					AskForDownload();
	virtual void					ReaskForDownload();

	virtual bool					IsChecked() const					{return m_NextDownloadRequest != 0;}
	virtual uint64					GetNextRequest() const				{return m_NextDownloadRequest;}

	virtual bool					SupportsHostCache() const;
	virtual QPair<const byte*, size_t> GetID() const;

	virtual QString					GetSoftware() const;

	virtual uint16					GetRemoteQueueRank() const			{return m_RemoteQueueRank;}
	virtual uint16					GetLocalQueueRank() const			{return m_LocalQueueRank;}

	virtual void					SendCommendIfNeeded();

	// Load/Store
	virtual QVariantMap				Store();
	virtual bool					Load(const QVariantMap& Transfer);

	static bool						ParseUrl(const QString& Url, CAddress& IP, uint16& Port, const QString& Type);

private slots:
	virtual void					OnHelloRecived();
	virtual void					OnSocketClosed();

protected:
	friend class CMuleClient;

	virtual void					SetQueueRank(uint16 uRank, bool bUDP = false);
	virtual void					SetFileName(const QString& FileName)					{m_RemoteFileName = FileName;}
	virtual	void					HandleComment(const QString& Description, uint8 Rating);
	virtual	void					SetRemoteAvailability(uint16 uAvailability)				{m_RemoteAvailability = uAvailability;}

	virtual void					OnDownloadStart();
	virtual void					OnDownloadStop();

	virtual void 					OnCrumbComplete(uint32 Index);

	virtual void					Init();

	virtual bool					CheckInterest();

	virtual void					RequestBlocks(bool bRetry = false);

	virtual uint16					GetQueueRank(bool bUDP = false);

	SMuleSource						m_Mule;

	union UStatus
	{
		UINT	Bits;
		struct SFlags
		{
			UINT
			// Upload
			RequestedUpload		: 1,
			OnLocalQueue		: 1,
			Uploading			: 1,
			//Download
			HasNeededParts		: 1,
			OnRemoteQueue		: 1, // if needer OnRemoteQueue nor Downloading than it means RemoteQueueFull
			Downloading			: 1,
			// other
			CommentSent			: 1;
		}
		Flags;
	}								m_Status;
	uint16							m_RemoteQueueRank;
	uint16							m_LocalQueueRank;
	uint64							m_NextDownloadRequest;
	uint64							m_LastUploadRequest;

	CMuleClient*					m_pClient;

	QString							m_RemoteFileName;
	QString							m_RemoteComment;
	enum EMuleRating
	{
		eNotRated = 0,
		eFake = 1,
		ePoor = 2,
		eFair = 3,
		eGood = 4,
		eExcellent = 5
	}								m_RemoteRating;
	uint16							m_RemoteAvailability;
};
