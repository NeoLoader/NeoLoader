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

#ifndef TORRENT_SESSION_SETTINGS_HPP_INCLUDED
#define TORRENT_SESSION_SETTINGS_HPP_INCLUDED

#include "libtorrent/version.hpp"
#include "libtorrent/config.hpp"
#include "libtorrent/version.hpp"

#include <string>

namespace libtorrent
{
	struct dht_settings
	{
		dht_settings()
			: max_peers_reply(100)
			, search_branching(5)
#ifndef TORRENT_NO_DEPRECATE
			, service_port(0)
#endif
			, max_fail_count(20)
			, max_torrents(2000)
			, max_dht_items(700)
			, max_torrent_search_reply(20)
			, restrict_routing_ips(true)
			, restrict_search_ips(true)
		{}
		
		// the maximum number of peers to send in a
		// reply to get_peers
		int max_peers_reply;

		// the number of simultanous "connections" when
		// searching the DHT.
		int search_branching;
		
#ifndef TORRENT_NO_DEPRECATE
		// the listen port for the dht. This is a UDP port.
		// zero means use the same as the tcp interface
		int service_port;
#endif
		
		// the maximum number of times a node can fail
		// in a row before it is removed from the table.
		int max_fail_count;

		// this is the max number of torrents the DHT will track
		int max_torrents;

		// max number of items the DHT will store
		int max_dht_items;

		// the max number of torrents to return in a
		// torrent search query to the DHT
		int max_torrent_search_reply;

		// when set, nodes whose IP address that's in
		// the same /24 (or /64 for IPv6) range in the
		// same routing table bucket. This is an attempt
		// to mitigate node ID spoofing attacks
		// also restrict any IP to only have a single
		// entry in the whole routing table
		bool restrict_routing_ips;

		// applies the same IP restrictions on nodes
		// received during a DHT search (traversal algorithm)
		bool restrict_search_ips;
	};
}

#endif
