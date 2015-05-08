#pragma once

#include "../../Networking/StreamSocket.h"
#include "../../../Framework/Buffer.h"

class CSymmetricKey;
class CSimpleDH;
class CAbstractKey;
class CTorrentServer;

#define PROTOCOL_ID "BitTorrent protocol"
#define HAND_SHAKE_SIZE (1 + 19 + 8 + 20 + 20)

class CTorrentSocket: public CStreamSocket
{
	Q_OBJECT

public:
	CTorrentSocket(CSocketThread* pSocketThread);
	virtual ~CTorrentSocket();

	virtual void		SendHandshake(const QByteArray& Handshake);
	virtual void		SendPacket(const QByteArray& Packet);
	virtual void		QueuePacket(uint64 ID, const QByteArray& Packet);

	virtual bool		InitCrypto(const QByteArray& InfoHash);
	virtual void		ProcessCrypto();

	virtual bool		SupportsEncryption();
	virtual bool		IsEncrypted()			{return m_CryptoKey != NULL;}
	virtual bool		CryptoInProgress()		{return m_CryptoState != eInit && m_CryptoState != eDone;}

signals:
	void				ReceivedHandshake(QByteArray Handshake);
	void				ReceivedPacket(QByteArray Packet);

private slots:
	virtual void		OnConnected();

protected:
	friend class CTorrentServer;

	virtual void		StreamOut(byte* Data, size_t Length);
	virtual void		StreamIn(byte* Data, size_t Length);
	virtual void		ProcessStream();
	virtual void		SetupRC4(bool Sender);
	virtual bool		FindSyncMark();
	virtual void		CryptoFailed();
	virtual void		FinishCrypto(uint16 Offset = 0);

	virtual void		SendPaddedKey(CAbstractKey* pKey);

	quint32				m_NextPacketLength;

	QByteArray			m_InfoHash;
	CSimpleDH*			m_KeyExchange;
	CAbstractKey*		m_TempKey;
	CSymmetricKey*		m_CryptoKey;
	enum ECrypto
	{
		eInit,
		eExchangeKey,
		
		eFindHash,
		eReadHash,
		eReadProvide,
		eEndProvide,
		eReadIA,

		eFindSelect,
		eReadSelect,
		eEndSelect,
		
		eDone
	}					m_CryptoState;
	UINT				m_CryptoMethod;
	QByteArray			m_SyncMark;
	QByteArray			m_Atomic;
};
