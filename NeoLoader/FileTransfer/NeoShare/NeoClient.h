#pragma once

#include "../P2PClient.h"
#include "NeoKad.h"
#include "NeoEntity.h"
#include "NeoSession.h"
class CFile;

class CNeoClient: public CP2PClient
{
	Q_OBJECT

public:
	CNeoClient(CNeoEntity* pNeo, QObject* qObject = 0);
	CNeoClient(CNeoSession* pSession, QObject* qObject = 0);
	~CNeoClient();

	virtual void					Process(UINT Tick);

	virtual QString					GetUrl();

	virtual QString					GetConnectionStr();

	virtual CFile*					GetFile();

	bool							Connect();
	bool							IsConnected()						{return m_Session && m_Session->IsConnected() && m_bHandshakeRecived;}
	bool							IsDisconnected()					{return m_Session == NULL;}
	void							Disconnect();

	void							RequestFile();
	void							RequestHashdata();
	void							RequestMetadata();
	void							RequestFileStatus();
	void							RequestDownload();
	void							SendQueueStatus(bool bAccepted);
	void							OfferUpload();
	void							RequestBlock(uint64 uOffset, uint32 uLength);

	void							SetNeoEntity(CNeoEntity* pNeo)		{m_pNeo = pNeo;}
	CNeoEntity*						GetNeoEntity()						{return m_pNeo;}

	const SNeoEntity&				GetNeo()							{return m_Neo;}

	CNeoSession*					GetSession()						{return m_Session;}

	bool							HasSentHandShake()					{return m_bHandshakeSent;}
	bool							HasReceivedHandShake()				{return m_bHandshakeRecived;}

	uint64							GetUploadedSize() const				{return m_UploadedSize;}
	uint64							GetDownloadedSize() const			{return m_DownloadedSize;}
	uint64							GetRequestCount() const				{return m_PendingRanges.count();}

	virtual void					AddLogLine(time_t uStamp, uint32 uFlag, const CLogMsg& Line);

private slots:
	void							OnConnected();
	void							OnDisconnected(int Error);

	void							OnActivity();

	void							OnProcessPacket(QString Name, QVariant Data);

	void							OnDataRead(uint64 Offset, uint64 Length, const QByteArray& Data, bool bOk, void* Aux);

	void							OnMetadataLoaded();

protected:
	friend class CNeoManager;
	bool							SendPacket(const QString& Name, const QVariantMap& Packet);
	void							KeepAlive();

	void							Init();

	void							SendHandshake();
	void							ProcessHandshake(const QVariantMap& InPacket);
	void							ProcessFileRequest(const QVariantMap& InPacket);
	void							SendFileInfo();
	void							ProcessFileStatusRequest(const QVariantMap& InPacket);
	void							ProcessFileStatus(const QVariantMap& InPacket);
	void							ProcessHashdataRequest(const QVariantMap& InPacket);
	void							ProcessHashdata(const QVariantMap& InPacket);
	void							ProcessMetadataRequest(const QVariantMap& InPacket);
	void							ProcessMetadata(const QVariantMap& InPacket);
	void							ProcessFileInfo(const QVariantMap& InPacket);
	void							ProcessBlockRequest(const QVariantMap& InPacket);
	void							RejectBlock(uint64 uOffset, uint32 uLength);
	void							ProcessRequestRejected(const QVariantMap& InPacket);
	void							ProcessDataBlock(const QVariantMap& InPacket);


	SNeoEntity						m_Neo;
	CNeoEntity*						m_pNeo;

	uint64							m_uTimeOut;
	uint64							m_uNextKeepAlive;
	bool							m_bHandshakeSent;
	bool							m_bHandshakeRecived;

	uint64							m_UploadedSize;
	uint64							m_DownloadedSize;
	QMap<uint64, uint64>			m_PendingRanges;

	CNeoSession*					m_Session;
};