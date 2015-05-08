/*****************************************************************************
Copyright (c) 2001 - 2011, The Board of Trustees of the University of Illinois.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above
  copyright notice, this list of conditions and the
  following disclaimer.

* Redistributions in binary form must reproduce the
  above copyright notice, this list of conditions
  and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the University of Illinois
  nor the names of its contributors may be used to
  endorse or promote products derived from this
  software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
****************************************************************************/

/****************************************************************************
written by
   Yunhong Gu, last updated 01/27/2011
*****************************************************************************/

#ifndef WIN32
   #include <netdb.h>
   #include <arpa/inet.h>
   #include <unistd.h>
   #include <fcntl.h>
   #include <cstring>
   #include <cstdio>
   #include <cerrno>
#else
   #include <winsock2.h>
   #include <ws2tcpip.h>
   #ifdef LEGACY_WIN32
      #include <wspiapi.h>
   #endif
#endif
#include "channel.h"
#include "packet.h"
#include "md5.h"
#include "rc4.h"
#include "common.h"
#include "debug.h"

#ifdef WIN32
   #define socklen_t int
#endif

#ifndef WIN32
   #define NET_ERROR errno
#else
   #define NET_ERROR WSAGetLastError()
#endif


CChannel::CChannel():
m_iIPversion(AF_INET),
m_iSockAddrSize(sizeof(sockaddr_in)),
m_RecvKey(0),
m_iSocket(),
m_iSndBufSize(65536),
m_iRcvBufSize(65536)
{
}

CChannel::CChannel(const int& version, const uint64_t& RecvKey, const bool& Obfuscate):
m_iIPversion(version),
m_RecvKey(RecvKey),
m_Obfuscate(Obfuscate),
m_iSocket(),
m_iSndBufSize(65536),
m_iRcvBufSize(65536)
{
   m_iSockAddrSize = (AF_INET == m_iIPversion) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);
}

CChannel::~CChannel()
{
}

void CChannel::open(const sockaddr* addr)
{
   // construct an socket
   m_iSocket = socket(m_iIPversion, SOCK_DGRAM, 0);

   #ifdef WIN32
      if (INVALID_SOCKET == m_iSocket)
   #else
      if (m_iSocket < 0)
   #endif
      throw CUDTException(1, 0, NET_ERROR);

   if (NULL != addr)
   {
      socklen_t namelen = m_iSockAddrSize;

      if (0 != bind(m_iSocket, addr, namelen))
         throw CUDTException(1, 3, NET_ERROR);
   }
   else
   {
      //sendto or WSASendTo will also automatically bind the socket
      addrinfo hints;
      addrinfo* res;

      memset(&hints, 0, sizeof(struct addrinfo));

      hints.ai_flags = AI_PASSIVE;
      hints.ai_family = m_iIPversion;
      hints.ai_socktype = SOCK_DGRAM;

      if (0 != getaddrinfo(NULL, "0", &hints, &res))
         throw CUDTException(1, 3, NET_ERROR);

      if (0 != bind(m_iSocket, res->ai_addr, res->ai_addrlen))
         throw CUDTException(1, 3, NET_ERROR);

      freeaddrinfo(res);
   }

   setUDPSockOpt();
}

void CChannel::open(UDPSOCKET udpsock)
{
   m_iSocket = udpsock;
   setUDPSockOpt();
}

void CChannel::setUDPSockOpt()
{
   #if defined(BSD) || defined(OSX)
      // BSD system will fail setsockopt if the requested buffer size exceeds system maximum value
      int maxsize = 64000;
      if (0 != setsockopt(m_iSocket, SOL_SOCKET, SO_RCVBUF, (char*)&m_iRcvBufSize, sizeof(int)))
         setsockopt(m_iSocket, SOL_SOCKET, SO_RCVBUF, (char*)&maxsize, sizeof(int));
      if (0 != setsockopt(m_iSocket, SOL_SOCKET, SO_SNDBUF, (char*)&m_iSndBufSize, sizeof(int)))
         setsockopt(m_iSocket, SOL_SOCKET, SO_SNDBUF, (char*)&maxsize, sizeof(int));
   #else
      // for other systems, if requested is greated than maximum, the maximum value will be automactally used
      if ((0 != setsockopt(m_iSocket, SOL_SOCKET, SO_RCVBUF, (char*)&m_iRcvBufSize, sizeof(int))) ||
          (0 != setsockopt(m_iSocket, SOL_SOCKET, SO_SNDBUF, (char*)&m_iSndBufSize, sizeof(int))))
         throw CUDTException(1, 3, NET_ERROR);
   #endif

   timeval tv;
   tv.tv_sec = 0;
   #if defined (BSD) || defined (OSX)
      // Known BSD bug as the day I wrote this code.
      // A small time out value will cause the socket to block forever.
      tv.tv_usec = 10000;
   #else
      tv.tv_usec = 100;
   #endif

   #ifdef UNIX
      // Set non-blocking I/O
      // UNIX does not support SO_RCVTIMEO
      int opts = fcntl(m_iSocket, F_GETFL);
      if (-1 == fcntl(m_iSocket, F_SETFL, opts | O_NONBLOCK))
         throw CUDTException(1, 3, NET_ERROR);
   #elif WIN32
      DWORD ot = 1; //milliseconds
      if (0 != setsockopt(m_iSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&ot, sizeof(DWORD)))
         throw CUDTException(1, 3, NET_ERROR);
   #else
      // Set receiving time-out value
      if (0 != setsockopt(m_iSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(timeval)))
         throw CUDTException(1, 3, NET_ERROR);
   #endif
}

void CChannel::close() const
{
   #ifndef WIN32
      ::close(m_iSocket);
   #else
      closesocket(m_iSocket);
   #endif
}

int CChannel::getSndBufSize()
{
   socklen_t size = sizeof(socklen_t);

   getsockopt(m_iSocket, SOL_SOCKET, SO_SNDBUF, (char *)&m_iSndBufSize, &size);

   return m_iSndBufSize;
}

int CChannel::getRcvBufSize()
{
   socklen_t size = sizeof(socklen_t);

   getsockopt(m_iSocket, SOL_SOCKET, SO_RCVBUF, (char *)&m_iRcvBufSize, &size);

   return m_iRcvBufSize;
}

void CChannel::setSndBufSize(const int& size)
{
   m_iSndBufSize = size;
}

void CChannel::setRcvBufSize(const int& size)
{
   m_iRcvBufSize = size;
}

void CChannel::getSockAddr(sockaddr* addr) const
{
   socklen_t namelen = m_iSockAddrSize;

   getsockname(m_iSocket, addr, &namelen);
}

void CChannel::getPeerAddr(sockaddr* addr) const
{
   socklen_t namelen = m_iSockAddrSize;

   getpeername(m_iSocket, addr, &namelen);
}

//////////////////////////////////////////////////////////////////////////////
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                          Radom Seed                           |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ <- from here on obfuscation starts
//   |P| | | |Discard|          Reserved             |  Padding Len  |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                                                               |
//   ~                      UDT Packet Header                        ~
//   |                                                               |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                                                               |
//   ~                Data / Control Information Field               ~
//   |                                                               |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                                                               |
//   ~                          Padding                              ~
//   |                                                               |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//   Discard: amount of key bytes to be discarded in multiple of 256 in addition to the first 256
//
//   bit P:
//      0: Plaintext Payload
//      1: Obfuscated Payload
//

int CChannel::sendto(const sockaddr* addr, CPacket& packet, const uint64_t& SendKey) const
{
   //UDT_TRACE_1("Sending packet, Sendkey: %I64u", SendKey);

   bool bControl = packet.getFlag() == 1;
   bool plObfuscate = bControl || m_Obfuscate;

   iovec iov[4];
   int iovlen = 0;
   uint32_t Crypto[2];
   uint32_t Header[4];
   char* Payload = NULL;
   int Length = 0;
   int uDiscard = 0; //total drop ist (n+1)*256
   if(packet.m_pcLen >= 10*1024)
	   uDiscard = 3; // discard only 1024 key bytes in total for large packets
   char Padding[64];
   int PadLen = rand()%32;
   if(60 + 20 + 8 + packet.m_iPktHdrSize + packet.m_pcLen + PadLen > 0xFFFF)
	   PadLen = 0; // packet at the maximal datagram limit, don't pad it
   
   rc4_sbox_t rc4;
   if(SendKey)
   {
      uint32_t& uSeed = Crypto[0];
	  uint32_t& uOpt = Crypto[1];

	  // generate a strong random seed
      md5_state_t Seed;
      md5_init(&Seed);
	  uint64_t utime = CTimer::getTime();
	  short urand = rand();
      md5_append(&Seed, (const unsigned char*)&urand, sizeof(urand));
	  md5_append(&Seed, (const unsigned char*)&utime, sizeof(utime));
	  md5_append(&Seed, (const unsigned char*)addr, m_iSockAddrSize);
	  unsigned char seed[16];
	  md5_finish(&Seed, seed);
	  uSeed = ((uint32_t*)seed)[0] ^ ((uint32_t*)seed)[1] ^ ((uint32_t*)seed)[2] ^ ((uint32_t*)seed)[3];

	  uOpt = (PadLen & 0xff);
	  if(bControl || plObfuscate)
		uOpt |= 1 << 31;
	  if(uDiscard)
		  uOpt |= (uDiscard & 0x0F) << 24;

      md5_state_t md5;
      md5_init(&md5);
      md5_append(&md5, (const unsigned char*)&SendKey, sizeof(SendKey));
	  md5_append(&md5, (const unsigned char*)&uSeed, sizeof(uSeed));
	  unsigned char hash[16];
      md5_finish(&md5, hash);
	  rc4_init(&rc4, hash, 16);
	  rc4_transform(&rc4, NULL, 256); // drop the first insecure 256 bytes

      for (int k = 1; k < 2; ++ k)
         Crypto[k] = ntohl(Crypto[k]);

	  rc4_transform(&rc4, (unsigned char*)(Crypto+1), 4);

	  if(uDiscard > 0)
		  rc4_transform(&rc4, NULL, uDiscard*256); // drop additional n*256 bytes

	  iov[iovlen].iov_len = 8;
      iov[iovlen].iov_base = (char*)Crypto;
	  iovlen++;
   }

   iov[iovlen].iov_len = packet.m_iPktHdrSize;
   iov[iovlen].iov_base = (char*)Header;
   iovlen++;
   memcpy(Header, (char*)packet.m_nHeader, packet.m_iPktHdrSize);
   // convert packet header into network order
   for (int k = 0; k < 4; ++ k)
      Header[k] = ntohl(Header[k]);
   if(SendKey)
      rc4_transform(&rc4, (unsigned char*)Header, packet.m_iPktHdrSize);

   if(bControl || plObfuscate)
   {
      Length = packet.m_pcLen;
	  Payload = new char[Length];
      iov[iovlen].iov_len = Length;
	  iov[iovlen].iov_base = Payload;
	  memcpy(Payload, (char*)packet.m_pcData, packet.m_pcLen);
   }
   else 
   {
	  iov[iovlen].iov_len = packet.m_pcLen;
      iov[iovlen].iov_base = packet.m_pcData;
   }
   iovlen++;

   if(bControl)
   {
      for (int i = 0, n = Length / 4; i < n; ++ i)
         *((uint32_t *)Payload + i) = htonl(*((uint32_t *)Payload + i));
   }

   if(plObfuscate && SendKey)
      rc4_transform(&rc4, (unsigned char*)Payload, Length);

   if(PadLen > 0)
   {
      for(int i = 0; i < PadLen/4; i++)
	     ((uint32_t*)Padding)[i] = rand();

	  iov[iovlen].iov_len = PadLen;
      iov[iovlen].iov_base = Padding;
	  iovlen++;
   }

   #ifndef WIN32
      msghdr mh;
      mh.msg_name = (sockaddr*)addr;
      mh.msg_namelen = m_iSockAddrSize;
      mh.msg_iov = iov;
      mh.msg_iovlen = iovlen;
      mh.msg_control = NULL;
      mh.msg_controllen = 0;
      mh.msg_flags = 0;

      int res = sendmsg(m_iSocket, &mh, 0);
   #else
      DWORD size = CPacket::m_iPktHdrSize + packet.getLength();
      int addrsize = m_iSockAddrSize;
      int res = WSASendTo(m_iSocket, (LPWSABUF)iov, iovlen, &size, 0, addr, addrsize, NULL, NULL);
      res = (0 == res) ? size : -1;
   #endif

   if(Payload)
      delete [] Payload;

   return res;
}

int CChannel::recvfrom(sockaddr* addr, CPacket& packet) const
{
   iovec iov[4];
   int iovlen = 0;
   uint32_t Crypto[2];
   //char Padding[64];
   
   if(m_RecvKey)
   {
	  iov[iovlen].iov_len = 8;
      iov[iovlen].iov_base = (char*)Crypto;
	  iovlen++;
   }

   iov[iovlen].iov_len = packet.m_iPktHdrSize;
   iov[iovlen].iov_base = (char*)packet.m_nHeader;
   iovlen++;

   iov[iovlen].iov_len = packet.m_pcLen;
   iov[iovlen].iov_base = packet.m_pcData;
   iovlen++;

   /*if(m_RecvKey)
   {
	  // Note: if the previuse payload is < than m_pcLen not all Padding wil end up here, we wil have to copy up
      iov[iovlen].iov_len = 64;
      iov[iovlen].iov_base = (char*)Padding;
	  iovlen++;
   }*/

   #ifndef WIN32
      msghdr mh;   
      mh.msg_name = addr;
      mh.msg_namelen = m_iSockAddrSize;
      mh.msg_iov = iov;
      mh.msg_iovlen = iovlen;
      mh.msg_control = NULL;
      mh.msg_controllen = 0;
      mh.msg_flags = 0;

      #ifdef UNIX
         fd_set set;
         timeval tv;
         FD_ZERO(&set);
         FD_SET(m_iSocket, &set);
         tv.tv_sec = 0;
         tv.tv_usec = 10000;
         select(m_iSocket+1, &set, NULL, &set, &tv);
      #endif

      int res = recvmsg(m_iSocket, &mh, 0);
   #else
      DWORD size = CPacket::m_iPktHdrSize + packet.getLength();
      DWORD flag = 0;
      int addrsize = m_iSockAddrSize;

      int res = WSARecvFrom(m_iSocket, (LPWSABUF)iov, iovlen, &size, &flag, addr, &addrsize, NULL, NULL);
      res = (0 == res) ? size : -1;
   #endif

   if (res <= 0)
   {
      packet.setLength(-1);
      return -1;
   }

   //UDT_TRACE_1("Reciving packet, m_RecvKey: %I64u", m_RecvKey);

   bool plObfuscate = false;
   int PadLen = 0;

   rc4_sbox_t rc4;
   if(m_RecvKey)
   {
	  res -= 2*4;
      uint32_t& uSeed = Crypto[0];

      md5_state_t md5;
      md5_init(&md5);
      md5_append(&md5, (const unsigned char*)&m_RecvKey, sizeof(m_RecvKey));
	  md5_append(&md5, (const unsigned char*)&uSeed, sizeof(uSeed));
	  unsigned char hash[16];
      md5_finish(&md5, hash);
	  rc4_init(&rc4, hash, 16);
	  rc4_transform(&rc4, NULL, 256); // drop the first insecure 256 bytes

	  rc4_transform(&rc4, (unsigned char*)(Crypto+1), 4);

      for (int k = 1; k < 2; ++ k)
         Crypto[k] = ntohl(Crypto[k]);

	  uint32_t& uOpt = Crypto[1];
	  PadLen = (uOpt & 0xff);
	  if(res >= PadLen)
		  res -= PadLen;
	  plObfuscate = (uOpt >> 31) == 1;

	  int uDiscard = (uOpt >> 24) & 0x0F;
	  if(uDiscard > 0)
		  rc4_transform(&rc4, NULL, uDiscard*256); // drop additional n*256 bytes
   }

   if (res < CPacket::m_iPktHdrSize)
   {
      packet.setLength(-1);
      return -1;
   }

   packet.setLength(res - CPacket::m_iPktHdrSize);

   if(m_RecvKey)
      rc4_transform(&rc4, (unsigned char*)packet.m_nHeader, packet.m_iPktHdrSize);
   // convert back into local host order
   for (int i = 0; i < 4; ++ i)
      packet.m_nHeader[i] = ntohl(packet.m_nHeader[i]);

   bool bControl = packet.getFlag() == 1;

   if(plObfuscate && m_RecvKey)
      rc4_transform(&rc4, (unsigned char*)packet.m_pcData, packet.m_pcLen);

   if(bControl)
   {
      for (int i = 0, n = packet.m_pcLen / 4; i < n; ++ i)
         *((uint32_t *)packet.m_pcData + i) = htonl(*((uint32_t *)packet.m_pcData + i));
   }

   return packet.getLength();
}
