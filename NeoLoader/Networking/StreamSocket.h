#pragma once

#include "../../Framework/Buffer.h"
#include "../../Framework/ObjectEx.h"
#include "../../Framework/Address.h"
#include "./BandwidthControl/BandwidthLimiter.h"

class CStreamServer;
class CStreamSocket;

#define SOCK_ERR_NONE		0
#define SOCK_ERR_NETWORK	1
#define SOCK_ERR_REFUSED	2
#define SOCK_ERR_RESET		3
#define SOCK_ERR_CRYPTO		4
#define SOCK_ERR_OVERSIZED	5

class CAbstractSocket: public QObject
{
	Q_OBJECT

public:
	CAbstractSocket(CStreamSocket* pStream);
	virtual ~CAbstractSocket() {}

	CStreamSocket*		GetStream()		{return (CStreamSocket*)parent();}

	virtual bool		IsValid() = 0;

	virtual bool		Process() = 0;

	virtual void		ConnectToHost(const CAddress& Address, quint16 Port) = 0;
	virtual void		DisconnectFromHost(int Error = 0) = 0;
	virtual	CAddress	GetAddress(quint16* pPort = NULL) const = 0;

	virtual UINT		GetSocketType() const = 0;

	virtual qint64		Read(char *data, qint64 maxlen);
    virtual qint64		Write(const char *data, qint64 len);
	virtual qint64		Recv(char *data, qint64 maxlen) = 0;
    virtual qint64		Send(const char *data, qint64 len) = 0;

	virtual qint64		RecvPending() const = 0;
	virtual bool		SendBlocking() const  {return m_Blocking;}

signals:
	void				Connected();
	void				Disconnected(int Error = 0);

protected:
	bool				m_Blocking;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////
//

class CStreamSocket: public QObject, public CBandwidthLimiter
{
	Q_OBJECT

public:
	CStreamSocket(CSocketThread* pSocketThread);
	virtual ~CStreamSocket();
	virtual void		Init(CAbstractSocket* pSocket, CStreamServer* pServer);
	virtual void		Dispose();

	virtual bool		Process();

	virtual void		ClearQueue();
	virtual quint64		QueueSize()									{QMutexLocker Locker(&m_Mutex); return m_QueuedSize;}
	virtual int			QueueCount()								{QMutexLocker Locker(&m_Mutex); return m_QueuedStreams.count();}
	virtual void		CancelStream(uint64 ID);

	enum EState
	{
		eNotConnected = 0,
		eConnecting,
		eHalfConnected,
		eIncoming,
		eConnected,
		eDisconnecting,
		eDisconnected,
		eConnectFailed
	};
	virtual EState		GetState() const							{return m_State;}

	virtual uint64		GetLastActivity() const						{return m_LastActivity;}

	void 				ConnectToHost(const CAddress& Address, quint16 Port);
	void 				DisconnectFromHost();

	const CAddress&	GetAddress() const								{QMutexLocker Locker(&m_Mutex); return m_Address;}
	quint16				GetPort() const								{QMutexLocker Locker(&m_Mutex); return m_Port;}

	UINT				GetSocketType() const						{return m_pSocket ? m_pSocket->GetSocketType() : 0;}

	virtual qint64		WriteToSocket();
    virtual qint64		ReadFromSocket();

	void				SetUpload(bool bSet)						{m_bUpload = bSet;}
	bool				IsUpload()									{return m_bUpload;}
	void				SetDownload(bool bSet)						{m_bDownload = bSet;}
	bool				IsDownload()								{return m_bDownload;}

	CStreamServer*		GetServer()									{return m_Server;}

	virtual bool		IsBlocking() const							{return m_pSocket ? m_pSocket->SendBlocking() : false;}

	void				AcceptConnect()								{if(m_State == eIncoming) m_State = eConnected;}

	static QString		GetErrorStr(int Error);

protected slots:
	virtual void		OnConnected();
	virtual void		OnDisconnected(int Error);

	virtual void 		OnConnectToHost(const CAddress& Address, quint16 Port);
	virtual void 		OnDisconnectFromHost();

signals:
	void				Connected();
	void				Disconnected(int Error = 0);
	void				NextPacketSend();

protected:
	virtual void		DisconnectFromHost(int Error);

	virtual void		QueueStream(uint64 ID, const QByteArray& Stream);
	virtual void		StreamOut(byte* Data, size_t Length);
	virtual void		StreamIn(byte* Data, size_t Length);
	virtual void		ProcessStream() = 0;

	volatile EState		m_State;

	volatile uint64		m_LastActivity;

	CBuffer				m_OutBuffer;
	CBuffer				m_InBuffer;

	struct SQueueEntry
	{
		QByteArray Stream;
		uint64 ID;
	};
	QList<SQueueEntry>	m_QueuedStreams;
	quint64				m_QueuedSize;

	mutable QMutex		m_Mutex;

	bool				m_bUpload;
	bool				m_bDownload;

	CStreamServer*		m_Server;

	CAbstractSocket*	m_pSocket;
	CAddress			m_Address;
	quint16				m_Port;
};

