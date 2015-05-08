#pragma once

#include "../../Networking/UTPSocket.h"
#include "../../../Framework/Buffer.h"

class CBandwidthLimit;
class CStreamSocket;
class CMuleUDP;

class CMuleServer: public CStreamServer
{
	Q_OBJECT

public:

	CMuleServer(const QByteArray& UserHash);

	virtual bool					IsExpected(const CAddress& Address)				{QMutexLocker Locker(&m_CryptoMutex); return m_Expected.contains(Address);}
	void							SetExpected(const QList<CAddress>& Expected)	{QMutexLocker Locker(&m_CryptoMutex); m_Expected = Expected;}

	virtual	CStreamSocket*			AllocSocket(SOCKET Sock = INVALID_SOCKET)		{return AllocSocket(false, Sock != INVALID_SOCKET ? &Sock : NULL);}
	virtual	CStreamSocket*			AllocUTPSocket(struct UTPSocket* pSock = NULL)	{return AllocSocket(true, pSock);}

	void							SetupCrypto(const CAddress& Address, quint16 Port, const QByteArray& UserHash);
	void							UpdateCrypto(const CAddress& Address, quint16 Port);
	bool							HasCrypto(const CAddress& Address, quint16 Port);
	QByteArray						GetUserHash(const CAddress& Address, quint16 Port);

	QByteArray						GetUserHash()		{QMutexLocker Locker(&m_CryptoMutex); return m_UserHash;}

	void							SetKadCrypto(const QByteArray& KadID, uint32 UDPKey) {QMutexLocker Locker(&m_CryptoMutex); m_KadID = KadID; m_UDPKey = UDPKey;}
	QByteArray						GetKadID()			{QMutexLocker Locker(&m_CryptoMutex); return m_KadID;}
	uint32							GetUDPKey()			{QMutexLocker Locker(&m_CryptoMutex); return m_UDPKey;}
	uint32							GetUDPKey(const CAddress& Address);

	void							SetServerKey(const CAddress& Address, uint32 uSvrKey);
	bool							GetServerKey(const CAddress& Address, uint32 &uSvrKey);

public slots:
	void							SendUDPPacket(QByteArray Packet, quint8 Prot, CAddress Address, quint16 uUDPPort, QByteArray Hash);
	void							SendKadPacket(QByteArray Packet, quint8 Prot, CAddress Address, quint16 uKadPort, QByteArray NodeID, quint32 UDPKey);
	void							SendSvrPacket(QByteArray Packet, quint8 Prot, CAddress Address, quint16 uSvrPort, quint32 UDPKey);

signals:
	void							ProcessUDPPacket(QByteArray Packet, quint8 Prot, CAddress Address, quint16 uUDPPort);
	void							ProcessKadPacket(QByteArray Packet, quint8 Prot, CAddress Address, quint16 uKadPort, quint32 UDPKey, bool bValidKey);
	void							ProcessSvrPacket(QByteArray Packet, quint8 Prot, CAddress Address, quint16 uUDPPort);

protected:
	friend class CMuleUDP;
	virtual CStreamSocket*			AllocSocket(bool bUTP, void* p);

	QByteArray						m_UserHash;

	struct SAddress
	{
		SAddress() : uUDPPort(0) {}
		SAddress(const CAddress& Addr, uint16 uPort){ 
			Address = Addr; 
			uUDPPort = uPort; 
		}

		bool operator< (const SAddress &Other) const {
			if(Address.Type() != Other.Address.Type())
				return Address.Type() < Other.Address.Type();
			if(Address.Type() == CAddress::IPv6)
			{
				if(int cmp = memcmp(Address.Data(), Other.Address.Data(), 16))
					return cmp < 0;
			}
			else if(Address.Type() == CAddress::IPv4)
			{
				uint32 r = Address.ToIPv4();
				uint32 l = Other.Address.ToIPv4();
				if(r != l)
					return r < l;
			}
			else{
				ASSERT(0);
				return false;
			}
			return uUDPPort < Other.uUDPPort;
		}

		CAddress Address;
		uint16		 uUDPPort;
	};

	struct SCryptoInfo
	{
		SCryptoInfo() : LastSeen(0) {}
		QByteArray	UserHash;
		uint64		LastSeen;
	};

	QMap<SAddress, SCryptoInfo>		m_UserHashes;

	QMutex							m_CryptoMutex;

	QByteArray						m_KadID;
	uint32							m_UDPKey;

	struct SServerKey
	{
		SServerKey() : uServerKey(0) , uTempKey(0), uTimeOut(0) {}
		uint32 uServerKey;
		uint32 uTempKey;
		uint64 uTimeOut;
	};

	QMap<CAddress, uint32>			m_ServerKeys;

	QList<CAddress>					m_Expected;
};

////////////////////////////////////////////////////////////////////////////////////////////////
//

class CMuleUDP: public CUtpListener
{
	Q_OBJECT

public:
	CMuleUDP(QObject* qObject = NULL);

	virtual void					SendDatagram(uint8 Prot, const char *data, qint64 len, const CAddress &host, quint16 port, const QByteArray& Hash);
	virtual void					SendDatagram(uint8 Prot, const char *data, qint64 len, const CAddress &host, quint16 port, const QByteArray& KadID, uint32 UDPKey);
	virtual void					SendDatagram(uint8 Prot, const char *data, qint64 len, const CAddress &host, quint16 port, uint32 UDPKey);

	virtual void					SendDatagram(const char *data, qint64 len, const CAddress &host, quint16 port);
	virtual void					ReciveDatagram(const char *data, qint64 len, const CAddress &host, quint16 port);

protected:
	bool							EncryptUDP(CBuffer& Packet, const CAddress& Address, const QByteArray& UserHash, bool bKad = false, const QByteArray& KadID = QByteArray(), uint32 UDPKey = 0);
	int								DecryptUDP(CBuffer& Packet, const CAddress& Address, uint16 UDPPort, uint32& UDPKey, bool& bValidKey);

	void							EncryptUDP(CBuffer& Packet, uint32 SvrKey);
	int								DecryptUDP(CBuffer& Packet, uint32 SvrKey);
};