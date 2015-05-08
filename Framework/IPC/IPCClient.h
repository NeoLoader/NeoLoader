#pragma once

#include "../../Framework/ObjectEx.h"

class CIPCSocket;

class NEOHELPER_EXPORT CIPCClient: public QObjectEx
{
	Q_OBJECT

public:
	CIPCClient(QObject* qObject = NULL);
	~CIPCClient();

	virtual bool		ConnectLocal(const QString& Name);
	virtual bool		ConnectRemote(const QString& Host, quint16 Port, QString UserName = "", QString Password = "");
	virtual void		Disconnect();

	virtual bool		IsConnected();
	virtual bool		IsDisconnected();

	virtual QString		GetLoginToken();
	virtual bool		SendRequest(const QString& Command, const QVariant& Parameters);
	virtual bool		SendRequest(const QString& Command, const QVariant& Parameters, QVariant& Result, int TimeOut = 3000);

signals:
	void				ConnectionEstablished();
	void				ResponseRecived(const QString& Command, const QVariant& Result);
	void				NotificationRecived(const QString& Command, const QVariant& Result);

private slots:
	virtual void		OnDisconnected();
	virtual void		OnResponse(const QString& Command, const QVariant& Parameters, qint64 Number);
	virtual void		OnEvent(const QString& Command, const QVariant& Parameters, qint64 Number);

protected:
	virtual void		Setup(CIPCSocket* pSocket);
	virtual bool		CanConnect();

	CIPCSocket*			m_Socket;
};

