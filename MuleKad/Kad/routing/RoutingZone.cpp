//
// This file is part of aMule Project
//
// Copyright (c) 2004-2011 Angel Vidal ( kry@amule.org )
// Copyright (c) 2003-2011 aMule Team ( admin@amule.org / http://www.amule.org )
// Copyright (c) 2003-2011 Barry Dunne ( http://www.emule-project.net )
// Copyright (C)2007-2011 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / http://www.emule-project.net )

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301, USA

// This work is based on the java implementation of the Kademlia protocol.
// Kademlia: Peer-to-peer routing based on the XOR metric
// Copyright (c) 2002-2011  Petar Maymounkov ( petar@post.harvard.edu )
// http://kademlia.scs.cs.nyu.edu

// Note To Mods //
/*
Please do not change anything here and release it..
There is going to be a new forum created just for the Kademlia side of the client..
If you feel there is an error or a way to improve something, please
post it in the forum first and let us look at it.. If it is a real improvement,
it will be added to the offical client.. Changing something without knowing
what all it does can cause great harm to the network if released in mass form..
Any mod that changes anything within the Kademlia side will not be allowed to advertise
there client on the eMule forum..
*/

/**
 * The *Zone* is just a node in a binary tree of *Zone*s.
 * Each zone is either an internal node or a leaf node.
 * Internal nodes have "bin == null" and "subZones[i] != null",
 * leaf nodes have "subZones[i] == null" and "bin != null".
 * 
 * All key unique id's are relative to the center (self), which
 * is considered to be 000..000
 */
#include "GlobalHeader.h"
#include "RoutingZone.h"

#include "../Protocols.h"
#include "Contact.h"
#include "RoutingBin.h"
#include "../kademlia/Defines.h"
#include "../kademlia/SearchManager.h"
#include "../kademlia/UDPFirewallTester.h"
#include "../net/KademliaUDPListener.h"
#include "../utils/KadUDPKey.h"
#include "../KadHandler.h"
#include "../UDPSocket.h"

#include <cmath>

////////////////////////////////////////
using namespace Kademlia;
////////////////////////////////////////

// This is just a safety precaution
#define CONTACT_FILE_LIMIT	500

CUInt128 CRoutingZone::me((uint32_t)0);

CRoutingZone::CRoutingZone()
{
	// Can only create routing zone after prefs
	// Set our KadID for creating the contact tree
	me = CKademlia::GetPrefs()->GetKadID();
	Init(NULL, 0, CUInt128((uint32_t)0));
}

void CRoutingZone::Init(CRoutingZone *super_zone, int level, const CUInt128& zone_index)
{
	// Init all Zone vars
	// Set this zone's parent
	m_superZone = super_zone;
	// Set this zone's level
	m_level = level;
	// Set this zone's CUInt128 index
	m_zoneIndex = zone_index;
	// Mark this zone as having no leafs.
	m_subZones[0] = NULL;
	m_subZones[1] = NULL;
	// Create a new contact bin as this is a leaf.
	m_bin = new CRoutingBin();

	// Set timer so that zones closer to the root are processed earlier.
	m_nextSmallTimer = time(NULL) + m_zoneIndex.Get32BitChunk(3);

	// Start this zone.
	StartTimer();
}

CRoutingZone::~CRoutingZone()
{
	// If this zone is a leaf, delete our contact bin.
	if (IsLeaf()) {
		delete m_bin;
	} else {
		// If this zone is a branch, delete its leafs.
		delete m_subZones[0];
		delete m_subZones[1];
	}
}

bool CRoutingZone::CanSplit() const throw()
{
	// Max levels allowed.
	if (m_level >= 127) {
		return false;
	}

	// Check if this zone is allowed to split.
	return ((m_zoneIndex < KK || m_level < KBASE) && m_bin->GetSize() == K);
}

// Returns true if a contact was added or updated, false if the routing table was not touched.
bool CRoutingZone::Add(const CUInt128& id, uint32_t ip, uint16_t port, uint16_t tport, uint8_t version, const CKadUDPKey& key, bool& ipVerified, bool update, bool fromHello)
{
	if (IsGoodIPPort(ip, port) && (IsLanIP(ip) || !CKadHandler::Instance()->UseLanMode())) {
		if (!CKadHandler::Instance()->IsFiltered(ip) && !(port == 53 && version <= 5) /*No DNS Port without encryption*/) {
			return AddUnfiltered(id, ip, port, tport, version, key, ipVerified, update, fromHello);
		}
	}
	return false;
}

// Returns true if a contact was added or updated, false if the routing table was not touched.
bool CRoutingZone::AddUnfiltered(const CUInt128& id, uint32_t ip, uint16_t port, uint16_t tport, uint8_t version, const CKadUDPKey& key, bool& ipVerified, bool update, bool fromHello)
{
	if (id != me) {
		CContact *contact = new CContact(id, ip, port, tport, version, key, ipVerified);
		if (fromHello) {
			contact->SetReceivedHelloPacket();
		}
		if (Add(contact, update, ipVerified)) {
			ASSERT(!update);
			return true;
		} else {
			delete contact;
			return update;
		}
	}
	return false;
}

bool CRoutingZone::Add(CContact *contact, bool& update, bool& outIpVerified)
{
	// If we're not a leaf, call add on the correct branch.
	if (!IsLeaf()) {
		return m_subZones[contact->GetDistance().GetBitNumber(m_level)]->Add(contact, update, outIpVerified);
	} else {
		// Do we already have a contact with this KadID?
		CContact *contactUpdate = m_bin->GetContact(contact->GetClientID());
		if (contactUpdate) {
			if (update) {
				if (contactUpdate->GetUDPKey().GetKeyValue(ntohl(CKadHandler::Instance()->GetPublicIP())) != 0 && contactUpdate->GetUDPKey().GetKeyValue(ntohl(CKadHandler::Instance()->GetPublicIP())) != contact->GetUDPKey().GetKeyValue(ntohl(CKadHandler::Instance()->GetPublicIP()))) {
					// if our existing contact has a UDPSender-Key (which should be the case for all > = 0.49a clients)
					// except if our IP has changed recently, we demand that the key is the same as the key we received
					// from the packet which wants to update this contact in order to make sure this is not a try to
					// hijack this entry
					LogKadLine(LOG_DEBUG /*logKadRouting*/, L"Sender (%s) tried to update contact entry but failed to provide the proper sender key (Sent Empty: %s for the entry (%s) - denying update"
					, IPToStr(contact->GetIPAddress()).c_str(), (contact->GetUDPKey().GetKeyValue(ntohl(CKadHandler::Instance()->GetPublicIP())) == 0 ? L"Yes" : L"No"), IPToStr(contactUpdate->GetIPAddress()).c_str());
					update = false;
				} else if (contactUpdate->GetVersion() >= 1 && contactUpdate->GetVersion() < 6 && contactUpdate->GetReceivedHelloPacket()) {
					// legacy kad2 contacts are allowed only to update their RefreshTimer to avoid having them hijacked/corrupted by an attacker
					// (kad1 contacts do not have this restriction as they might turn out as kad2 later on)
					// only exception is if we didn't received a HELLO packet from this client yet
					if (contactUpdate->GetIPAddress() == contact->GetIPAddress() && contactUpdate->GetTCPPort() == contact->GetTCPPort() && contactUpdate->GetVersion() == contact->GetVersion() && contactUpdate->GetUDPPort() == contact->GetUDPPort()) {
						ASSERT(!contact->IsIPVerified());	// legacy kad2 nodes should be unable to verify their IP on a HELLO
						outIpVerified = contactUpdate->IsIPVerified();
						m_bin->SetAlive(contactUpdate);
						LogKadLine(LOG_DEBUG /*logKadRouting*/, L"Updated kad contact refreshtimer only for legacy kad2 contact (%s, %u)", IPToStr(contactUpdate->GetIPAddress()).c_str(), contactUpdate->GetVersion());
					} else {
						LogKadLine(LOG_DEBUG /*logKadRouting*/, L"Rejected value update for legacy kad2 contact (%s -> %s, %u -> %u)"
							, IPToStr(contactUpdate->GetIPAddress()).c_str(), IPToStr(contact->GetIPAddress()).c_str(), contactUpdate->GetVersion(), contact->GetVersion());
						update = false;
					}
				} else {
#ifdef _DEBUG
					// just for outlining, get removed anyway
					//debug logging stuff - remove later
					if (contact->GetUDPKey().GetKeyValue(ntohl(CKadHandler::Instance()->GetPublicIP())) == 0) {
						if (contact->GetVersion() >= 6 && contact->GetType() < 2) {
							LogKadLine(LOG_DEBUG /*logKadRouting*/, L"Updating > 0.49a + type < 2 contact without valid key stored %s", IPToStr(contact->GetIPAddress()).c_str());
						}
					} else {
						LogKadLine(LOG_DEBUG /*logKadRouting*/, L"Updating contact, passed key check %s", IPToStr(contact->GetIPAddress()).c_str());
					}

					if (contactUpdate->GetVersion() >= 1 && contactUpdate->GetVersion() < 6) {
						ASSERT(!contactUpdate->GetReceivedHelloPacket());
						LogKadLine(LOG_DEBUG /*logKadRouting*/, L"Accepted update for legacy kad2 contact, because of first HELLO (%s -> %s, %u -> %u)"
							, IPToStr(contactUpdate->GetIPAddress()).c_str(), IPToStr(contact->GetIPAddress()).c_str(), contactUpdate->GetVersion(), contact->GetVersion());
					}
#endif
					// All other nodes (Kad1, Kad2 > 0.49a with UDPKey checked or not set, first hello updates) are allowed to do full updates
					// do not let Kad1 responses overwrite Kad2 ones
					if (m_bin->ChangeContactIPAddress(contactUpdate, contact->GetIPAddress()) && contact->GetVersion() >= contactUpdate->GetVersion()) {
						contactUpdate->SetUDPPort(contact->GetUDPPort());
						contactUpdate->SetTCPPort(contact->GetTCPPort());
						contactUpdate->SetVersion(contact->GetVersion());
						contactUpdate->SetUDPKey(contact->GetUDPKey());
						// don't unset the verified flag (will clear itself on ipchanges)
						if (!contactUpdate->IsIPVerified()) {
							contactUpdate->SetIPVerified(contact->IsIPVerified());
						}
						outIpVerified = contactUpdate->IsIPVerified();
						m_bin->SetAlive(contactUpdate);
						if (contact->GetReceivedHelloPacket()) {
							contactUpdate->SetReceivedHelloPacket();
						}
					} else {
						update = false;
					}
				}
			}
			return false;
		} else if (m_bin->GetRemaining()) {
			update = false;
			// This bin is not full, so add the new contact
			return m_bin->AddContact(contact);
		} else if (CanSplit()) {
			// This bin was full and split, call add on the correct branch.
			Split();
			return m_subZones[contact->GetDistance().GetBitNumber(m_level)]->Add(contact, update, outIpVerified);
		} else {
			update = false;
			return false;
		}
	}
}

CContact *CRoutingZone::GetContact(const CUInt128& id) const throw()
{
	if (IsLeaf()) {
		return m_bin->GetContact(id);
	} else {
		CUInt128 distance = CKademlia::GetPrefs()->GetKadID();
		distance ^= id;
		return m_subZones[distance.GetBitNumber(m_level)]->GetContact(id);
	}
}

CContact *CRoutingZone::GetContact(uint32_t ip, uint16_t port, bool tcpPort) const throw()
{
	if (IsLeaf()) {
		return m_bin->GetContact(ip, port, tcpPort);
	} else {
		CContact *contact = m_subZones[0]->GetContact(ip, port, tcpPort);
		return (contact != NULL) ? contact : m_subZones[1]->GetContact(ip, port, tcpPort);
	}
}

CContact *CRoutingZone::GetRandomContact(uint32_t maxType, uint32_t minKadVersion) const
{
	if (IsLeaf()) {
		return m_bin->GetRandomContact(maxType, minKadVersion);
	} else {
		unsigned zone = (uint16_t)GetRand64() & 1 /* GetRandomUint16() % 2 */;
		CContact *contact = m_subZones[zone]->GetRandomContact(maxType, minKadVersion);
		return (contact != NULL) ? contact : m_subZones[1 - zone]->GetRandomContact(maxType, minKadVersion);
	}
}

void CRoutingZone::GetClosestTo(uint32_t maxType, const CUInt128& target, const CUInt128& distance, uint32_t maxRequired, ContactMap *result, bool emptyFirst, bool inUse) const
{
	// If leaf zone, do it here
	if (IsLeaf()) {
		m_bin->GetClosestTo(maxType, target, maxRequired, result, emptyFirst, inUse);
		return;
	}

	// otherwise, recurse in the closer-to-the-target subzone first
	int closer = distance.GetBitNumber(m_level);
	m_subZones[closer]->GetClosestTo(maxType, target, distance, maxRequired, result, emptyFirst, inUse);

	// if still not enough tokens found, recurse in the other subzone too
	if (result->size() < maxRequired) {
		m_subZones[1-closer]->GetClosestTo(maxType, target, distance, maxRequired, result, false, inUse);
	}
}

void CRoutingZone::GetAllEntries(ContactList *result, bool emptyFirst) const
{
	if (IsLeaf()) {
		m_bin->GetEntries(result, emptyFirst);
	} else {
		m_subZones[0]->GetAllEntries(result, emptyFirst);
		m_subZones[1]->GetAllEntries(result, false);			
	}
}

void CRoutingZone::TopDepth(int depth, ContactList *result, bool emptyFirst) const
{
	if (IsLeaf()) {
		m_bin->GetEntries(result, emptyFirst);
	} else if (depth <= 0) {
		RandomBin(result, emptyFirst);
	} else {
		m_subZones[0]->TopDepth(depth-1, result, emptyFirst);
		m_subZones[1]->TopDepth(depth-1, result, false);
	}
}

void CRoutingZone::RandomBin(ContactList *result, bool emptyFirst) const
{
	if (IsLeaf()) {
		m_bin->GetEntries(result, emptyFirst);
	} else {
		m_subZones[rand()&1]->RandomBin(result, emptyFirst);
	}
}

uint32_t CRoutingZone::GetMaxDepth() const throw()
{
	if (IsLeaf()) {
		return 0;
	}
	return 1 + Max(m_subZones[0]->GetMaxDepth(), m_subZones[1]->GetMaxDepth());
}

void CRoutingZone::Split()
{
	StopTimer();
		
	m_subZones[0] = GenSubZone(0);
	m_subZones[1] = GenSubZone(1);

	ContactList entries;
	m_bin->GetEntries(&entries);
	m_bin->m_dontDeleteContacts = true;
	delete m_bin;
	m_bin = NULL;

	for (ContactList::const_iterator it = entries.begin(); it != entries.end(); ++it) {
		if (!m_subZones[(*it)->GetDistance().GetBitNumber(m_level)]->m_bin->AddContact(*it)) {
			delete *it;
		}
	}
}

uint32_t CRoutingZone::Consolidate()
{
	uint32_t mergeCount = 0;

	if (IsLeaf()) {
		return mergeCount;
	}

	ASSERT(m_bin == NULL);

	if (!m_subZones[0]->IsLeaf()) {
		mergeCount += m_subZones[0]->Consolidate();
	}
	if (!m_subZones[1]->IsLeaf()) {
		mergeCount += m_subZones[1]->Consolidate();
	}

	if (m_subZones[0]->IsLeaf() && m_subZones[1]->IsLeaf() && GetNumContacts() < K / 2) {
		m_bin = new CRoutingBin();

		m_subZones[0]->StopTimer();
		m_subZones[1]->StopTimer();

		ContactList list0;
		ContactList list1;
		m_subZones[0]->m_bin->GetEntries(&list0);
		m_subZones[1]->m_bin->GetEntries(&list1);

		m_subZones[0]->m_bin->m_dontDeleteContacts = true;
		m_subZones[1]->m_bin->m_dontDeleteContacts = true;

		delete m_subZones[0];
		delete m_subZones[1];

		m_subZones[0] = NULL;
		m_subZones[1] = NULL;

		for (ContactList::const_iterator it = list0.begin(); it != list0.end(); ++it) {
			m_bin->AddContact(*it);
		}
		for (ContactList::const_iterator it = list1.begin(); it != list1.end(); ++it) {
			m_bin->AddContact(*it);
		}

		StartTimer();

		mergeCount++;
	}
	return mergeCount;
}

CRoutingZone *CRoutingZone::GenSubZone(unsigned side)
{
	ASSERT(side <= 1);

	CUInt128 newIndex(m_zoneIndex);
	newIndex <<= 1;
	newIndex += side;
	return new CRoutingZone(this, m_level + 1, newIndex);
}

void CRoutingZone::StartTimer()
{
	// Start filling the tree, closest bins first.
	m_nextBigTimer = time(NULL) + SEC(10);
	CKademlia::AddEvent(this);
}

void CRoutingZone::StopTimer()
{
	CKademlia::RemoveEvent(this);
}

bool CRoutingZone::OnBigTimer() const
{
	if (IsLeaf() && (m_zoneIndex < KK || m_level < KBASE || m_bin->GetRemaining() >= (K * 0.8))) {
		RandomLookup();
		return true;
	}

	return false;
}

//This is used when we find a leaf and want to know what this sample looks like.
//We fall back two levels and take a sample to try to minimize any areas of the 
//tree that will give very bad results.
uint32_t CRoutingZone::EstimateCount() const
{
	if (!IsLeaf()) {
		return 0;
	}

	if (m_level < KBASE) {
		return (uint32_t)(pow(2.0, (int)m_level) * K);
	}

	CRoutingZone* curZone = m_superZone->m_superZone->m_superZone;

	// Find out how full this part of the tree is.
	float modify = ((float)curZone->GetNumContacts()) / (float)(K * 2);

	// First calculate users assuming the tree is full.
	// Modify count by bin size.
	// Modify count by how full the tree is.
	
	// LowIDModififier
	// Modify count by assuming 20% of the users are firewalled and can't be a contact for < 0.49b nodes
	// Modify count by actual statistics of Firewalled ratio for >= 0.49b if we are not firewalled ourself
	// Modify count by 40% for >= 0.49b if we are firewalled ourself (the actual Firewalled count at this date on kad is 35-55%)
	const float firewalledModifyOld = 1.20f;
	float firewalledModifyNew = 0;
	if (CUDPFirewallTester::IsFirewalledUDP(true)) {
		firewalledModifyNew = 1.40f;	// we are firewalled and can't get the real statistics, assume 40% firewalled >=0.49b nodes
	} else if (CKademlia::GetPrefs()->StatsGetFirewalledRatio(true) > 0) {
		firewalledModifyNew = 1.0 + (CKademlia::GetPrefs()->StatsGetFirewalledRatio(true));	// apply the firewalled ratio to the modify
		ASSERT(firewalledModifyNew > 1.0 && firewalledModifyNew < 1.90);
	}
	float newRatio = CKademlia::GetPrefs()->StatsGetKadV8Ratio();
	float firewalledModifyTotal = 0;
	if (newRatio > 0 && firewalledModifyNew > 0) {	// weight the old and the new modifier based on how many new contacts we have
		firewalledModifyTotal = (newRatio * firewalledModifyNew) + ((1 - newRatio) * firewalledModifyOld); 
	} else {
		firewalledModifyTotal = firewalledModifyOld;
	}
	ASSERT(firewalledModifyTotal > 1.0 && firewalledModifyTotal < 1.90);

	return (uint32_t)(pow(2.0, (int)m_level - 2) * (float)K * modify * firewalledModifyTotal);
}

void CRoutingZone::OnSmallTimer()
{
	if (!IsLeaf()) {
		return;
	}
	
	CContact *c = NULL;
	time_t now = time(NULL);
	ContactList entries;

	// Remove dead entries
	m_bin->GetEntries(&entries);
	for (ContactList::iterator it = entries.begin(); it != entries.end(); ++it) {
		c = *it;
		if (c->GetType() == 4) {
			if ((c->GetExpireTime() > 0) && (c->GetExpireTime() <= now)) {
				if (!c->InUse()) {
					m_bin->RemoveContact(c);
					delete c;
				}
				continue;
			}
		}
		if(c->GetExpireTime() == 0) {
			c->SetExpireTime(now);
		}
	}

	c = m_bin->GetOldest();
	if (c != NULL) {
		if (c->GetExpireTime() >= now || c->GetType() == 4) {
			m_bin->PushToBottom(c);
			c = NULL;
		}
	}

	if (c != NULL) {
		c->CheckingType();
		if (c->GetVersion() >= 6) {
			DebugSend(L"Kad2HelloReq", c->GetIPAddress(), c->GetUDPPort());
			CUInt128 clientID = c->GetClientID();
			CKademlia::GetUDPListener()->SendMyDetails(KADEMLIA2_HELLO_REQ, c->GetIPAddress(), c->GetUDPPort(), c->GetVersion(), c->GetUDPKey(), &clientID, false);
			if (c->GetVersion() >= 8) {
				// FIXME:
				// This is a bit of a work around for statistic values. Normally we only count values from incoming HELLO_REQs for
				// the firewalled statistics in order to get numbers from nodes which have us on their routing table,
				// however if we send a HELLO due to the timer, the remote node won't send a HELLO_REQ itself anymore (but
				// a HELLO_RES which we don't count), so count those statistics here. This isn't really accurate, but it should
				// do fair enough. Maybe improve it later for example by putting a flag into the contact and make the answer count
				CKademlia::GetPrefs()->StatsIncUDPFirewalledNodes(false);
				CKademlia::GetPrefs()->StatsIncTCPFirewalledNodes(false);
			}
		} else if (c->GetVersion() >= 2) {
			DebugSend(L"Kad2HelloReq", c->GetIPAddress(), c->GetUDPPort());
			CKademlia::GetUDPListener()->SendMyDetails(KADEMLIA2_HELLO_REQ, c->GetIPAddress(), c->GetUDPPort(), c->GetVersion(), 0, NULL, false);
			ASSERT(c->GetUDPKey() == CKadUDPKey(0));
		} else {
			ASSERT(0);
		}
	}
}

void CRoutingZone::RandomLookup() const
{
	// Look-up a random client in this zone
	CUInt128 prefix(m_zoneIndex);
	prefix <<= 128 - m_level;
	CUInt128 random(prefix, m_level);
	random ^= me;
	CSearchManager::FindNode(random, false);
}

uint32_t CRoutingZone::GetNumContacts() const throw()
{
	if (IsLeaf()) {
		return m_bin->GetSize();
	} else {
		return m_subZones[0]->GetNumContacts() + m_subZones[1]->GetNumContacts();
	}
}

void CRoutingZone::GetNumContacts(uint32_t& nInOutContacts, uint32_t& nInOutFilteredContacts, uint8_t minVersion) const throw()
{
	if (IsLeaf()) {
		m_bin->GetNumContacts(nInOutContacts, nInOutFilteredContacts, minVersion);
	} else {
		m_subZones[0]->GetNumContacts(nInOutContacts, nInOutFilteredContacts, minVersion);
		m_subZones[1]->GetNumContacts(nInOutContacts, nInOutFilteredContacts, minVersion);
	}
}

uint32_t CRoutingZone::GetBootstrapContacts(ContactList *results, uint32_t maxRequired) const
{
	ASSERT(m_superZone == NULL);

	results->clear();

	uint32_t count = 0;
	ContactList top;
	TopDepth(LOG_BASE_EXPONENT, &top);
	if (top.size() > 0) {
		for (ContactList::const_iterator it = top.begin(); it != top.end(); ++it) {
			results->push_back(*it);
			count++;
			if (count == maxRequired) {
				break;
			}
		}
	}

	return count;
}

bool CRoutingZone::VerifyContact(const CUInt128& id, uint32_t ip)
{
	CContact* contact = GetContact(id);
	if (contact == NULL) {
		return false;
	} else if (ip != contact->GetIPAddress()) {
		return false;
	} else {
		if (contact->IsIPVerified()) {
			LogKadLine(LOG_DEBUG /*logKadRouting*/, L"Sender already verified (sender: %s)", IPToStr(ip).c_str());
		} else {
			contact->SetIPVerified(true);
		}
		return true;
	}
}

void CRoutingZone::SetAllContactsVerified()
{
	if (IsLeaf()) {
		m_bin->SetAllContactsVerified();
	} else {
		m_subZones[0]->SetAllContactsVerified();
		m_subZones[1]->SetAllContactsVerified();
	}
}

bool CRoutingZone::IsAcceptableContact(const CContact *toCheck) const
{
	// Check if we know a contact with the same ID or IP but notmatching IP/ID and other limitations, similar checks like when adding a node to the table except allowing duplicates
	// we use this to check KADEMLIA_RES routing answers on searches
	if (toCheck->GetVersion() <= 1) {
		// No Kad1 contacts allowed
		return false;
	}
	CContact *duplicate = GetContact(toCheck->GetClientID());
	if (duplicate != NULL) {
		if ((duplicate->IsIPVerified() && duplicate->GetIPAddress() != toCheck->GetIPAddress()) || duplicate->GetUDPPort() != toCheck->GetUDPPort()) {
			// already existing verified node with different IP
			return false;
		} else {
			// node exists already in our routing table, that's fine
			return true;
		}
	}
	// if the node is not yet known, check if our IP limitations would hit
	return CRoutingBin::CheckGlobalIPLimits(toCheck->GetIPAddress(), toCheck->GetUDPPort());
}

bool CRoutingZone::HasOnlyLANNodes() const throw()
{
	if (IsLeaf()) {
		return m_bin->HasOnlyLANNodes();
	} else {
		return m_subZones[0]->HasOnlyLANNodes() && m_subZones[1]->HasOnlyLANNodes();
	}
}
