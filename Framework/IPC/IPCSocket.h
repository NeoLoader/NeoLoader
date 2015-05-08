#pragma once
#include "../../Framework/ObjectEx.h"
#include "../../Framework/Buffer.h"
#include "IPCServer.h"
#include "IPCClient.h"

class CKeyExchange;
class CSymmetricKey;
class CAbstractKey;

class NEOHELPER_EXPORT CIPCSocket: public QObjectEx
{
	Q_OBJECT

public:
	CIPCSocket(QLocalSocket* pLocal, bool bIncomming = false);
	CIPCSocket(QTcpSocket* pRemote, bool bIncomming = false);
	~CIPCSocket();

	virtual sint64		SendRequest(const QString& Name, const QVariant& Data);
	virtual bool		SendRequest(const QString& Name, const QVariant& Data, QVariant& Result, int TimeOut = 3000);
	virtual bool		SendResponse(const QString& Name, const QVariant& Data, sint64 Number);
	virtual sint64		SendEvent(const QString& Name, const QVariant& Data);

	virtual void		Disconnect(const QString& Error = "");
	virtual bool		IsConnected()	{return m_bConnected;}

	virtual void		SetLogin(const QString& User, const QString& Password)	{m_User = User; m_Password = Password;}
	virtual QString		GetLoginToken()							{return m_LoginToken;}
	
	virtual CIPCServer*	GetServer() {return qobject_cast<CIPCServer*>(parent());}
	//virtual CIPCClient*	GetClient() {return qobject_cast<CIPCClient*>(parent());}

	enum EEncoding
	{
		eUnknown = 0,
		eXML,
		eJson,
		eBencode,
		eBinary,
	};

	static QByteArray	Variant2String(const QVariant& Variant, EEncoding Encoding);
	static QVariant		String2Variant(const QByteArray& String, EEncoding &Encoding);

signals:
	void				Connected();
	void				Request(const QString& Name, const QVariant& Data, qint64 Number);
	void				Response(const QString& Name, const QVariant& Data, qint64 Number);
	void				Event(const QString& Name, const QVariant& Data, qint64 Number);
	void				Disconnected();

private slots:
	virtual void		OnConnected();
	virtual void		OnDisconnected();
	virtual void		OnReadyRead();
	//virtual void		OnError(QAbstractSocket::SocketError socketError);
	//virtual void		OnError(QLocalSocket::LocalSocketError socketError);
	virtual void		OnTimer();

protected:
	virtual void		Init();
	virtual void		CloseSocket();
	virtual void		Send(const QString& Type, const QString& Name, const QVariant& Data, EEncoding Encoding, sint64 Number);
	virtual void		Receive(const QString& Type, const QString& Name, const QVariant& Data, EEncoding Encoding, sint64 Number);
	virtual QIODevice*	Dev()		{return m_pLocal ? (QIODevice*)m_pLocal : (QIODevice*)m_pRemote;}

	virtual bool		ProcessEncryption(const QVariantMap& Data);
	virtual void		SendLoginReq();
	virtual void		ProcessLoginReq(const QVariantMap& Data);
	virtual void		ProcessLoginRes(const QVariantMap& Data);

	QLocalSocket*		m_pLocal;
	QTcpSocket*			m_pRemote;
	EEncoding			m_Encoding;
	CBuffer				m_ReadBuffer;
	QString				m_User;
	QString				m_Password;
	QString				m_LoginToken;
	CSymmetricKey*		m_CryptoKey;
	CKeyExchange*		m_Exchange;
	bool				m_bConnected;
	int					m_Encrypt;
	sint64				m_Counter;
	QVariant*			m_pResult;
};
