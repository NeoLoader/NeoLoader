#pragma once

#include "CoreBus.h"
#include "../../Framework/IPC/IPCSocket.h"

class CCoreClient: public CIPCClient
{
	Q_OBJECT

public:
	CCoreClient(QObject* qObject = NULL);

	virtual bool		HasCore();
	virtual bool		IsConnected();
	virtual bool		IsDisconnected();
	virtual void		Disconnect();

	virtual QString		GetLoginToken();

	/*virtual int			ConnectBus(QString BusName, quint16 BusPort);
	virtual bool		ListNodes(bool inLAN = false);
	virtual QList<CCoreBus::SCore> GetNodeList();*/

	virtual bool		SendRequest(const QString& Command, const QVariant& Parameters);
	virtual void		DispatchResponse(const QString& Command, const QVariant& Parameters);

private slots:
	virtual  void		OnResponse(const QString& Command, const QVariant& Parameters) {OnResponse(Command, Parameters, 0);}
	virtual  void		OnResponse(const QString& Command, const QVariant& Parameters, sint64 Number);

signals:
	void				Request(QString Command, QVariant Parameters);
	
protected:
	virtual bool		CanConnect();

	virtual bool		HasTimedOut()						{return m_uTimeOut && m_uTimeOut < GetCurTick();}
	virtual void		SetTimeOut();
	virtual void		StopTimeOut()						{m_uTimeOut = 0;}

	//CCoreBus*			m_Bus;
	uint64				m_uTimeOut;
};
