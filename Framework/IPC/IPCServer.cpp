#include "GlobalHeader.h"
#include "IPCServer.h"
#include "IPCSocket.h"

CIPCServer::CIPCServer(QObject* qObject)
 : QObjectEx(qObject)
{
	m_Port = 0;
	m_Local = NULL;
	m_Remote = NULL;
}

CIPCServer::~CIPCServer()
{
	if(m_Local)
		m_Local->close();
	if(m_Remote)
		m_Remote->close();
}

void CIPCServer::LocalListen(const QString& Name)
{
	if(!Name.isEmpty())
	{
		m_Local = new QLocalServer(this);

		if(m_Local->listen(Name))
		{
			m_Name = Name;
			LogLine(LOG_NOTE, tr("Start local server %1").arg(Name));
		}
		else
		{
			LogLine(LOG_WARNING, tr("Failed to start local server name %1, selecting alternative name").arg(Name));

			QString tmpName = QString("%1_%2").arg(Name).arg(GetRand64());
			if(m_Local->listen(tmpName))
			{
				m_Name = tmpName;
				LogLine(LOG_WARNING, tr("Started Local server witn temporary name %1").arg(tmpName));
			}
			else
				LogLine(LOG_ERROR, tr("Failed to start local server!"));
		}

		connect(m_Local, SIGNAL(newConnection()), this, SLOT(OnLocalConnection()));
	}
	else
		m_Local = NULL;
}

void CIPCServer::RemoteListen(quint16 Port)
{
	if(Port != 0)
	{
		m_Remote = new QTcpServer(this);

		if(m_Remote->listen(QHostAddress::Any, Port)) 
		{
			m_Port = Port;
			LogLine(LOG_NOTE, tr("Start remote server on port %1").arg(Port));
		}
		else
		{
			LogLine(LOG_WARNING, tr("Failed to start remote server on port %1, selecting alternative port").arg(Port));

			quint16 tmpPort = Port + GetRandomInt(1, 1000);
			if(m_Remote->listen(QHostAddress::Any, tmpPort)) 
			{
				m_Port = tmpPort;
				LogLine(LOG_WARNING, tr("Started remote server witn temporary port %1").arg(tmpPort));
			}
			else
				LogLine(LOG_ERROR, tr("Failed to start remote server!"));
		}

		connect(m_Remote, SIGNAL(newConnection()), this, SLOT(OnRemoteConnection()));
	}
	else
		m_Remote = NULL;
}

void CIPCServer::OnLocalConnection()
{
	AddSocket(new CIPCSocket(m_Local->nextPendingConnection(), true));
}

void CIPCServer::OnRemoteConnection()
{
	AddSocket(new CIPCSocket(m_Remote->nextPendingConnection(), true));
}

void CIPCServer::AddSocket(CIPCSocket* pSocket)
{
	pSocket->setParent(this);
	connect(pSocket, SIGNAL(Disconnected()), this, SLOT(OnDisconnected()));
	connect(pSocket, SIGNAL(Request(const QString&, const QVariant&, qint64)), this, SLOT(OnRequest(const QString&, const QVariant&, qint64)));
	m_Clients.append(pSocket);
}

void CIPCServer::OnDisconnected()
{
	CIPCSocket* pSocket = (CIPCSocket*)sender();
	m_Clients.removeAll(pSocket);
	pSocket->deleteLater();
}

bool CIPCServer::HasLoginToken(const QString &LoginToken)
{
	foreach(CIPCSocket* pClient, m_Clients)
	{
		if(pClient->GetLoginToken() == LoginToken)
			return true;
	}
	return false;
}

void CIPCServer::OnRequest(const QString& Command, const QVariant& Parameters, sint64 Number)
{
	QVariant Result = ProcessRequest(Command, Parameters);
	((CIPCSocket*)sender())->SendResponse(Command, Result, Number);
}

QVariant CIPCServer::ProcessRequest(const QString& Command, const QVariant& Parameters)
{
	QVariant Result;
	emit RequestRecived(Command, Parameters, Result);
	return Result;
}

int CIPCServer::PushNotification(const QString& Command, const QVariant& Parameters)
{
	int Count = 0;
	foreach(CIPCSocket* pClient, m_Clients)
	{
		if(pClient->SendEvent(Command, Parameters) != 0)
			Count++;
	}
	return Count;
}