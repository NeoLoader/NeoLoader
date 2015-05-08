#include "GlobalHeader.h"
#include "HttpServer.h"
#include "HttpSocket.h"

CHttpServer::CHttpServer(int Port, QObject* qObject)
{
	m_Port = Port;
	if(!Listen(Port))
		LogLine(LOG_ERROR, tr("Failed to listen on http port"));

	m_TransferBufferSize = KB2B(4);
	m_KeepAlive = 115; // seconds
}

bool CHttpServer::Listen(int Port)
{
	QTcpServer* pListener = new QTcpServer(this);
	pListener->listen(QHostAddress::Any, Port);
    if (!pListener->isListening()) 
	{
		delete pListener;
		return false;
	}
	connect(pListener, SIGNAL(newConnection()), this, SLOT(OnConnection()));

	m_Listners.insert(Port, pListener);
	return true;
}

void CHttpServer::Process()
{
	foreach(CHttpSocket* pHttpSocket, m_Sockets.values())
	{
		switch(pHttpSocket->m_TransactionState)
		{
			case CHttpSocket::eWaiting:
				if(!pHttpSocket->KeepAlive())
					pHttpSocket->Drop();
				break;
			case CHttpSocket::eReading:
				if(pHttpSocket->m_pSocket->bytesAvailable() > 0)
				{
					pHttpSocket->TryReadSocket();
					HandleSocket(pHttpSocket);
				}
				break;
			case CHttpSocket::eWriting:
				pHttpSocket->TrySendBuffer();
				break;
		}
	}
}

void CHttpServer::RegisterHandler(CHttpHandler* pHandler, QString Path, quint16 LocalPort)
{
	ASSERT(!m_Handlers.contains(Path));
	m_Handlers.insert(Path, qMakePair(pHandler, LocalPort));
}

CHttpHandler* CHttpServer::GetHandler(QString Path, quint16 LocalPort)
{
	for(;;)
	{
		QPair<CHttpHandler*, quint16> Handler = m_Handlers.value(Path);
		if(Handler.first && ((Handler.second ? Handler.second : m_Port) == LocalPort))
			return Handler.first;

		int Pos = Path.lastIndexOf("/");
		if(Pos == -1)
			break;
		Path.truncate(Pos);
	}
	return NULL;
}

void CHttpServer::OnConnection()
{
	QTcpServer* pListener = (QTcpServer*)sender();
	QTcpSocket* pSocket = pListener->nextPendingConnection();
	connect(pSocket, SIGNAL(readyRead()), this, SLOT(OnReadyRead()));
	connect(pSocket, SIGNAL(bytesWritten(qint64)), this, SLOT(OnBytesWritten(qint64)));
	connect(pSocket, SIGNAL(disconnected()), this, SLOT(OnDisconnect()));
	pSocket->setReadBufferSize(m_TransferBufferSize);
	m_Sockets.insert(pSocket, new CHttpSocket(pSocket, m_KeepAlive, this));
}

void CHttpServer::OnReadyRead()
{
	QTcpSocket* pSocket = (QTcpSocket*)sender();
	if(!m_Sockets.contains(pSocket))
		return;

	CHttpSocket* pHttpSocket = m_Sockets.value(pSocket);
	pHttpSocket->TryReadSocket();

	HandleSocket(pHttpSocket);
}

void CHttpServer::OnBytesWritten(qint64 bytes)
{
	QTcpSocket* pSocket = (QTcpSocket*)sender();
	if(!m_Sockets.contains(pSocket))
		return;

	CHttpSocket* pHttpSocket = m_Sockets.value(pSocket);
	if(pHttpSocket->m_TransactionState == CHttpSocket::eWriting)
		pHttpSocket->TrySendBuffer();
}

void CHttpServer::OnDisconnect()
{
	QTcpSocket* pSocket = (QTcpSocket*)sender();
	if(!m_Sockets.contains(pSocket))
		return;

	CHttpSocket* pHttpSocket = m_Sockets.take(pSocket);
	if(CHttpHandler* pHandler = GetHandler(pHttpSocket->GetPath(), pHttpSocket->GetLocalPort()))
		pHandler->ReleaseRequest(pHttpSocket);
	pHttpSocket->deleteLater();
	pSocket->deleteLater();
}

void CHttpServer::HandleSocket(CHttpSocket* pHttpSocket)
{
	if(pHttpSocket->m_ResponseCode == CHttpSocket::eHandling || pHttpSocket->m_ResponseCode == CHttpSocket::eWriting)
	{
		LogLine(LOG_ERROR, tr("recived new request, befoure the previouse response was completly sent"));
		return;
	}

	if(pHttpSocket->m_RequestType == CHttpSocket::eNone) // are we still completing the header?
	{
		int End = pHttpSocket->m_RequestBuffer.indexOf("\r\n\r\n");
		if(End == -1) // hreader not yet complee
			return;

		ASSERT(pHttpSocket->m_TransactionState == CHttpSocket::eWaiting);
		pHttpSocket->m_TransactionState = CHttpSocket::eReading;

		//TRACE(L"Socket %d recived hreader %S", (int)pHttpSocket, pHttpSocket->m_RequestBuffer.left(End).data());
		QStringList RequestHeader = QString(pHttpSocket->m_RequestBuffer.left(End)).split('\n');
		pHttpSocket->m_RequestBuffer.remove(0,End+4);
		pHttpSocket->m_Downloaded -= End+4;

		if(int Code = pHttpSocket->HandleHeader(RequestHeader))
			pHttpSocket->RespondWithError(Code);
		else if(CHttpHandler* pHandler = GetHandler(pHttpSocket->GetPath(), pHttpSocket->GetLocalPort()))
			pHandler->HandleRequest(pHttpSocket);
		else
			pHttpSocket->RespondWithError(404, "No service abvailable for this this path");
	}

	if(pHttpSocket->m_TransactionState == CHttpSocket::eReading)
		pHttpSocket->HandleData();
	else if(pHttpSocket->m_TransactionState == CHttpSocket::eFailing)
		pHttpSocket->SendResponse();
}
