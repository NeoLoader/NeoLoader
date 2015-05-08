#include "GlobalHeader.h"
#include "MuleSocket.h"
#include "MuleServer.h"
#include "../../NeoCore.h"
#include "../../../Framework/Scope.h"
#include "../../../Framework/Cryptography/AbstractKey.h"
#include "../../../Framework/Cryptography/SymmetricKey.h"
#include "../../../Framework/Cryptography/HashFunction.h"
#include "MuleTags.h"
#include "../../Common/SimpleDH.h"

const size_t EM_DH_Size			= 96;
byte EM_DH_Base					= 2;
byte EM_DH_Prime[EM_DH_Size]	= {	0xF2,0xBF,0x52,0xC5,0x5F,0x58,0x7A,0xDD,0x53,0x71,0xA9,0x36,
									0xE8,0x86,0xEB,0x3C,0x62,0x17,0xA3,0x3E,0xC3,0x4C,0xB4,0x0D,
									0xC7,0x3A,0x41,0xA6,0x43,0xAF,0xFC,0xE7,0x21,0xFC,0x28,0x63,
									0x66,0x53,0x5B,0xDB,0xCE,0x25,0x9F,0x22,0x86,0xDA,0x4A,0x91,
									0xB2,0x07,0xCB,0xAA,0x52,0x55,0xD4,0xF6,0x1C,0xCE,0xAE,0xD4,
									0x5A,0xD5,0xE0,0x74,0x7D,0xF7,0x78,0x18,0x28,0x10,0x5F,0x34,
									0x0F,0x76,0x23,0x87,0xF8,0x8B,0x28,0x91,0x42,0xFB,0x42,0x68,
									0x8F,0x05,0x15,0x0F,0x54,0x8B,0x5F,0x43,0x6A,0xF7,0x0D,0xF3	};


CMuleSocket::CMuleSocket(CSocketThread* pSocketThread)
: CStreamSocket(pSocketThread)
{
	m_NextPacketProt = 0;
	m_NextPacketLength = -1;

	m_CryptoKey = NULL;
	m_KeyExchange = NULL;
	m_CryptoState = eInit;
}

CMuleSocket::~CMuleSocket()
{
	delete m_CryptoKey;
	delete m_KeyExchange;
}

void CMuleSocket::SendPacket(const QByteArray& Packet, uint8 Prot)
{
	CBuffer Header(1 + 4);
	Header.WriteValue<uint8>(Prot);
	Header.WriteValue<uint32>(Packet.size());

	QMutexLocker Locker(&m_Mutex);
	StreamOut(Header.GetBuffer(),Header.GetSize());
	StreamOut((byte*)Packet.data(), Packet.size());
}

void CMuleSocket::StreamOut(byte* Data, size_t Length)
{
	if(m_CryptoKey)
		m_CryptoKey->Encrypt(Data,Data,Length);
	CStreamSocket::StreamOut(Data, Length);
}

void CMuleSocket::StreamIn(byte* Data, size_t Length)
{
	if(m_CryptoKey)
		m_CryptoKey->Decrypt(Data,Data,Length);
	CStreamSocket::StreamIn(Data, Length);
}

void CMuleSocket::QueuePacket(uint64 ID, const QByteArray& Packet, uint8 Prot)
{
	CBuffer Header(1 + 4);
	Header.WriteValue<uint8>(Prot);
	Header.WriteValue<uint32>(Packet.size());

	QueueStream(ID, Header.ToByteArray() + Packet);
}

void CMuleSocket::ProcessStream()
{
	if(m_CryptoState != eDone)	
		ProcessCrypto();	

	if(m_CryptoState != eDone)
		return;
	
	do
	{
		// Find the packet length
		if (m_NextPacketLength == -1) 
		{
			if (m_InBuffer.GetSize() < 5)
				break;

			m_NextPacketProt = m_InBuffer.ReadValue<uint8>();
			m_NextPacketLength = m_InBuffer.ReadValue<uint32>();
			if (m_NextPacketLength > MB2B(2))  // Prevent DoS
			{
				DisconnectFromHost(SOCK_ERR_OVERSIZED);
				break;
			}
		}

		// Wait with parsing until the whole packet has been received
		if (m_InBuffer.GetSizeLeft() < m_NextPacketLength)
			break;

		emit ReceivedPacket(m_InBuffer.ReadQData(m_NextPacketLength), m_NextPacketProt);
		m_InBuffer.ShiftData(5 + m_NextPacketLength);

		m_NextPacketProt = 0;
		m_NextPacketLength = -1;
	}
	while (m_InBuffer.GetSizeLeft() > 0);
}

void CMuleSocket::OnConnected()
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

/* Comment copyed form eMule (Fair Use):
Basic Obfuscated Handshake Protocol Client <-> Client:
	-Keycreation:
	 - Client A (Outgoing connection):
				Sendkey:	Md5(<UserHashClientB 16><MagicValue34 1><RandomKeyPartClientA 4>)  21
				Receivekey: Md5(<UserHashClientB 16><MagicValue203 1><RandomKeyPartClientA 4>) 21
	 - Client B (Incoming connection):
				Sendkey:	Md5(<UserHashClientB 16><MagicValue203 1><RandomKeyPartClientA 4>) 21
				Receivekey: Md5(<UserHashClientB 16><MagicValue34 1><RandomKeyPartClientA 4>)  21
		NOTE: First 1024 Bytes are discarded

	- Handshake
			-> The handshake is encrypted - except otherwise noted - by the Keys created above
			-> Handshake is blocking - do not start sending an answer before the request is completly received (this includes the random bytes)
			-> EncryptionMethod = 0 is Obfuscation and the only supported right now
		Client A: <SemiRandomNotProtocolMarker 1[Unencrypted]><RandomKeyPart 4[Unencrypted]><MagicValue 4><EncryptionMethodsSupported 1><EncryptionMethodPreferred 1><PaddingLen 1><RandomBytes PaddingLen%max256>	
		Client B: <MagicValue 4><EncryptionMethodsSelected 1><PaddingLen 1><RandomBytes PaddingLen%max 256>
			-> The basic handshake is finished here, if an additional/different EncryptionMethod was selected it may continue negotiating details for this one

	- Overhead: 18-48 (~33) Bytes + 2 * IP/TCP Headers per Connection
	
	- Security for Basic Obfuscation:
			- Random looking stream, very limited protection against passive eavesdropping single connections
	
	- Additional Comments:
			- RandomKeyPart is needed to make multiple connections between two clients look different (but still random), since otherwise the same key
			  would be used and RC4 would create the same output. Since the key is a MD5 hash it doesnt weakens the key if that part is known
		    - Why DH-KeyAgreement isn't used as basic obfuscation key: It doesn't offers substantial more protection against passive connection based protocol identification, it has about 200 bytes more overhead,
			  needs more CPU time, we cannot say if the received data is junk, unencrypted or part of the keyagreement before the handshake is finished without loosing the complete randomness,
			  it doesn't offers substantial protection against eavesdropping without added authentification
*/

void CMuleSocket::InitCrypto(const QByteArray& UserHash)
{
	ASSERT(!UserHash.isEmpty());

	QMutexLocker Locker(&m_Mutex);

	uint32 Rand = GetRand64();

	m_CryptoKey = new CSymmetricKey(CAbstractKey::eRC4);

	//Sendkey
	CBuffer KeyData(21);
	KeyData.WriteQData(UserHash, 16);
	ASSERT(KeyData.GetPosition() == 16);
	KeyData.WriteValue<uint8>(MAGICVALUE_REQUESTER);
	KeyData.WriteValue<uint32>(Rand);

	CHashFunction MD5Hash(CAbstractKey::eMD5);
	MD5Hash.Add(&KeyData);
	MD5Hash.Finish();

	m_CryptoKey->SetupEncryption(MD5Hash.GetKey(), 16);

	//Receivekey
	KeyData.SetPosition(16);
	KeyData.WriteValue<uint8>(MAGICVALUE_SERVER);

	MD5Hash.Reset();
	MD5Hash.Add(&KeyData);
	MD5Hash.Finish();

	m_CryptoKey->SetupDecryption(MD5Hash.GetKey(), 16);

	uint8 Prot = 0;
	for (;;) 
	{
		Prot = GetRand64();
		switch (Prot) 
		{
			case OP_EDONKEYPROT:
			case OP_PACKEDPROT:
			case OP_EMULEPROT:
				continue;
			//default: 
		}
		break;
	}
	uint8 Padding = GetRand64() % (theCore->Cfg()->GetUInt("Ed2kMule/CryptTCPPaddingLength") + 1);
	CBuffer TmpPacket(CRYPT_HEADER_REQUESTER + Padding);
	TmpPacket.WriteValue<uint8>(Prot);
	TmpPacket.WriteValue<uint32>(Rand);
	TmpPacket.WriteValue<uint32>(MAGICVALUE_SYNC);
	TmpPacket.WriteValue<uint8>(0); // EncryptionMethodsSupported
	TmpPacket.WriteValue<uint8>(0); // EncryptionMethodPreferred
	TmpPacket.WriteValue<uint8>(Padding);
	if(Padding)
		TmpPacket.WriteData(CAbstractKey(Padding,true).GetKey(), Padding);

	m_CryptoKey->Encrypt(TmpPacket.GetBuffer() + 5, TmpPacket.GetBuffer() + 5, TmpPacket.GetSize() - 5);
	CStreamSocket::StreamOut(TmpPacket.GetBuffer(), TmpPacket.GetSize());

	m_CryptoState = ePending;
}

/* Comment copyed form eMule (Fair Use):
Basic Obfuscated Handshake Protocol Client <-> Server:
    - RC4 Keycreation:
     - Client (Outgoing connection):
                Sendkey:    Md5(<S 96><MagicValue34 1>)  97
                Receivekey: Md5(<S 96><MagicValue203 1>) 97
     - Server (Incomming connection):
                Sendkey:    Md5(<S 96><MagicValue203 1>)  97
                Receivekey: Md5(<S 96><MagicValue34 1>) 97
    
     NOTE: First 1024 Bytes are discarded

    - Handshake
            -> The handshake is encrypted - except otherwise noted - by the Keys created above
            -> Handshake is blocking - do not start sending an answer before the request is completly received (this includes the random bytes)
            -> EncryptionMethod = 0 is Obfuscation and the only supported right now
        
        Client: <SemiRandomNotProtocolMarker 1[Unencrypted]><G^A 96 [Unencrypted]><RandomBytes 0-15 [Unencrypted]>    
        Server: <G^B 96 [Unencrypted]><MagicValue 4><EncryptionMethodsSupported 1><EncryptionMethodPreferred 1><PaddingLen 1><RandomBytes PaddingLen>
        Client: <MagicValue 4><EncryptionMethodsSelected 1><PaddingLen 1><RandomBytes PaddingLen> (Answer delayed till first payload to save a frame)
        
            
            -> The basic handshake is finished here, if an additional/different EncryptionMethod was selected it may continue negotiating details for this one

    - Overhead: 206-251 (~229) Bytes + 2 * IP/TCP Headers Headers per Connectionon

	- DH Agreement Specifics: sizeof(a) and sizeof(b) = 128 Bits, g = 2, p = dh768_p (see below), sizeof p, s, etc. = 768 bits
*/

void CMuleSocket::InitCrypto()
{
	QMutexLocker Locker(&m_Mutex);

	uint8 Prot = 0;
	for (;;) 
	{
		Prot = GetRand64();
		switch (Prot) 
		{
			case OP_EDONKEYPROT:
			case OP_PACKEDPROT:
			case OP_EMULEPROT:
				continue;
			//default: 
		}
		break;
	}

	size_t Padding = rand() % 16;
	CBuffer TmpPacket(1 + EM_DH_Size + Padding);
	TmpPacket.WriteValue<uint8>(Prot);
	m_KeyExchange = new CSimpleDH(EM_DH_Size, EM_DH_Prime, EM_DH_Base);
	CScoped<CAbstractKey> pKey = m_KeyExchange->InitialsieKeyExchange();
	TmpPacket.WriteData(pKey->GetKey(), pKey->GetSize());
	if(Padding)
		TmpPacket.WriteData(CAbstractKey(Padding,true).GetKey(), Padding);

	CStreamSocket::StreamOut(TmpPacket.GetBuffer(), TmpPacket.GetSize());

	m_CryptoState = ePending;
}

void CMuleSocket::ProcessCrypto()
{
	CBuffer RawPacket(m_InBuffer.GetBuffer(), m_InBuffer.GetSize(), true);

	if(m_CryptoState == eInit || m_CryptoState == ePending)
	{
		if(m_CryptoKey) // we started it
		{
			if(RawPacket.GetSize() < CRYPT_HEADER_SERVER)
				return; // not enough data

			// already decrypted
			uint32 Test = RawPacket.ReadValue<uint32>();
			if(Test != MAGICVALUE_SYNC)
			{
				m_CryptoState = eDone; // invalid crypto
				DisconnectFromHost(SOCK_ERR_CRYPTO);
				return;
			}

			// this 1 is always 0
			//RawPacket.ReadValue<uint8>(); // EncryptionMethodsSelected

			m_InBuffer.ShiftData(CRYPT_HEADER_SERVER - 1); // leave padding length
			RawPacket.SetPosition(0); // Note: the RawPacket uses the same buffer as m_InBuffer, shifting the data, does not invalidate the poitner but moves thedata
			m_CryptoState = ePadding;

			CStreamSocket::OnConnected();
		}
		else 
		{
			size_t uShift;
			if(m_KeyExchange) // server DH answer
			{
				if(RawPacket.GetSize() < EM_DH_Size + 4 + 2 + 1)
					return; // not enough data

				CAbstractKey Key(RawPacket.ReadData(EM_DH_Size), EM_DH_Size);
				CScoped<CAbstractKey> pTempKey = m_KeyExchange->FinaliseKeyExchange(&Key);

				m_CryptoKey = new CSymmetricKey(CAbstractKey::eRC4);

				//Sendkey
				CBuffer KeyData(EM_DH_Size + 1);
				KeyData.WriteData(pTempKey->GetKey(), pTempKey->GetSize());
				KeyData.WriteValue<uint8>(MAGICVALUE_REQUESTER);

				CHashFunction MD5Hash(CAbstractKey::eMD5);
				MD5Hash.Add(&KeyData);
				MD5Hash.Finish();

				m_CryptoKey->SetupEncryption(MD5Hash.GetKey(), 16);

				//Receivekey
				KeyData.SetPosition(EM_DH_Size);
				KeyData.WriteValue<uint8>(MAGICVALUE_SERVER);

				MD5Hash.Reset();
				MD5Hash.Add(&KeyData);
				MD5Hash.Finish();

				m_CryptoKey->SetupDecryption(MD5Hash.GetKey(), 16);

				uShift = EM_DH_Size + 4 + 2;
			}
			else // Incoming
			{
				uint8 Prot = RawPacket.ReadValue<uint8>();
				switch (Prot) 
				{
					case OP_EDONKEYPROT:
					case OP_PACKEDPROT:
					case OP_EMULEPROT:
						m_CryptoState = eDone; // no crypto
						return;
					//default: 
				}

				if(RawPacket.GetSize() < CRYPT_HEADER_REQUESTER)
					return; // not enough data

				uint32 Rand = RawPacket.ReadValue<uint32>();

				m_CryptoKey = new CSymmetricKey(CAbstractKey::eRC4);

				//Sendkey
				CBuffer KeyData(21);
				KeyData.WriteQData(((CMuleServer*)m_Server)->GetUserHash(), 16);
				ASSERT(KeyData.GetPosition() == 16);
				KeyData.WriteValue<uint8>(MAGICVALUE_SERVER);
				KeyData.WriteValue<uint32>(Rand);

				CHashFunction MD5Hash(CAbstractKey::eMD5);
				MD5Hash.Add(&KeyData);
				MD5Hash.Finish();

				m_CryptoKey->SetupEncryption(MD5Hash.GetKey(), 16);

				//Receivekey
				KeyData.SetPosition(16);
				KeyData.WriteValue<uint8>(MAGICVALUE_REQUESTER);

				MD5Hash.Reset();
				MD5Hash.Add(&KeyData);
				MD5Hash.Finish();

				m_CryptoKey->SetupDecryption(MD5Hash.GetKey(), 16);

				uShift = CRYPT_HEADER_REQUESTER - 1;
			}

			// decrypt whats cahed in the buffer, all later data will be decrypted immidetly after read
			m_CryptoKey->Decrypt(RawPacket.ReadData(0), RawPacket.ReadData(0), RawPacket.GetSizeLeft());

			uint32 Test = RawPacket.ReadValue<uint32>();
			if(Test != MAGICVALUE_SYNC)
			{
				m_CryptoState = eDone; // invalid crypto
				DisconnectFromHost(SOCK_ERR_CRYPTO);
				return;
			}

			// this 2 are always 0
			//RawPacket.ReadValue<uint8>(); // EncryptionMethodsSupported
			//RawPacket.ReadValue<uint8>(); // EncryptionMethodPreferred

			m_InBuffer.ShiftData(uShift); // leave unencrypted padding length
			RawPacket.SetPosition(0); // Note: the RawPacket uses the same buffer as m_InBuffer, shifting the data, does not invalidate the poitner but moves thedata
			m_CryptoState = ePadding;

			// respond
			uint8 Padding = GetRand64() % (theCore->Cfg()->GetUInt("Ed2kMule/CryptTCPPaddingLength") + 1);
			CBuffer TmpPacket(CRYPT_HEADER_SERVER + Padding);
			TmpPacket.WriteValue<uint32>(MAGICVALUE_SYNC);
			TmpPacket.WriteValue<uint8>(0); // EncryptionMethodsSelected
			TmpPacket.WriteValue<uint8>(Padding);
			if(Padding)
				TmpPacket.WriteData(CAbstractKey(Padding,true).GetKey(), Padding);

			StreamOut(TmpPacket.GetBuffer(), TmpPacket.GetSize());

			if(m_KeyExchange) // this was a outgoing server connection
			{
				delete m_KeyExchange;
				m_KeyExchange = NULL;

				CStreamSocket::OnConnected();
			}
		}
	}
	ASSERT(m_CryptoState == ePadding);
	
	size_t Padding = RawPacket.ReadValue<uint8>();
	if(RawPacket.GetSizeLeft() < Padding)
		return; // not enough data
	m_InBuffer.ShiftData(1 + Padding);
	m_CryptoState = eDone;
}

bool CMuleSocket::IsEncrypted()
{
	QMutexLocker Locker(&m_Mutex);
	if(GetSocketType() == SOCK_UTP) // UTP sockets are encrypted on the UDP level
		return ((CMuleServer*)m_Server)->HasCrypto(m_Address, m_Port);
	return m_CryptoKey != NULL || m_CryptoState == ePending;
}