#pragma once

#include "../../Framework/Buffer.h"
#include "../../Framework/ObjectEx.h"
#include "StreamSocket.h"
#include "ListenSocket.h"

//class CTcpListener: public QTcpServer
class CTcpListener: public QObject
{
	Q_OBJECT

public:
	CTcpListener(QObject* qObject = NULL);
	~CTcpListener();

	bool				IsValid();
	void				Close();
	bool				Listen(quint16 Port, const CAddress& IP);

	void				Process();

signals:
	void				Connection(CStreamSocket* pSocket);

protected:
	friend class CTcpSocket;

	sockaddr*			m_sa;
	int					m_sa_len;
	SOCKET				m_Socket;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////
//

#define SOCK_TCP 'tcp'

class CTcpSocket: public CAbstractSocket
{
	Q_OBJECT

public:
	CTcpSocket(CStreamSocket* parent);
	virtual ~CTcpSocket();

	virtual bool		IsValid()									{return m_Socket != INVALID_SOCKET;}

	virtual bool		Process();

	virtual void		SetSocket(SOCKET Socket, bool Connected = false);
	virtual void		ClearSocket(bool Closed = false);

	virtual void		ConnectToHost(const CAddress& Address, quint16 Port);
	virtual void		DisconnectFromHost(int Error = 0);
	virtual	CAddress GetAddress(quint16* pPort = NULL) const;

	virtual UINT		GetSocketType() const						{return SOCK_TCP;}

	virtual qint64		Recv(char *data, qint64 maxlen);
    virtual qint64		Send(const char *data, qint64 len);

	virtual qint64		RecvPending() const;

protected:
	SOCKET				m_Socket;
	//CStreamSocket::EState m_State;
	bool				m_Connected;
#ifdef MSS
	void				AddFrameOH(int Size, CBandwidthCounter::EType TypeDown, CBandwidthCounter::EType TypeUp);
	void				AddSynOH(bool In);
	int					m_FrameOH;
#endif
};
