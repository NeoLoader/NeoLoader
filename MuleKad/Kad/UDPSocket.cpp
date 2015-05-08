//
// This file is part of the MuleKad Project.
//
// Copyright (c) 2012 David Xanatos ( XanatosDavid@googlemail.com )
//
// Any parts of this program derived from the xMule, lMule or eMule project,
// or contributed by third-party developers are copyrighted by their
// respective authors.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301, USA
//
#include "GlobalHeader.h"
#include "Types.h"
#include "Protocols.h"
#include "UDPSocket.h"
#include "kademlia/Prefs.h"
#include "kademlia/Kademlia.h"
#include "utils/KadUDPKey.h"
#include "../../Framework/Buffer.h"
#include "../../Framework/Scope.h"
#include "../../Framework/Cryptography/SymmetricKey.h"
#include "../../Framework/Cryptography/HashFunction.h"
#include "KadHandler.h"
#include "../../zlib/zlib.h"
#include "Packet.h"
#include "../../Framework/Exception.h"

CUDPSocket*	CUDPSocket::m_Instance = NULL;

CUDPSocket::CUDPSocket(uint16_t UDPPort, uint32_t UDPKey)
{
	ASSERT(m_Instance == NULL);
	m_Instance = this;

	m_UDPKey = UDPKey;

	// initialise IP v4 socket
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
	m_socket = socket(PF_INET, SOCK_DGRAM, 0);
    if(m_socket < 0)
		LogLine(LOG_ERROR, _T("Couln't open socket"));
	else 
	{
		sin.sin_port = htons(UDPPort);
        int rc = ::bind(m_socket, (struct sockaddr*)&sin, sizeof(sin));
        if(rc < 0) 
		{
            LogLine(LOG_ERROR, _T("Couln't bind socket"));
			closesocket(m_socket);
			m_socket = -1;
		}
    }
}

CUDPSocket::~CUDPSocket()
{
	closesocket(m_socket);
	m_socket = -1;

	m_Instance = NULL;
}

void CUDPSocket::Process()
{
	for(;;)
	{
		fd_set readfds;
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 0;

        FD_ZERO(&readfds);
        FD_SET(m_socket, &readfds);
        if(select(m_socket + 1, &readfds, NULL, NULL, &tv) == SOCKET_ERROR) 
		{
			LogLine(LOG_ERROR, _T("Socket select failed"));
			break;
        }
		if(!FD_ISSET(m_socket, &readfds))
			break;

		struct sockaddr_in from;
		socklen_t fromlen = sizeof(from);
		char* buf = new char [8*1024];
		size_t len = recvfrom(m_socket, buf, 8*1024 - 1, 0, (struct sockaddr*)&from, &fromlen);
        
#ifndef WIN32
		ProcessPacket(ntohl(from.sin_addr.s_addr), ntohs(from.sin_port), (uint8_t*)buf, len);
#else
		ProcessPacket(ntohl(from.sin_addr.S_un.S_addr), ntohs(from.sin_port), (uint8_t*)buf, len);
#endif

		delete [] buf;
    }
}

void CUDPSocket::SendPacket(char*& buf, size_t& len, uint32_t ip, uint16_t port, bool bEncrypt, const uint8_t* pachTargetClientHashORKadID, bool bKad, uint32_t nReceiverVerifyKey)
{
	ASSERT(bKad);
	if (bEncrypt)
		len = EncryptSendClient((uint8_t**)&buf, len, pachTargetClientHashORKadID, bKad, nReceiverVerifyKey, (bKad ? GetUDPVerifyKey(ntohl(ip)) : 0));

	sockaddr_in to;
	socklen_t tolen = sizeof(to);
	to.sin_family = AF_INET;
	to.sin_addr.s_addr = ntohl(ip);
	to.sin_port = ntohs(port);
	sendto(m_socket, buf, len, 0, (struct sockaddr*)&to, tolen);
}

void CUDPSocket::ProcessPacket(uint32_t ip, uint16_t port, uint8_t* buffer, size_t length)
{
	if(!CKadHandler::Instance() || !CKadHandler::Instance()->IsRunning())
		return;

	uint8_t *decryptedBuffer;
	uint32_t receiverVerifyKey;
	uint32_t senderVerifyKey;
	int packetLen = DecryptReceivedClient(buffer, length, &decryptedBuffer, ip, &receiverVerifyKey, &senderVerifyKey);

	if (packetLen >= 1) {
		uint8_t protocol = decryptedBuffer[0];
		uint8_t opcode	 = decryptedBuffer[1];
		try {
			switch (protocol) {
				case OP_EMULEPROT:
				{
					switch(opcode)
					{
						case OP_REASKCALLBACKUDP:
						{
							CBuffer Packet(decryptedBuffer + 2, packetLen - 2, true);
							CKadHandler::Instance()->RelayUDPPacket(ip, port, decryptedBuffer + 2, packetLen - 2);
							return;
						}

						case OP_DIRECTCALLBACKREQ: 
						{
							CBuffer Packet(decryptedBuffer + 2, packetLen - 2, true);
							uint16_t uTCPPort = Packet.ReadValue<uint16>();
							QByteArray UserHash = Packet.ReadQData(16);
							uint8_t ConOpts = Packet.ReadValue<uint8>();
							CKadHandler::Instance()->DirectCallback(ip, uTCPPort, UserHash, ConOpts);
							return;
						}

						default:
							LogKadLine(LOG_DEBUG /*logClientKadUDP*/, L"Recived not implemented eMule protocol packet");
					}
					break;
				}

				case OP_KADEMLIAHEADER:
				case OP_KADEMLIAPACKEDPROT:
					if (packetLen >= 2) {
						CKadHandler::Instance()->ProcessPacket(QByteArray::fromRawData((char*)decryptedBuffer, packetLen), ip, port, (GetUDPVerifyKey(ntohl(ip)) == receiverVerifyKey), senderVerifyKey);
						//Kademlia::CKademlia::ProcessPacket(decryptedBuffer, packetLen, ip, port, (GetUDPVerifyKey(ntohl(ip)) == receiverVerifyKey), Kademlia::CKadUDPKey(senderVerifyKey, ntohl(CKadHandler::Instance()->GetPublicIP())));
					} else {
						LogKadLine(LOG_DEBUG /*logClientKadUDP*/, L"Kad packet too short");
					}
					break;

				default:
					LogKadLine(LOG_DEBUG /*logClientUDP*/, L"Unknown opcode on received packet: 0x%x", protocol);
			}
		} 
		catch(const CException& Exception){
			LogKadLine(LOG_DEBUG /*logClientUDP*/, L"Error while parsing UDP packet: %s", Exception.GetLine().c_str());
		}
	}
}

uint32_t CUDPSocket::GetUDPVerifyKey(uint32_t targetIP)
{
	return GetUDPVerifyKey(m_UDPKey, targetIP);
}

uint32_t CUDPSocket::GetUDPVerifyKey(uint32_t key, uint32_t targetIP)
{
	uint64_t buffer = ((uint64_t)key) << 32 | targetIP;
	CHashFunction Hash(CAbstractKey::eMD5);
	Hash.Add((const uint8_t *)&buffer, 8);
	Hash.Finish();
	return (uint32_t)(*((uint32_t*)Hash.GetKey()) ^ *((uint32_t*)Hash.GetKey() + 1) ^ *((uint32_t*)Hash.GetKey() + 2) ^ *((uint32_t*)Hash.GetKey() + 3)) % 0xFFFFFFFE + 1;
}

bool IsLanIP(uint32_t nIP)
{
	nIP = ntohl(nIP);

	// LAN IP's
	// -------------------------------------------
	//	0.*								"This" Network
	//	10.0.0.0 - 10.255.255.255		Class A
	//	172.16.0.0 - 172.31.255.255		Class B
	//	192.168.0.0 - 192.168.255.255	Class C

	uint8_t nFirst = (uint8)nIP;
	uint8_t nSecond = (uint8)(nIP >> 8);

	if (nFirst==192 && nSecond==168) // check this 1st, because those LANs IPs are mostly spreaded
		return true;

	if (nFirst==172 && nSecond>=16 && nSecond<=31)
		return true;

	if (nFirst==0 || nFirst==10)
		return true;

	return false; 
}

bool IsGoodIPPort(uint32_t nIP, uint16_t nPort)
{
	nIP = ntohl(nIP);

	// always filter following IP's
	// -------------------------------------------
	// 0.0.0.0							invalid
	// 127.0.0.0 - 127.255.255.255		Loopback
    // 224.0.0.0 - 239.255.255.255		Multicast
    // 240.0.0.0 - 255.255.255.255		Reserved for Future Use
	// 255.255.255.255					invalid

	if (nIP==0 || (uint8_t)nIP==127 || (uint8_t)nIP>=224)
	{
#ifdef _DEBUG
		if(nIP==0x0100007F) // allow localhost in debug builds
			return true;
#endif
		return false;
	}

	return nPort!=0;
}

wstring IPToStr(uint32_t ip)
{
	wstringstream wString;
	wString << (uint8_t)(ip >> 24) << L"." << (uint8_t)(ip >> 16) << L"." << (uint8_t)(ip >> 8) << L"." << (uint8_t)ip;
	return wString.str();
}

wstring IPToStr(uint32_t ip, uint16_t port)
{
	wstringstream wString;
	wString << (uint8_t)(ip >> 24) << L"." << (uint8_t)(ip >> 16) << L"." << (uint8_t)(ip >> 8) << L"." << (uint8_t)ip;
	wString	<< L":" << (uint32_t)port;
	return wString.str();
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//

#define CRYPT_HEADER_WITHOUTPADDING		    8
#define	MAGICVALUE_UDP						91
#define MAGICVALUE_UDP_SYNC_CLIENT			0x395F2EC1

int CUDPSocket::DecryptReceivedClient(uint8_t *bufIn, int bufLen, uint8_t **bufOut, uint32_t ip, uint32_t *receiverVerifyKey, uint32_t *senderVerifyKey)
{
	int result = bufLen;
	*bufOut = bufIn;

	if (receiverVerifyKey == NULL || senderVerifyKey == NULL) {
		ASSERT(0);;
		return result;
	}

	*receiverVerifyKey = 0;
	*senderVerifyKey = 0;

	if (result <= CRYPT_HEADER_WITHOUTPADDING)
		return result;

	switch (bufIn[0]) {
		case OP_EMULEPROT:
		case OP_KADEMLIAPACKEDPROT:
		case OP_KADEMLIAHEADER:
		case OP_UDPRESERVEDPROT1:
		case OP_UDPRESERVEDPROT2:
		case OP_PACKEDPROT:
			return result; // no encrypted packet (see description on top)
		default:
			;
	}

	// might be an encrypted packet, try to decrypt
	CScoped<CDecryptionKey> pRC4Key = CDecryptionKey::Make(CAbstractKey::eWeakRC4);
	uint32_t value = 0;
	// check the marker bit which type this packet could be and which key to test first, this is only an indicator since old clients have it set random
	// see the header for marker bits explanation
	uint8_t currentTry = ((bufIn[0] & 0x03) == 3) ? 1 : (bufIn[0] & 0x03);
	uint8_t tries;
	if (Kademlia::CKademlia::GetPrefs() == NULL) {
		// if kad never run, no point in checking anything except for ed2k encryption
		tries = 1;
		currentTry = 1;
	} else {
		tries = 3;
	}
	bool kad = false;
	do {
		tries--;
		CHashFunction MD5Hash(CAbstractKey::eMD5);

		if (currentTry == 0) {
			// kad packet with NodeID as key
			kad = true;
			if (Kademlia::CKademlia::GetPrefs()) {
				uint8_t keyData[18];
				Kademlia::CKademlia::GetPrefs()->GetKadID().StoreCryptValue((uint8_t *)&keyData);
				memcpy(keyData + 16, bufIn + 1, 2); // random key part sent from remote client
				MD5Hash.Add(keyData, sizeof(keyData));
			}
		} else if (currentTry == 1) {
			// ed2k packet
			kad = false;
			uint8_t keyData[23];
			CKadHandler::Instance()->GetEd2kHash().StoreCryptValue(keyData);
			CBuffer Buffer(keyData + 16,5,true);
			Buffer.WriteValue<uint32_t>(ip);
			ASSERT(Buffer.GetPosition() + 16 == 20);
			Buffer.WriteValue<uint8>(MAGICVALUE_UDP);
			memcpy(keyData + 21, bufIn + 1, 2); // random key part sent from remote client
			MD5Hash.Add(keyData, sizeof(keyData));
		} else if (currentTry == 2) {
			// kad packet with ReceiverKey as key
			kad = true;
			if (Kademlia::CKademlia::GetPrefs()) {
				uint8_t keyData[6];
				CBuffer Buffer(keyData,4,true);
				Buffer.WriteValue<uint32_t>(GetUDPVerifyKey(ntohl(ip)));
				memcpy(keyData + 4, bufIn + 1, 2); // random key part sent from remote client
				MD5Hash.Add(keyData, sizeof(keyData));
			}
		} else {
			ASSERT(0);
		}
		MD5Hash.Finish();

		ASSERT(MD5Hash.GetSize() == 16);
		pRC4Key->Reset();
		pRC4Key->Setup(MD5Hash.GetKey(), 16);
		pRC4Key->Process(bufIn + 3, (uint8_t*)&value, sizeof(value));

		currentTry = (currentTry + 1) % 3;
	} while (value != MAGICVALUE_UDP_SYNC_CLIENT && tries > 0); // try to decrypt as ed2k as well as kad packet if needed (max 3 rounds)

	if (value != MAGICVALUE_UDP_SYNC_CLIENT)
	{
		//DebugLogWarning(_T("Obfuscated packet expected but magicvalue mismatch on UDP packet from clientIP: %s"), ipstr(dwIP));
		return bufLen; // pass through, let the Receivefunction do the errorhandling on this junk
	}

	// yup this is an encrypted packet
// 	// debugoutput notices
// 	// the following cases are "allowed" but shouldn't happen given that there is only our implementation yet
// 	if (bKad && (pbyBufIn[0] & 0x01) != 0)
// 		DebugLog(_T("Received obfuscated UDP packet from clientIP: %s with wrong key marker bits (kad packet, ed2k bit)"), ipstr(dwIP));
// 	else if (bKad && !bKadRecvKeyUsed && (pbyBufIn[0] & 0x02) != 0)
// 		DebugLog(_T("Received obfuscated UDP packet from clientIP: %s with wrong key marker bits (kad packet, nodeid key, recvkey bit)"), ipstr(dwIP));
// 	else if (bKad && bKadRecvKeyUsed && (pbyBufIn[0] & 0x02) == 0)
// 		DebugLog(_T("Received obfuscated UDP packet from clientIP: %s with wrong key marker bits (kad packet, recvkey key, nodeid bit)"), ipstr(dwIP));

	uint8_t padLen;
	pRC4Key->Process(bufIn + 7, (uint8_t*)&padLen, 1);
	result -= CRYPT_HEADER_WITHOUTPADDING;

	if (result <= padLen) {
		//DebugLogError(_T("Invalid obfuscated UDP packet from clientIP: %s, Paddingsize (%u) larger than received bytes"), ipstr(dwIP), byPadLen);
		return bufLen; // pass through, let the Receivefunction do the errorhandling on this junk
	}

	if (padLen > 0) {
		pRC4Key->Discard(padLen);
	}

	result -= padLen;

	if (kad) {
		if (result <= 8) {
			//DebugLogError(_T("Obfuscated Kad packet with mismatching size (verify keys missing) received from clientIP: %s"), ipstr(dwIP));
			return bufLen; // pass through, let the Receivefunction do the errorhandling on this junk;
		}
		// read the verify keys
		pRC4Key->Process(bufIn + CRYPT_HEADER_WITHOUTPADDING + padLen, (uint8_t*)receiverVerifyKey, 4);
		pRC4Key->Process(bufIn + CRYPT_HEADER_WITHOUTPADDING + padLen + 4, (uint8_t*)senderVerifyKey, 4);
		result -= 8;
	}

	*bufOut = bufIn + (bufLen - result);

	pRC4Key->Process((uint8_t*)*bufOut, (uint8_t*)*bufOut, result);
	return result; // done
}

// Encrypt packet. Key used:
// clientHashOrKadID != NULL					-> clientHashOrKadID
// clientHashOrKadID == NULL && kad && receiverVerifyKey != 0	-> receiverVerifyKey
// else								-> ASSERT
int CUDPSocket::EncryptSendClient(uint8_t **buf, int bufLen, const uint8_t *clientHashOrKadID, bool kad, uint32_t receiverVerifyKey, uint32_t senderVerifyKey)
{
	ASSERT(CKadHandler::Instance()->GetPublicIP() != 0 || kad);
	ASSERT(clientHashOrKadID != NULL || receiverVerifyKey != 0);
	ASSERT((receiverVerifyKey == 0 && senderVerifyKey == 0) || kad);

	uint8_t padLen = 0;			// padding disabled for UDP currently
	const uint32_t cryptHeaderLen = padLen + CRYPT_HEADER_WITHOUTPADDING + (kad ? 8 : 0);
	uint32_t cryptedLen = bufLen + cryptHeaderLen;
	uint8_t *cryptedBuffer = new uint8_t[cryptedLen];
	bool kadRecvKeyUsed = false;

	uint16_t randomKeyPart = 0x08; //GetRand64();
	CScoped<CEncryptionKey> pRC4Key = CEncryptionKey::Make(CAbstractKey::eWeakRC4);
	CHashFunction MD5Hash(CAbstractKey::eMD5);
	if (kad) {
		if ((clientHashOrKadID == NULL || (*((uint64*)&clientHashOrKadID) == 0 && *(((uint64*)&clientHashOrKadID)+1) == 0)) && receiverVerifyKey != 0) {
			kadRecvKeyUsed = true;
			uint8_t keyData[6];
			CBuffer Buffer(keyData,sizeof(keyData),true);
			Buffer.WriteValue<uint32_t>(receiverVerifyKey);
			Buffer.WriteValue<uint16_t>(randomKeyPart);
			MD5Hash.Add(keyData, sizeof(keyData));
			//DEBUG_ONLY( DebugLog(_T("Creating obfuscated Kad packet encrypted by ReceiverKey (%u)"), nReceiverVerifyKey) );  
		}
		else if (clientHashOrKadID != NULL && !(*((uint64*)&clientHashOrKadID) == 0 && *(((uint64*)&clientHashOrKadID)+1) == 0)) {
			uint8_t keyData[18];
			CBuffer Buffer(keyData,sizeof(keyData),true);
			Buffer.WriteData(clientHashOrKadID,16);
			Buffer.WriteValue<uint16_t>(randomKeyPart);
			MD5Hash.Add(keyData, sizeof(keyData));
			//DEBUG_ONLY( DebugLog(_T("Creating obfuscated Kad packet encrypted by Hash/NodeID %s"), md4str(pachClientHashOrKadID)) );  
		}
		else {
			ASSERT(0);
			return bufLen;
		}
	} else {
		uint8_t keyData[23];
		CBuffer Buffer(keyData,sizeof(keyData),true);
		Buffer.WriteData(clientHashOrKadID,16);
		uint32_t ip = ntohl(CKadHandler::Instance()->GetPublicIP());
		Buffer.WriteValue<uint32>(ip);
		Buffer.WriteValue<uint8_t>(MAGICVALUE_UDP);
		Buffer.WriteValue<uint16_t>(randomKeyPart);
		MD5Hash.Add(keyData, sizeof(keyData));
	}
	MD5Hash.Finish();

	ASSERT(MD5Hash.GetSize() == 16);
	pRC4Key->Setup(MD5Hash.GetKey(), 16);

	// create the semi random byte encryption header
	uint8_t semiRandomNotProtocolMarker = 0;
	int i;
	for (i = 0; i < 128; i++) {
		semiRandomNotProtocolMarker = 0x08;//GetRand64();
		semiRandomNotProtocolMarker = kad ? (semiRandomNotProtocolMarker & 0xFE) : (semiRandomNotProtocolMarker | 0x01); // set the ed2k/kad marker bit
		if (kad) {
			// set the ed2k/kad and nodeid/recvkey markerbit
			semiRandomNotProtocolMarker = kadRecvKeyUsed ? ((semiRandomNotProtocolMarker & 0xFE) | 0x02) : (semiRandomNotProtocolMarker & 0xFC);
		} else {
			// set the ed2k/kad marker bit
			semiRandomNotProtocolMarker = (semiRandomNotProtocolMarker | 0x01);
		}

		bool bOk = false;
		switch (semiRandomNotProtocolMarker) { // not allowed values
			case OP_EMULEPROT:
			case OP_KADEMLIAPACKEDPROT:
			case OP_KADEMLIAHEADER:
			case OP_UDPRESERVEDPROT1:
			case OP_UDPRESERVEDPROT2:
			case OP_PACKEDPROT:
				break;
			default:
				bOk = true;
		}

		if (bOk) {
			break;
		}
	}

	if (i >= 128) {
		// either we have _real_ bad luck or the randomgenerator is a bit messed up
		ASSERT(0);
		semiRandomNotProtocolMarker = 0x01;
	}

	cryptedBuffer[0] = semiRandomNotProtocolMarker;
	CBuffer Buffer(cryptedBuffer+1,2,true);
	Buffer.WriteValue<uint16_t>(randomKeyPart);

	uint32_t magicValue = MAGICVALUE_UDP_SYNC_CLIENT;
	pRC4Key->Process((uint8_t*)&magicValue, cryptedBuffer + 3, 4);
	pRC4Key->Process((uint8_t*)&padLen, cryptedBuffer + 7, 1);

	for (int j = 0; j < padLen; j++) {
		uint8_t byRand = (uint8_t)rand();	// they actually don't really need to be random, but it doesn't hurt either
		pRC4Key->Process((uint8_t*)&byRand, cryptedBuffer + CRYPT_HEADER_WITHOUTPADDING + j, 1);
	}

	if (kad) {
		pRC4Key->Process((uint8_t*)&receiverVerifyKey, cryptedBuffer + CRYPT_HEADER_WITHOUTPADDING + padLen, 4);
		pRC4Key->Process((uint8_t*)&senderVerifyKey, cryptedBuffer + CRYPT_HEADER_WITHOUTPADDING + padLen + 4, 4);
	}

	pRC4Key->Process(*buf, cryptedBuffer + cryptHeaderLen, bufLen);
	delete [] *buf;
	*buf = cryptedBuffer;

	return cryptedLen;
}
