#pragma once
#include "../ObjectEx.h"
class CHttpSocket;
class CHttpServer;
class CHttpHandler;
#include "HttpHelper.h"

class NEOHELPER_EXPORT CHttpHandler
{
protected:
	friend class CHttpServer;
	friend class CHttpSocket;

	virtual void	HandleRequest(CHttpSocket* pRequest) = 0;
	virtual void	ReleaseRequest(CHttpSocket* pRequest) = 0;
};


class NEOHELPER_EXPORT CHttpServer: public QObjectEx
{
	Q_OBJECT

public:
	CHttpServer(int Port, QObject* qObject = NULL);

	void			Process();

	int				GetPort()	{return m_Port;}

	bool			Listen(int Port);

	void			RegisterHandler(CHttpHandler* pHandler, QString Path = "", quint16 LocalPort = 0);

	void			SetKeepAlive(uint32 KeepAlive) {m_KeepAlive = KeepAlive;}
	void			SetTransferBufferSize(uint64 TransferBufferSize) {m_TransferBufferSize = TransferBufferSize;}

private slots:
	void			OnConnection();

	void			OnReadyRead();
	void			OnBytesWritten(qint64 bytes);
	void			OnDisconnect();

protected:
	friend class CHttpSocket;

	void			HandleSocket(CHttpSocket* pHttpSocket);
	CHttpHandler*	GetHandler(QString Path, quint16 LocalPort);

	QMap<QTcpSocket*, CHttpSocket*>	m_Sockets;
	QMap<QString, QPair<CHttpHandler*, quint16> >	m_Handlers;

	QMap<int, QTcpServer*>	m_Listners;
	int				m_Port;

	uint64			m_TransferBufferSize;
	uint32			m_KeepAlive;
};
