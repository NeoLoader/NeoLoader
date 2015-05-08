/*

Copyright (c) 2009, Arvid Norberg
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the distribution.
    * Neither the name of the author nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef TORRENT_ADDRESS_HPP_INCLUDED
#define TORRENT_ADDRESS_HPP_INCLUDED

#include "config.hpp"
#include "error_code.hpp"
#include "assert.hpp"
#ifndef __APPLE__
#include <array>
#else
#include <vector>
class array_16: public std::vector<unsigned char>
{
public:
	array_16() {resize(16);}
};
#endif

#ifndef WIN32
   #include <unistd.h>
   #include <cstdlib>
   #include <cstring>
   #include <netdb.h>
   #include <arpa/inet.h>
   #include <sys/types.h>
   #include <sys/socket.h>
   #include <netinet/in.h>
#endif
#ifdef USING_QT
// Note: this is not realy a Qt dependance but its a dependace on neoHelper lib,
//	to provide a custom inet_ntop and inet_pton so that the binarys run in ond WinXP machines.
#include "../../Framework/Address.h"
#else
#define _inet_ntop inet_ntop
#define _inet_pton inet_pton
#ifdef WIN32
   #include <winsock2.h>
   #include <ws2tcpip.h>
   #include <wspiapi.h>
#endif
#endif

namespace libtorrent
{
	class address
	{
	public:
#ifdef __APPLE__
		typedef array_16 bytes_type;
#else
		typedef std::array<unsigned char, 16> bytes_type;
#endif

		address() {len_ = 4; memset(&ip_[0], 0, len_);}
		address(const bytes_type& ip, size_t len)		{ip_ = ip; len_ = len;}

		const bytes_type& to_bytes() const				{return ip_;}

		unsigned long to_ulong() const 
		{
			unsigned long ip = 0;
			if(len_ == 4)
				memcpy(&ip, &ip_[0], len_);
			return ntohl(ip);
		};

		bool operator < (const address &other) const	{if(len_ != other.len_) return len_ < other.len_; 
															return memcmp(&ip_[0], &other.ip_[0], len_) < 0;}
		bool operator == (const address &other) const	{return (len_ == other.len_) && memcmp(&ip_[0], &other.ip_[0], len_) == 0;}
		bool operator != (const address &other) const	{return !(*this == other);}

		bool is_v4() const		{return len_ == 4;}
		bool is_v6() const		{return len_ == 16;}

		const address& to_v4() const	{TORRENT_ASSERT(is_v4()); return *this;}
		const address& to_v6() const	{TORRENT_ASSERT(is_v6()); return *this;}

		std::string to_string() const
		{
			char Dest[46];
			if(is_v4())
				_inet_ntop(AF_INET, (void*)&ip_[0], Dest, 46);
			else if(is_v6())
				_inet_ntop(AF_INET6, (void*)&ip_[0], Dest, 46);
			else
				Dest[0] = 0;
			return Dest;
		}
		std::string to_string(boost::system::error_code& ec) const {return to_string();}

		bool is_v4_mapped() const
		{
			if(len_ != 16)
				return false;
			return ((ip_[0] == 0) && (ip_[1] == 0) && (ip_[2] == 0) && (ip_[3] == 0)
			  && (ip_[4] == 0) && (ip_[5] == 0) && (ip_[6] == 0) && (ip_[7] == 0)
			  && (ip_[8] == 0) && (ip_[9] == 0) && (ip_[10] == 0xff) && (ip_[11] == 0xff));
		}

		bool is_multicast() const
		{
			if(len_ == 4)
				return (to_ulong() & 0xF0000000) == 0xE0000000;
			return (ip_[0] == 0xff);
		}

		static address v4_mapped(const address& addr)
		{
			bytes_type v4_bytes = addr.to_bytes();
			bytes_type v6_bytes;
			memset(&v6_bytes[0], 0, 10);
			v6_bytes[10] = 0xFF;
			v6_bytes[11] = 0xFF;
			memcpy(&v6_bytes[12], &v4_bytes[0], 4);
			return address(v6_bytes, 16);
		}

	protected:

		bytes_type ip_;
		size_t len_;
	};

	class address_v4: public address
	{
	public:
		__inline static size_t size() {return 4;}

		address_v4() {len_ = size();}
		address_v4(const bytes_type& ip)	{ip_ = ip; len_ = size();}
		address_v4(const std::string& str) 
		{
			unsigned long ip = inet_addr(str.c_str());
			len_ = size();
			memcpy(&ip_[0], &ip, size());
		}
		address_v4(const address& other) : address(other) {TORRENT_ASSERT(is_v4());}
		address_v4(unsigned long ip) 
		{
			ip = ntohl(ip);
			len_ = size();
			memcpy(&ip_[0], &ip, size());
		}

		static address loopback()
		{
			return address_v4(0x7F000001);
		}
		static address broadcast()
		{
			return address_v4(0xFFFFFFFF);
		}

		static address any() { return address_v4(); }
	};

	class address_v6: public address
	{
	public:
		__inline static size_t size() {return 16;}

		address_v6() {len_ = size();}
		address_v6(const bytes_type& ip)	{ip_ = ip; len_ = size();}
		address_v6(const std::string& str) 
		{
			len_ = size();
			_inet_pton(AF_INET6, str.c_str(), &ip_[0]);
		}
		address_v6(const address& other) : address(other) {TORRENT_ASSERT(is_v6());}

		static address loopback()
		{
			address_v6::bytes_type v6_bytes;
			memset(&v6_bytes[0], 0, 15);
			v6_bytes[15] = 1;
			return address(v6_bytes, 16);
		}

		static address any() { return address_v6(); }
	};

	__inline std::ostream& operator<<(std::ostream& os, const address& addr)
	{
		os << addr.to_string();
		return os;
	}
}

#endif

