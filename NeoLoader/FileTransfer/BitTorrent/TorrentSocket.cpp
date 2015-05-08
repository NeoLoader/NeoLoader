#include "GlobalHeader.h"
#include "TorrentSocket.h"
#include "TorrentServer.h"
#include "TorrentManager.h"
#include "../../NeoCore.h"
#include "../Framework/Cryptography/AbstractKey.h"
#include "../Framework/Cryptography/SymmetricKey.h"
#include "../Framework/Cryptography/HashFunction.h"
#include "../Framework/Cryptography/KeyExchange.h"
#include "../Framework/Scope.h"
#include "../../Common/SimpleDH.h"

const size_t BT_DH_Size			= 96;
byte BT_DH_Base					= 2;
byte BT_DH_Prime[BT_DH_Size]	= { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xC9, 0x0F, 0xDA, 0xA2,
									0x21, 0x68, 0xC2, 0x34, 0xC4, 0xC6, 0x62, 0x8B, 0x80, 0xDC, 0x1C, 0xD1,
									0x29, 0x02, 0x4E, 0x08, 0x8A, 0x67, 0xCC, 0x74, 0x02, 0x0B, 0xBE, 0xA6,
									0x3B, 0x13, 0x9B, 0x22, 0x51, 0x4A, 0x08, 0x79, 0x8E, 0x34, 0x04, 0xDD,
									0xEF, 0x95, 0x19, 0xB3, 0xCD, 0x3A, 0x43, 0x1B, 0x30, 0x2B, 0x0A, 0x6D,
									0xF2, 0x5F, 0x14, 0x37, 0x4F, 0xE1, 0x35, 0x6D, 0x6D, 0x51, 0xC2, 0x45,
									0xE4, 0x85, 0xB5, 0x76, 0x62, 0x5E, 0x7E, 0xC6, 0xF4, 0x4C, 0x42, 0xE9,
									0xA6, 0x3A, 0x36, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x05, 0x63 };

CTorrentSocket::CTorrentSocket(CSocketThread* pSocketThread)
 : CStreamSocket(pSocketThread)
{
	m_NextPacketLength = 0;

	m_KeyExchange = NULL;
	m_TempKey = NULL;
	m_CryptoKey = NULL;
	m_CryptoState = eInit;
	m_CryptoMethod = CAbstractKey::eUndefined;
}

CTorrentSocket::~CTorrentSocket()
{
	delete m_KeyExchange;
	delete m_TempKey;
	delete m_CryptoKey;
}

void CTorrentSocket::SendHandshake(const QByteArray& Handshake)
{
	ASSERT(Handshake.size() == HAND_SHAKE_SIZE);

	QMutexLocker Locker(&m_Mutex);
	if(m_CryptoState == eInit)
		m_Atomic = Handshake;
	else
		StreamOut((byte*)Handshake.data(), Handshake.length());
}

void CTorrentSocket::SendPacket(const QByteArray& Packet)
{
	CBuffer Header(4);
	Header.WriteValue<uint32>(Packet.size(), true);

	QMutexLocker Locker(&m_Mutex);
	StreamOut(Header.GetBuffer(), Header.GetSize());
	StreamOut((byte*)Packet.data(), Packet.size());
}

void CTorrentSocket::StreamOut(byte* Data, size_t Length)
{
	ASSERT((!m_CryptoKey && !m_KeyExchange) || m_CryptoState == eDone); // we are not allowed to send anythign untill crypto is done, unless its an not encrypted connection
	if(m_CryptoKey && m_CryptoState == eDone)
		m_CryptoKey->Encrypt(Data,Data,Length);
	CStreamSocket::StreamOut(Data, Length);
}

void CTorrentSocket::StreamIn(byte* Data, size_t Length)
{
	if(m_CryptoKey && m_CryptoState == eDone)
		m_CryptoKey->Decrypt(Data,Data,Length);
	CStreamSocket::StreamIn(Data, Length);
}

void CTorrentSocket::QueuePacket(uint64 ID, const QByteArray& Packet)
{
	CBuffer Header(4);
	Header.WriteValue<uint32>(Packet.size(), true);

	QueueStream(ID, Header.ToByteArray() + Packet);
}

void CTorrentSocket::ProcessStream()
{
	if(m_CryptoState != eDone)	
		ProcessCrypto();	

	if(m_CryptoState != eDone)
		return;

	if (m_NextPacketLength == 0) 
	{
		// Check that we received enough data
		if (m_InBuffer.GetSize() < HAND_SHAKE_SIZE)
			return;
		m_NextPacketLength = -1;

		emit ReceivedHandshake(m_InBuffer.ReadQData(HAND_SHAKE_SIZE));
		m_InBuffer.ShiftData(HAND_SHAKE_SIZE);
	}

	do 
	{
		// Find the packet length
		if (m_NextPacketLength == -1) 
		{
			if (m_InBuffer.GetSize() < 4)
				break;

			m_NextPacketLength = m_InBuffer.ReadValue<uint32>(true);
			if (m_NextPacketLength > KB2B(256))  // Prevent DoS
			{
				DisconnectFromHost(SOCK_ERR_OVERSIZED);
				break;
			}
		}

		// Wait with parsing until the whole packet has been received
		if (m_InBuffer.GetSizeLeft() < m_NextPacketLength)
			break;

		emit ReceivedPacket(m_InBuffer.ReadQData(m_NextPacketLength));
		m_InBuffer.ShiftData(4 + m_NextPacketLength);

		m_NextPacketLength = -1;
	}
	while (m_InBuffer.GetSizeLeft() > 0);
}

void CTorrentSocket::OnConnected()
{
	if(m_CryptoState == eInit)
	{
		m_CryptoState = eDone;
		CStreamSocket::OnConnected();
	}
	else
	{
		m_State = eHalfConnected;
		//emit bytesWritten(0);
	}
}

/*
Message Stream Encryption format specification Version 1.0

##### Constants/Variables
	Pubkey of A(Initiator ): Ya = (G^Xa) mod P
	Pubkey of B(Receiver): Yb = (G^Xb) mod P

	Prime P is a 768 bit safe prime:
		FFFFFFFFFFFFFFFF C90FDAA22168C234
		C4C6628B80DC1CD1 29024E088A67CC74
		020BBEA63B139B22 514A08798E3404DD
		EF9519B3CD3A431B 302B0A6DF25F1437
		4FE1356D6D51C245 E485B576625E7EC6
		F44C42E9A63A3621 0000000000090563
	Generator G is :2

	PadA, PadB: Random data with a random length of 0 to 512 bytes each

	DH secret: S = (Ya^Xb) mod P = (Yb^Xa) mod P

	SKEY = Stream Identifier/Shared secret (Torrent InfoHash)

	VC is a verification constant 0x0000000000000000.

###### Functions
	len(X) specifies the length of X in 2 bytes.
	Thus the maximum length that can be specified is 65535 bytes, this is
	important for the IA block.

	ENCRYPT() is RC4, that uses one of the following keys to send data:
	"HASH('keyA', S, SKEY)" if you're A
	"HASH('keyB', S, SKEY)" if you're B
	The first 1024 bytes of the RC4 output are discarded.
	consecutive calls to ENCRYPT() by one side continue the encryption
	stream (no reinitialization, no keychange). They are only used to distinguish
	semantically seperate content. 

	ENCRYPT2() is the negotiated crypto method.
	Current options are:
	 0x01 Plaintext. After the specified length (see IA/IB) each side sends unencrypted payload
	 0x02 RC4-160. The ENCRYPT() RC4 encoding is continued (no reinitialization, no keychange)

	HASH() is SHA1 binary output (20 bytes)

###### The handshake "packet" format
	1 A->B: Diffie Hellman Ya, PadA
	2 A<-B: Diffie Hellman Yb, PadB
	3 A->B: HASH('req1', S), HASH('req2', SKEY) xor HASH('req3', S), 
			ENCRYPT(VC, crypto_provide, len(PadC), PadC, len(IA)), ENCRYPT(IA)
	4 A<-B: ENCRYPT(VC, crypto_select, len(padD), padD), 
			ENCRYPT2(Payload Stream)
	5 A->B: ENCRYPT2(Payload Stream)

*/

#ifdef _DEBUG
CAbstractKey* g_pPubKey = NULL;
CAbstractKey* g_pPrivKey = NULL;

struct __SInit_DH_BT
{
	__SInit_DH_BT()
	{
		CSimpleDH* pKeyExchange = new CSimpleDH(BT_DH_Size, BT_DH_Prime, BT_DH_Base);
		g_pPubKey = pKeyExchange->InitialsieKeyExchange();
		g_pPrivKey = pKeyExchange;
	}
	~__SInit_DH_BT()
	{
		delete g_pPubKey;
		delete g_pPrivKey;
	}
} __SInit_DH_BT;
#endif

bool CTorrentSocket::InitCrypto(const QByteArray& InfoHash)
{
	QMutexLocker Locker(&m_Mutex);

	m_InfoHash = InfoHash;
	if(m_InfoHash.isEmpty())
		return false;

	m_KeyExchange = new CSimpleDH(BT_DH_Size, BT_DH_Prime, BT_DH_Base);

#ifdef _DEBUG
	m_KeyExchange->SetKey(g_pPrivKey->GetKey(), g_pPrivKey->GetSize());
	SendPaddedKey(new CAbstractKey(g_pPubKey->GetKey(), g_pPubKey->GetSize()));
#else
	// send the sender pub key
	SendPaddedKey(m_KeyExchange->InitialsieKeyExchange());
#endif

	m_CryptoState = eExchangeKey;
	return true;
}

void CTorrentSocket::SendPaddedKey(CAbstractKey* pKey)
{
	uint16 Padding = GetRand64() % (theCore->Cfg()->GetUInt("BitTorrent/CryptTCPPaddingLength") + 1);
	CBuffer KeyPacket(BT_DH_Size + Padding);
	KeyPacket.WriteData(pKey->GetKey(), pKey->GetSize());
	if(Padding)
		KeyPacket.WriteData(CAbstractKey(Padding,true).GetKey(), Padding);
	CStreamSocket::StreamOut(KeyPacket.GetBuffer(), KeyPacket.GetSize());
	delete pKey;
}

void CTorrentSocket::ProcessCrypto()
{
	switch(m_CryptoState)
	{
		case eInit:
		{
			if(m_InBuffer.GetSize() < 1 + 19)
				break;

			if (*m_InBuffer.GetBuffer() == 19 && memcmp(m_InBuffer.GetBuffer()+1, PROTOCOL_ID, 19) == 0)
			{
				m_CryptoState = eDone;
				ASSERT(m_KeyExchange == NULL);
				ASSERT(m_CryptoKey == NULL);
				break;
			}

			m_CryptoState = eExchangeKey;
		}
		case eExchangeKey:
		{
			if(m_InBuffer.GetSize() < BT_DH_Size)
				break;

			CAbstractKey Key(m_InBuffer.GetBuffer(), BT_DH_Size);

			if(!m_KeyExchange) // if no exchange is already allocatet it means this is an Incoming connection
			{
				m_KeyExchange = new CSimpleDH(BT_DH_Size, BT_DH_Prime, BT_DH_Base);

#ifdef _DEBUG
				m_KeyExchange->SetKey(g_pPrivKey->GetKey(), g_pPrivKey->GetSize());
				SendPaddedKey(new CAbstractKey(g_pPubKey->GetKey(), g_pPubKey->GetSize()));
#else
				// send the reciver pub key
				SendPaddedKey(m_KeyExchange->InitialsieKeyExchange());
#endif

				m_CryptoState = eFindHash;
			}

			m_TempKey = m_KeyExchange->FinaliseKeyExchange(&Key);
			delete m_KeyExchange;
			m_KeyExchange = NULL;

			if(!m_TempKey)
			{
				CryptoFailed();
				break;
			}

			// if we are the sender, send infohash segment
			if(m_CryptoState == eFindHash)
				break;

			m_CryptoState = eFindSelect; // wait for crypto select

			// write sync
			CHashFunction Hash(CAbstractKey::eSHA1);
			Hash.Add((byte*)"req1", 4);
			Hash.Add(m_TempKey->GetKey(), m_TempKey->GetSize());
			Hash.Finish();

			CStreamSocket::StreamOut(Hash.GetKey(), Hash.GetSize());


			// write info key
			CHashFunction Hash2(CAbstractKey::eSHA1);
			Hash2.Add((byte*)"req2", 4);
			Hash2.Add((byte*)m_InfoHash.data(), m_InfoHash.size());
			Hash2.Finish();

			CHashFunction Hash3(CAbstractKey::eSHA1);
			Hash3.Add((byte*)"req3", 4);
			Hash3.Add(m_TempKey->GetKey(), m_TempKey->GetSize());
			Hash3.Finish();

			CAbstractKey HashX(KEY_160BIT);
			for(int i=0; i<KEY_160BIT; i++)
				HashX.GetKey()[i] = Hash2.GetKey()[i] ^ Hash3.GetKey()[i];

			CStreamSocket::StreamOut(HashX.GetKey(), HashX.GetSize());

			SetupRC4(true);


			// write crypto provide
			uint16 Padding = GetRand64() % (theCore->Cfg()->GetUInt("BitTorrent/CryptTCPPaddingLength") + 1);
			CBuffer Provide(8 + 4 + 2 + Padding);

			Provide.WriteValue<uint64>(0, true);

			uint32 uProvide = 0;
			if(theCore->m_TorrentManager->RequiresCryptLayer())
				uProvide |= 0x02; // rc4
			else if(theCore->m_TorrentManager->SupportsCryptLayer())
				uProvide |= 0x03; // booth
			else
				uProvide |= 0x01; // plaintext
			Provide.WriteValue<uint32>(uProvide, true);

			Provide.WriteValue<uint16>(Padding, true);
			if(Padding)
				Provide.WriteData(CAbstractKey(Padding,true).GetKey(), Padding);
			m_CryptoKey->Encrypt(Provide.GetBuffer(), Provide.GetBuffer(), Provide.GetSize());
			CStreamSocket::StreamOut(Provide.GetBuffer(), Provide.GetSize());


			// write IA
			uint16 LenIA = m_Atomic.size();
			CBuffer IA(2 + LenIA);
			IA.WriteValue<uint16>(LenIA, true);
			IA.WriteQData(m_Atomic);
			m_CryptoKey->Encrypt(IA.GetBuffer(), IA.GetBuffer(), IA.GetSize());
			CStreamSocket::StreamOut(IA.GetBuffer(), IA.GetSize());
			break;
		}

		case eFindHash:
		{
			if(m_SyncMark.isEmpty())
			{
				CHashFunction Hash(CAbstractKey::eSHA1);
				Hash.Add((byte*)"req1", 4);
				Hash.Add(m_TempKey->GetKey(), m_TempKey->GetSize());
				Hash.Finish();
				m_SyncMark = Hash.ToByteArray();
			}
			if(!FindSyncMark())
				break;

			m_CryptoState = eReadHash;
		}
		case eReadHash:
		{
			if(m_InBuffer.GetSize() < 20)
				break;

			CAbstractKey HashX(m_InBuffer.GetBuffer(),KEY_160BIT);
			m_InBuffer.ShiftData(20);

			CHashFunction Hash3(CAbstractKey::eSHA1);
			Hash3.Add((byte*)"req3", 4);
			Hash3.Add(m_TempKey->GetKey(), m_TempKey->GetSize());
			Hash3.Finish();

			CAbstractKey Hash2(KEY_160BIT);
			for(int i=0; i<KEY_160BIT; i++)
				Hash2.GetKey()[i] = HashX.GetKey()[i] ^ Hash3.GetKey()[i];

			m_InfoHash = ((CTorrentServer*)m_Server)->GetInfoHash(Hash2.ToByteArray());
			if(m_InfoHash.isEmpty())
			{
				CryptoFailed();
				break;
			}

			SetupRC4(false);

			m_CryptoState = eReadProvide; // wait for crypto provide
		}
		case eReadProvide:
		{
			if(m_InBuffer.GetSize() < 8 + 4 + 2)
				break;

			m_CryptoKey->Decrypt(m_InBuffer.GetBuffer(), m_InBuffer.GetBuffer(), 8 + 4 + 2);
			CBuffer Provide(m_InBuffer.GetBuffer(), 8 + 4, true);
	
			if(Provide.ReadValue<uint64>(true) != 0)
			{
				CryptoFailed();
				break;
			}
			uint32 uProvide = Provide.ReadValue<uint32>(true);
	
			m_InBuffer.ShiftData(8 + 4);

			// write crypto select
			uint16 Padding = GetRand64() % (theCore->Cfg()->GetUInt("BitTorrent/CryptTCPPaddingLength") + 1);
			CBuffer Select(8 + 4 + 2 + Padding);

			Select.WriteValue<uint64>(0, true);

			uint32 uSelect = 0;
			if(theCore->m_TorrentManager->RequestsCryptLayer() && (uProvide & 0x02) != 0)
			{
				m_CryptoMethod = CAbstractKey::eRC4;
				uSelect = 0x02; // rc4
			}
			else if(!theCore->m_TorrentManager->RequiresCryptLayer() && (uProvide & 0x01) != 0)
			{
				m_CryptoMethod = CAbstractKey::eNone;
				uSelect = 0x01; // plaintext
			}
			else
			{
				CryptoFailed();
				break;
			}
			Select.WriteValue<uint32>(uSelect, true);

			Select.WriteValue<uint16>(Padding, true);
			if(Padding)
				Select.WriteData(CAbstractKey(Padding,true).GetKey(), Padding);
			m_CryptoKey->Encrypt(Select.GetBuffer(), Select.GetBuffer(), Select.GetSize());
			CStreamSocket::StreamOut(Select.GetBuffer(), Select.GetSize());

			m_CryptoState = eEndProvide;
		}
		case eEndProvide:
		{
			CBuffer Provide(m_InBuffer.GetBuffer(), m_InBuffer.GetSize(), true);
			size_t Padding = Provide.ReadValue<uint16>(true);
			if(m_InBuffer.GetSize() < 2 + Padding + 2)
				break;
			m_CryptoKey->Decrypt(m_InBuffer.GetBuffer() + 2, m_InBuffer.GetBuffer() + 2, Padding + 2);
			m_InBuffer.ShiftData(2 + Padding);

			m_CryptoState = eReadIA;
		}
		case eReadIA:
		{
			CBuffer IA(m_InBuffer.GetBuffer(), m_InBuffer.GetSize(), true);
			size_t LenIA = IA.ReadValue<uint16>(true);
			if(m_InBuffer.GetSize() < 2 + LenIA)
				break;
			m_InBuffer.ShiftData(2);

			// decrypt atomic payload, with default rc4 befoure switchign encryption methods
			m_CryptoKey->Decrypt(m_InBuffer.GetBuffer(), m_InBuffer.GetBuffer(), LenIA);

			FinishCrypto(LenIA);

			m_CryptoState = eDone;
			break;
		}

		case eFindSelect:
		{
			if(m_SyncMark.isEmpty())
			{
				m_SyncMark.fill(0, 8);
				m_CryptoKey->Decrypt((byte*)m_SyncMark.data(), (byte*)m_SyncMark.data(), 8);
			}
			if(!FindSyncMark())
				break;

			m_CryptoState = eReadSelect;
		}
		case eReadSelect:
		{
			if(m_InBuffer.GetSize() < 4 + 2)
				break;

			m_CryptoKey->Decrypt(m_InBuffer.GetBuffer(), m_InBuffer.GetBuffer(), 4 + 2);
			CBuffer Select(m_InBuffer.GetBuffer(), 4, true);

			uint32 uSelect = Select.ReadValue<uint32>(true);
			if(uSelect == 0x02) 
				m_CryptoMethod = CAbstractKey::eRC4;
			else if(uSelect == 0x01)
				m_CryptoMethod = CAbstractKey::eNone;
			else
			{
				CryptoFailed();
				break;
			}

			m_InBuffer.ShiftData(4);

			m_CryptoState = eEndSelect;
		}
		case eEndSelect:
		{
			CBuffer Select(m_InBuffer.GetBuffer(), m_InBuffer.GetSize(), true);
			size_t Padding = Select.ReadValue<uint16>(true);
			if(m_InBuffer.GetSize() < 2 + Padding)
				break;
			m_CryptoKey->Decrypt(m_InBuffer.GetBuffer() + 2, m_InBuffer.GetBuffer() + 2, Padding);
			m_InBuffer.ShiftData(2 + Padding);

			FinishCrypto();

			m_CryptoState = eDone;
			CStreamSocket::OnConnected();
			break;
		}
	}
}

void CTorrentSocket::SetupRC4(bool Sender)
{
	ASSERT(m_CryptoKey == NULL);
	m_CryptoKey = new CSymmetricKey(CAbstractKey::eRC4);

	//Sendkey
	CHashFunction Hash(CAbstractKey::eSHA1);
	Hash.Add((byte*)(Sender ? "keyA" : "keyB"), 4);
	Hash.Add(m_TempKey->GetKey(), m_TempKey->GetSize());
	Hash.Add((byte*)m_InfoHash.data(), m_InfoHash.size());
	Hash.Finish();

	m_CryptoKey->SetupEncryption(Hash.GetKey(), KEY_160BIT);

	//Receivekey
	Hash.Reset();
	Hash.Add((byte*)(Sender ? "keyB" : "keyA"), 4);
	Hash.Add(m_TempKey->GetKey(), m_TempKey->GetSize());
	Hash.Add((byte*)m_InfoHash.data(), m_InfoHash.size());
	Hash.Finish();

	m_CryptoKey->SetupDecryption(Hash.GetKey(), KEY_160BIT);
}

bool CTorrentSocket::FindSyncMark()
{
	int Index = m_InBuffer.ToByteArray().indexOf(m_SyncMark);
	if(Index == -1)
	{
		if(m_InBuffer.GetSize() > 96 + 512 + (size_t)m_SyncMark.size())
			CryptoFailed();
		return false;
	}
	m_InBuffer.ShiftData(Index + m_SyncMark.size());
	return true;
}

void CTorrentSocket::FinishCrypto(uint16 Offset)
{
	switch(m_CryptoMethod)
	{
		case CAbstractKey::eRC4:											break;  // keep going
		case CAbstractKey::eNone: delete m_CryptoKey; m_CryptoKey = NULL;	return; // return rest is plaintext
		default: ASSERT(0);
	}

	// decrypt whats left in buffer with new method
	m_CryptoKey->Decrypt(m_InBuffer.GetBuffer() + Offset, m_InBuffer.GetBuffer() + Offset, m_InBuffer.GetSize() - Offset);
}

void CTorrentSocket::CryptoFailed()
{
	m_CryptoState = eDone; // invalid crypto
	DisconnectFromHost(SOCK_ERR_CRYPTO);
}

bool CTorrentSocket::SupportsEncryption()	
{
	return m_CryptoMethod != CAbstractKey::eUndefined;
}
