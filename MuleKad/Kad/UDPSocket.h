#pragma once
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

#ifndef WIN32
   #include <unistd.h>
   #include <cstdlib>
   #include <cstring>
   #include <netdb.h>
   #include <arpa/inet.h>
   #include <sys/types.h>
   #include <sys/socket.h>
   #include <netinet/in.h>
   #define SOCKET int
   #define SOCKET_ERROR (-1)
   #define closesocket close
#else
   #include <winsock2.h>
   #include <ws2tcpip.h>
   #include <wspiapi.h>
#endif

class CPacket;

class CUDPSocket
{
public:
	CUDPSocket(uint16_t UDPPort, uint32_t UDPKey);
	~CUDPSocket();

	static CUDPSocket*	Instance()		{return m_Instance;}

	void				Process();

	void				SendPacket(char*& buf, size_t& len, uint32_t ip, uint16_t port, bool bEncrypt, const uint8_t* pachTargetClientHashORKadID, bool bKad, uint32_t nReceiverVerifyKey);
	void				ProcessPacket(uint32_t ip, uint16_t port, uint8_t* buffer, size_t length);

	uint32_t			GetUDPVerifyKey(uint32_t targetIP);
	uint32_t			GetUDPVerifyKey(uint32_t key, uint32_t targetIP);

	int					DecryptReceivedClient(uint8_t *bufIn, int bufLen, uint8_t **bufOut, uint32_t ip, uint32_t *receiverVerifyKey, uint32_t *senderVerifyKey);
	int					EncryptSendClient(uint8_t **buf, int bufLen, const uint8_t *clientHashOrKadID, bool kad, uint32_t receiverVerifyKey, uint32_t senderVerifyKey);

protected:
	uint32_t			m_UDPKey;
	SOCKET				m_socket;

	static CUDPSocket*	m_Instance;
};

bool IsLanIP(uint32_t nIP);
bool IsGoodIPPort(uint32_t nIP, uint16_t nPort);
wstring IPToStr(uint32_t ip);
wstring IPToStr(uint32_t ip, uint16_t port);


