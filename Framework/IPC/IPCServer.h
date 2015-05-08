#pragma once
#include "../../Framework/ObjectEx.h"
#include <QLocalServer>
#include <QTcpServer>

class CIPCSocket;

class NEOHELPER_EXPORT CIPCServer: public QObjectEx
{
	Q_OBJECT

public:
	CIPCServer(QObject* qObject = NULL);
	virtual ~CIPCServer();

	virtual void		LocalListen(const QString& Name);
	virtual void		RemoteListen(quint16 Port);

	QString				GetName()	{return m_Name;}
	quint16				GetPort()	{return m_Port;}

	virtual QString		CheckLogin(const QString &UserName, const QString &Password) {return "Anonymus";}
	virtual bool		HasLoginToken(const QString &LoginToken);

	virtual int			GetClientCount()	{return m_Clients.count();}

	virtual QVariant	ProcessRequest(const QString& Command, const QVariant& Parameters);

	virtual int			PushNotification(const QString& Command, const QVariant& Parameters);

signals:
	void				RequestRecived(const QString& Command, const QVariant& Parameters, QVariant& Result);

private slots:
	virtual void		OnLocalConnection();
	virtual void		OnRemoteConnection();
	virtual void		OnDisconnected();
	virtual void		OnRequest(const QString& Command, const QVariant& Parameters, qint64 Number);

protected:
	virtual void		AddSocket(CIPCSocket* pSocket);

	QLocalServer*		m_Local;
	QTcpServer*			m_Remote;	
	QList<CIPCSocket*>	m_Clients;

	QString				m_Name;
	quint16				m_Port;
};
