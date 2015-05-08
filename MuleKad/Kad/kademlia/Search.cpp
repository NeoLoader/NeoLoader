//
// This file is part of the MuleKad Project.
//
// Copyright (c) 2012 David Xanatos ( XanatosDavid@googlemail.com )
// Copyright (c) 2004-2011 Angel Vidal ( kry@amule.org )
// Copyright (c) 2004-2011 aMule Team ( admin@amule.org / http://www.amule.org )
// Copyright (c) 2003-2011 Barry Dunne (http://www.emule-project.net)
// Copyright (c) 2004-2011 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / http://www.emule-project.net )
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

#include "GlobalHeader.h"
#include "Search.h"

#include "../Protocols.h"
#include "../Constants.h"
#include "../FileTags.h"
#include "Defines.h"
#include "UDPFirewallTester.h"
#include "Indexed.h"
#include "../routing/RoutingZone.h"
#include "../routing/Contact.h"
#include "../net/KademliaUDPListener.h"
#include "../utils/KadClientSearcher.h"
#include "../KadHandler.h"
#include "../../../Framework/Buffer.h"
#include "../../../Framework/Strings.h"
#include "../UDPSocket.h"
#include "../../../Framework/Exception.h"

////////////////////////////////////////
using namespace Kademlia;
////////////////////////////////////////

CSearch::CSearch()
{
	m_created = time(NULL);
	m_type = (uint32_t)-1;
	m_answers = 0;
	m_totalRequestAnswers = 0;
	m_totalPacketSent = 0;
	m_searchID = (uint32_t)-1;
	m_stopping = false;
	m_totalLoad = 0;
	m_totalLoadResponses = 0;
	m_lastResponse = m_created;
	m_searchTermsData = NULL;
	m_searchTermsDataSize = 0;
	m_nodeSpecialSearchRequester = NULL;
	m_closestDistantFound = 0;
	m_requestedMoreNodesContact = NULL;
}

CSearch::~CSearch()
{
	// remember the closest node we found and tried to contact (if any) during this search
	// for statistical caluclations, but only if its a certain type
	switch (m_type) {
		case NODECOMPLETE:
		case FILE:
		case KEYWORD:
		case NOTES:
		case STOREFILE:
		case STOREKEYWORD:
		case STORENOTES:
		case FINDSOURCE: // maybe also exclude
			if (m_closestDistantFound != 0) {
				CKademlia::StatsAddClosestDistance(m_closestDistantFound);
			}
			break;
		default: // NODE, NODESPECIAL, NODEFWCHECKUDP, FINDBUDDY
			break;
	}

	if (m_nodeSpecialSearchRequester != NULL) {
		// inform requester that our search failed
		m_nodeSpecialSearchRequester->KadSearchIPByNodeIDResult(KCSR_NOTFOUND, 0, 0);
	}

	CKadHandler::Instance()->SearchFinished(GetSearchID());

	// Decrease the use count for any contacts that are in our contact list.
	for (ContactMap::iterator it = m_inUse.begin(); it != m_inUse.end(); ++it) {
		it->second->DecUse();
	}

	// Delete any temp contacts...
	for (ContactList::const_iterator it = m_delete.begin(); it != m_delete.end(); ++it) {
		if (!(*it)->InUse()) {
			delete *it;
		}
	}

	// Check if this search was containing an overload node and adjust time of next time we use that node.
	if (CKademlia::IsRunning() && GetNodeLoad() > 20) {
		switch(GetSearchTypes()) {
			case CSearch::STOREKEYWORD:
				Kademlia::CKademlia::GetIndexed()->AddLoad(GetTarget(), ((uint32_t)(DAY2S(7)*((double)GetNodeLoad()/100.0))+(uint32_t)time(NULL)));
				break;
		}
	}

	if (m_searchTermsData) {
		delete [] m_searchTermsData;
	}
}

void CSearch::Go()
{
	// Start with a lot of possible contacts, this is a fallback in case search stalls due to dead contacts
	if (m_possible.empty()) {
		CUInt128 distance(CKademlia::GetPrefs()->GetKadID() ^ m_target);
		CKademlia::GetRoutingZone()->GetClosestTo(3, m_target, distance, 50, &m_possible, true, true);
	}

	if (!m_possible.empty()) {
		//Lets keep our contact list entries in mind to dec the inUse flag.
		for (ContactMap::iterator it = m_possible.begin(); it != m_possible.end(); ++it) {
			m_inUse[it->first] = it->second;
		}

		ASSERT(m_possible.size() == m_inUse.size());

		// Take top ALPHA_QUERY to start search with.
		int count = m_type == NODE ? 1 : min(ALPHA_QUERY, (int)m_possible.size());

		// Send initial packets to start the search.
		ContactMap::iterator it = m_possible.begin();
		for (int i = 0; i < count; i++) {
			CContact *c = it->second;
			// Move to tried
			m_tried[it->first] = c;
			// Send the KadID so other side can check if I think it has the right KadID.
			// Send request
			SendFindValue(c);
			++it;
		}
	}
}

//If we allow about a 15 sec delay before deleting, we won't miss a lot of delayed returning packets.
void CSearch::PrepareToStop() throw()
{
	// Check if already stopping.
	if (m_stopping) {
		return;
	}

	// Set basetime by search type.
	uint32_t baseTime = 0;
	switch (m_type) {
		case NODE:
		case NODECOMPLETE:
		case NODESPECIAL:
		case NODEFWCHECKUDP:
			baseTime = SEARCHNODE_LIFETIME;
			break;
		case FILE:
			baseTime = SEARCHFILE_LIFETIME;
			break;
		case KEYWORD:
			baseTime = SEARCHKEYWORD_LIFETIME;
			break;
		case NOTES:
			baseTime = SEARCHNOTES_LIFETIME;
			break;
		case STOREFILE:
			baseTime = SEARCHSTOREFILE_LIFETIME;
			break;
		case STOREKEYWORD:
			baseTime = SEARCHSTOREKEYWORD_LIFETIME;
			break;
		case STORENOTES:
			baseTime = SEARCHSTORENOTES_LIFETIME;
			break;
		case FINDBUDDY:
			baseTime = SEARCHFINDBUDDY_LIFETIME;
			break;
		case FINDSOURCE:
			baseTime = SEARCHFINDSOURCE_LIFETIME;
			break;
		default:
			baseTime = SEARCH_LIFETIME;
	}

	// Adjust created time so that search will delete within 15 seconds.
	// This gives late results time to be processed.
	m_created = time(NULL) - baseTime + SEC(15);
	m_stopping = true;	
}

void CSearch::JumpStart()
{
	// If we had a response within the last 3 seconds, no need to jumpstart the search.
	if ((time_t)(m_lastResponse + SEC(3)) > time(NULL)) {
		return;
	}

	// If we ran out of contacts, stop search.
	if (m_possible.empty()) {
		PrepareToStop();
		return;
	}

	// Is this a find lookup and are the best two (=KADEMLIA_FIND_VALUE) nodes dead/unreachable?
	// In this case try to discover more close nodes before using our other results
	// The reason for this is that we may not have found the closest node alive due to results being limited to 2 contacts,
	// which could very well have been the duplicates of our dead closest nodes
	bool lookupCloserNodes = false;
	if (m_requestedMoreNodesContact == NULL && GetRequestContactCount() == KADEMLIA_FIND_VALUE && m_tried.size() >= 3 * KADEMLIA_FIND_VALUE) {
		ContactMap::const_iterator it = m_tried.begin();
		lookupCloserNodes = true;
		for (unsigned i = 0; i < KADEMLIA_FIND_VALUE; i++) {
			if (m_responded.count(it->first) > 0) {
				lookupCloserNodes = false;
				break;
			}
			++it;
		}
		if (lookupCloserNodes) {
			while (it != m_tried.end()) {
				if (m_responded.count(it->first) > 0) {
					LogKadLine(LOG_DEBUG /*logKadSearch*/, L"Best %d nodes for lookup (id=%x) were unreachable or dead, reasking closest for more", KADEMLIA_FIND_VALUE, GetSearchID());
					SendFindValue(it->second, true);
					return;
				}
				++it;
			}
		}
	}

	// Search for contacts that can be used to jumpstart a stalled search.
	while (!m_possible.empty()) {
		// Get a contact closest to our target.
		CContact *c = m_possible.begin()->second;
	
		// Have we already tried to contact this node.
		if (m_tried.count(m_possible.begin()->first) > 0) {
			// Did we get a response from this node, if so, try to store or get info.
			if (m_responded.count(m_possible.begin()->first) > 0) {
				StorePacket();
			}
			// Remove from possible list.
			m_possible.erase(m_possible.begin());
		} else {
			// Add to tried list.
			m_tried[m_possible.begin()->first] = c;
			// Send the KadID so other side can check if I think it has the right KadID.
			// Send request
			SendFindValue(c);
			break;
		}
	}
	
}

void CSearch::ProcessResponse(uint32_t fromIP, uint16_t fromPort, ContactList *results)
{
	LogKadLine(LOG_DEBUG /*logKadSearch*/, L"Processing search response from %s", IPToStr(fromIP, fromPort).c_str());

	ContactList::iterator response;
	// Remember the contacts to be deleted when finished
	for (response = results->begin(); response != results->end(); ++response) {
		m_delete.push_back(*response);
	}

	m_lastResponse = time(NULL);

	// Find contact that is responding.
	CUInt128 fromDistance(0u);
	CContact *fromContact = NULL;
	for (ContactMap::const_iterator it = m_tried.begin(); it != m_tried.end(); ++it) {
		CContact *tmpContact = it->second;
		if ((tmpContact->GetIPAddress() == fromIP) && (tmpContact->GetUDPPort() == fromPort)) {
			fromDistance = it->first;
			fromContact = tmpContact;
			break;
		}
	}

	// Make sure the node is not sending more results than we requested, which is not only a protocol violation
	// but most likely a malicious answer
	if (results->size() > GetRequestContactCount() && !(m_requestedMoreNodesContact == fromContact && results->size() <= KADEMLIA_FIND_VALUE_MORE)) {
		LogKadLine(LOG_DEBUG /*logKadSearch*/, L"Node %s sent more contacts than requested on a routing query, ignoring response", IPToStr(fromIP).c_str());
		return;
	}

	if (m_type == NODEFWCHECKUDP) {
		m_answers++;
		return;
	}

	// Not interested in responses for FIND_NODE, will be added to contacts by udp listener
	if (m_type == NODE) {
		LogKadLine(LOG_DEBUG /*logKadSearch*/, L"Node type search result, discarding.");
		// Note that we got an answer.
		m_answers++;
		// We clear the possible list to force the search to stop.
		m_possible.clear();
		return;
	}

	if (fromContact != NULL) {
		bool providedCloserContacts = false;
		std::map<uint32_t, unsigned> receivedIPs;
		std::map<uint32_t, unsigned> receivedSubnets;
		// A node is not allowed to answer with contacts to itself
		receivedIPs[fromIP] = 1;
		receivedSubnets[fromIP & 0xFFFFFF00] = 1;
		// Loop through their responses
		for (ContactList::iterator it = results->begin(); it != results->end(); ++it) {
			// Get next result
			CContact *c = *it;
			// calc distance this result is to the target
			CUInt128 distance(c->GetClientID() ^ m_target);

			if (distance < fromDistance) {
				providedCloserContacts = true;
			}

			// Ignore this contact if already known or tried it.
			if (m_possible.count(distance) > 0) {
				LogKadLine(LOG_DEBUG /*logKadSearch*/, L"Search result from already known client: ignore");
				continue;
			}
			if (m_tried.count(distance) > 0) {
				LogKadLine(LOG_DEBUG /*logKadSearch*/, L"Search result from already tried client: ignore");
				continue;
			}

			// We only accept unique IPs in the answer, having multiple IDs pointing to one IP in the routing tables
			// is no longer allowed since eMule0.49a, aMule-2.2.1 anyway
			if (receivedIPs.count(c->GetIPAddress()) > 0) {
				LogKadLine(LOG_DEBUG /*logKadSearch*/, L"Multiple KadIDs pointing to same IP (%s) in Kad2Res answer - ignored, sent by %s", IPToStr(c->GetIPAddress()).c_str(), IPToStr(fromContact->GetIPAddress()).c_str());
				continue;
			} else {
				receivedIPs[c->GetIPAddress()] = 1;
			}
				// and no more than 2 IPs from the same /24 subnet
			if (receivedSubnets.count(c->GetIPAddress() & 0xFFFFFF00) > 0 && !IsLanIP(c->GetIPAddress())) {
				ASSERT(receivedSubnets.find(c->GetIPAddress() & 0xFFFFFF00) != receivedSubnets.end());
				int subnetCount = receivedSubnets.find(c->GetIPAddress() & 0xFFFFFF00)->second;
				if (subnetCount >= 2) {
					LogKadLine(LOG_DEBUG /*logKadSearch*/, L"More than 2 KadIDs pointing to same subnet (%s/24) in Kad2Res answer - ignored, sent by %s", IPToStr(c->GetIPAddress() & 0xFFFFFF00).c_str(), IPToStr(fromContact->GetIPAddress()).c_str());
					continue;
				} else {
					receivedSubnets[c->GetIPAddress() & 0xFFFFFF00] = subnetCount + 1;
				}
			} else {
				receivedSubnets[c->GetIPAddress() & 0xFFFFFF00] = 1;
			}

			// Add to possible
			m_possible[distance] = c;

			// Verify if the result is closer to the target than the one we just checked.
			if (distance < fromDistance) {
				// The top ALPHA_QUERY of results are used to determine if we send a request.
				bool top = false;
				if (m_best.size() < ALPHA_QUERY) {
					top = true;
					m_best[distance] = c;
				} else {
					ContactMap::iterator worst = m_best.end();
					--worst;
					if (distance < worst->first) {
						// Prevent having more than ALPHA_QUERY within the Best list.
						m_best.erase(worst);
						m_best[distance] = c;
						top = true;
					}
				}

				if (top) {
					// We determined this contact is a candidate for a request.
					// Add to tried
					m_tried[distance] = c;
					// Send the KadID so other side can check if I think it has the right KadID.
					// Send request
					SendFindValue(c);
				}
			}
		}

		// Add to list of people who responded.
		m_responded[fromDistance] = providedCloserContacts;

		// Complete node search, just increment the counter.
		if (m_type == NODECOMPLETE || m_type == NODESPECIAL) {
			LogKadLine(LOG_DEBUG /*logKadSearch*/, L"Search result type: Node %s", (m_type == NODECOMPLETE ? L"Complete" : L"Special"));
			m_answers++;
		}
	}
}

void CSearch::StorePacket()
{
	ASSERT(!m_possible.empty());

	// This method is currently only called by jumpstart so only use best possible.
	ContactMap::const_iterator possible = m_possible.begin();
	CUInt128 fromDistance(possible->first);
	CContact *from = possible->second;

	if (fromDistance < m_closestDistantFound || m_closestDistantFound == 0) {
		m_closestDistantFound = fromDistance;
	}

	// Make sure this is a valid node to store.
	if (fromDistance.Get32BitChunk(0) > SEARCHTOLERANCE && !IsLanIP(from->GetIPAddress())) {
		return;
	}

	// What kind of search are we doing?
	switch (m_type) {
		case FILE: {
			LogKadLine(LOG_DEBUG /*logKadSearch*/, L"Search request type: File");
			CBuffer searchTerms;
			m_target.Write(&searchTerms);
			if (from->GetVersion() >= 3) {
				// Find file we are storing info about.
				if (uint64_t uSize = CKadHandler::Instance()->GetSearchedSize(m_searchID)) {
					// Start position range (0x0 to 0x7FFF)
					searchTerms.WriteValue<uint16_t>(0);
					searchTerms.WriteValue<uint64_t>(uSize);
					DebugSend(L"Kad2SearchSourceReq", from->GetIPAddress(), from->GetUDPPort());
					if (from->GetVersion() >= 6) {
						CUInt128 clientID = from->GetClientID();
						CKademlia::GetUDPListener()->SendPacket(searchTerms, KADEMLIA2_SEARCH_SOURCE_REQ, from->GetIPAddress(), from->GetUDPPort(), from->GetUDPKey(), &clientID);
					} else {
						CKademlia::GetUDPListener()->SendPacket(searchTerms, KADEMLIA2_SEARCH_SOURCE_REQ, from->GetIPAddress(), from->GetUDPPort(), 0, NULL);
						ASSERT(from->GetUDPKey() == CKadUDPKey(0));
					}
				} else {
					LogKadLine(LOG_DEBUG /*logKadSearch*/, L"Searched file is missing or not synchronised, search abbroted");
					PrepareToStop();
					break;
				}
			} else {
				searchTerms.WriteValue<uint8_t>(1);
				DebugSend(L"KadSearchReq(File)", from->GetIPAddress(), from->GetUDPPort());
				CKademlia::GetUDPListener()->SendPacket(searchTerms, KADEMLIA_SEARCH_REQ, from->GetIPAddress(), from->GetUDPPort(), 0, NULL);
			}
			m_totalRequestAnswers++;
			break;
		}
		case KEYWORD: {
			LogKadLine(LOG_DEBUG /*logKadSearch*/, L"Search request type: Keyword");
			CBuffer searchTerms;
			m_target.Write(&searchTerms);
			if (from->GetVersion() >= 3) {
				if (m_searchTermsDataSize == 0) {
					// Start position range (0x0 to 0x7FFF)
					searchTerms.WriteValue<uint16_t>(0);
				} else {
					// Start position range (0x8000 to 0xFFFF)
					searchTerms.WriteValue<uint16_t>(0x8000);
					searchTerms.WriteData(m_searchTermsData, m_searchTermsDataSize);
				}
				DebugSend(L"Kad2SearchKeyReq", from->GetIPAddress(), from->GetUDPPort());
			} else {
				if (m_searchTermsDataSize == 0) {
					searchTerms.WriteValue<uint8_t>(0);
					// We send this extra byte to flag we handle large files.
					searchTerms.WriteValue<uint8_t>(0);
				} else {
					// Set to 2 to flag we handle large files.
					searchTerms.WriteValue<uint8_t>(2);
					searchTerms.WriteData(m_searchTermsData, m_searchTermsDataSize);
				}
				DebugSend(L"KadSearchReq(Keyword)", from->GetIPAddress(), from->GetUDPPort());
			}
			if (from->GetVersion() >= 6) {
				CUInt128 clientID = from->GetClientID();
				CKademlia::GetUDPListener()->SendPacket(searchTerms, KADEMLIA2_SEARCH_KEY_REQ, from->GetIPAddress(), from->GetUDPPort(), from->GetUDPKey(), &clientID);
			} else if (from->GetVersion() >= 3) {
				CKademlia::GetUDPListener()->SendPacket(searchTerms, KADEMLIA2_SEARCH_KEY_REQ, from->GetIPAddress(), from->GetUDPPort(), 0, NULL);
				ASSERT(from->GetUDPKey() == CKadUDPKey(0));
			} else {
				CKademlia::GetUDPListener()->SendPacket(searchTerms, KADEMLIA_SEARCH_REQ, from->GetIPAddress(), from->GetUDPPort(), 0, NULL);
			}
			m_totalRequestAnswers++;
			break;
		}
		case NOTES: {
			LogKadLine(LOG_DEBUG /*logKadSearch*/, L"Search request type: Notes");
			// Write complete packet.
			CBuffer searchTerms;
			m_target.Write(&searchTerms);
			if (from->GetVersion() >= 3) {
				// Find file we are storing info about.
				if (uint64_t uSize = CKadHandler::Instance()->GetSearchedSize(m_searchID)) {
					// Start position range (0x0 to 0x7FFF)
					searchTerms.WriteValue<uint64_t>(uSize);
					DebugSend(L"Kad2SearchNotesReq", from->GetIPAddress(), from->GetUDPPort());
					if (from->GetVersion() >= 6) {
						CUInt128 clientID = from->GetClientID();
						CKademlia::GetUDPListener()->SendPacket(searchTerms, KADEMLIA2_SEARCH_NOTES_REQ, from->GetIPAddress(), from->GetUDPPort(), from->GetUDPKey(), &clientID);
					} else {
						CKademlia::GetUDPListener()->SendPacket(searchTerms, KADEMLIA2_SEARCH_NOTES_REQ, from->GetIPAddress(), from->GetUDPPort(), 0, NULL);
						ASSERT(from->GetUDPKey() == CKadUDPKey(0));
					}
				} else {
					LogKadLine(LOG_DEBUG /*logKadSearch*/, L"Searched file is missing or not synchronised, search abbroted");
					PrepareToStop();
					break;
				}
			} else {
				CKademlia::GetPrefs()->GetKadID().Write(&searchTerms);
				DebugSend(L"KadSearchNotesReq", from->GetIPAddress(), from->GetUDPPort());
				CKademlia::GetUDPListener()->SendPacket(searchTerms, KADEMLIA_SEARCH_NOTES_REQ, from->GetIPAddress(), from->GetUDPPort(), 0, NULL);
			}
			m_totalRequestAnswers++;
			break;
		}
		case STOREFILE: {
			LogKadLine(LOG_DEBUG /*logKadSearch*/, L"Search request type: StoreFile");
			// Try to store ourselves as a source to a Node.
			// As a safeguard, check to see if we already stored to the max nodes.
			if (m_answers > SEARCHSTOREFILE_TOTAL) {
				PrepareToStop();
				break;
			}

			// Find the file we are trying to store as a source to.
			SFileInfo* file = CKadHandler::Instance()->GetFile(m_target);
			if (file) {
				// Get our clientID for the packet.
				CUInt128 id(CKadHandler::Instance()->GetEd2kHash());
				TagPtrList taglist;

				//We can use type for different types of sources. 
				//1 HighID sources..
				//2 cannot be used as older clients will not work.
				//3 Firewalled Kad Source.
				//4 >4GB file HighID Source.
				//5 >4GB file Firewalled Kad source.
				//6 Firewalled source with Direct Callback (supports >4GB)

				bool directCallback = false;
				if (CKadHandler::Instance()->IsFirewalled()) {
					directCallback = (Kademlia::CKademlia::IsRunning() && !Kademlia::CUDPFirewallTester::IsFirewalledUDP(true) && Kademlia::CUDPFirewallTester::IsVerified());
					if (directCallback) {
						// firewalled, but direct udp callback is possible so no need for buddies
						// We are not firewalled..
						taglist.push_back(new CTagVarInt(TAG_SOURCETYPE, 6));
						taglist.push_back(new CTagVarInt(TAG_SOURCEPORT, CKadHandler::Instance()->GetTCPPort()));
						if (!CKademlia::GetPrefs()->GetUseExternKadPort()) {
							taglist.push_back(new CTagInt16(TAG_SOURCEUPORT, CKadHandler::Instance()->GetKadPort(true)));
						}
						if (from->GetVersion() >= 2) {
							taglist.push_back(new CTagVarInt(TAG_FILESIZE, file->uSize));
						}
					} else if (CKadHandler::Instance()->HasBuddy()) {	// We are firewalled, make sure we have a buddy.
						// We send the ID to our buddy so they can do a callback.
						CUInt128 buddyID(true);
						buddyID ^= CKademlia::GetPrefs()->GetKadID();
						taglist.push_back(new CTagInt8(TAG_SOURCETYPE, (file->uSize > OLD_MAX_FILE_SIZE) ? 5 : 3));
						taglist.push_back(new CTagVarInt(TAG_SERVERIP, CKadHandler::Instance()->GetBuddyIP()));
						taglist.push_back(new CTagVarInt(TAG_SERVERPORT, CKadHandler::Instance()->GetBuddyPort()));
						taglist.push_back(new CTagString(TAG_BUDDYHASH, buddyID.ToHexString()));
						taglist.push_back(new CTagVarInt(TAG_SOURCEPORT, CKadHandler::Instance()->GetTCPPort()));
						if (!CKademlia::GetPrefs()->GetUseExternKadPort()) {
							taglist.push_back(new CTagInt16(TAG_SOURCEUPORT, CKadHandler::Instance()->GetKadPort(true)));
						}
						if (from->GetVersion() >= 2) {
							taglist.push_back(new CTagVarInt(TAG_FILESIZE, file->uSize));
						}
					} else {
						// We are firewalled, but lost our buddy.. Stop everything.
						PrepareToStop();
						break;
					}
				} else {
					// We're not firewalled..
					taglist.push_back(new CTagInt8(TAG_SOURCETYPE, (file->uSize > OLD_MAX_FILE_SIZE) ? 4 : 1));
					taglist.push_back(new CTagVarInt(TAG_SOURCEPORT, CKadHandler::Instance()->GetTCPPort()));
					if (!CKademlia::GetPrefs()->GetUseExternKadPort()) {
						taglist.push_back(new CTagInt16(TAG_SOURCEUPORT, CKadHandler::Instance()->GetKadPort(true)));
					}
					if (from->GetVersion() >= 2) {
						taglist.push_back(new CTagVarInt(TAG_FILESIZE, file->uSize));
					}
				}

				taglist.push_back(new CTagInt8(TAG_ENCRYPTION, CKademlia::GetPrefs()->GetMyConnectOptions(true, true)));

				// IPv6
				if(CKadHandler::Instance()->HasIPv6())
					taglist.push_back(new CTagString(TAG_IPv6, CKadHandler::Instance()->GetIPv6().ToHexString()));
				//

				// Send packet
				CKademlia::GetUDPListener()->SendPublishSourcePacket(*from, m_target, id, taglist);
				m_totalRequestAnswers++;
				// Delete all tags.
				deleteTagPtrListEntries(&taglist);
			} else {
				PrepareToStop();
			}
			break;
		}
		case STOREKEYWORD: {
			LogKadLine(LOG_DEBUG /*logKadSearch*/, L"Search request type: StoreKeyword");
			// Try to store keywords to a Node.
			// As a safeguard, check to see if we already stored to the max nodes.
			if (m_answers > SEARCHSTOREKEYWORD_TOTAL) {
				PrepareToStop();
				break;
			}

			uint16_t count = m_fileIDs.size();
			if (count == 0) {
				PrepareToStop();
				break;
			} else if (count > 150) {
				count = 150;
			}

			UIntList::const_iterator itListFileID = m_fileIDs.begin();

			while (count && (itListFileID != m_fileIDs.end())) {
				uint16_t packetCount = 0;
				CBuffer packetdata(1024*50); // Allocate a good amount of space.			
				m_target.Write(&packetdata);
				packetdata.WriteValue<uint16_t>(0); // Will be updated before sending.
				while ((packetCount < 50) && (itListFileID != m_fileIDs.end())) {
					CUInt128 id(*itListFileID);
					SFileInfo* file = CKadHandler::Instance()->GetFile(id);
					if (file) {
						count--;
						packetCount++;
						id.Write(&packetdata);
						PreparePacketForTags(&packetdata, file);
					}
					++itListFileID;
				}

				// Correct file count.
				uint64_t current_pos = packetdata.GetPosition();
				packetdata.SetPosition(16);
				packetdata.WriteValue<uint16_t>(packetCount);
				packetdata.SetPosition(current_pos);

				// Send packet
				if (from->GetVersion() >= 6) {
					DebugSend(L"Kad2PublishKeyReq", from->GetIPAddress(), from->GetUDPPort());
					CUInt128 clientID = from->GetClientID();
					CKademlia::GetUDPListener()->SendPacket(packetdata, KADEMLIA2_PUBLISH_KEY_REQ, from->GetIPAddress(), from->GetUDPPort(), from->GetUDPKey(), &clientID);
				} else if (from->GetVersion() >= 2) {
 					DebugSend(L"Kad2PublishKeyReq", from->GetIPAddress(), from->GetUDPPort());
					CKademlia::GetUDPListener()->SendPacket(packetdata, KADEMLIA2_PUBLISH_KEY_REQ, from->GetIPAddress(), from->GetUDPPort(), 0, NULL);
					ASSERT(from->GetUDPKey() == CKadUDPKey(0));
				} else {
					ASSERT(0);
				}
			}
			m_totalRequestAnswers++;
			break;
		}
		case STORENOTES: {
			LogKadLine(LOG_DEBUG /*logKadSearch*/, L"Search request type: StoreNotes");
			// Find file we are storing info about.
			SFileInfo* file = CKadHandler::Instance()->GetFile(m_target);

			if (file) {
				CBuffer packetdata(1024*2);
				// Send the hash of the file we're storing info about.
				m_target.Write(&packetdata);
				// Send our ID with the info.
				CKademlia::GetPrefs()->GetKadID().Write(&packetdata);

				// Create our taglist.
				TagPtrList taglist;
				taglist.push_back(new CTagString(TAG_FILENAME, file->sName));
				if (file->uRating != 0) {
					taglist.push_back(new CTagVarInt(TAG_FILERATING, file->uRating));
				}
				if (!file->sComment.empty()) {
					taglist.push_back(new CTagString(TAG_DESCRIPTION, file->sComment));
				}
				if (from->GetVersion() >= 2) {
					taglist.push_back(new CTagVarInt(TAG_FILESIZE, file->uSize));
				}
				uint32 count = taglist.size();
				ASSERT( count <= 0xFF );
				packetdata.WriteValue<uint8_t>(count);
				for (TagPtrList::const_iterator it = taglist.begin(); it != taglist.end(); it++) {
					(**it).ToBuffer(&packetdata);
				}

				// Send packet
				if (from->GetVersion() >= 6) {
					DebugSend(L"Kad2PublishNotesReq", from->GetIPAddress(), from->GetUDPPort());
					CUInt128 clientID = from->GetClientID();
					CKademlia::GetUDPListener()->SendPacket(packetdata, KADEMLIA2_PUBLISH_NOTES_REQ, from->GetIPAddress(), from->GetUDPPort(), from->GetUDPKey(), &clientID);
				} else if (from->GetVersion() >= 2) {
					DebugSend(L"Kad2PublishNotesReq", from->GetIPAddress(), from->GetUDPPort());
					CKademlia::GetUDPListener()->SendPacket(packetdata, KADEMLIA2_PUBLISH_NOTES_REQ, from->GetIPAddress(), from->GetUDPPort(), 0, NULL);
					ASSERT(from->GetUDPKey() == CKadUDPKey(0));
				} else {
					ASSERT(0);
				}
				m_totalRequestAnswers++;
				// Delete all tags.
				deleteTagPtrListEntries(&taglist);
			} else {
				PrepareToStop();
			}
			break;
		}
		case FINDBUDDY:
		{
			LogKadLine(LOG_DEBUG /*logKadSearch*/, L"Search request type: FindBuddy");
			// Send a buddy request as we are firewalled.
			// As a safeguard, check to see if we already requested the max nodes.
			if (m_answers > SEARCHFINDBUDDY_TOTAL) {
				PrepareToStop();
				break;
			}

			CBuffer packetdata;
			// Send the ID we used to find our buddy. Used for checks later and allows users to callback someone if they change buddies.
			m_target.Write(&packetdata);
			// Send client hash so they can do a callback.
			CKadHandler::Instance()->GetEd2kHash().Write(&packetdata);
			// Send client port so they can do a callback.
			packetdata.WriteValue<uint16_t>(CKadHandler::Instance()->GetTCPPort());

			DebugSend(L"KadFindBuddyReq", from->GetIPAddress(), from->GetUDPPort());
			if (from->GetVersion() >= 6) {
				CUInt128 clientID = from->GetClientID();
				CKademlia::GetUDPListener()->SendPacket(packetdata, KADEMLIA_FINDBUDDY_REQ, from->GetIPAddress(), from->GetUDPPort(), from->GetUDPKey(), &clientID);
			} else {
				CKademlia::GetUDPListener()->SendPacket(packetdata, KADEMLIA_FINDBUDDY_REQ, from->GetIPAddress(), from->GetUDPPort(), 0, NULL);
				ASSERT(from->GetUDPKey() == CKadUDPKey(0));
			}
			m_answers++;
			break;
		}
		case FINDSOURCE:
		{
			LogKadLine(LOG_DEBUG /*logKadSearch*/, L"Search request type: FindSource");
			// Try to find if this is a buddy to someone we want to contact.
			// As a safeguard, check to see if we already requested the max nodes.
			if (m_answers > SEARCHFINDSOURCE_TOTAL) {
				PrepareToStop();
				break;
			}

			CBuffer packetdata(34);
			// This is the ID that the person we want to contact used to find a buddy.
			m_target.Write(&packetdata);
			if (m_fileIDs.size() != 1) {
				throw CException(LOG_ERROR, L"Kademlia.CSearch.processResponse: m_fileIDs.size() != 1");
			}
			// Currently, we limit the type of callbacks for sources. We must know a file this person has for it to work.
			m_fileIDs.front().Write(&packetdata);
			// Send our port so the callback works.
			packetdata.WriteValue<uint16_t>(CKadHandler::Instance()->GetTCPPort());
			// Send packet
			DebugSend(L"KadCallbackReq", from->GetIPAddress(), from->GetUDPPort());
			if (from->GetVersion() >= 6) {
				CUInt128 clientID = from->GetClientID();
				CKademlia::GetUDPListener()->SendPacket(packetdata, KADEMLIA_CALLBACK_REQ, from->GetIPAddress(), from->GetUDPPort(), from->GetUDPKey(), &clientID);
			} else {
				CKademlia::GetUDPListener()->SendPacket( packetdata, KADEMLIA_CALLBACK_REQ, from->GetIPAddress(), from->GetUDPPort(), 0, NULL);
				ASSERT(from->GetUDPKey() == CKadUDPKey(0));
			}
			m_answers++;
			break;
		}
		case NODESPECIAL: {
			// we are looking for the IP of a given NodeID, so we just check if we 0 distance and if so, report the
			// tip to the requester
			if (fromDistance == 0) {
				m_nodeSpecialSearchRequester->KadSearchIPByNodeIDResult(KCSR_SUCCEEDED, from->GetIPAddress(), from->GetTCPPort());
				m_nodeSpecialSearchRequester = NULL;
				PrepareToStop();
			}
			break;
		 }
		case NODECOMPLETE:
			LogKadLine(LOG_DEBUG /*logKadSearch*/, L"Search request type: NodeComplete");
			break;
		case NODE:
			LogKadLine(LOG_DEBUG /*logKadSearch*/, L"Search request type: Node");
			break;
		default:
			LogKadLine(LOG_DEBUG /*logKadSearch*/, L"Search result type: Unknown (%i)", m_type);
			break;
	}
}

void CSearch::ProcessResult(const CUInt128& answer, TagPtrList *info)
{
	wstring type = L"Unknown";
	switch (m_type) {
		case FILE:
			type = L"File";
			ProcessResultFile(answer, info);
			break;
		case KEYWORD:
			type = L"Keyword";
			ProcessResultKeyword(answer, info);
			break;
		case NOTES:
			type = L"Notes";
			ProcessResultNotes(answer, info);
			break;
	}
	LogKadLine(LOG_DEBUG /*logKadSearch*/, L"Got result (%s)", type.c_str());
}

void CSearch::ProcessResultFile(const CUInt128& answer, TagPtrList *info)
{
	// Process a possible source to a file.
	// Set of data we could receive from the result.
	uint8_t type = 0;
	uint32_t ip = 0;
	uint16_t tcp = 0;
	uint16_t udp = 0;
	uint32_t buddyip = 0;
	uint16_t buddyport = 0;
	uint8_t byCryptOptions = 0; // 0 = not supported.
	CUInt128 buddy;
	CUInt128 IPv6; // IPv6

	for (TagPtrList::const_iterator it = info->begin(); it != info->end(); ++it) {
		CTag *tag = *it;
		if (!tag->GetName().compare(TAG_SOURCETYPE)) {
			type = tag->GetInt();
		} else if (!tag->GetName().compare(TAG_SOURCEIP)) {
			ip = tag->GetInt();
		} else if (!tag->GetName().compare(TAG_SOURCEPORT)) {
			tcp = tag->GetInt();
		} else if (!tag->GetName().compare(TAG_SOURCEUPORT)) {
			udp = tag->GetInt();
		} else if (!tag->GetName().compare((TAG_SERVERIP))) {
			buddyip = tag->GetInt();
		} else if (!tag->GetName().compare(TAG_SERVERPORT)) {
			buddyport = tag->GetInt();
		} else if (!tag->GetName().compare(TAG_BUDDYHASH)) {
			if(!tag->IsStr() || !buddy.FromHexString(tag->GetStr()))
				LogKadLine(LOG_DEBUG /*logKadSearch*/, L"+++ Invalid TAG_BUDDYHASH tag");
		} else if (!tag->GetName().compare(TAG_ENCRYPTION)) {
			byCryptOptions = (uint8)tag->GetInt();
		} 
		// IPv6
		else if (!tag->GetName().compare(TAG_IPv6)) {
			if(!tag->IsStr() || !IPv6.FromHexString(tag->GetStr()))
				LogKadLine(LOG_DEBUG /*logKadSearch*/, L"+++ Invalid TAG_IPv6 tag");
		}
		//
	}

	// Process source based on its type. Currently only one method is needed to process all types.
	switch( type ) {
		case 1:
		case 3:
		case 4:
		case 5:
		case 6:
			LogKadLine(LOG_DEBUG /*logKadSearch*/, L"Trying to add a source type %i, ip %s", type, IPToStr(ip, udp).c_str());
			m_answers++;
			CKadHandler::Instance()->SourceFound(m_searchID, &answer, &buddy, type, ip, tcp, udp, buddyip, buddyport, byCryptOptions, &IPv6);
			break;
		case 2: 
			//Don't use this type, some clients will process it wrong.
		default:
			break;
	}
}

void CSearch::ProcessResultNotes(const CUInt128& answer, TagPtrList *info)
{
	// Process a received Note to a file.
	// Create a Note and set the IDs.
	CEntry* entry = new CEntry();
	entry->m_uKeyID.SetValue(m_target);
	entry->m_uSourceID.SetValue(answer);

	// Loop through tags and pull wanted into. Currently we only keep Filename, Rating, Comment.
	for (TagPtrList::iterator it = info->begin(); it != info->end(); ++it) {
		CTag *tag = *it;
		if (!tag->GetName().compare(TAG_SOURCEIP)) {
			entry->m_uIP = tag->GetInt();
		} else if (!tag->GetName().compare(TAG_SOURCEPORT)) {
			entry->m_uTCPport = tag->GetInt();
		} else if (!tag->GetName().compare(TAG_FILENAME)) {
			entry->SetFileName(tag->GetStr());
		} else if (!tag->GetName().compare(TAG_DESCRIPTION)) {
			wstring strComment(tag->GetStr());
			entry->AddTag(tag);
			*it = NULL;	// Prevent actual data being freed
		} else if (!tag->GetName().compare(TAG_FILERATING)) {
			entry->AddTag(tag);
			*it = NULL;	// Prevent actual data being freed
		}
	}

	if(CKadHandler::Instance()->NoteFounds(m_searchID, entry))
		m_answers++;
}

void CSearch::ProcessResultKeyword(const CUInt128& answer, TagPtrList *info)
{
	// Process a keyword that we received.
	// Set of data we can use for a keyword result.
	wstring name;
	uint64_t size = 0;
	wstring type;
	wstring format;
	uint32_t availability = 0;
	uint32_t publishInfo = 0;
	// Flag that is set if we want this keyword
	bool bFileName = false;
	bool bFileSize = false;
	TagPtrList taglist;

  try{
	for (TagPtrList::const_iterator it = info->begin(); it != info->end(); ++it) {
		CTag* tag = *it;
		if (tag->GetName() == TAG_FILENAME) {
			name = tag->GetStr();
			bFileName = !name.empty();
		} else if (tag->GetName() == TAG_FILESIZE) {
			if (tag->IsBsob() && (tag->GetBsobSize() == 8)) {
				// Kad1.0 uint64 type using a BSOB.
				CBuffer Buffer(tag->GetBsob(),8,true);
				size = Buffer.ReadValue<uint64_t>();
			} else {
				ASSERT(tag->IsInt());
				size = tag->GetInt();
			}
			bFileSize = true;
		} else if (tag->GetName() == TAG_FILETYPE) {
			type = tag->GetStr();
		} else if (tag->GetName() == TAG_FILEFORMAT) {
			format = tag->GetStr();
		} else if (tag->GetName() == TAG_SOURCES) {
			availability = tag->GetInt();
			// Some rouge client was setting a invalid availability, just set it to 0.
			if( availability > 65500 ) {
				availability = 0;
			}
		} else if (tag->GetName() == TAG_PUBLISHINFO) {
			// we don't keep this as tag, but as a member property of the searchfile, as we only need its informations
			// in the search list and don't want to carry the tag over when downloading the file (and maybe even wrongly publishing it)
			publishInfo = (uint32_t)tag->GetInt();
#ifdef _DEBUG
			uint32_t differentNames = (publishInfo & 0xFF000000) >> 24;
			uint32_t publishersKnown = (publishInfo & 0x00FF0000) >> 16;
			uint32_t trustValue = publishInfo & 0x0000FFFF;
			LogKadLine(LOG_DEBUG /*logKadSearch*/, L"Received PublishInfo Tag: %u different names, %u publishers, %.2f trustvalue", differentNames, publishersKnown, ((double)trustValue/ 100.0));
#endif
		}
		//EK-ToDo-AICH: <-------------------
		/*else if (!tag->GetName() == TAG_KADAICHHASHRESULT) {
			if (pTag->IsBsob()) {
				CSafeMemFile fileAICHTag(pTag->GetBsob(), pTag->GetBsobSize());
				try
				{
					uint8 byCount = fileAICHTag.ReadUInt8();
					for (uint8 i = 0; i < byCount; i++)
					{
						uint8 byPopularity = fileAICHTag.ReadUInt8();
						if (byPopularity > 0)
						{
							aAICHHashPopularity.Add(byPopularity);
							aAICHHashs.Add(CAICHHash(&fileAICHTag));
						}
					}
				}
				catch (CFileException* pError)
				{
					DebugLogError(_T("ProcessResultKeyword: Corrupt or invalid TAG_KADAICHHASHRESULT received - ip: %s)") , ipstr(ntohl(uFromIP)));
					pError->Delete();
					aAICHHashPopularity.RemoveAll();
					aAICHHashs.RemoveAll();
				}
			}
		}*/
		else {
			taglist.push_back(tag->CloneTag());
		}
	}
  }
  catch(...)
  {
	  deleteTagPtrListEntries(&taglist);
	  throw;
  }

	// If we don't have a valid filename and filesize, drop this keyword.
	if (!bFileName || !bFileSize) {
		LogKadLine(LOG_DEBUG /*logKadSearch*/, L"No %s on search result, ignoring", (!bFileName ? L"filename" : L"filesize"));
		deleteTagPtrListEntries(&taglist);
		return;
	}

	// the file name of the current search response is stored in "name"
	// the list of words the user entered is stored in "m_words"
	// so the file name now gets parsed for all the words entered by the user (even repetitive ones):

	// Step 1: Get the words of the response file name
	WordList listFileNameWords;
	CSearchManager::GetWords(name, &listFileNameWords, true);

	// Step 2: Look for each entered search word in those present in the filename
	bool bFileNameMatchesSearch = true;  // this will be set to "false", if not all search words are found in the file name

	for (WordList::const_iterator itSearchWords = m_words.begin(); itSearchWords != m_words.end(); ++itSearchWords) {
		bool bSearchWordPresent = false;
		for (WordList::iterator itFileNameWords = listFileNameWords.begin(); itFileNameWords != listFileNameWords.end(); ++itFileNameWords) {
			if (!CompareStr(*itFileNameWords,*itSearchWords)) {
				listFileNameWords.erase(itFileNameWords);  // remove not to find same word twice
				bSearchWordPresent = true;
				break;  // found word, go on using the next searched word
			}
		}
		if (!bSearchWordPresent) {
			bFileNameMatchesSearch = false;  // not all search words were found in the file name
			break;
		}
	}

	// Step 3: Accept result only if all(!) words are found
	if (bFileNameMatchesSearch) {
		m_answers++;
		CKadHandler::Instance()->FilesFound(m_searchID, &answer, name, size, type, publishInfo, availability, taglist);
	}
	else
		deleteTagPtrListEntries(&taglist);
}

void CSearch::SendFindValue(CContact *contact, bool reaskMore)
{
	// Found a node that we think has contacts closer to our target.
	try {
		if (m_stopping) {
			return;
		}

		CBuffer packetdata(33);
		// The number of returned contacts is based on the type of search.
		uint8_t contactCount = GetRequestContactCount();

		if (reaskMore) {
			if (m_requestedMoreNodesContact == NULL) {
				m_requestedMoreNodesContact = contact;
				ASSERT(contactCount == KADEMLIA_FIND_VALUE);
				contactCount = KADEMLIA_FIND_VALUE_MORE;
			} else {
				ASSERT(0);
			}
		}

		if (contactCount > 0) {
			packetdata.WriteValue<uint8_t>(contactCount);
		} else {
			return;
		}

		// Put the target we want into the packet.
		m_target.Write(&packetdata);
		// Add the ID of the contact we're contacting for sanity checks on the other end.
		contact->GetClientID().Write(&packetdata);
		// Inc the number of packets sent.
		m_totalPacketSent++;

		if (contact->GetVersion() >= 2) {
			if (contact->GetVersion() >= 6) {
				CUInt128 clientID = contact->GetClientID();
				CKademlia::GetUDPListener()->SendPacket(packetdata, KADEMLIA2_REQ, contact->GetIPAddress(), contact->GetUDPPort(), contact->GetUDPKey(), &clientID);
			} else {
				CKademlia::GetUDPListener()->SendPacket(packetdata, KADEMLIA2_REQ, contact->GetIPAddress(), contact->GetUDPPort(), 0, NULL);
				ASSERT(contact->GetUDPKey() == CKadUDPKey(0));
			}
#ifdef _DEBUG
			switch (m_type) {
				case NODE:
					DebugSend(L"Kad2Req(Node)", contact->GetIPAddress(), contact->GetUDPPort());
					break;
				case NODECOMPLETE:
					DebugSend(L"Kad2Req(NodeComplete)", contact->GetIPAddress(), contact->GetUDPPort());
					break;
				case NODESPECIAL:
					DebugSend(L"Kad2Req(NodeSpecial)", contact->GetIPAddress(), contact->GetUDPPort());
					break;
				case NODEFWCHECKUDP:
					DebugSend(L"Kad2Req(NodeFWCheckUDP)", contact->GetIPAddress(), contact->GetUDPPort());
					break;
				case FILE:
					DebugSend(L"Kad2Req(File)", contact->GetIPAddress(), contact->GetUDPPort());
					break;
				case KEYWORD:
					DebugSend(L"Kad2Req(Keyword)", contact->GetIPAddress(), contact->GetUDPPort());
					break;
				case STOREFILE:
					DebugSend(L"Kad2Req(StoreFile)", contact->GetIPAddress(), contact->GetUDPPort());
					break;
				case STOREKEYWORD:
					DebugSend(L"Kad2Req(StoreKeyword)", contact->GetIPAddress(), contact->GetUDPPort());
					break;
				case STORENOTES:
					DebugSend(L"Kad2Req(StoreNotes)", contact->GetIPAddress(), contact->GetUDPPort());
					break;
				case NOTES:
					DebugSend(L"Kad2Req(Notes)", contact->GetIPAddress(), contact->GetUDPPort());
					break;
				default:
					DebugSend(L"Kad2Req", contact->GetIPAddress(), contact->GetUDPPort());
					break;
			}
#endif
		} else {
			ASSERT(0);
		}
	} 
	catch(const CException& Exception){
		LogKadLine(LOG_DEBUG /*logKadSearch*/, L"Exception in CSearch::SendFindValue: %s", Exception.GetLine().c_str());
	}
}

// TODO: Redundant metadata checks
void CSearch::PreparePacketForTags(CBuffer *bio, SFileInfo *file)
{
	// We're going to publish a keyword, set up the tag list
	TagPtrList taglist;
	
	try {
		if (file && bio) {
			// Name, Size
			taglist.push_back(new CTagString(TAG_FILENAME, file->sName));
			/*if (file->uSize > OLD_MAX_EMULE_FILE_SIZE)
			{
				// TODO: As soon as we drop Kad1 support, we should switch to Int64 tags (we could do now already for kad2 nodes only but no advantage in that)
				byte byValue[8];
				*((uint64*)byValue) = file->uSize;
				taglist.push_back(new CTagBsob(TAG_FILESIZE, byValue, sizeof(byValue)));
			}
			else*/
			taglist.push_back(new CTagVarInt(TAG_FILESIZE, file->uSize));
			taglist.push_back(new CTagVarInt(TAG_SOURCES, file->uCompleteSourcesCount));

			// eD2K file type (Audio, Video, ...)
			// NOTE: Archives and CD-Images are published with file type "Pro"
			wstring strED2KFileType(GetED2KFileTypeSearchTerm(GetED2KFileTypeID(file->sName)));
			if (!strED2KFileType.empty()) {
				taglist.push_back(new CTagString(TAG_FILETYPE, strED2KFileType));
			}
			
			// additional meta data (Artist, Album, Codec, Length, ...)
			for(TagPtrList::iterator I = file->TagList.begin(); I != file->TagList.end(); I++)
			{
				const ::CTag* pTag = *I;
				// skip string tags with empty string values
				if (pTag->IsStr() && pTag->GetStr().empty()) {
					continue;
				}
				// skip integer tags with '0' values
				if (pTag->IsInt() && pTag->GetInt() == 0) {
					continue;
				}

				wstring szKadTagName = pTag->GetName();
				if (pTag->IsStr()) {
					taglist.push_back(new CTagString(szKadTagName, pTag->GetStr()));
				} else {
					taglist.push_back(new CTagVarInt(szKadTagName, pTag->GetInt()));
				}
			}

			uint32 count = taglist.size();
			ASSERT( count <= 0xFF );
			bio->WriteValue<uint8_t>(count);
			for (TagPtrList::const_iterator it = taglist.begin(); it != taglist.end(); it++) {
				(**it).ToBuffer(bio);
			}
		} else {
			//If we get here.. Bad things happen.. Will fix this later if it is a real issue.
			ASSERT(0);
		}
	} 
	catch(const CException& Exception){
		LogKadLine(LOG_DEBUG /*logKadSearch*/, L"Exception in CSearch::PreparePacketForTags: %s", Exception.GetLine().c_str());
	}

	deleteTagPtrListEntries(&taglist);
}

void CSearch::SetSearchTermData(uint32_t searchTermsDataSize, const uint8_t *searchTermsData)
{
	m_searchTermsDataSize = searchTermsDataSize;
	m_searchTermsData = new uint8_t [searchTermsDataSize];
	memcpy(m_searchTermsData, searchTermsData, searchTermsDataSize);
}

uint8_t CSearch::GetRequestContactCount() const
{
	// Returns the amount of contacts we request on routing queries based on the search type
	switch (m_type) {
		case NODE:
		case NODECOMPLETE:
		case NODESPECIAL:
		case NODEFWCHECKUDP:
			return KADEMLIA_FIND_NODE;
		case FILE:
		case KEYWORD:
		case FINDSOURCE:
		case NOTES:
			return KADEMLIA_FIND_VALUE;
		case FINDBUDDY:
		case STOREFILE:
		case STOREKEYWORD:
		case STORENOTES:
			return KADEMLIA_STORE;
		default:
			LogKadLine(LOG_DEBUG /*logKadSearch*/, L"Invalid search type. (CSearch::GetRequestContactCount())");
			ASSERT(0);
			return 0;
	}
}
