#include "GlobalHeader.h"
#include "IPCClient.h"
#include "IPCSocket.h"

CIPCClient::CIPCClient(QObject* qObject)
: QObjectEx(qObject)
{
	m_Socket = NULL;
}

CIPCClient::~CIPCClient()
{
	Disconnect();
}

bool CIPCClient::IsConnected()
{
	if(m_Socket)
		return m_Socket->IsConnected();
	return false;
}

bool CIPCClient::IsDisconnected()
{
	if(m_Socket)
		return false;
	return true;
}

QString CIPCClient::GetLoginToken()
{
	if(m_Socket)
		return m_Socket->GetLoginToken();
	return "";
}

bool CIPCClient::ConnectLocal(const QString& Name)
{
	if(!CanConnect())
		return false;

	//LogLine(LOG_DEBUG, tr("connecting to local %1 Server").arg(Name));
	QLocalSocket* pLocal = new QLocalSocket();
	CIPCSocket* pSocket = new CIPCSocket(pLocal);
	Setup(pSocket);
	pLocal->connectToServer(Name);
	return true;
}

bool CIPCClient::ConnectRemote(const QString& Host, quint16 Port, QString UserName, QString Password)
{
	if(!CanConnect())
		return false;

	//LogLine(LOG_DEBUG, tr("connecting to remote %1:%2 Server").arg(Host).arg(Port));
	QTcpSocket* pRemote = new QTcpSocket();
	CIPCSocket* pSocket = new CIPCSocket(pRemote);
	Setup(pSocket);
	pSocket->SetLogin(UserName, Password);
	pRemote->connectToHost(Host, Port);
	return true;
}

void CIPCClient::Setup(CIPCSocket* pSocket)
{
	pSocket->setParent(this);
	connect(pSocket, SIGNAL(Disconnected()), this, SLOT(OnDisconnected()));
	connect(pSocket, SIGNAL(Connected()), this, SIGNAL(ConnectionEstablished()));
	connect(pSocket, SIGNAL(Response(const QString&, const QVariant&, qint64)), this, SLOT(OnResponse(const QString&, const QVariant&, qint64)));
	connect(pSocket, SIGNAL(Event(const QString&, const QVariant&, qint64)), this, SLOT(OnEvent(const QString&, const QVariant&, qint64)));
	m_Socket = pSocket;
}

void CIPCClient::Disconnect()
{
	if(m_Socket)
		m_Socket->Disconnect();
	if(m_Socket) // Disconnect -> ... -> OnDisconnected <- but not alway
	{
		m_Socket->deleteLater();
		m_Socket = NULL;
	}
}

void CIPCClient::OnDisconnected()
{
	CIPCSocket* pSocket = (CIPCSocket*)sender();
	if(pSocket == m_Socket)
	{
		m_Socket = NULL;
		pSocket->deleteLater();
	}
}

bool CIPCClient::CanConnect()
{
	if(m_Socket)
	{
		LogLine(LOG_ERROR, tr("this Client is already connected to a Server"));
		return false;
	}
	return true;
}

bool CIPCClient::SendRequest(const QString& Command, const QVariant& Parameters)
{
	if(!m_Socket)
		return false; // not connected
	return m_Socket->SendRequest(Command, Parameters) != 0;
}

bool CIPCClient::SendRequest(const QString& Command, const QVariant& Parameters, QVariant& Result, int TimeOut)
{
	if(!m_Socket)
		return false; // not connected
	return m_Socket->SendRequest(Command, Parameters, Result, TimeOut);
}

void CIPCClient::OnResponse(const QString& Command, const QVariant& Parameters, sint64 Number)
{
	emit ResponseRecived(Command, Parameters);
}

void CIPCClient::OnEvent(const QString& Command, const QVariant& Parameters, sint64 Number)
{
	emit NotificationRecived(Command, Parameters);
}
