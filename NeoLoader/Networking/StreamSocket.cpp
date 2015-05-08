#include "GlobalHeader.h"
#include "StreamSocket.h"
#include "../NeoCore.h"
#include "TCPSocket.h"

CAbstractSocket::CAbstractSocket(CStreamSocket* pStream) 
 : QObject(pStream) 
{ 
	m_Blocking = false; 
}

qint64 CAbstractSocket::Read(char *data, qint64 maxlen)
{
	CStreamSocket* pStreamSocket = GetStream();
	pStreamSocket->RequestBandwidth(CBandwidthLimiter::eDownChannel, Min(maxlen, RecvPending()));

	qint64 uToRead = Min(maxlen, pStreamSocket->GetQuota(CBandwidthLimiter::eDownChannel));
	if(uToRead <= 0)
		return 0;
#ifdef MSS
	if(theCore->m_Network->UseTransportLimiting() && pStreamSocket->GetQuota(CBandwidthLimiter::eUpChannel) < 0)
		return 0;
#endif
	ASSERT(uToRead > 0);
	qint64 uRead = Recv(data, uToRead);
	if(uRead == -1)
		return -1;
	ASSERT(uToRead >= uRead);

	pStreamSocket->CountBandwidth(CBandwidthLimiter::eDownChannel, uRead, pStreamSocket->IsDownload() ? CBandwidthCounter::ePayload : CBandwidthCounter::eProtocol);
	return uRead;
}

qint64 CAbstractSocket::Write(const char *data, qint64 len)	
{
	CStreamSocket* pStreamSocket = GetStream();
	pStreamSocket->RequestBandwidth(CBandwidthLimiter::eUpChannel, len);

	qint64 uToWrite = Min(len, pStreamSocket->GetQuota(CBandwidthLimiter::eUpChannel));
	if(uToWrite <= 0)
		return 0;
#ifdef MSS
	if(theCore->m_Network->UseTransportLimiting() && pStreamSocket->GetQuota(CBandwidthLimiter::eDownChannel) < 0)
		return 0;
#endif
	ASSERT(uToWrite > 0);
	qint64 uWriten = Send(data, uToWrite);
	if(uWriten == -1)
		return -1;
	ASSERT(uToWrite >= uWriten);

	pStreamSocket->CountBandwidth(CBandwidthLimiter::eUpChannel, uWriten, pStreamSocket->IsUpload() ? CBandwidthCounter::ePayload : CBandwidthCounter::eProtocol);
	return uWriten;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//

CStreamSocket::CStreamSocket(CSocketThread* pSocketThread)
 : CBandwidthLimiter(pSocketThread)
{
	m_State = eNotConnected;

	m_LastActivity = ::GetCurTick();

	m_QueuedSize = 0;

	m_bUpload = false;
	m_bDownload = false;

	m_Server = NULL;
	m_pSocket = NULL;
	m_Port = 0;
}

CStreamSocket::~CStreamSocket() 
{
	ASSERT(m_pSocket == NULL);
}

void CStreamSocket::Init(CAbstractSocket* pSocket, CStreamServer* pServer)
{
	ASSERT(m_pSocket == NULL);

	m_Server = pServer;

	connect(pSocket, SIGNAL(Connected()), this, SLOT(OnConnected()));
	connect(pSocket, SIGNAL(Disconnected(int)), this, SLOT(OnDisconnected(int)));

	m_pSocket = pSocket;

	if(pSocket->IsValid())
	{
		m_State = eIncoming;

		m_Address = m_pSocket->GetAddress(&m_Port);
		ASSERT(m_Address.Type() != CAddress::None);
	}
}

void CStreamSocket::Dispose()
{
	delete m_pSocket;
	m_pSocket = NULL;

	CBandwidthLimiter::Dispose();
}

bool CStreamSocket::Process()
{
	ASSERT(m_pSocket);
	bool bRet = m_pSocket->Process();
	if(m_State == eHalfConnected || m_State == eConnected)
	{
		bRet |= WriteToSocket() != 0;
		bRet |= ReadFromSocket() != 0;
	}
	if(bRet)
		m_LastActivity = ::GetCurTick();
	return bRet;
}

void CStreamSocket::StreamOut(byte* Data, size_t Length)
{
	m_OutBuffer.AppendData(Data, Length);
	//if(m_State == eConnected)
	//	emit bytesWritten(0);
}

void CStreamSocket::StreamIn(byte* Data, size_t Length)
{
	m_InBuffer.AppendData(Data, Length);
}

void CStreamSocket::QueueStream(uint64 ID, const QByteArray& Stream)
{
	SQueueEntry Entry;
	Entry.Stream = Stream;
	Entry.ID = ID;

	QMutexLocker Locker(&m_Mutex);
	m_QueuedStreams.append(Entry);
	m_QueuedSize += Entry.Stream.size();

	//if(m_State == eConnected)
	//	emit bytesWritten(0);
}

void CStreamSocket::ClearQueue()
{
	QMutexLocker Locker(&m_Mutex);
	m_QueuedStreams.clear();
	m_QueuedSize = 0;
}

void CStreamSocket::CancelStream(uint64 ID)
{
	QMutexLocker Locker(&m_Mutex);
	for(int i=0; i < m_QueuedStreams.size(); i++)
	{
		if(m_QueuedStreams.at(i).ID == ID)
		{
			m_QueuedSize -= m_QueuedStreams.at(i).Stream.size();
			m_QueuedStreams.removeAt(i);
			break;
		}
	}
}

qint64 CStreamSocket::WriteToSocket()
{
	QMutexLocker Locker(&m_Mutex);
	
	uint64 Total = 0;
	while(m_OutBuffer.GetSize() > 0 || !m_QueuedStreams.isEmpty())
	{
		if(m_OutBuffer.GetSize() == 0 && !m_QueuedStreams.isEmpty())
		{
			SQueueEntry Entry = m_QueuedStreams.takeFirst();
			m_QueuedSize -= Entry.Stream.size();
			StreamOut((byte*)Entry.Stream.data(), Entry.Stream.size());
			emit NextPacketSend();
		}

		quint64 uWriten = m_pSocket->Write((char*)m_OutBuffer.GetBuffer(), m_OutBuffer.GetSize());
		if(uWriten == 0 || uWriten == -1)
			break;
		m_OutBuffer.ShiftData(uWriten);
		Total += uWriten;

		if(m_OutBuffer.GetSize() > 0)
			break; // if we did not send all it means the socket blocked, so we dont try again or else we would set the blocking flag
	}
	return Total;
}

qint64 CStreamSocket::ReadFromSocket()
{
	QMutexLocker Locker(&m_Mutex);

	quint64 Total = 0;
	while(m_pSocket->RecvPending())
	{
		const qint64 Size = 16*1024;
		char Buffer[Size];
		qint64 uRead = m_pSocket->Read(Buffer,Size);
		if(uRead == 0 || uRead == -1)
			break;
		StreamIn((byte*)Buffer, uRead);
		Total += uRead;
	}
	if(Total)
		ProcessStream();
    return Total;
}

void CStreamSocket::ConnectToHost(const CAddress& Address, quint16 Port)
{
	ASSERT(m_pSocket);
	QMetaObject::invokeMethod(this, "OnConnectToHost", Qt::AutoConnection, Q_ARG(CAddress, Address), Q_ARG(quint16, Port));
}

void CStreamSocket::OnConnectToHost(const CAddress& Address, quint16 Port)
{
	m_State = eConnecting;
	ASSERT(m_pSocket);
	m_pSocket->ConnectToHost(Address, Port);

	QMutexLocker Locker(&m_Mutex);
	m_Address = Address;
	m_Port = Port;
}

void CStreamSocket::DisconnectFromHost()
{
	ASSERT(m_pSocket);
	QMetaObject::invokeMethod(this, "OnDisconnectFromHost", Qt::AutoConnection);
}

void CStreamSocket::OnDisconnectFromHost()
{
	ASSERT(m_pSocket);
	m_pSocket->DisconnectFromHost();
}

void CStreamSocket::OnConnected()
{
	m_State = eConnected; 
	emit Connected();
	//emit bytesWritten(0);
}

void CStreamSocket::OnDisconnected(int Error)
{
	if(m_State == eConnected)
		m_State = eDisconnected; 
	else
		m_State = eConnectFailed; 
	emit Disconnected(Error);
}

void CStreamSocket::DisconnectFromHost(int Error)
{
	m_pSocket->DisconnectFromHost(Error);
}

QString CStreamSocket::GetErrorStr(int Error)
{
	switch(Error)
	{
	case SOCK_ERR_NONE:			return tr("Socket Closed");
	case SOCK_ERR_NETWORK:		return tr("Network Error");
	case SOCK_ERR_REFUSED:		return tr("Connection Refused");
	case SOCK_ERR_RESET:		return tr("Connection Reset");
	case SOCK_ERR_CRYPTO:		return tr("Encryption Failed");
	case SOCK_ERR_OVERSIZED:	return tr("Buffer Limit Excided");
	default:					return tr("Unknown Socket Error");
	}
}
