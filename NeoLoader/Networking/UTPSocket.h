#pragma once
//#include "GlobalHeader.h"

#include "../../Framework/ObjectEx.h"
#include "../../Framework/Buffer.h"
#include "../../Framework/Address.h"
#include "StreamSocket.h"
#include "ListenSocket.h"

class CUtpTimer: public QObject
{
	Q_OBJECT
public:
	CUtpTimer(QObject* qObject = NULL);
	~CUtpTimer();
protected:
	void						timerEvent(QTimerEvent* pEvent);
	int							m_uTimerID;
};

class CUtpListener: public QObject
{
	Q_OBJECT

public:
	CUtpListener(QObject* qObject = NULL);
	~CUtpListener();

	virtual bool		IsValid();
	virtual void		Close();
	virtual bool		Bind(quint16 Port, const CAddress& IP);

	virtual void		Process();

	virtual void		SendDatagram(const char *data, qint64 len, const CAddress &host, quint16 port);
	virtual void		ReciveDatagram(const char *data, qint64 len, const CAddress &host, quint16 port);

signals:
	void				Connection(CStreamSocket* pSocket);

protected:
	friend void got_incoming_connection(void *userdata, struct UTPSocket *socket);

	SOCKET				m_Socket;

	//static int			m_Counter;
	//static CUtpTimer*	m_pTimer;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////
//

#define SOCK_UTP 'utp'

class CUtpSocket: public CAbstractSocket
{
	Q_OBJECT

public:
	CUtpSocket(CStreamSocket* parent);
	virtual ~CUtpSocket();

	virtual bool		IsValid()									{return m_Socket != NULL;}

	virtual bool		Process();

	virtual void		SetSocket(struct UTPSocket* Socket, bool Connected = false);

	virtual void		ConnectToHost(const CAddress& Address, quint16 Port);
	virtual void		DisconnectFromHost(int Error = 0);
	virtual	CAddress GetAddress(quint16* pPort = NULL) const;

	virtual UINT		GetSocketType() const						{return SOCK_UTP;}

	virtual qint64		Recv(char *data, qint64 maxlen);
    virtual qint64		Send(const char *data, qint64 len);

	virtual qint64		RecvPending() const							{return m_ReadBuffer.GetSize();}

protected:
	friend void utp_state(void* userdata, int state);
	friend void utp_error(void* userdata, int errcode);
	friend void utp_read(void* userdata, const byte* bytes, size_t count);
	friend size_t utp_get_rb_size(void* userdata);
	friend void utp_write(void* userdata, byte* bytes, size_t count);

	struct UTPSocket*	m_Socket;
	//CStreamSocket::EState m_State;
	int					m_iClosed;

	CBuffer				m_ReadBuffer;
	CBuffer				m_WriteBuffer;
};
