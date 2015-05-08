/*

Copyright (c) 2007, Arvid Norberg
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

#include "pch.h"

#include <string>

#include "broadcast_socket.hpp"
#include "address.hpp"

namespace libtorrent
{
	bool is_local(address const& a)
	{
		TORRENT_TRY {
#if TORRENT_USE_IPV6
			if (a.is_v6())
			{
				return a.to_v6() == address_v6::loopback();
			}
#endif
			address_v4 a4 = a.to_v4();
			unsigned long ip = a4.to_ulong();
			return ((ip & 0xff000000) == 0x0a000000 // 10.x.x.x
				|| (ip & 0xfff00000) == 0xac100000 // 172.16.x.x
				|| (ip & 0xffff0000) == 0xc0a80000 // 192.168.x.x
				|| (ip & 0xffff0000) == 0xa9fe0000 // 169.254.x.x
				|| (ip & 0xff000000) == 0x7f000000); // 127.x.x.x
		} TORRENT_CATCH(std::exception& e) { return false; }
	}

	bool is_loopback(address const& addr)
	{
#if TORRENT_USE_IPV6
		TORRENT_TRY {
			if (addr.is_v4())
				return addr.to_v4() == address_v4::loopback();
			else
				return addr.to_v6() == address_v6::loopback();
		} TORRENT_CATCH(std::exception& e) { return false; }
#else
		return addr.to_v4() == address_v4::loopback();
#endif
	}

	bool is_multicast(address const& addr)
	{
#if TORRENT_USE_IPV6
		TORRENT_TRY {
			if (addr.is_v4())
				return addr.to_v4().is_multicast();
			else
				return addr.to_v6().is_multicast();
		} TORRENT_CATCH(std::exception& e) { return false; }
#else
		return addr.to_v4().is_multicast();
#endif
	}

	bool is_any(address const& addr)
	{
		TORRENT_TRY {
#if TORRENT_USE_IPV6
		if (addr.is_v4())
			return addr.to_v4() == address_v4::any();
		else if (addr.to_v6().is_v4_mapped())
			return (addr.to_v6().to_v4() == address_v4::any());
		else
			return addr.to_v6() == address_v6::any();
#else
		return addr.to_v4() == address_v4::any();
#endif
		} TORRENT_CATCH(std::exception& e) { return false; }
	}

	TORRENT_EXPORT bool is_teredo(address const& addr)
	{
#if TORRENT_USE_IPV6
		TORRENT_TRY {
			if (!addr.is_v6()) return false;
			boost::uint8_t teredo_prefix[] = {0x20, 0x01, 0, 0};
			address_v6::bytes_type b = addr.to_v6().to_bytes();
			return memcmp(&b[0], teredo_prefix, 4) == 0;
		} TORRENT_CATCH(std::exception& e) { return false; }
#else
		return false;
#endif
	}

	/*bool supports_ipv6()
	{
#if TORRENT_USE_IPV6
		TORRENT_TRY {
			error_code ec;
			address::from_string("::1", ec);
			return !ec;
		} TORRENT_CATCH(std::exception& e) { return false; }
#else
		return false;
#endif
	}*/

	// count the length of the common bit prefix
	int common_bits(unsigned char const* b1
		, unsigned char const* b2, int n)
	{
		for (int i = 0; i < n; ++i, ++b1, ++b2)
		{
			unsigned char a = *b1 ^ *b2;
			if (a == 0) continue;
			int ret = i * 8 + 8;
			for (; a > 0; a >>= 1) --ret;
			return ret;
		}
		return n * 8;
	}

	// returns the number of bits in that differ from the right
	// between the addresses. The larger number, the further apart
	// the IPs are
	int cidr_distance(address const& a1, address const& a2)
	{
#if TORRENT_USE_IPV6
		if (a1.is_v4() && a2.is_v4())
		{
#endif
			// both are v4
			address_v4::bytes_type b1 = a1.to_v4().to_bytes();
			address_v4::bytes_type b2 = a2.to_v4().to_bytes();
			return address_v4::size() * 8
				- common_bits(b1.data(), b2.data(), b1.size());
#if TORRENT_USE_IPV6
		}
	
		address_v6::bytes_type b1;
		address_v6::bytes_type b2;
		if (a1.is_v4()) b1 = address_v6::v4_mapped(a1.to_v4()).to_bytes();
		else b1 = a1.to_v6().to_bytes();
		if (a2.is_v4()) b2 = address_v6::v4_mapped(a2.to_v4()).to_bytes();
		else b2 = a2.to_v6().to_bytes();
		return address_v6::size() * 8
			- common_bits(b1.data(), b2.data(), b1.size());
#endif
	}
}


