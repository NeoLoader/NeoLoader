#pragma once
//#include "GlobalHeader.h"

#include "../Framework/ObjectEx.h"
#include <QCoreApplication>

class CSettings;
class CIPCServer;
class CKadHandler;
class CRequestManager;

#define MULE_KAD_VERSION_MJR	0
#define MULE_KAD_VERSION_MIN 	2
#define MULE_KAD_VERSION_UPD 	2

class CMuleKad: public CLoggerTmpl<QObject>
{
	Q_OBJECT

public:
	CMuleKad(QObject *parent = 0);
	~CMuleKad();

	void				SetEmbedded()		{m_bEmbedded = true;}
	void				Process();
	CSettings*			Cfg()						{return m_Settings;}
	CKadHandler*		Kad()						{return m_KadHandler;}

	uint32				GetKey()					{return m_UDPKey;}

	void				Connect();
	void				Disconnect();

private slots:
	void				Shutdown()					{QCoreApplication::exit(0);}
	void				OnRequestRecived(const QString& Command, const QVariant& Parameters, QVariant& Result);
	void				OnRequestFinished();

	void				SendPacket(QByteArray Data, quint32 IPv4, quint16 UDPPort, bool Encrypt, QByteArray TargetKadID, quint32 UDPKey);

signals:
	void				ProcessPacket(QByteArray Data, quint32 IPv4, quint16 UDPPort, bool validKey, quint32 UDPKey);

protected:
	void				SetupSocket();
	void				CheckAndBootstrap();

	void				timerEvent(QTimerEvent* pEvent)
	{
		if(pEvent->timerId() == m_uTimerID)
			Process();
	}

	CIPCServer*			m_pInterface;
	CSettings*			m_Settings;

	CKadHandler*		m_KadHandler;

	QByteArray			m_KadID;
	uint32				m_UDPKey;

	int					m_uTimerID;

	CRequestManager*	m_pRequestManager;
	QNetworkReply*		m_pBootstrap;

	bool				m_bEmbedded;
	uint64				m_uLastContact;
};
