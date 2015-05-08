#pragma once

#include "../Transfer.h"

struct SNeoEntity
{
	SNeoEntity() 
		: IsHub(false) {}

	QByteArray	TargetID;	// Rendevouz Area
	QByteArray	EntityID;	// Remote EntityID
	QByteArray	MyEntityID;	// Local EntityID - outgoing entity, so that we apear consistent as one client to the remote side during one session

	bool		IsHub;
};

class CNeoClient;

class CNeoEntity: public CP2PTransfer
{
	Q_OBJECT

public:
	CNeoEntity(CFileHashPtr& pHash);
	CNeoEntity(CFileHashPtr& pHash, const SNeoEntity& Neo);
	~CNeoEntity();

	virtual bool					Process(UINT Tick);

	virtual ETransferType			GetType()				{return eNeoShare;}

	virtual QString 				GetUrl() const;
	virtual QString					GetConnectionStr() const;
	virtual void					Hold(bool bDelete);

	virtual	bool 					IsUpload() const		{return m_Status.Flags.RequestedUpload;}
	virtual	bool 					IsWaitingUpload() const {return m_Status.Flags.OnLocalQueue;}
	virtual	bool 					IsActiveUpload() const	{return m_Status.Flags.Uploading;}

	virtual	bool 					IsDownload() const		{return true;}
	virtual	bool 					IsInteresting() const	{return m_Status.Flags.HasNeededParts;}
	virtual	bool 					IsWaitingDownload() const {return m_Status.Flags.OnRemoteQueue;}
	virtual	bool 					IsActiveDownload() const{return m_Status.Flags.Downloading;}

	virtual CFileHashPtr			GetHash()					{return m_pHash;}

	virtual bool					StartUpload();
	virtual void					StopUpload(bool bStalled = false);

	virtual bool					IsConnected() const			{return m_pClient != NULL;}
	virtual CNeoClient*				GetClient() const			{return m_pClient;}
	virtual bool					IsHub() const				{return m_Neo.IsHub;}

	virtual void					AttacheClient(CNeoClient* pClient);
	virtual bool					Connect();
	virtual void					Disconnect();
	virtual void					DetacheClient();

	virtual CBandwidthLimit*		GetUpLimit();
	virtual CBandwidthLimit*		GetDownLimit();

	const SNeoEntity&				GetNeo() const				{return m_Neo;}

	virtual qint64					PendingBytes() const;
	virtual qint64					PendingBlocks() const;
	virtual qint64					LastUploadedBytes() const;
	virtual bool					IsBlocking() const;
	virtual qint64					RequestedBlocks() const;
	virtual qint64					LastDownloadedBytes() const;

	virtual bool					IsSeed() const;

	virtual void					AskForDownload();

	virtual bool					IsChecked() const			{return m_NextDownloadRequest != 0;}
	virtual void					SetChecked();
	virtual uint64					GetNextRequest() const		{return m_NextDownloadRequest;}

	virtual bool					SupportsHostCache() const	{return true;} // N-ToDo
	virtual QPair<const byte*, size_t> GetID() const;

	virtual QString					GetSoftware() const			{return m_Software;}

	// Load & Store
	virtual QVariantMap				Store();
	virtual bool					Load(const QVariantMap& Transfer);

protected:
	friend class CNeoClient;

	virtual void					Init();

	virtual void					OnConnected();

	virtual bool					CheckInterest();
	virtual void					RequestBlocks(bool bRetry = false);

	virtual void					DownloadRequested();
	virtual void					QueueStatus(bool bAccepted);
	virtual void					UploadOffered();

	SNeoEntity						m_Neo;

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
			OnRemoteQueue		: 1,
			Downloading			: 1;
		}
		Flags;
	}								m_Status;
	uint64							m_NextDownloadRequest;
	uint64							m_LastUploadRequest;
	int								m_RequestVolume;

	CFileHashPtr					m_pHash;

	QString							m_Software;

	CNeoClient*						m_pClient;
};