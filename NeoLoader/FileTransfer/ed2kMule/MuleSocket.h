#pragma once

#include "../../Networking/StreamSocket.h"
#include "../../../Framework/Buffer.h"

class CSymmetricKey;
class CMuleServer;
class CSimpleDH;

class CMuleSocket: public CStreamSocket
{
	Q_OBJECT

public:
	CMuleSocket(CSocketThread* pSocketThread);
	virtual ~CMuleSocket();

	virtual void		SendPacket(const QByteArray& Packet, uint8 Prot);
	virtual void		QueuePacket(uint64 ID, const QByteArray& Packet, uint8 Prot);

	virtual void		InitCrypto(const QByteArray& UserHash);
	virtual void		InitCrypto();
	virtual void		ProcessCrypto();

	virtual bool		IsEncrypted();

signals:
	void				ReceivedPacket(QByteArray Packet, quint8 Prot);

private slots:
	virtual void		OnConnected();

protected:
	friend class CMuleServer;

	virtual void		StreamOut(byte* Data, size_t Length);
	virtual void		StreamIn(byte* Data, size_t Length);
	virtual void		ProcessStream();

	uint8				m_NextPacketProt;
	quint32				m_NextPacketLength;

	CSymmetricKey*		m_CryptoKey;
	CSimpleDH*			m_KeyExchange;
	enum ECrypto
	{
		eInit,
		ePending,
		ePadding,
		eDone
	}					m_CryptoState;
};
