#pragma once

#include "../../../Framework/ObjectEx.h"
#include "../../../Framework/Cryptography/AsymmetricKey.h"
#include "../../Networking/BandwidthControl/BandwidthLimiter.h"

class CNeoSession: public QObjectEx, public CBandwidthLimiter
{
	Q_OBJECT
public:
	CNeoSession(const QByteArray& EntityID, const QByteArray& TargetID, const QByteArray& SessionID, QObject* pObject = NULL);
	~CNeoSession() {}

	void				Dispose();

	void				Process(bool bConnected, const QVariantMap& Session = QVariantMap());

	bool				IsConnected() const				{return m_Connected;}

	const QByteArray&	GetEntityID() const				{return m_EntityID;}
	const QByteArray&	GetTargetID() const				{return m_TargetID;}
	const QByteArray&	GetSessionID() const			{return m_SessionID;}
	const QByteArray&	GetMyEntityID() const;

	void				SetUpload(bool bSet)				{m_bUpload = bSet;}
	bool				IsUpload()							{return m_bUpload;}
	void				SetDownload(bool bSet)				{m_bDownload = bSet;}
	bool				IsDownload()						{return m_bDownload;}

	
	quint64				QueueSize()							{return m_QueuedSize + m_LastQueuedSize;}
	int					GetQueueCount() const				{return m_QueuedStreams.count();}

	void				SendPacket(QString Name, QVariant Data);
	void				QueuePacket(QString Name, QVariant Data);

signals:
	void				Connected();
	void				Disconnected(int Error = 0);

	void				Activity();

	void				ProcessPacket(QString Name, QVariant Data);

protected:
	void				ProcessStream();

	QByteArray			m_EntityID;
	QByteArray			m_TargetID;
	QByteArray			m_SessionID;
	bool				m_Connected;

	bool				m_bUpload;
	bool				m_bDownload;


	CBuffer				m_OutBuffer;
	CBuffer				m_InBuffer;

	QList<CBuffer>		m_QueuedStreams;
	quint64				m_QueuedSize;
	quint64				m_LastQueuedSize;
};