#pragma once

#include "DHT.h"
#include <private/qobject_p.h>

class CDHTPrivate: public QObjectPrivate
{
	Q_DECLARE_PUBLIC(CDHT)
public:

	static bool SendPacket(void* userdata, libtorrent::entry& e, udp::endpoint const& ep, int flags);
	static void PeersFound(void* userdata, std::vector<tcp::endpoint> const& e, libtorrent::sha1_hash const& ih);
	static void EndLookup(void* userdata, libtorrent::sha1_hash const& ih);
	static void SetAddress(void* userdata, libtorrent::address const& ip, libtorrent::address const& source) {
		CDHT* pDHT = (CDHT*)userdata;
		pDHT->d_func()->SetAddress(ip, source);
	}
	
protected:
	void SetAddress(libtorrent::address const& ip, libtorrent::address const& source);

	struct external_ip_t
	{
		external_ip_t(): num_votes(0) {}

		bool add_vote(libtorrent::sha1_hash const& k)
		{
			if (voters.find(k)) return false;
			voters.set(k);
			++num_votes;
			return true;
		}
		bool operator<(external_ip_t const& rhs) const
		{
			return num_votes < rhs.num_votes;
		}

		// this is a bloom filter of the IPs that have
		// reported this address
		libtorrent::bloom_filter<16> voters;
		// this is the actual external address
		libtorrent::address addr;
		// the total number of votes for this IP
		boost::uint16_t num_votes;
	};

	libtorrent::bloom_filter<32> m_external_address_voters;
	std::vector<external_ip_t> m_external_addresses;
	libtorrent::address m_external_address;

	std::map<std::string, udp::endpoint> m_router_nodes;
};
