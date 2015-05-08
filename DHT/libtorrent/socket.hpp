/*

Copyright (c) 2003, Arvid Norberg
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

#ifndef TORRENT_SOCKET_HPP_INCLUDED
#define TORRENT_SOCKET_HPP_INCLUDED

#include "address.hpp"

namespace libtorrent
{
	class endpoint
	{
	public:
		endpoint() : port_(0) {}
		endpoint(libtorrent::address addr, uint16_t port)
			: port_(port), address_(addr) {}

		bool operator < (const endpoint &other) const	
		{
			if(port_ < other.port_) return true;
			if(port_ > other.port_) return false;
			return address_ < other.address_;
		}
		bool operator == (const endpoint &other) const		{return port_ == other.port_ && address_ == other.address_;}
		bool operator != (const endpoint &other) const		{return !(*this == other);}

		void set_address(const libtorrent::address& addr)	{address_ = addr;}
		const libtorrent::address&	address() const			{return address_;}
		void set_port(boost::uint16_t port)					{port_ = port;}
		boost::uint16_t	port() const						{return port_;}

	protected:
		libtorrent::address address_;
		boost::uint16_t port_;
	};

	__inline std::ostream& operator<<(std::ostream& os, endpoint const& ep)
	{
		return os;
	}


#define udp libtorrent
#define tcp libtorrent
}

#endif // TORRENT_SOCKET_HPP_INCLUDED

