#include "pch.h"
#include "DHT.h"
#include "kademlia/node.hpp"
#include "libtorrent/bencode.hpp"
#include "libtorrent/lazy_entry.hpp"
#include "libtorrent/time.hpp"
#include "libtorrent/broadcast_socket.hpp"
#include "libtorrent/socket_io.hpp"
#include "libtorrent/random.hpp"
#include "libtorrent/session_status.hpp"
#include <QDebug>
#include "DHT_p.h"

int _TPeerList_type = qRegisterMetaType<TPeerList>("TPeerList");

libtorrent::address CAddr2Addr(const CAddress& Address);
CAddress Addr2CAddr(const libtorrent::address& addr);

CDHT::CDHT(const QByteArray& NodeID, const CAddress& Address, QObject* parent)
 : QObject(*new CDHTPrivate, parent)
{
	Q_D(CDHT);

	libtorrent::update_time_now();
	quint64 now = libtorrent::time_now().time;

	m_last_new_key = now;
	m_next_tick = now + libtorrent::seconds(5).diff;
	m_next_timeout = now + libtorrent::seconds(1).diff;

	m_dht_settings = new libtorrent::dht_settings;

	d->m_external_address = CAddr2Addr(Address);
	m_dht = new libtorrent::dht::node_impl(&CDHTPrivate::SendPacket, *m_dht_settings, NodeID.size() == 20 ? libtorrent::dht::node_id(NodeID.data()) : libtorrent::dht::node_id(), d->m_external_address, &CDHTPrivate::SetAddress, this);

	m_uTimerID = startTimer(10);

	connect(this, SIGNAL(AddressChanged()), this, SLOT(Restart()), Qt::QueuedConnection);
}

CDHT::~CDHT()
{
	killTimer(m_uTimerID);

	delete m_dht;
	delete m_dht_settings;
}

void CDHT::Bootstrap(const TPeerList& PeerList)
{
	std::vector<udp::endpoint> initial_nodes;
	foreach(const SPeer& Peer, PeerList)
		initial_nodes.push_back(udp::endpoint(CAddr2Addr(Peer.Address), Peer.Port));
	m_dht->bootstrap(initial_nodes);
}

void CDHT::AddNode(const SPeer& Peer)
{
	m_dht->add_node(udp::endpoint(CAddr2Addr(Peer.Address), Peer.Port));
}

void CDHT::AddRouterNode(const QString& Host, quint16 Port)
{
	Q_D(CDHT);
	TORRENT_ASSERT(Port);
	if(d->m_router_nodes.insert(std::map<std::string, udp::endpoint>::value_type(Host.toStdString(), udp::endpoint(libtorrent::address(), Port))).second)
		QHostInfo::lookupHost(Host, this, SLOT(OnHostInfo(const QHostInfo&)));
}

void CDHT::OnHostInfo(const QHostInfo& HostInfo)
{
	Q_D(CDHT);
	std::map<std::string, udp::endpoint>::iterator I = d->m_router_nodes.find(HostInfo.hostName().toStdString());
	if(I == d->m_router_nodes.end())
	{
		TORRENT_ASSERT(0);
		return;
	}
	
	QList<QHostAddress> Addresses = HostInfo.addresses();
	if(!Addresses.isEmpty())
	{
		I->second.set_address(CAddr2Addr(CAddress(Addresses.at(qrand()%Addresses.count()).toString())));
		m_dht->add_router_node(I->second);
	}
	else
		d->m_router_nodes.erase(I);
}

void CDHT::Restart()
{
	Q_D(CDHT);

	QPair<QByteArray, TPeerList> State = GetState();

	delete m_dht;

	m_dht = new libtorrent::dht::node_impl(&CDHTPrivate::SendPacket, *m_dht_settings, libtorrent::dht::node_id(State.first), d->m_external_address, &CDHTPrivate::SetAddress, this);

	for(std::map<std::string, udp::endpoint>::iterator I = d->m_router_nodes.begin(); I != d->m_router_nodes.end(); I++)
		m_dht->add_router_node(I->second);

	Bootstrap(State.second);
}

CAddress CDHT::GetAddress()
{
	Q_D(CDHT);
	return Addr2CAddr(d->m_external_address);
}

void add_node_fun(void* userdata, libtorrent::dht::node_entry const& e)
{
	TPeerList& list = *(TPeerList*)userdata;
	list.append(SPeer(Addr2CAddr(e.ep().address()), e.ep().port()));
}
	
QPair<QByteArray, TPeerList> CDHT::GetState()
{
	QPair<QByteArray, TPeerList> State;
	State.first = QByteArray((char*)m_dht->nid().begin(), m_dht->nid().end() - m_dht->nid().begin());

	m_dht->m_table.for_each_node(&add_node_fun, &add_node_fun, &State.second);
	libtorrent::dht::bucket_t cache;
	m_dht->replacement_cache(cache);
	for (libtorrent::dht::bucket_t::iterator i(cache.begin()) , end(cache.end()); i != end; ++i)
		State.second.append(SPeer(Addr2CAddr(i->addr), i->port));

	return State;
}

QVariantMap CDHT::GetStatus()
{
	libtorrent::session_status status;
	m_dht->status(status);

	QVariantMap Status;
	Status["Nodes"] = status.dht_nodes;
	//int dht_node_cache;
	//int dht_torrents;
	Status["GlobalNodes"] = (int)status.dht_global_nodes;
	//size_type dht_global_nodes;
	//std::vector<dht_lookup> active_requests;
	Status["Lookups"] = (int)status.active_requests.size();
	//std::vector<dht_routing_bucket> dht_routing_table;
	//int dht_total_allocations;
	return Status;
}

void CDHT::timerEvent(QTimerEvent* pEvent)
{
	if(pEvent->timerId() != m_uTimerID)
		return ;

	libtorrent::update_time_now();
	quint64 now = libtorrent::time_now().time;

	if(now > m_next_tick)
	{
		m_next_tick = now + libtorrent::seconds(5).diff;
		m_dht->tick();
	}

	if(now > m_next_timeout)
	{
		libtorrent::time_duration d = m_dht->connection_timeout();
		m_next_timeout = now + d.diff;
	}
	
	if (now - m_last_new_key > libtorrent::minutes(5).diff)
	{
		m_last_new_key = now;
		m_dht->new_write_key();
	}
}

void CDHT::ProcessDHTPacket(QByteArray Packet, CAddress Address, quint16 uDHTPort)
{
	libtorrent::lazy_entry e;
	int pos;
	libtorrent::error_code ec;
	int ret = libtorrent::lazy_bdecode(Packet.begin(), Packet.end(), e, ec, &pos, 10, 500);
	if (ret != 0)
		return;

	libtorrent::dht::msg m(e, udp::endpoint(CAddr2Addr(Address), uDHTPort));
	if (e.type() != libtorrent::lazy_entry::dict_t)
		return;

	m_dht->incoming(m);
}

bool CDHTPrivate::SendPacket(void* userdata, libtorrent::entry& e, udp::endpoint const& ep, int flags)
{
	CDHT* pDHT = (CDHT*)userdata;

	static char const version_str[] = {'L', 'T'
		, LIBTORRENT_VERSION_MAJOR, LIBTORRENT_VERSION_MINOR};
	e["v"] = std::string(version_str, version_str + 4);

	std::vector<char> m_send_buf;
	libtorrent::bencode(std::back_inserter(m_send_buf), e);

	emit pDHT->SendDHTPacket(QByteArray(&m_send_buf[0], m_send_buf.size()), Addr2CAddr(ep.address()), ep.port());
	return true;
}

void CDHT::Announce(const QByteArray& InfoHash, quint16 port, bool seed)
{
	TORRENT_ASSERT(InfoHash.size() == 20);
	libtorrent::sha1_hash ih(InfoHash.data());
	m_dht->announce(ih, port, seed, &CDHTPrivate::PeersFound, &CDHTPrivate::EndLookup, this);
}

void CDHTPrivate::PeersFound(void* userdata, std::vector<tcp::endpoint> const& e, libtorrent::sha1_hash const& ih)
{
	CDHT* pDHT = (CDHT*)userdata;

	QByteArray InfoHash((const char*)ih.begin(), ih.end() - ih.begin());
	TPeerList PeerList;
	for(std::vector<tcp::endpoint>::const_iterator I = e.begin(); I != e.end(); I++)
		PeerList.append(SPeer(Addr2CAddr(I->address()), I->port()));
	emit pDHT->PeersFound(InfoHash, PeerList);
}

void CDHTPrivate::EndLookup(void* userdata, libtorrent::sha1_hash const& ih)
{
	CDHT* pDHT = (CDHT*)userdata;

	QByteArray InfoHash((const char*)ih.begin(), ih.end() - ih.begin());
	emit pDHT->EndLookup(InfoHash);
}

void CDHTPrivate::SetAddress(libtorrent::address const& ip, libtorrent::address const& source)
{
	if (is_any(ip)) return;
	if (is_local(ip)) return;
	if (is_loopback(ip)) return;

#if defined TORRENT_VERBOSE_LOGGING
	(*m_logger) << time_now_string() << ": set_external_address(" << print_address(ip)
		<< ", " << source_type << ", " << print_address(source) << ")\n";
#endif
	// this is the key to use for the bloom filters
	// it represents the identity of the voter
	libtorrent::sha1_hash k;
	libtorrent::hash_address(source, k);

	// do we already have an entry for this external IP?
#ifdef _USE_BIND
	std::vector<external_ip_t>::iterator i = std::find_if(m_external_addresses.begin()
		, m_external_addresses.end(), boost::bind(&external_ip_t::addr, _1) == ip);
#else
	std::vector<external_ip_t>::iterator i;
	{
		std::vector<external_ip_t>::iterator first = m_external_addresses.begin();
		std::vector<external_ip_t>::iterator last = m_external_addresses.end();
		for (; first != last; ++first) {
			if ((*first).addr == ip) {
				last = first;
				break;
			}
		}
		i = last;
	}
#endif

	if (i == m_external_addresses.end())
	{
		// each IP only gets to add a new IP once
		if (m_external_address_voters.find(k)) return;
		
		if (m_external_addresses.size() > 20)
		{
			if (libtorrent::random() < UINT_MAX / 2)
			{
#if defined TORRENT_VERBOSE_LOGGING
				(*m_logger) << time_now_string() << ": More than 20 slots, dopped\n";
#endif
				return;
			}
			// use stable sort here to maintain the fifo-order
			// of the entries with the same number of votes
			// this will sort in ascending order, i.e. the lowest
			// votes first. Also, the oldest are first, so this
			// is a sort of weighted LRU.
			std::stable_sort(m_external_addresses.begin(), m_external_addresses.end());
			// erase the first element, since this is the
			// oldest entry and the one with lowst number
			// of votes. This makes sense because the oldest
			// entry has had the longest time to receive more
			// votes to be bumped up
#if defined TORRENT_VERBOSE_LOGGING
			(*m_logger) << "  More than 20 slots, dopping "
				<< print_address(m_external_addresses.front().addr)
				<< " (" << m_external_addresses.front().num_votes << ")\n";
#endif
			m_external_addresses.erase(m_external_addresses.begin());
		}
		m_external_addresses.push_back(external_ip_t());
		i = m_external_addresses.end() - 1;
		i->addr = ip;
	}
	// add one more vote to this external IP
	if (!i->add_vote(k)) 
		return;
		
	i = std::max_element(m_external_addresses.begin(), m_external_addresses.end());
	TORRENT_ASSERT(i != m_external_addresses.end());

#if defined TORRENT_VERBOSE_LOGGING
	for (std::vector<external_ip_t>::iterator j = m_external_addresses.begin()
		, end(m_external_addresses.end()); j != end; ++j)
	{
		(*m_logger) << ((j == i)?"-->":"   ")
			<< print_address(j->addr) << " votes: "
			<< j->num_votes << "\n";
	}
#endif
	if (i->addr == m_external_address) 
		return;

#if defined TORRENT_VERBOSE_LOGGING
	(*m_logger) << "  external IP updated\n";
#endif
	m_external_address = i->addr;
	m_external_address_voters.clear();

	// since we have a new external IP now, we need to
	// restart the DHT with a new node ID
	Q_Q(CDHT);
	emit q->AddressChanged();
}

libtorrent::address CAddr2Addr(const CAddress& Address)
{
	libtorrent::address addr;
	if(Address.Type() == CAddress::IPv4)
		addr = libtorrent::address_v4(Address.ToIPv4());
	else if(Address.Type() == CAddress::IPv6)
	{
		libtorrent::address::bytes_type bytes_v6;
		memcpy(&bytes_v6[0], Address.Data(), 16);
		addr = libtorrent::address_v6(bytes_v6);
	}
	return addr;
}

CAddress Addr2CAddr(const libtorrent::address& addr)
{
	CAddress Address;
	if(addr.is_v4())
		Address = CAddress(addr.to_ulong());
	else if(addr.is_v6())
		Address = CAddress((const unsigned char*)&addr.to_bytes()[0]);
	return Address;
}
