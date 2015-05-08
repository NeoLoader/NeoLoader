#include "GlobalHeader.h"
#include "MuleServer.h"
#include "MuleSocket.h"
#include "../../NeoCore.h"
#include "../../../Framework/Cryptography/HashFunction.h"
#include "MuleTags.h"
#include "../Framework/Cryptography/SymmetricKey.h"
#include "../Framework/Cryptography/HashFunction.h"
#include "../../../Framework/Scope.h"
#include "../../../Framework/Exception.h"
#include "../Transfer.h"
#include "../../Networking/BandwidthControl/BandwidthLimit.h"

CMuleServer::CMuleServer(const QByteArray& UserHash) 
{
	m_UserHash = UserHash;
	m_UDPKey = 0;

	m_ServerV4uTP = new CMuleUDP(this);
	connect(m_ServerV4uTP, SIGNAL(Connection(CStreamSocket*)), this, SIGNAL(Connection(CStreamSocket*)));
	
	if(m_ServerV6)
	{
		m_ServerV6uTP = new CMuleUDP(this);
		connect(m_ServerV6uTP, SIGNAL(Connection(CStreamSocket*)), this, SIGNAL(Connection(CStreamSocket*)));
	}
}

CStreamSocket* CMuleServer::AllocSocket(bool bUTP, void* p)
{
	CMuleSocket* pSocket = new CMuleSocket((CSocketThread*)thread());
	pSocket->AddUpLimit(m_UpLimit);
	pSocket->AddDownLimit(m_DownLimit);
	if(bUTP)
		InitUTP(pSocket, (struct UTPSocket*)p);
	else
		Init(pSocket, p ? *(SOCKET*)p : INVALID_SOCKET);
	AddSocket(pSocket);
	return pSocket;
}

void CMuleServer::SetupCrypto(const CAddress& Address, quint16 Port, const QByteArray& UserHash)
{
	QMutexLocker Locker(&m_CryptoMutex);
	SCryptoInfo& CryptoInfo = m_UserHashes[SAddress(Address, Port)];
	CryptoInfo.UserHash = UserHash;
	CryptoInfo.LastSeen = GetCurTick();
	
	// EM-ToDo-Now: cleanup to old hashes from list
}

void CMuleServer::UpdateCrypto(const CAddress& Address, quint16 Port)
{
	QMutexLocker Locker(&m_CryptoMutex);
	m_UserHashes[SAddress(Address, Port)].LastSeen = GetCurTick();
}

QByteArray CMuleServer::GetUserHash(const CAddress& Address, quint16 Port)
{
	QMutexLocker Locker(&m_CryptoMutex);
	QMap<SAddress, SCryptoInfo>::iterator I = m_UserHashes.find(SAddress(Address, Port));
	if(I == m_UserHashes.end())
		return QByteArray(); // EM-ToDo-Now: decline connection if crypto is mandatory!!!!

	SCryptoInfo& CryptoInfo = I.value();
	CryptoInfo.LastSeen = GetCurTick();
	// Note: if we dont know how to encrypt but have got a incomming encrypted packet, 
	//	we use our own hash to encrpt as we expect the remote side to know it and try it.
	if(CryptoInfo.UserHash.isEmpty())
		return m_UserHash;
	return CryptoInfo.UserHash;
}

bool CMuleServer::HasCrypto(const CAddress& Address, quint16 Port)
{
	QMutexLocker Locker(&m_CryptoMutex);
	return m_UserHashes.contains(SAddress(Address, Port));
}

void CMuleServer::SendUDPPacket(QByteArray Packet, quint8 Prot, CAddress Address, quint16 uUDPPort, QByteArray Hash)
{
	if(Address.Type() == CAddress::IPv4)
		((CMuleUDP*)m_ServerV4uTP)->SendDatagram(Prot, Packet.data(), Packet.size(), Address, uUDPPort, Hash);
	else if(Address.Type() == CAddress::IPv6)
		((CMuleUDP*)m_ServerV6uTP)->SendDatagram(Prot, Packet.data(), Packet.size(), Address, uUDPPort, Hash);
}

void CMuleServer::SendKadPacket(QByteArray Packet, quint8 Prot, CAddress Address, quint16 uKadPort, QByteArray NodeID, quint32 UDPKey)
{
	if(Address.Type() == CAddress::IPv4)
		((CMuleUDP*)m_ServerV4uTP)->SendDatagram(Prot, Packet.data(), Packet.size(), Address, uKadPort, NodeID, UDPKey);
	//else if(Address.Type() == CAddress::IPv6)
	//	((CMuleUDP*)m_ServerV6uTP)->SendDatagram(Prot, Packet.data(), Packet.size(), Address, uKadPort, NodeID, UDPKey);
}

void CMuleServer::SendSvrPacket(QByteArray Packet, quint8 Prot, CAddress Address, quint16 uSvrPort, quint32 UDPKey)
{
	if(Address.Type() == CAddress::IPv4)
		((CMuleUDP*)m_ServerV4uTP)->SendDatagram(Prot, Packet.data(),Packet.size(), Address, uSvrPort, UDPKey);
	else if(Address.Type() == CAddress::IPv6)
		((CMuleUDP*)m_ServerV6uTP)->SendDatagram(Prot, Packet.data(),Packet.size(), Address, uSvrPort, UDPKey);
}

uint32 GetUDPVerifyKey(uint32 UDPKey, const CAddress& Address)
{
	uint32 IPv4 = _ntohl(Address.ToIPv4()); // In Network Order

	uint64 buffer = ((uint64)UDPKey) << 32 | IPv4;
	CHashFunction Hash(CAbstractKey::eMD5);
	Hash.Add((const uint8 *)&buffer, 8);
	Hash.Finish();
	return (uint32)(*((uint32*)Hash.GetKey()) ^ *((uint32*)Hash.GetKey() + 1) ^ *((uint32*)Hash.GetKey() + 2) ^ *((uint32*)Hash.GetKey() + 3)) % 0xFFFFFFFE + 1;
}

uint32 CMuleServer::GetUDPKey(const CAddress& Address)
{
	QMutexLocker Locker(&m_CryptoMutex);
	return GetUDPVerifyKey(m_UDPKey, Address);
}

void CMuleServer::SetServerKey(const CAddress& Address, uint32 uSvrKey)
{
	QMutexLocker Locker(&m_CryptoMutex);
	m_ServerKeys[Address] = uSvrKey;
}

bool CMuleServer::GetServerKey(const CAddress& Address, uint32 &uSvrKey)
{
	QMutexLocker Locker(&m_CryptoMutex);
	QMap<CAddress, uint32>::Iterator I = m_ServerKeys.find(Address);
	if(I != m_ServerKeys.end())
	{
		uSvrKey = I.value();
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////
//

CMuleUDP::CMuleUDP(QObject* qObject)
:CUtpListener(qObject)
{
}

void CMuleUDP::SendDatagram(uint8 Prot, const char *data, qint64 len, const CAddress &host, quint16 port, const QByteArray& Hash) // to Mule Client
{
	CBuffer RawPacket;
	RawPacket.AllocBuffer(1 + len);
	RawPacket.WriteValue<uint8>(Prot);
	RawPacket.WriteData(data, len);

	if(!Hash.isEmpty() && !EncryptUDP(RawPacket, host, Hash))
		return;

	CMuleServer* pServer = qobject_cast<CMuleServer*>(parent());
	pServer->CountUpUDP((int)RawPacket.GetSize(), host.Type());

	CUtpListener::SendDatagram((char*)RawPacket.GetBuffer(),RawPacket.GetSize(), host, port);
}

void CMuleUDP::SendDatagram(uint8 Prot, const char *data, qint64 len, const CAddress &host, quint16 port, const QByteArray& KadID, uint32 UDPKey) // to Kad Client
{
	CBuffer RawPacket;
	RawPacket.AllocBuffer(1 + len);
	RawPacket.WriteValue<uint8>(Prot);
	RawPacket.WriteData(data, len);

	if((!KadID.isEmpty() || UDPKey != 0) && !EncryptUDP(RawPacket, host, QByteArray(), true, KadID, UDPKey))
		return;

	CMuleServer* pServer = qobject_cast<CMuleServer*>(parent());
	pServer->CountUpUDP((int)RawPacket.GetSize(), host.Type());

	CUtpListener::SendDatagram((char*)RawPacket.GetBuffer(),RawPacket.GetSize(), host, port);
}

void CMuleUDP::SendDatagram(uint8 Prot, const char *data, qint64 len, const CAddress &host, quint16 port, uint32 UDPKey) // to Donkey Server
{
	CMuleServer* pServer = qobject_cast<CMuleServer*>(parent());
	pServer->SetServerKey(host, UDPKey);

	if(Prot)
	{
		CBuffer RawPacket;
		RawPacket.AllocBuffer(1 + len);
		RawPacket.WriteValue<uint8>(Prot);
		RawPacket.WriteData(data, len);

		if(UDPKey)
			EncryptUDP(RawPacket, UDPKey);

		pServer->CountUpUDP((int)RawPacket.GetSize(), host.Type());

		CUtpListener::SendDatagram((char*)RawPacket.GetBuffer(), RawPacket.GetSize(), host, port);
	}
	else
	{
		pServer->CountUpUDP((int)len, host.Type());

		CUtpListener::SendDatagram(data, len, host, port);
	}
}

union UUtpHdr
{
	uint32 Bits;
	struct SUtpHdr
	{
		uint32
		ver:	4,
		type:	4,
		ext:	8,
		connid:	16;
	} Fields;
};

void CMuleUDP::SendDatagram(const char *data, qint64 len, const CAddress &host, quint16 port) // to UTP Tunnel to Mule
{
	CMuleServer* pServer = qobject_cast<CMuleServer*>(parent());
	QByteArray UserHash = pServer->GetUserHash(host, port);

	ASSERT(len >= 4);

	UUtpHdr UtpHdr;
	UtpHdr.Bits = *((uint32*)data);
	if(UtpHdr.Fields.type == 4) // ST_SYN
	{
		CBuffer KeyPacket;
		KeyPacket.AllocBuffer(1 + 16);
		KeyPacket.WriteValue<uint8>(0xFF); // Key Frame
		KeyPacket.WriteQData(pServer->GetUserHash());

		SendDatagram(OP_UDPRESERVEDPROT2, (char*)KeyPacket.GetBuffer(), KeyPacket.GetSize(), host, port, UserHash);
	}

	CBuffer RawPacket;
	RawPacket.AllocBuffer(1 + 1 + len);
	RawPacket.WriteValue<uint8>(OP_UDPRESERVEDPROT2);
	RawPacket.WriteValue<uint8>(0x00); // UTP Frame
	RawPacket.WriteData(data, len);

	if(!UserHash.isEmpty() && !EncryptUDP(RawPacket, host, UserHash))
		return;

	ASSERT(RawPacket.GetSize() > len); // dont count utp data, only crypto and mule header
	pServer->CountUpUDP(RawPacket.GetSize() - len, CAddress::None);

	CUtpListener::SendDatagram((char*)RawPacket.GetBuffer(), RawPacket.GetSize(), host, port);
}

void CMuleUDP::ReciveDatagram(const char *data, qint64 len, const CAddress &host, quint16 port)
{
	CMuleServer* pServer = qobject_cast<CMuleServer*>(parent());
	bool bUTP = false;
	size_t CryptoSize = 0;
	try
	{
		CBuffer RawPacket(data, len, true);

		uint8 Prot = RawPacket.ReadValue<uint8>();
		uint32 UDPKey = 0;
		bool bValidKey = false;
		if(pServer->GetServerKey(host, UDPKey)) // Note: we do not expect to be talked to by a server unless we talked first
		{
			switch (Prot)
			{
				case OP_EDONKEYPROT:
					break; // not encrypted
				default: //encrypted
				{
					if(!DecryptUDP(RawPacket, UDPKey))
						return;
					Prot = RawPacket.ReadValue<uint8>();
				}
			}
		}
		else
		{
			switch (Prot)
			{
				//case OP_EDONKEYPROT: // Note normal mules allow this
				case OP_EMULEPROT:
				case OP_KADEMLIAPACKEDPROT:
				case OP_KADEMLIAHEADER:
				case OP_UDPRESERVEDPROT1:
				case OP_UDPRESERVEDPROT2:
				case OP_PACKEDPROT:
					break; // not encrypted
				default: //encrypted
				{
					int Ret = DecryptUDP(RawPacket, host, port, UDPKey, bValidKey);
					if(Ret == 1 || Ret == 2)
						pServer->UpdateCrypto(host, port);
					else if(!Ret)
						return;
					CryptoSize = RawPacket.GetPosition(); // here we just skiped the crypto header so position == crypto size
					Prot = RawPacket.ReadValue<uint8>();
				}
			}
		}

		
		switch (Prot)
		{
			case OP_EDONKEYPROT:
				emit pServer->ProcessSvrPacket(QByteArray((const char*)RawPacket.GetData(0), (int)RawPacket.GetSizeLeft()), Prot, host, port);
				break;
			case OP_PACKEDPROT:
			case OP_EMULEPROT:
				emit pServer->ProcessUDPPacket(QByteArray((const char*)RawPacket.GetData(0), (int)RawPacket.GetSizeLeft()), Prot, host, port);
				break;
			case OP_KADEMLIAPACKEDPROT:
			case OP_KADEMLIAHEADER:
				emit pServer->ProcessKadPacket(QByteArray((const char*)RawPacket.GetData(0), (int)RawPacket.GetSizeLeft()), Prot, host, port, UDPKey, bValidKey);
				break;
			case OP_UDPRESERVEDPROT1:
				break;
			case OP_UDPRESERVEDPROT2:
			{
				uint8 Opcode = RawPacket.ReadValue<uint8>();
				if(Opcode == 0x00) // UTP Frame
				{
					bUTP = true;
					CUtpListener::ReciveDatagram((const char*)RawPacket.GetData(0), RawPacket.GetSizeLeft(), host, port);
				}
				else if(Opcode == 0xFF) // Key Frame
					pServer->SetupCrypto(host, port, RawPacket.ReadQData(16));
				break;
			}
		}
	}
	catch(const CException&)
	{
	}
	if(bUTP) // dont count utp data, only crypto and mule header
		pServer->CountDownUDP(1 + 1 + CryptoSize, CAddress::None);
	else
		pServer->CountDownUDP((int)len, host.Type());
}

/* Comment copyed form eMule (Fair Use):
****************************** ED2K Packets

	-Keycreation Client <-> Clinet:
	 - Client A (Outgoing connection):
				Sendkey:	Md5(<UserHashClientB 16><IPClientA 4><MagicValue91 1><RandomKeyPartClientA 2>)  23
	 - Client B (Incomming connection):
				Receivekey: Md5(<UserHashClientB 16><IPClientA 4><MagicValue91 1><RandomKeyPartClientA 2>)  23
	 - Note: The first 1024 Bytes will be _NOT_ discarded for UDP keys to safe CPU time

	- Handshake
			-> The handshake is encrypted - except otherwise noted - by the Keys created above
			-> Padding is cucrently not used for UDP meaning that PaddingLen will be 0, using PaddingLens up to 16 Bytes is acceptable however
		Client A: <SemiRandomNotProtocolMarker 7 Bits[Unencrypted]><ED2K Marker 1Bit = 1><RandomKeyPart 2[Unencrypted]><MagicValue 4><PaddingLen 1><RandomBytes PaddingLen%16>	
	
	- Additional Comments:
			- For obvious reasons the UDP handshake is actually no handshake. If a different Encryption method (or better a different Key) is to be used this has to be negotiated in a TCP connection
		    - SemiRandomNotProtocolMarker is a Byte which has a value unequal any Protocol header byte. This is a compromiss, turning in complete randomness (and nice design) but gaining
			  a lower CPU usage
		    - Kad/Ed2k Marker are only indicators, which possibility could be tried first, and should not be trusted

****************************** KAD Packets
			  
	-Keycreation Client <-> Client:
											(Used in general in request packets)
	 - Client A (Outgoing connection):
				Sendkey:	Md5(<KadID 16><RandomKeyPartClientA 2>)  18
	 - Client B (Incomming connection):
				Receivekey: Md5(<KadID 16><RandomKeyPartClientA 2>)  18
	               -- OR --					(Used in general in response packets)
	 - Client A (Outgoing connection):
				Sendkey:	Md5(<ReceiverKey 4><RandomKeyPartClientA 2>)  6
	 - Client B (Incomming connection):
				Receivekey: Md5(<ReceiverKey 4><RandomKeyPartClientA 2>)  6

	 - Note: The first 1024 Bytes will be _NOT_ discarded for UDP keys to safe CPU time

	- Handshake
			-> The handshake is encrypted - except otherwise noted - by the Keys created above
			-> Padding is cucrently not used for UDP meaning that PaddingLen will be 0, using PaddingLens up to 16 Bytes is acceptable however
		Client A: <SemiRandomNotProtocolMarker 6 Bits[Unencrypted]><Kad Marker 2Bit = 0 or 2><RandomKeyPart 2[Unencrypted]><MagicValue 4><PaddingLen 1><RandomBytes PaddingLen%16><ReceiverVerifyKey 4><SenderVerifyKey 4>

	- Overhead: 16 Bytes per UDP Packet
	
	- Kad/Ed2k Marker:
		 x 1	-> Most likely an ED2k Packet, try Userhash as Key first
		 0 0	-> Most likely an Kad Packet, try NodeID as Key first
		 1 0	-> Most likely an Kad Packet, try SenderKey as Key first

	- Additional Comments:
			- For obvious reasons the UDP handshake is actually no handshake. If a different Encryption method (or better a different Key) is to be used this has to be negotiated in a TCP connection
		    - SemiRandomNotProtocolMarker is a Byte which has a value unequal any Protocol header byte. This is a compromiss, turning in complete randomness (and nice design) but gaining
			  a lower CPU usage
		    - Kad/Ed2k Marker are only indicators, which possibility could be tried first, and need not be trusted
			- Packets which use the senderkey are prone to BruteForce attacks, which take only a few minutes (2^32)
			  which is while not acceptable for encryption fair enough for obfuscation
*/

bool CMuleUDP::EncryptUDP(CBuffer& Packet, const CAddress& Address, const QByteArray& UserHash, bool bKad, const QByteArray& KadID, uint32 UDPKey)
{
	if(Address.Type() == CAddress::None)
		return false;

	uint16 Rand = 0x08; //GetRand64();

	int Kad = 0;
	CHashFunction MD5Hash(CAbstractKey::eMD5);
	if(bKad)
	{
		if(KadID.isEmpty() && UDPKey != 0)
		{
			Kad = 2;
			CBuffer KeyData(6);
			KeyData.WriteValue<uint32>(UDPKey);
			KeyData.WriteValue<uint16>(Rand);
			MD5Hash.Add(&KeyData);
		}
		else if(!KadID.isEmpty())
		{
			Kad = 1;
			CBuffer KeyData(18);

			KeyData.WriteQData(KadID, 16); // Note that kad ID is already properly fucked up!

			KeyData.WriteValue<uint16>(Rand);
			MD5Hash.Add(&KeyData);
		}
		else
		{
			ASSERT(0);
			return false;
		}
	}
	else
	{
		CAddress MyAddress = ((CMuleServer*)parent())->GetAddress(Address.Type()); // EM-ToDo: use more reliable IP
		if(MyAddress.IsNull())
			return false;

		if(!UserHash.isEmpty())
		{
			CBuffer KeyData(23);
			KeyData.WriteQData(UserHash, 16);
			if(MyAddress.Type() == CAddress::IPv4)
				KeyData.WriteValue<uint32>(_ntohl(MyAddress.ToIPv4()));
			else if(MyAddress.Type() == CAddress::IPv6)
				KeyData.WriteData(MyAddress.Data(), 16);
			//ASSERT(KeyData.GetPosition() == 20);
			KeyData.WriteValue<uint8>(MAGICVALUE_UDP);
			KeyData.WriteValue<uint16>(Rand);
			MD5Hash.Add(&KeyData);
		}
		else
		{
			ASSERT(0);
			return false;
		}
	}

	MD5Hash.Finish();

	uint8 Prot = 0;
	for (;;) 
	{
		Prot = 0x08; //GetRand64();

		// Set Marker Bits
		Prot = Kad ? (Prot & 0xFE) : (Prot | 0x01);
		if (Kad)
			Prot = (Kad == 2) ? ((Prot & 0xFE) | 0x02) : (Prot & 0xFC);
		else
			Prot = (Prot | 0x01);

		switch (Prot) 
		{
			case OP_EDONKEYPROT: // Note normal mules allow this
			case OP_EMULEPROT:
			case OP_KADEMLIAPACKEDPROT:
			case OP_KADEMLIAHEADER:
			case OP_UDPRESERVEDPROT1:
			case OP_UDPRESERVEDPROT2:
			case OP_PACKEDPROT:
				continue;
			//default: 
		}
		break;
	}

	uint8 Padding = 0; // no padding for UDP
	CBuffer TmpPacket(CRYPT_HEADER_WITHOUTPADDING + Padding + (bKad ? 8 : 0) + Packet.GetSize());
	TmpPacket.WriteValue<uint8>(Prot);
	TmpPacket.WriteValue<uint16>(Rand);
	ASSERT(TmpPacket.GetPosition() == 3);
	TmpPacket.WriteValue<uint32>(MAGICVALUE_UDP_SYNC_CLIENT);
	TmpPacket.WriteValue<uint8>(Padding);
	if(Padding)
		TmpPacket.WriteQData(CAbstractKey(Padding, true).ToByteArray(), Padding);
	if(bKad)
	{
		TmpPacket.WriteValue<uint32>(UDPKey);
		TmpPacket.WriteValue<uint32>(((CMuleServer*)parent())->GetUDPKey(Address));
	}
	TmpPacket.WriteData(Packet.GetBuffer(), Packet.GetSize());

	CScoped<CEncryptionKey> pRC4key = CEncryptionKey::Make(CAbstractKey::eWeakRC4);
	ASSERT(MD5Hash.GetSize() == 16);
	pRC4key->Setup(MD5Hash.GetKey(), 16);

	pRC4key->Process(TmpPacket.GetData(3, 0), TmpPacket.GetData(3, 0), TmpPacket.GetSize() - 3);

	size_t uLen = TmpPacket.GetSize();
	byte* pBuffer = TmpPacket.GetBuffer(true);
	Packet.SetBuffer(pBuffer, uLen, false);

	return true;
}

int CMuleUDP::DecryptUDP(CBuffer& Packet, const CAddress& Address, uint16 UDPPort, uint32& UDPKey, bool& bValidKey)
{
	if (Packet.GetSize() <= CRYPT_HEADER_WITHOUTPADDING)
		return 0;

	uint16 Rand = Packet.ReadValue<uint16>();

	size_t Pos = Packet.GetPosition();
	CHashFunction MD5Hash(CAbstractKey::eMD5);
	CScoped<CDecryptionKey> pRC4key = CDecryptionKey::Make(CAbstractKey::eWeakRC4);

	int Ret=1; // EM-ToDo check hint
	for(; Ret < 5; Ret++)
	{
		if(Ret == 1 || Ret == 2) // ed2k Packet
		{
			CBuffer KeyData(23);
			if(Ret == 1)
				KeyData.WriteQData(((CMuleServer*)parent())->GetUserHash(), 16);
			else if(Ret == 2)
			{
				// EM-ToDo-Now: remember what worked and try the woking one next time for this client
				CMuleServer* pServer = qobject_cast<CMuleServer*>(parent());
				QByteArray UserHash = pServer->GetUserHash(Address, UDPPort);
				if(UserHash.isEmpty())
					continue; // we dont know him and he does not know us
				KeyData.WriteQData(UserHash, 16);
			}

			if(Address.Type() == CAddress::IPv4)
				KeyData.WriteValue<uint32>(_ntohl(Address.ToIPv4()));
			else if(Address.Type() == CAddress::IPv6)
				KeyData.WriteData(Address.Data(), 16);
			//ASSERT(KeyData.GetPosition() == 20);
			KeyData.WriteValue<uint8>(MAGICVALUE_UDP);
			KeyData.WriteValue<uint16>(Rand);

			MD5Hash.Add(&KeyData);
		}
		else 
		{
			QByteArray KadID = ((CMuleServer*)parent())->GetKadID();
			if(KadID.isEmpty())
				continue; 
			if(Ret == 3) // kad with NodeID
			{
				CBuffer KeyData(18);

				// Note the kad ID has to be properly fucked up!
				uint32* uKadID = (uint32*)KadID.data();
				KeyData.WriteValue<uint32>(uKadID[0], true);
				KeyData.WriteValue<uint32>(uKadID[1], true);
				KeyData.WriteValue<uint32>(uKadID[2], true);
				KeyData.WriteValue<uint32>(uKadID[3], true);

				KeyData.WriteValue<uint16>(Rand);
				MD5Hash.Add(&KeyData);
			}
			else if(Ret == 4) // kad with UDPKey
			{
				CBuffer KeyData(6);
				KeyData.WriteValue<uint32>(((CMuleServer*)parent())->GetUDPKey(Address));
				KeyData.WriteValue<uint16>(Rand);
				MD5Hash.Add(&KeyData);
			}
		}
		MD5Hash.Finish();

		ASSERT(MD5Hash.GetSize() == 16);
		pRC4key->Setup(MD5Hash.GetKey(), 16);

		uint32 Test;
		ASSERT(sizeof(Test) == 4);
		pRC4key->Process(Packet.ReadData(4), (byte*)&Test, 4);
		if(Test == MAGICVALUE_UDP_SYNC_CLIENT)
			break; // juhu it probably decrypted just fine

		// Reset for next attempt
		Packet.SetPosition(Pos);
		MD5Hash.Reset();
		pRC4key->Reset();
	}
	if(Ret == 5)
	{
		LogLine(LOG_DEBUG | LOG_ERROR, tr("Recived offuscated eMule packet that failed to decrypt!"));
		return false;
	}

	uint8 Padding;
	ASSERT(sizeof(Padding) == 1);
	pRC4key->Process(Packet.ReadData(1), (byte*)&Padding, 1);
	if(Packet.GetSizeLeft() <= Padding)
		return 0; // passed magic value but still only junk

	if (Padding > 0) // skip padding
	{
		Packet.ReadData(Padding);
		pRC4key->Discard(Padding);
	}

	if((Ret == 3 || Ret == 4) && Packet.GetSizeLeft() > 8) // KadPacket
	{
		pRC4key->Process(Packet.GetData(0), Packet.GetData(0), 8);

		bValidKey = Packet.ReadValue<uint32>() == ((CMuleServer*)parent())->GetUDPKey(Address);
		UDPKey = Packet.ReadValue<uint32>();
	}

	size_t HeaderSize = Packet.GetPosition();
	pRC4key->Process(Packet.GetData(0),Packet.GetData(0),Packet.GetSizeLeft());
	Packet.ShiftData(HeaderSize); 
	
	return Ret;
}

/* Comment copyed form eMule (Fair Use):
****************************** Server Packets

	-Keycreation Client <-> Server:
	 - Client A (Outgoing connection client -> server):
				Sendkey:	Md5(<BaseKey 4><MagicValueClientServer 1><RandomKeyPartClientA 2>)  7
	 - Client B (Incomming connection):
				Receivekey: Md5(<BaseKey 4><MagicValueServerClient 1><RandomKeyPartClientA 2>)  7
	 - Note: The first 1024 Bytes will be _NOT_ discarded for UDP keys to safe CPU time

	- Handshake
			-> The handshake is encrypted - except otherwise noted - by the Keys created above
			-> Padding is cucrently not used for UDP meaning that PaddingLen will be 0, using PaddingLens up to 16 Bytes is acceptable however
		Client A: <SemiRandomNotProtocolMarker 1[Unencrypted]><RandomKeyPart 2[Unencrypted]><MagicValue 4><PaddingLen 1><RandomBytes PaddingLen%16>	

	- Overhead: 8 Bytes per UDP Packet
	
	- Security for Basic Obfuscation:
			- Random looking packets, very limited protection against passive eavesdropping single packets
	
	- Additional Comments:
			- For obvious reasons the UDP handshake is actually no handshake. If a different Encryption method (or better a different Key) is to be used this has to be negotiated in a TCP connection
		    - SemiRandomNotProtocolMarker is a Byte which has a value unequal any Protocol header byte. This is a compromiss, turning in complete randomness (and nice design) but gaining
			  a lower CPU usage
*/

int CMuleUDP::DecryptUDP(CBuffer& Packet, uint32 SvrKey)
{
	if (Packet.GetSize() <= CRYPT_HEADER_WITHOUTPADDING || SvrKey == 0)
		return 0;

	CHashFunction MD5Hash(CAbstractKey::eMD5);
	CBuffer KeyData(7);
	KeyData.WriteValue<uint32>(SvrKey);
	KeyData.WriteValue<uint8>(MAGICVALUE_UDP_SERVERCLIENT);
	KeyData.WriteValue<uint16>(Packet.ReadValue<uint16>());
	MD5Hash.Add(&KeyData);
	MD5Hash.Finish();

	CScoped<CDecryptionKey> pRC4key = CDecryptionKey::Make(CAbstractKey::eWeakRC4);
	ASSERT(MD5Hash.GetSize() == 16);
	pRC4key->Setup(MD5Hash.GetKey(), 16);

	uint32 Test;
	ASSERT(sizeof(Test) == 4);
	pRC4key->Process(Packet.ReadData(4), (byte*)&Test, 4);
	if(Test != MAGICVALUE_UDP_SYNC_SERVER)
		return 0; // damn it failed

	uint8 Padding;
	ASSERT(sizeof(Padding) == 1);
	pRC4key->Process(Packet.ReadData(1), (byte*)&Padding, 1);
	if(Packet.GetSizeLeft() <= Padding)
		return 0; // passed magic value but still only junk

	if (Padding > 0) // skip padding
	{
		Packet.ReadData(Padding);
		pRC4key->Discard(Padding);
	}

	size_t HeaderSize = Packet.GetPosition();
	pRC4key->Process(Packet.GetData(0),Packet.GetData(0),Packet.GetSizeLeft());
	Packet.ShiftData(HeaderSize); 

	return 1;
}

void CMuleUDP::EncryptUDP(CBuffer& Packet, uint32 SvrKey)
{
	uint16 Rand = GetRand64();

	CHashFunction MD5Hash(CAbstractKey::eMD5);
	CBuffer KeyData(7);
	KeyData.WriteValue<uint32>(SvrKey);
	KeyData.WriteValue<uint8>(MAGICVALUE_UDP_CLIENTSERVER);
	KeyData.WriteValue<uint16>(Rand);
	MD5Hash.Add(&KeyData);
	MD5Hash.Finish();

	uint8 Prot = 0;
	for (;;) 
	{
		Prot = GetRand64();

		switch (Prot) 
		{
			case OP_EDONKEYPROT:
				continue;
			//default: 
		}
		break;
	}

	uint8 Padding = 0; // no padding for UDP
	CBuffer TmpPacket(CRYPT_HEADER_WITHOUTPADDING + Packet.GetSize());
	TmpPacket.WriteValue<uint8>(Prot);
	TmpPacket.WriteValue<uint16>(Rand);
	ASSERT(TmpPacket.GetPosition() == 3);
	TmpPacket.WriteValue<uint32>(MAGICVALUE_UDP_SYNC_SERVER);
	TmpPacket.WriteValue<uint8>(Padding);
	if(Padding)
		TmpPacket.WriteQData(CAbstractKey(Padding, true).ToByteArray(), Padding);
	TmpPacket.WriteData(Packet.GetBuffer(), Packet.GetSize());

	CScoped<CEncryptionKey> pRC4key = CEncryptionKey::Make(CAbstractKey::eWeakRC4);
	ASSERT(MD5Hash.GetSize() == 16);
	pRC4key->Setup(MD5Hash.GetKey(), 16);

	pRC4key->Process(TmpPacket.GetData(3, 0), TmpPacket.GetData(3, 0), TmpPacket.GetSize() - 3);

	size_t uLen = TmpPacket.GetSize();
	byte* pBuffer = TmpPacket.GetBuffer(true);
	Packet.SetBuffer(pBuffer, uLen, false);
}