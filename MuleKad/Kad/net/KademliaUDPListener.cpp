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

#include "KademliaUDPListener.h"
#include "../Protocols.h"
#include "../Constants.h"
#include "../FileTags.h"
#include "../Packet.h"
#include "../routing/Contact.h"
#include "../routing/RoutingZone.h"
#include "../kademlia/Indexed.h"
#include "../kademlia/Defines.h"
#include "../kademlia/UDPFirewallTester.h"
#include "../KadHandler.h"
#include "../utils/KadUDPKey.h"
#include "../utils/KadClientSearcher.h"
#include "../../../Framework/Buffer.h"
#include "../../../Framework/Strings.h"
#include "../UDPSocket.h"
#include "../../../Framework/Exception.h"

#define CHECK_PACKET_SIZE(OP, SIZE) \
	if (lenPacket OP (uint32_t)(SIZE)) { \
		throw CException(LOG_ERROR, L"***NOTE: Received wrong size (%d) packet in %S", (int)lenPacket, __FUNCTION__); \
	}

#define CHECK_PACKET_MIN_SIZE(SIZE)		CHECK_PACKET_SIZE(<, SIZE)
#define CHECK_PACKET_EXACT_SIZE(SIZE)	CHECK_PACKET_SIZE(!=, SIZE)

#define CHECK_TRACKED_PACKET(OPCODE) \
	if (!IsOnOutTrackList(ip, OPCODE)) { \
		throw CException(LOG_ERROR, L"***NOTE: Received unrequested response packet, size (%d) in %S", (int)lenPacket, __FUNCTION__); \
	}


////////////////////////////////////////
using namespace Kademlia;
////////////////////////////////////////

CKademliaUDPListener::CKademliaUDPListener()
{
}

CKademliaUDPListener::~CKademliaUDPListener()
{
	// report timeout to all pending FetchNodeIDRequests
	for (FetchNodeIDList::iterator it = m_fetchNodeIDRequests.begin(); it != m_fetchNodeIDRequests.end(); ++it) {
		it->requester->KadSearchNodeIDByIPResult(KCSR_TIMEOUT, NULL);
	}
}

// Used by Kad1.0 and Kad2.0
void CKademliaUDPListener::Bootstrap(uint32_t ip, uint16_t port, uint8_t kadVersion, const CUInt128* cryptTargetID)
{
	ASSERT(ip);

	DebugSend(L"Kad2BootstrapReq", ip, port);
	CBuffer bio(0);
	if (kadVersion >= 6) {
		SendPacket(bio, KADEMLIA2_BOOTSTRAP_REQ, ip, port, 0, cryptTargetID);
	} else {
		SendPacket(bio, KADEMLIA2_BOOTSTRAP_REQ, ip, port, 0, NULL);
	}
}

// Used by Kad1.0 and Kad2.0
void CKademliaUDPListener::SendMyDetails(uint8_t opcode, uint32_t ip, uint16_t port, uint8_t kadVersion, const CKadUDPKey& targetKey, const CUInt128* cryptTargetID, bool requestAckPacket)
{
	CBuffer packetdata;
	CKademlia::GetPrefs()->GetKadID().Write(&packetdata);
	
	if (kadVersion > 1) {
		packetdata.WriteValue<uint16_t>(CKadHandler::Instance()->GetTCPPort());
		packetdata.WriteValue<uint8_t>(KADEMLIA_VERSION);
		// Tag Count.
		uint8_t tagCount = 0;
		if (!CKademlia::GetPrefs()->GetUseExternKadPort()) {
			tagCount++;
		}
		if (kadVersion >= 8 && (requestAckPacket || CKademlia::GetPrefs()->GetFirewalled() || CUDPFirewallTester::IsFirewalledUDP(true))) {
			tagCount++;
		}
		packetdata.WriteValue<uint8_t>(tagCount);
		if (!CKademlia::GetPrefs()->GetUseExternKadPort()) {
			CTagVarInt(TAG_SOURCEUPORT, CKadHandler::Instance()->GetKadPort(true)).ToBuffer(&packetdata);
		}
		if (kadVersion >= 8 && (requestAckPacket || CKademlia::GetPrefs()->GetFirewalled() || CUDPFirewallTester::IsFirewalledUDP(true))) {
			// if we're firewalled we send this tag, so the other client doesn't add us to his routing table (if UDP firewalled) and for statistics reasons (TCP firewalled)
			// 5 - reserved (!)
			// 1 - requesting HELLO_RES_ACK
			// 1 - TCP firewalled
			// 1 - UDP firewalled
			CTagVarInt(TAG_KADMISCOPTIONS, (uint8_t)(
				(requestAckPacket ? 1 : 0) << 2 |
				(CKademlia::GetPrefs()->GetFirewalled() ? 1 : 0) << 1 |
				(CUDPFirewallTester::IsFirewalledUDP(true) ? 1 : 0)
			)).ToBuffer(&packetdata);
		}
		if (kadVersion >= 6) {
			if (cryptTargetID == NULL || *cryptTargetID == 0) {
				LogKadLine(LOG_DEBUG /*logClientKadUDP*/, L"Sending hello response to crypt enabled Kad Node which provided an empty NodeID: %s (%u)", IPToStr(ip).c_str(), kadVersion);
				SendPacket(packetdata, opcode, ip, port, targetKey, NULL);
			} else {
				SendPacket(packetdata, opcode, ip, port, targetKey, cryptTargetID);
			}
		} else {
			SendPacket(packetdata, opcode, ip, port, 0, NULL);
			ASSERT(targetKey.IsEmpty());
		}
	} else {
		ASSERT(0);
	}
}

// Kad1.0 and Kad2.0 currently.
void CKademliaUDPListener::FirewalledCheck(uint32_t ip, uint16_t port, const CKadUDPKey& senderKey, uint8_t kadVersion)
{
	if (kadVersion > 6) {
		// new opcode since 0.49a with extended informations to support obfuscated connections properly
		CBuffer packetdata(19);
		packetdata.WriteValue<uint16_t>(CKadHandler::Instance()->GetTCPPort());
		CKadHandler::Instance()->GetEd2kHash().Write(&packetdata);
		packetdata.WriteValue<uint8_t>(CKademlia::GetPrefs()->GetMyConnectOptions(true, false));
		DebugSend(L"KadFirewalled2Req", ip, port);
		SendPacket(packetdata, KADEMLIA_FIREWALLED2_REQ, ip, port, senderKey, NULL);
	} else {
		if(CKadHandler::Instance()->RequiresCryptLayer())
			return;

		CBuffer packetdata(2);
		packetdata.WriteValue<uint16_t>(CKadHandler::Instance()->GetTCPPort());
		DebugSend(L"KadFirewalledReq", ip, port);
		SendPacket(packetdata, KADEMLIA_FIREWALLED_REQ, ip, port, senderKey, NULL);
	}
	CKadHandler::Instance()->AddKadFirewallRequest(ip);
}

void CKademliaUDPListener::SendNullPacket(uint8_t opcode, uint32_t ip, uint16_t port, const CKadUDPKey& targetKey, const CUInt128* cryptTargetID)
{
	CBuffer packetdata(0);
	SendPacket(packetdata, opcode, ip, port, targetKey, cryptTargetID);
}

void CKademliaUDPListener::SendPublishSourcePacket(const CContact& contact, const CUInt128 &targetID, const CUInt128 &contactID, const TagPtrList& tags)
{
	uint8_t opcode;
	CBuffer packetdata;
	targetID.Write(&packetdata);
	if (contact.GetVersion() >= 4/*47c*/) {
		opcode = KADEMLIA2_PUBLISH_SOURCE_REQ;
		contactID.Write(&packetdata);
		DebugSend(L"Kad2PublishSrcReq", contact.GetIPAddress(), contact.GetUDPPort());
	} else {
		opcode = KADEMLIA_PUBLISH_REQ;
		//We only use this for publishing sources now.. So we always send one here..
		packetdata.WriteValue<uint16_t>(1);
		contactID.Write(&packetdata);
		DebugSend(L"KadPublishReq", contact.GetIPAddress(), contact.GetUDPPort());
	}
	uint32 count = tags.size();
	ASSERT( count <= 0xFF );
	packetdata.WriteValue<uint8_t>(count);
	for (TagPtrList::const_iterator it = tags.begin(); it != tags.end(); it++) {
		(**it).ToBuffer(&packetdata);
	}
	if (contact.GetVersion() >= 6) {	// obfuscated ?
		CUInt128 clientID = contact.GetClientID();
		SendPacket(packetdata, opcode, contact.GetIPAddress(), contact.GetUDPPort(), contact.GetUDPKey(), &clientID);
	} else {
		SendPacket(packetdata, opcode, contact.GetIPAddress(), contact.GetUDPPort(), 0, NULL);
	}
}

void CKademliaUDPListener::ProcessPacket(const uint8_t* data, uint32_t lenData, uint32_t ip, uint16_t port, bool validReceiverKey, const CKadUDPKey& senderKey)
{
	// we do not accept (<= 0.48a) unencrypted incoming packets from port 53 (DNS) to avoid attacks based on DNS protocol confusion
	if (port == 53 && senderKey.IsEmpty()) {
		LogKadLine(LOG_DEBUG /*logKadPacketTracking*/, L"Dropping incoming unencrypted packet on port 53 (DNS), IP: %s", IPToStr(ip).c_str());
		return;
	}

	//Update connection state only when it changes.
	bool curCon = CKademlia::GetPrefs()->HasHadContact();
	CKademlia::GetPrefs()->SetLastContact();
	CUDPFirewallTester::Connected();

	uint8_t opcode = data[1];
	const uint8_t *packetData = data + 2;
	uint32_t lenPacket = lenData - 2;

	if (!InTrackListIsAllowedPacket(ip, opcode, validReceiverKey)) {
		return;
	}

	switch (opcode) {
		case KADEMLIA2_BOOTSTRAP_REQ:
			DebugRecv(L"Kad2BootstrapReq", ip, port);
			Process2BootstrapRequest(ip, port, senderKey);
			break;
		case KADEMLIA2_BOOTSTRAP_RES:
			DebugRecv(L"Kad2BootstrapRes", ip, port);
			Process2BootstrapResponse(packetData, lenPacket, ip, port, senderKey, validReceiverKey);
			break;
		case KADEMLIA2_HELLO_REQ:
			DebugRecv(L"Kad2HelloReq", ip, port);
			Process2HelloRequest(packetData, lenPacket, ip, port, senderKey, validReceiverKey);
			break;
		case KADEMLIA2_HELLO_RES:
			DebugRecv(L"Kad2HelloRes", ip, port);
			Process2HelloResponse(packetData, lenPacket, ip, port, senderKey, validReceiverKey);
			break;
		case KADEMLIA2_HELLO_RES_ACK:
			DebugRecv(L"Kad2HelloResAck", ip, port);
			Process2HelloResponseAck(packetData, lenPacket, ip, validReceiverKey);
			break;
		case KADEMLIA2_REQ:
			DebugRecv(L"Kad2Req", ip, port);
			ProcessKademlia2Request(packetData, lenPacket, ip, port, senderKey);
			break;
		case KADEMLIA2_RES:
			DebugRecv(L"Kad2Res", ip, port);
			ProcessKademlia2Response(packetData, lenPacket, ip, port, senderKey);
			break;
		case KADEMLIA2_SEARCH_NOTES_REQ:
			DebugRecv(L"Kad2SearchNotesReq", ip, port);
			Process2SearchNotesRequest(packetData, lenPacket, ip, port, senderKey);
			break;
		case KADEMLIA2_SEARCH_KEY_REQ:
			DebugRecv(L"Kad2SearchKeyReq", ip, port);
			Process2SearchKeyRequest(packetData, lenPacket, ip, port, senderKey);
			break;
		case KADEMLIA2_SEARCH_SOURCE_REQ:
			DebugRecv(L"Kad2SearchSourceReq", ip, port);
			Process2SearchSourceRequest(packetData, lenPacket, ip, port, senderKey);
			break;
		case KADEMLIA_SEARCH_RES:
			DebugRecv(L"KadSearchRes", ip, port);
			ProcessSearchResponse(packetData, lenPacket);
			break;
		case KADEMLIA_SEARCH_NOTES_RES:
			DebugRecv(L"KadSearchNotesRes", ip, port);
			ProcessSearchNotesResponse(packetData, lenPacket, ip);
			break;
		case KADEMLIA2_SEARCH_RES:
			DebugRecv(L"Kad2SearchRes", ip, port);
			Process2SearchResponse(packetData, lenPacket, senderKey);
			break;
		case KADEMLIA2_PUBLISH_NOTES_REQ:
			DebugRecv(L"Kad2PublishNotesReq", ip, port);
			Process2PublishNotesRequest(packetData, lenPacket, ip, port, senderKey);
			break;
		case KADEMLIA2_PUBLISH_KEY_REQ:
			DebugRecv(L"Kad2PublishKeyReq", ip, port);
			Process2PublishKeyRequest(packetData, lenPacket, ip, port, senderKey);
			break;
		case KADEMLIA2_PUBLISH_SOURCE_REQ:
			DebugRecv(L"Kad2PublishSourceReq", ip, port);
			Process2PublishSourceRequest(packetData, lenPacket, ip, port, senderKey);
			break;
		case KADEMLIA_PUBLISH_RES:
			DebugRecv(L"KadPublishRes", ip, port);
			ProcessPublishResponse(packetData, lenPacket, ip);
			break;
		case KADEMLIA2_PUBLISH_RES:
			DebugRecv(L"Kad2PublishRes", ip, port);
			Process2PublishResponse(packetData, lenPacket, ip, port, senderKey);
			break;
		case KADEMLIA_FIREWALLED_REQ:
			DebugRecv(L"KadFirewalledReq", ip, port);
			ProcessFirewalledRequest(packetData, lenPacket, ip, port, senderKey);
			break;
		case KADEMLIA_FIREWALLED2_REQ:
			DebugRecv(L"KadFirewalled2Req", ip, port);
			ProcessFirewalled2Request(packetData, lenPacket, ip, port, senderKey);
			break;
		case KADEMLIA_FIREWALLED_RES:
			DebugRecv(L"KadFirewalledRes", ip, port);
			ProcessFirewalledResponse(packetData, lenPacket, ip, senderKey);
			break;
		case KADEMLIA_FIREWALLED_ACK_RES:
			DebugRecv(L"KadFirewalledAck", ip, port);
			ProcessFirewalledAckResponse(lenPacket);
			break;
		case KADEMLIA_FINDBUDDY_REQ:
			DebugRecv(L"KadFindBuddyReq", ip, port);
			ProcessFindBuddyRequest(packetData, lenPacket, ip, port, senderKey);
			break;
		case KADEMLIA_FINDBUDDY_RES:
			DebugRecv(L"KadFindBuddyRes", ip, port);
			ProcessFindBuddyResponse(packetData, lenPacket, ip, port, senderKey);
			break;
		case KADEMLIA_CALLBACK_REQ:
			DebugRecv(L"KadCallbackReq", ip, port);
			ProcessCallbackRequest(packetData, lenPacket, ip, port, senderKey);
			break;
		case KADEMLIA2_PING:
			DebugRecv(L"Kad2Ping", ip, port);
			Process2Ping(ip, port, senderKey);
			break;
		case KADEMLIA2_PONG:
			DebugRecv(L"Kad2Pong", ip, port);
			Process2Pong(packetData, lenPacket, ip);
			break;
		case KADEMLIA2_FIREWALLUDP:
			DebugRecv(L"Kad2FirewallUDP", ip, port);
			Process2FirewallUDP(packetData, lenPacket, ip);
			break;

		// old Kad1 opcodes which we don't handle anymore
		case KADEMLIA_BOOTSTRAP_REQ_DEPRECATED:
		case KADEMLIA_BOOTSTRAP_RES_DEPRECATED:
		case KADEMLIA_HELLO_REQ_DEPRECATED:
		case KADEMLIA_HELLO_RES_DEPRECATED:
		case KADEMLIA_REQ_DEPRECATED:
		case KADEMLIA_RES_DEPRECATED:
		case KADEMLIA_PUBLISH_NOTES_REQ_DEPRECATED:
		case KADEMLIA_PUBLISH_NOTES_RES_DEPRECATED:
		case KADEMLIA_SEARCH_REQ:
		case KADEMLIA_PUBLISH_REQ:
		case KADEMLIA_SEARCH_NOTES_REQ:
			break;

		default: {
			throw CException(LOG_ERROR, L"Unknown opcode %d on CKademliaUDPListener::ProcessPacket()", opcode);
		}
	}
}

// Used only for Kad2.0
bool CKademliaUDPListener::AddContact2(const uint8_t *data, uint32_t lenData, uint32_t ip, uint16_t& port, uint8_t *outVersion, const CKadUDPKey& udpKey, bool& ipVerified, bool update, bool fromHelloReq, bool* outRequestsACK, CUInt128* outContactID)
{
	if (outRequestsACK != 0) {
		*outRequestsACK = false;
	}

	CBuffer bio(data, lenData, true);
	CUInt128 id;
	id.Read(&bio);
	if (outContactID != NULL) {
		*outContactID = id;
	}
	uint16_t tport = bio.ReadValue<uint16_t>();
	uint8_t version = bio.ReadValue<uint8_t>();
	if (version == 0) {
		throw CException(LOG_ERROR, L"***NOTE: Received invalid Kademlia2 version (%d) in %S", (int)version, __FUNCTION__);
	}
	if (outVersion != NULL) {
		*outVersion = version;
	}
	bool udpFirewalled = false;
	bool tcpFirewalled = false;
	uint8_t tags = bio.ReadValue<uint8_t>();
	while (tags) {
		CTag *tag = CTag::FromBuffer(&bio);
		if (!tag->GetName().compare(TAG_SOURCEUPORT)) {
			if (tag->IsInt() && (uint16_t)tag->GetInt() > 0) {
				port = tag->GetInt();
			}
		} else if (!tag->GetName().compare(TAG_KADMISCOPTIONS)) {
			if (tag->IsInt() && tag->GetInt() > 0) {
				udpFirewalled = (tag->GetInt() & 0x01) > 0;
				tcpFirewalled = (tag->GetInt() & 0x02) > 0;
				if ((tag->GetInt() & 0x04) > 0) {
					if (outRequestsACK != NULL) {
						if (version >= 8) {
							*outRequestsACK = true;
						}
					} else {
						ASSERT(0);
					}
				}
			}
		}
		delete tag;
		--tags;
	}

	// check if we are waiting for informations (nodeid) about this client and if so inform the requester
	for (FetchNodeIDList::iterator it = m_fetchNodeIDRequests.begin(); it != m_fetchNodeIDRequests.end(); ++it) {
		if (it->ip == ip && it->tcpPort == tport) {
			LogKadLine(LOG_DEBUG /*logKadMain*/, L"Result Addcontact: %s", id.ToHexString().c_str());
			uint8_t uchID[16];
			id.ToByteArray(uchID);
			it->requester->KadSearchNodeIDByIPResult(KCSR_SUCCEEDED, uchID);
			m_fetchNodeIDRequests.erase(it);
			break;
		}
	}

	if (fromHelloReq && version >= 8) {
		// this is just for statistic calculations. We try to determine the ratio of (UDP) firewalled users,
		// by counting how many of all nodes which have us in their routing table (our own routing table is supposed
		// to have no UDP firewalled nodes at all) and support the firewalled tag are firewalled themself.
		// Obviously this only works if we are not firewalled ourself
		CKademlia::GetPrefs()->StatsIncUDPFirewalledNodes(udpFirewalled);
		CKademlia::GetPrefs()->StatsIncTCPFirewalledNodes(tcpFirewalled);
	}

	if (!udpFirewalled) {	// do not add (or update) UDP firewalled sources to our routing table
		return CKademlia::GetRoutingZone()->Add(id, ip, port, tport, version, udpKey, ipVerified, update, true);
	} else {
		LogKadLine(LOG_DEBUG /*logKadRouting*/, L"Not adding firewalled client to routing table (%s)", IPToStr(ip).c_str());
		return false;
	}
}

// KADEMLIA2_BOOTSTRAP_REQ
// Used only for Kad2.0
void CKademliaUDPListener::Process2BootstrapRequest(uint32_t ip, uint16_t port, const CKadUDPKey& senderKey)
{
	// Get some contacts to return
	ContactList contacts;
	uint16_t numContacts = (uint16_t)CKademlia::GetRoutingZone()->GetBootstrapContacts(&contacts, 20);

	// Create response packet
	//We only collect a max of 20 contacts here.. Max size is 521.
	//2 + 25(20) + 19
	CBuffer packetdata(521);

	CKademlia::GetPrefs()->GetKadID().Write(&packetdata);
	packetdata.WriteValue<uint16_t>(CKadHandler::Instance()->GetTCPPort());
	packetdata.WriteValue<uint8_t>(KADEMLIA_VERSION);

	// Write packet info
	packetdata.WriteValue<uint16_t>(numContacts);
	CContact *contact;
	for (ContactList::const_iterator it = contacts.begin(); it != contacts.end(); ++it) {
		contact = *it;
		contact->GetClientID().Write(&packetdata);
		packetdata.WriteValue<uint32_t>(contact->GetIPAddress());
		packetdata.WriteValue<uint16_t>(contact->GetUDPPort());
		packetdata.WriteValue<uint16_t>(contact->GetTCPPort());
		packetdata.WriteValue<uint8_t>(contact->GetVersion());
	}

	// Send response
	DebugSend(L"Kad2BootstrapRes", ip, port);
	SendPacket(packetdata, KADEMLIA2_BOOTSTRAP_RES, ip, port, senderKey, NULL);
}

// KADEMLIA2_BOOTSTRAP_RES
// Used only for Kad2.0
void CKademliaUDPListener::Process2BootstrapResponse(const uint8_t *packetData, uint32_t lenPacket, uint32_t ip, uint16_t port, const CKadUDPKey& senderKey, bool validReceiverKey)
{
	CHECK_TRACKED_PACKET(KADEMLIA2_BOOTSTRAP_REQ);

	CRoutingZone *routingZone = CKademlia::GetRoutingZone();

	// How many contacts were given
	CBuffer bio(packetData, lenPacket, true);
	CUInt128 contactID;
	contactID.Read(&bio);
	uint16_t tport = bio.ReadValue<uint16_t>();
	uint8_t version = bio.ReadValue<uint8_t>();
	// if we don't know any Contacts yet and try to Bootstrap, we assume that all contacts are verified,
	// in order to speed up the connecting process. The attackvectors to exploit this are very small with no
	// major effects, so that's a good trade
	bool assumeVerified = CKademlia::GetRoutingZone()->GetNumContacts() == 0;

	if (CKademlia::s_bootstrapList.empty()) {
		routingZone->Add(contactID, ip, port, tport, version, senderKey, validReceiverKey, true, false);
	}
	LogKadLine(LOG_DEBUG /*logClientKadUDP*/, L"Inc Kad2 Bootstrap packet from %s", IPToStr(ip).c_str());

	uint16_t numContacts = bio.ReadValue<uint16_t>();
	while (numContacts) {
		contactID.Read(&bio);
		ip = bio.ReadValue<uint32_t>();
		port = bio.ReadValue<uint16_t>();
		tport = bio.ReadValue<uint16_t>();
		version = bio.ReadValue<uint8_t>();
		TRACE(L"recived IP: %s", IPToStr(ip, port).c_str());
		bool verified = assumeVerified;
		routingZone->Add(contactID, ip, port, tport, version, 0, verified, false, false);
		numContacts--;
	}
}

// KADEMLIA2_HELLO_REQ
// Used in Kad2.0 only
void CKademliaUDPListener::Process2HelloRequest(const uint8_t *packetData, uint32_t lenPacket, uint32_t ip, uint16_t port, const CKadUDPKey& senderKey, bool validReceiverKey)
{
#ifdef _DEBUG
	uint16_t dbgOldUDPPort = port;
#endif
	uint8_t contactVersion = 0;
	CUInt128 contactID;
	bool addedOrUpdated = AddContact2(packetData, lenPacket, ip, port, &contactVersion, senderKey, validReceiverKey, true, true, NULL, &contactID); // might change (udp)port, validReceiverKey
	ASSERT(contactVersion >= 2);
#ifdef _DEBUG
	if (dbgOldUDPPort != port) {
		LogKadLine(LOG_DEBUG /*logClientKadUDP*/, L"KadContact %s uses his internal (%u) instead external (%u) UDP Port", IPToStr(ip).c_str(), port, dbgOldUDPPort);
	}
#endif

	DebugSend(L"Kad2HelloRes", ip, port);
	// if this contact was added or updated (so with other words not filtered or invalid) to our routing table and did not already send a valid
	// receiver key or is already verified in the routing table, we request an additional ACK package to complete a three-way-handshake and
	// verify the remote IP
	SendMyDetails(KADEMLIA2_HELLO_RES, ip, port, contactVersion, senderKey, &contactID, addedOrUpdated && !validReceiverKey);

	if (addedOrUpdated && !validReceiverKey && contactVersion == 7 && !HasActiveLegacyChallenge(ip)) {
		// Kad Version 7 doesn't support HELLO_RES_ACK but sender/receiver keys, so send a ping to validate
		DebugSend(L"Kad2Ping", ip, port);
		SendNullPacket(KADEMLIA2_PING, ip, port, senderKey, NULL);
#ifdef _DEBUG
		CContact* contact = CKademlia::GetRoutingZone()->GetContact(contactID);
		if (contact != NULL) {
			if (contact->GetType() < 2) {
				LogKadLine(LOG_DEBUG /*logKadRouting*/, L"Sending (ping) challenge to a long known contact (should be verified already) - %s", IPToStr(ip).c_str());
			}
		} else {
			ASSERT(0);
		}
#endif
	} else if (CKademlia::GetPrefs()->FindExternKadPort(false) && contactVersion > 5) {	// do we need to find out our extern port?
		DebugSend(L"Kad2Ping", ip, port);
		SendNullPacket(KADEMLIA2_PING, ip, port, senderKey, NULL);
	}

	if (addedOrUpdated && !validReceiverKey && contactVersion < 7 && !HasActiveLegacyChallenge(ip)) {
		// we need to verify this contact but it doesn't support HELLO_RES_ACK nor keys, do a little workaround
		SendLegacyChallenge(ip, port, contactID);
	}

	// Check if firewalled
	if (CKademlia::GetPrefs()->GetRecheckIP() && CKadHandler::Instance()->GetTCPPort() != 0) {
		FirewalledCheck(ip, port, senderKey, contactVersion);
	}
}

// KADEMLIA2_HELLO_RES_ACK
// Used in Kad2.0 only
void CKademliaUDPListener::Process2HelloResponseAck(const uint8_t *packetData, uint32_t lenPacket, uint32_t ip, bool validReceiverKey)
{
	CHECK_PACKET_MIN_SIZE(17);
	CHECK_TRACKED_PACKET(KADEMLIA2_HELLO_RES);

	if (!validReceiverKey) {
		LogKadLine(LOG_DEBUG /*logClientKadUDP*/, L"Receiver key is invalid! (sender: %s)", IPToStr(ip).c_str());
		return;
	}

	// Additional packet to complete a three-way-handshake, making sure the remote contact is not using a spoofed ip.
	CBuffer bio(packetData, lenPacket, true);
	CUInt128 remoteID;
	remoteID.Read(&bio);
	if (!CKademlia::GetRoutingZone()->VerifyContact(remoteID, ip)) {
		LogKadLine(LOG_DEBUG /*logKadRouting*/, L"Unable to find valid sender in routing table (sender: %s)", IPToStr(ip).c_str());
	} else {
		LogKadLine(LOG_DEBUG /*logKadRouting*/, L"Verified contact (%s) by HELLO_RES_ACK", IPToStr(ip).c_str());
	}
}

// KADEMLIA2_HELLO_RES
// Used in Kad2.0 only
void CKademliaUDPListener::Process2HelloResponse(const uint8_t *packetData, uint32_t lenPacket, uint32_t ip, uint16_t port, const CKadUDPKey& senderKey, bool validReceiverKey)
{
	CHECK_TRACKED_PACKET(KADEMLIA2_HELLO_REQ);

	// Add or Update contact.
	uint8_t contactVersion;
	CUInt128 contactID;
	bool sendACK = false;
	bool addedOrUpdated = AddContact2(packetData, lenPacket, ip, port, &contactVersion, senderKey, validReceiverKey, true, false, &sendACK, &contactID);

	if (sendACK) {
		// the client requested us to send an ACK packet, which proves that we're not a spoofed fake contact
		// fulfill his wish
		if (senderKey.IsEmpty()) {
			// but we don't have a valid sender key - there is no point to reply in this case
			// most likely a bug in the remote client
			LogKadLine(LOG_DEBUG /*logClientKadUDP*/, L"Remote client demands ACK, but didn't send any sender key! (sender: %s)", IPToStr(ip).c_str());
		} else {
			CBuffer packet(17);
			CKademlia::GetPrefs()->GetKadID().Write(&packet);
			packet.WriteValue<uint8_t>(0);	// no tags at this time
			DebugSend(L"Kad2HelloResAck", ip, port);
			SendPacket(packet, KADEMLIA2_HELLO_RES_ACK, ip, port, senderKey, NULL);
		}
	} else if (addedOrUpdated && !validReceiverKey && contactVersion < 7) {
		// even though this is supposably an answer to a request from us, there are still possibilities to spoof
		// it, as long as the attacker knows that we would send a HELLO_REQ (which in this case is quite often),
		// so for old Kad Version which doesn't support keys, we need
		SendLegacyChallenge(ip, port, contactID);
	}

	// do we need to find out our extern port?
	if (CKademlia::GetPrefs()->FindExternKadPort(false) && contactVersion > 5) {
		DebugSend(L"Kad2Ping", ip, port);
		SendNullPacket(KADEMLIA2_PING, ip, port, senderKey, NULL);
	}

	// Check if firewalled
	if (CKademlia::GetPrefs()->GetRecheckIP() && CKadHandler::Instance()->GetTCPPort() != 0) {
		FirewalledCheck(ip, port, senderKey, contactVersion);
	}
}

// KADEMLIA2_REQ
// Used in Kad2.0 only
void CKademliaUDPListener::ProcessKademlia2Request(const uint8_t *packetData, uint32_t lenPacket, uint32_t ip, uint16_t port, const CKadUDPKey& senderKey)
{
	// Get target and type
	CBuffer bio(packetData, lenPacket, true);
	uint8_t type = bio.ReadValue<uint8_t>();
	type &= 0x1F;
	if (type == 0) {
		throw CException(LOG_ERROR, L"***NOTE: Received wrong type (%d) in %S", (int)type , __FUNCTION__);
	}

	// This is the target node trying to be found.
	CUInt128 target;
	target.Read(&bio);
	// Convert Target to Distance as this is how we store contacts.
	CUInt128 distance(CKademlia::GetPrefs()->GetKadID());
	distance.XOR(target);

	// This makes sure we are not mistaken identify. Some client may have fresh installed and have a new KadID.
	CUInt128 check;
	check.Read(&bio);
	if (CKademlia::GetPrefs()->GetKadID() == check) {
		// Get required number closest to target
		ContactMap results;
		CKademlia::GetRoutingZone()->GetClosestTo(2, target, distance, type, &results);
		uint8_t count = (uint8_t)results.size();

		// Write response
		// Max count is 32. size 817..
		// 16 + 1 + 25(32)
		CBuffer packetdata(817);
		target.Write(&packetdata);;
		packetdata.WriteValue<uint8_t>(count);
		CContact *c;
		for (ContactMap::const_iterator it = results.begin(); it != results.end(); ++it) {
			c = it->second;
			c->GetClientID().Write(&packetdata);;
			packetdata.WriteValue<uint32_t>(c->GetIPAddress());
			packetdata.WriteValue<uint16_t>(c->GetUDPPort());
			packetdata.WriteValue<uint16_t>(c->GetTCPPort());
			packetdata.WriteValue<uint8_t>(c->GetVersion()); //<- Kad Version inserted to allow backward compatibility.
		}

		DebugSend(L"Kad2Res", ip, port);
		SendPacket(packetdata, KADEMLIA2_RES, ip, port, senderKey, NULL);
	}
}

// KADEMLIA2_RES
// Used in Kad2.0 only
void CKademliaUDPListener::ProcessKademlia2Response(const uint8_t *packetData, uint32_t lenPacket, uint32_t ip, uint16_t port, const CKadUDPKey& senderKey)
{
	CHECK_TRACKED_PACKET(KADEMLIA2_REQ);

	// Used Pointers
	CRoutingZone *routingZone = CKademlia::GetRoutingZone();

	// What search does this relate to
	CBuffer bio(packetData, lenPacket, true);
	CUInt128 target;
	target.Read(&bio);
	uint8_t numContacts = bio.ReadValue<uint8_t>();

	// Is this one of our legacy challenge packets?
	CUInt128 contactID;
	if (IsLegacyChallenge(target, ip, KADEMLIA2_REQ, contactID)) {
		// yup it is, set the contact as verified
		if (!routingZone->VerifyContact(contactID, ip)) {
			LogKadLine(LOG_DEBUG /*logKadRouting*/, L"Unable to find valid sender in routing table (sender: %s)", IPToStr(ip).c_str());
		} else {
			LogKadLine(LOG_DEBUG /*logKadRouting*/, L"Verified contact with legacy challenge (Kad2Req) - %s", IPToStr(ip).c_str());
		}
		return;	// we do not actually care for its other content
	}
	// Verify packet is expected size
	CHECK_PACKET_EXACT_SIZE(16+1 + (16+4+2+2+1)*numContacts);

	// is this a search for firewallcheck ips?
	bool isFirewallUDPCheckSearch = false;
	if (CUDPFirewallTester::IsFWCheckUDPRunning() && CSearchManager::IsFWCheckUDPSearch(target)) {
		isFirewallUDPCheckSearch = true;
	}

#ifdef _DEBUG
	uint32_t ignoredCount = 0;
	uint32_t kad1Count = 0;
#endif
	ContactList* results = new ContactList;
  try{
	for (uint8_t i = 0; i < numContacts; i++) {
		CUInt128 id;
		id.Read(&bio);
		uint32_t contactIP = bio.ReadValue<uint32_t>();
		uint16_t contactPort = bio.ReadValue<uint16_t>();
		uint16_t tport = bio.ReadValue<uint16_t>();
		uint8_t version = bio.ReadValue<uint8_t>();
		if (version > 1) {	// Kad1 nodes are no longer accepted and ignored
			if (IsGoodIPPort(contactIP, contactPort) && (!IsLanIP(contactIP) || !CKadHandler::Instance()->UseLanMode())) {
				if (!CKadHandler::Instance()->IsFiltered(contactIP) && !(contactPort == 53 && version <= 5) /*No DNS Port without encryption*/) {
					if (isFirewallUDPCheckSearch) {
						// UDP FirewallCheck searches are special. The point is we need an IP which we didn't sent a UDP message yet
						// (or in the near future), so we do not try to add those contacts to our routingzone and we also don't
						// deliver them back to the searchmanager (because he would UDP-ask them for further results), but only report
						// them to FirewallChecker - this will of course cripple the search but thats not the point, since we only
						// care for IPs and not the random set target
						CUDPFirewallTester::AddPossibleTestContact(id, contactIP, contactPort, tport, target, version, 0, false);
					} else {
						bool verified = false;
						bool wasAdded = routingZone->AddUnfiltered(id, contactIP, contactPort, tport, version, 0, verified, false, false);
						CContact *temp = new CContact(id, contactIP, contactPort, tport, version, 0, false, target);
						if (wasAdded || routingZone->IsAcceptableContact(temp)) {
							results->push_back(temp);
						} else {
#ifdef _DEBUG
							ignoredCount++;
#endif
							delete temp;
						}
					}
				}
			}
		} else {
#ifdef _DEBUG
			kad1Count++;
#endif
		}
	}
  }catch(...) {
	  ASSERT(0);
	  delete results;
	  throw;
  }
#ifdef _DEBUG
	if (ignoredCount > 0) {
		LogKadLine(LOG_DEBUG /*logKadRouting*/, L"Ignored %u bad %s in routing answer from %s", ignoredCount, (ignoredCount > 1 ? L"contacts" : L"contact"), IPToStr(ip).c_str());
	}
	if (kad1Count > 0) {
		LogKadLine(LOG_DEBUG /*logKadRouting*/, L"Ignored %u kad1 %s in routing answer from %s", kad1Count, (kad1Count > 1 ? L"contacts" : L"contact"), IPToStr(ip).c_str());
	}
#endif

	CSearchManager::ProcessResponse(target, ip, port, results);
}

void CKademliaUDPListener::Free(SSearchTerm* pSearchTerms)
{
	if (pSearchTerms) {
		Free(pSearchTerms->left);
		Free(pSearchTerms->right);
		delete pSearchTerms;
	}
}

SSearchTerm* CKademliaUDPListener::CreateSearchExpressionTree(CBuffer& bio, int iLevel)
{
	// the max. depth has to match our own limit for creating the search expression 
	// (see also 'ParsedSearchExpression' and 'GetSearchPacket')
	if (iLevel >= 24){
		LogKadLine(LOG_DEBUG /*logKadSearch*/, L"***NOTE: Search expression tree exceeds depth limit!");
		return NULL;
	}
	iLevel++;

	uint8_t op = bio.ReadValue<uint8_t>();
	if (op == 0x00) {
		uint8_t boolop = bio.ReadValue<uint8_t>();
		if (boolop == 0x00) { // AND
			SSearchTerm* pSearchTerm = new SSearchTerm;
			pSearchTerm->type = SSearchTerm::AND;
			//TRACE(" AND");
			if ((pSearchTerm->left = CreateSearchExpressionTree(bio, iLevel)) == NULL){
				ASSERT(0);
				delete pSearchTerm;
				return NULL;
			}
			if ((pSearchTerm->right = CreateSearchExpressionTree(bio, iLevel)) == NULL){
				ASSERT(0);
				Free(pSearchTerm->left);
				delete pSearchTerm;
				return NULL;
			}
			return pSearchTerm;
		} else if (boolop == 0x01) { // OR
			SSearchTerm* pSearchTerm = new SSearchTerm;
			pSearchTerm->type = SSearchTerm::OR;
			//TRACE(" OR");
			if ((pSearchTerm->left = CreateSearchExpressionTree(bio, iLevel)) == NULL){
				ASSERT(0);
				delete pSearchTerm;
				return NULL;
			}
			if ((pSearchTerm->right = CreateSearchExpressionTree(bio, iLevel)) == NULL){
				ASSERT(0);
				Free(pSearchTerm->left);
				delete pSearchTerm;
				return NULL;
			}
			return pSearchTerm;
		} else if (boolop == 0x02) { // NOT
			SSearchTerm* pSearchTerm = new SSearchTerm;
			pSearchTerm->type = SSearchTerm::NOT;
			//TRACE(" NOT");
			if ((pSearchTerm->left = CreateSearchExpressionTree(bio, iLevel)) == NULL){
				ASSERT(0);
				delete pSearchTerm;
				return NULL;
			}
			if ((pSearchTerm->right = CreateSearchExpressionTree(bio, iLevel)) == NULL){
				ASSERT(0);
				Free(pSearchTerm->left);
				delete pSearchTerm;
				return NULL;
			}
			return pSearchTerm;
		} else {
			LogKadLine(LOG_DEBUG /*logKadSearch*/, L"*** Unknown boolean search operator 0x%02x (CreateSearchExpressionTree)", boolop);
			return NULL;
		}
	} else if (op == 0x01) { // String
		wstring str(bio.ReadString(CBuffer::eUtf8));
		
		// Make lowercase, the search code expects lower case strings!
		str = MkLower(str);
		
		SSearchTerm* pSearchTerm = new SSearchTerm;
		pSearchTerm->type = SSearchTerm::String;
		pSearchTerm->astr = new vector<wstring>;

		vector<wstring> word_vector = CSearchManager::SplitSearchWords(str);
		for(vector<wstring>::iterator I = word_vector.begin(); I != word_vector.end(); I++)
		{
			wstring strTok = *I;
			if (!strTok.empty()) {
				pSearchTerm->astr->push_back(strTok);
			}
		}

		return pSearchTerm;
	} else if (op == 0x02) { // Meta tag
		// read tag value
		wstring strValue(bio.ReadString(CBuffer::eUtf8));
		// Make lowercase, the search code expects lower case strings!
		strValue = MkLower(strValue);

		// read tag name
		wstring strTagName =  bio.ReadString(CBuffer::eUtf8_BOM);

		SSearchTerm* pSearchTerm = new SSearchTerm;
		pSearchTerm->type = SSearchTerm::MetaTag;
		pSearchTerm->tag = new CTagString(strTagName, strValue);
		return pSearchTerm;
	}
	else if (op == 0x03 || op == 0x08) { // Numeric relation (0x03=32-bit or 0x08=64-bit)
		static const struct {
			SSearchTerm::ESearchTermType eSearchTermOp;
			wstring pszOp;
		} _aOps[] =
		{
			{ SSearchTerm::OpEqual,			L"="	}, // mmop=0x00
			{ SSearchTerm::OpGreater,		L">"	}, // mmop=0x01
			{ SSearchTerm::OpLess,			L"<"	}, // mmop=0x02
			{ SSearchTerm::OpGreaterEqual,	L">="	}, // mmop=0x03
			{ SSearchTerm::OpLessEqual,		L"<="	}, // mmop=0x04
			{ SSearchTerm::OpNotEqual,		L"<>"	}  // mmop=0x05
		};

		// read tag value
		uint64_t ullValue = (op == 0x03) ? bio.ReadValue<uint32_t>() : bio.ReadValue<uint64_t>();

		// read integer operator
		uint8_t mmop = bio.ReadValue<uint8_t>();
		if (mmop >= ARRSIZE(_aOps)){
			LogKadLine(LOG_DEBUG /*logKadSearch*/, L"*** Unknown integer search op=0x%02x (CreateSearchExpressionTree)", mmop);
			return NULL;
		}

		// read tag name
		wstring strTagName = bio.ReadString(CBuffer::eUtf8_BOM);

		SSearchTerm* pSearchTerm = new SSearchTerm;
		pSearchTerm->type = _aOps[mmop].eSearchTermOp;
		pSearchTerm->tag = new CTagVarInt(strTagName, ullValue);

		return pSearchTerm;
	} else {
		LogKadLine(LOG_DEBUG /*logKadSearch*/, L"*** Unknown search op=0x%02x (CreateSearchExpressionTree)", op);
		return NULL;
	}
}

// KADEMLIA2_SEARCH_KEY_REQ
// Used in Kad2.0 only
void CKademliaUDPListener::Process2SearchKeyRequest(const uint8_t *packetData, uint32_t lenPacket, uint32_t ip, uint16_t port, const CKadUDPKey& senderKey)
{
	CBuffer bio(packetData, lenPacket, true);
	CUInt128 target;
	target.Read(&bio);
	uint16_t startPosition = bio.ReadValue<uint16_t>();
	bool restrictive = ((startPosition & 0x8000) == 0x8000);
	startPosition &= 0x7FFF;
	SSearchTerm* pSearchTerms = NULL;
	if (restrictive) {
		pSearchTerms = CreateSearchExpressionTree(bio, 0);
		if (pSearchTerms == NULL) {
			throw CException(LOG_ERROR, L"Invalid search expression");
		}
	}
	CKademlia::GetIndexed()->SendValidKeywordResult(target, pSearchTerms, ip, port, false, startPosition, senderKey);
	if (pSearchTerms) {
		Free(pSearchTerms);
	}
}

// KADEMLIA2_SEARCH_SOURCE_REQ
// Used in Kad2.0 only
void CKademliaUDPListener::Process2SearchSourceRequest(const uint8_t *packetData, uint32_t lenPacket, uint32_t ip, uint16_t port, const CKadUDPKey& senderKey)
{
	CBuffer bio(packetData, lenPacket, true);
	CUInt128 target;
	target.Read(&bio);
	uint16_t startPosition = (bio.ReadValue<uint16_t>() & 0x7FFF);
	uint64_t fileSize = bio.ReadValue<uint64_t>();
	CKademlia::GetIndexed()->SendValidSourceResult(target, ip, port, startPosition, fileSize, senderKey);
}

void CKademliaUDPListener::ProcessSearchResponse(CBuffer& bio)
{
	// What search does this relate to
	CUInt128 target;
	target.Read(&bio);

	// How many results..
	uint16_t count = bio.ReadValue<uint16_t>();
	while (count > 0) {
		// What is the answer
		CUInt128 answer;
		answer.Read(&bio);

		// Get info about answer
		// NOTE: this is the one and only place in Kad where we allow string conversion to local code page in
		// case we did not receive an UTF8 string. this is for backward compatibility for search results which are 
		// supposed to be 'viewed' by user only and not feed into the Kad engine again!
		// If that tag list is once used for something else than for viewing, special care has to be taken for any
		// string conversion!
 		TagPtrList tags;
	  try{
		uint8_t count = bio.ReadValue<uint8_t>();
		for (uint8_t i = 0; i < count; i++)
		{
			CTag* tag = CTag::FromBuffer(&bio, true/*bOptACP*/);
			tags.push_back(tag);
		}
		CSearchManager::ProcessResult(target, answer, &tags);
	  }catch(...){
		  //ASSERT(0);
		  for (TagPtrList::const_iterator it = tags.begin(); it != tags.end(); ++it)
			delete *it;
		  throw;
	  }
		count--;
		for (TagPtrList::const_iterator it = tags.begin(); it != tags.end(); ++it)
			delete *it;
	}
}


// KADEMLIA_SEARCH_RES
// Used in Kad1.0 only
void CKademliaUDPListener::ProcessSearchResponse(const uint8_t *packetData, uint32_t lenPacket)
{
	// Verify packet is expected size
	CHECK_PACKET_MIN_SIZE(37);

	CBuffer bio(packetData, lenPacket, true);
	ProcessSearchResponse(bio);
}

// KADEMLIA2_SEARCH_RES
// Used in Kad2.0 only
void CKademliaUDPListener::Process2SearchResponse(const uint8_t *packetData, uint32_t lenPacket, const CKadUDPKey& senderKey)
{
	CBuffer bio(packetData, lenPacket, true);

	// Who sent this packet.
	CUInt128().Read(&bio); // value unused

	ProcessSearchResponse(bio);
}

// KADEMLIA2_PUBLISH_KEY_REQ
// Used in Kad2.0 only
void CKademliaUDPListener::Process2PublishKeyRequest(const uint8_t *packetData, uint32_t lenPacket, uint32_t ip, uint16_t port, const CKadUDPKey& senderKey)
{
	//Used Pointers
	CIndexed *indexed = CKademlia::GetIndexed();

	// check if we are UDP firewalled
	if (CUDPFirewallTester::IsFirewalledUDP(true)) {
		//We are firewalled. We should not index this entry and give publisher a false report.
		return;
	}

	CBuffer bio(packetData, lenPacket, true);
	CUInt128 file;
	file.Read(&bio);

	CUInt128 distance(CKademlia::GetPrefs()->GetKadID());
	distance.XOR(file);

	if (distance.Get32BitChunk(0) > SEARCHTOLERANCE && !IsLanIP(ip)) {
		return;
	}

	uint16_t count = bio.ReadValue<uint16_t>();
	uint8_t load = 0;
	while (count > 0) {

		CUInt128 target;
		target.Read(&bio);

		Kademlia::CKeyEntry* entry = new Kademlia::CKeyEntry();
		try
		{
			entry->m_uIP = ip;
			entry->m_uUDPport = port;
			entry->m_uKeyID.SetValue(file);
			entry->m_uSourceID.SetValue(target);
			entry->m_tLifeTime = (uint32_t)time(NULL) + KADEMLIAREPUBLISHTIMEK;
			entry->m_bSource = false;
			uint32_t tags = bio.ReadValue<uint8_t>();
			while (tags > 0) {
				CTag *tag = CTag::FromBuffer(&bio);
				if (tag) {
					if (!tag->GetName().compare(TAG_FILENAME)) {
						if (entry->GetCommonFileName().empty()) {
							entry->SetFileName(tag->GetStr());
						}
						delete tag; // tag is no longer stored, but membervar is used
					} else if (!tag->GetName().compare(TAG_FILESIZE)) {
						if (entry->m_uSize == 0) {
							if (tag->IsBsob() && tag->GetBsobSize() == 8) {
								CBuffer Buffer(tag->GetBsob(),8,true);
								entry->m_uSize = Buffer.ReadValue<uint64_t>();
							} else {
								entry->m_uSize = tag->GetInt();
							}
						}
						delete tag; // tag is no longer stored, but membervar is used
					}
					else if (!tag->GetName().compare(TAG_KADAICHHASHPUB)) {
						if(tag->IsBsob() && tag->GetBsobSize() == 20) {
							if (entry->GetAICHHashCount() == 0) {
								entry->AddRemoveAICHHash(tag->GetBsob(), true);
							} else
								LogKadLine(LOG_DEBUG /*logKadSearch*/, L"Multiple TAG_KADAICHHASHPUB tags received for single file from %s",IPToStr(ip).c_str());
						} else
							LogKadLine(LOG_DEBUG /*logKadSearch*/, L"Bad TAG_KADAICHHASHPUB received from %s",IPToStr(ip).c_str());
						delete tag;
					}
					else {
						//TODO: Filter tags
						entry->AddTag(tag);
					}
				}
				tags--;
			}
		} catch(...) {
			ASSERT(0);
			delete entry;
			throw;
		}

		if (!indexed->AddKeyword(file, target, entry, load)) {
			//We already indexed the maximum number of keywords.
			//We do not index anymore but we still send a success..
			//Reason: Because if a VERY busy node tells the publisher it failed,
			//this busy node will spread to all the surrounding nodes causing popular
			//keywords to be stored on MANY nodes..
			//So, once we are full, we will periodically clean our list until we can
			//begin storing again..
			delete entry;
			entry = NULL;
		}
		count--;
	}
	CBuffer packetdata(17);
	file.Write(&packetdata);;
	packetdata.WriteValue<uint8_t>(load);
	DebugSend(L"Kad2PublishRes", ip, port);
	SendPacket(packetdata, KADEMLIA2_PUBLISH_RES, ip, port, senderKey, NULL);
}

// KADEMLIA2_PUBLISH_SOURCE_REQ
// Used in Kad2.0 only
void CKademliaUDPListener::Process2PublishSourceRequest(const uint8_t *packetData, uint32_t lenPacket, uint32_t ip, uint16_t port, const CKadUDPKey& senderKey)
{
	//Used Pointers
	CIndexed *indexed = CKademlia::GetIndexed();

	// check if we are UDP firewalled
	if (CUDPFirewallTester::IsFirewalledUDP(true)) {
		//We are firewalled. We should not index this entry and give publisher a false report.
		return;
	}

	CBuffer bio(packetData, lenPacket, true);
	CUInt128 file;
	file.Read(&bio);

	CUInt128 distance(CKademlia::GetPrefs()->GetKadID());
	distance.XOR(file);

	if (distance.Get32BitChunk(0) > SEARCHTOLERANCE && !IsLanIP(ip)) {
		return;
	}

	uint8_t load = 0;
	bool flag = false;
	CUInt128 target;
	target.Read(&bio);
	Kademlia::CEntry* entry = new Kademlia::CEntry();
	try {
		entry->m_uIP = ip;
		entry->m_uUDPport = port;
		entry->m_uKeyID.SetValue(file);
		entry->m_uSourceID.SetValue(target);
		entry->m_bSource = false;
		entry->m_tLifeTime = (uint32_t)time(NULL) + KADEMLIAREPUBLISHTIMES;
		bool addUDPPortTag = true;
		uint32_t tags = bio.ReadValue<uint8_t>();
		while (tags > 0) {
			CTag *tag = CTag::FromBuffer(&bio);
			if (tag) {
				if (!tag->GetName().compare(TAG_SOURCETYPE)) {
					if (entry->m_bSource == false) {
						entry->AddTag(new CTagVarInt(TAG_SOURCEIP, entry->m_uIP));
						entry->AddTag(tag);
						entry->m_bSource = true;
					} else {
						//More than one sourcetype tag found.
						delete tag;
					}
				} else if (!tag->GetName().compare(TAG_FILESIZE)) {
					if (entry->m_uSize == 0) {
						if (tag->IsBsob() && tag->GetBsobSize() == 8) {
							CBuffer Buffer(tag->GetBsob(),8,true);
							entry->m_uSize = Buffer.ReadValue<uint64_t>();
						} else {
							entry->m_uSize = tag->GetInt();
						}
					}
					delete tag;
				} else if (!tag->GetName().compare(TAG_SOURCEPORT)) {
					if (entry->m_uTCPport == 0) {
						entry->m_uTCPport = (uint16_t)tag->GetInt();
						entry->AddTag(tag);
					} else {
						//More than one port tag found
						delete tag;
					}
				} else if (!tag->GetName().compare(TAG_SOURCEUPORT)) {
					if (addUDPPortTag && tag->IsInt() && tag->GetInt() != 0) {
						entry->m_uUDPport = (uint16_t)tag->GetInt();
						entry->AddTag(tag);
						addUDPPortTag = false;
					} else {
						//More than one udp port tag found
						delete tag;
					}
				} else {
					//TODO: Filter tags
					entry->AddTag(tag);
				}
			}
			tags--;
		}
		if (addUDPPortTag) {
			entry->AddTag(new CTagVarInt(TAG_SOURCEUPORT, entry->m_uUDPport));
		}
	} catch(...) {
		ASSERT(0);
		delete entry;
		throw;
	}

	if (entry->m_bSource == true) {
		if (indexed->AddSources(file, target, entry, load)) {
			flag = true;
		} else {
			delete entry;
			entry = NULL;
		}
	} else {
		delete entry;
		entry = NULL;
	}
	if (flag) {
		CBuffer packetdata(17);
		file.Write(&packetdata);;
		packetdata.WriteValue<uint8_t>(load);
		DebugSend(L"Kad2PublishRes", ip, port);
		SendPacket(packetdata, KADEMLIA2_PUBLISH_RES, ip, port, senderKey, NULL);
	}
}

// KADEMLIA_PUBLISH_RES
// Used only by Kad1.0
void CKademliaUDPListener::ProcessPublishResponse(const uint8_t *packetData, uint32_t lenPacket, uint32_t ip)
{
	// Verify packet is expected size
	CHECK_PACKET_MIN_SIZE(16);
	CHECK_TRACKED_PACKET(KADEMLIA_PUBLISH_REQ);

	CBuffer bio(packetData, lenPacket, true);
	CUInt128 file;
	file.Read(&bio);

	bool loadResponse = false;
	uint8_t load = 0;
	if (bio.GetLength() > bio.GetPosition()) {
		loadResponse = true;
		load = bio.ReadValue<uint8_t>();
	}

	CSearchManager::ProcessPublishResult(file, load, loadResponse);
}

// KADEMLIA2_PUBLISH_RES
// Used only by Kad2.0
void CKademliaUDPListener::Process2PublishResponse(const uint8_t *packetData, uint32_t lenPacket, uint32_t ip, uint16_t port, const CKadUDPKey& senderKey)
{
	if (!IsOnOutTrackList(ip, KADEMLIA2_PUBLISH_KEY_REQ) && !IsOnOutTrackList(ip, KADEMLIA2_PUBLISH_SOURCE_REQ) && !IsOnOutTrackList(ip, KADEMLIA2_PUBLISH_NOTES_REQ)) {
		throw CException(LOG_ERROR, L"***NOTE: Received unrequested response packet, size (%d) in %S", (int)lenPacket, __FUNCTION__);
	}
	CBuffer bio(packetData, lenPacket, true);
	CUInt128 file;
	file.Read(&bio);
	uint8_t load = bio.ReadValue<uint8_t>();
	CSearchManager::ProcessPublishResult(file, load, true);
	if (bio.GetLength() > bio.GetPosition()) {
		// for future use
		uint8_t options = bio.ReadValue<uint8_t>();
		bool requestACK = (options & 0x01) > 0;
		if (requestACK && !senderKey.IsEmpty()) {
			DebugSend(L"Kad2PublishResAck", ip, port);
			SendNullPacket(KADEMLIA2_PUBLISH_RES_ACK, ip, port, senderKey, NULL);
		}
	}
}

// KADEMLIA2_SEARCH_NOTES_REQ
// Used only by Kad2.0
void CKademliaUDPListener::Process2SearchNotesRequest(const uint8_t *packetData, uint32_t lenPacket, uint32_t ip, uint16_t port, const CKadUDPKey& senderKey)
{
	CBuffer bio(packetData, lenPacket, true);
	CUInt128 target;
	target.Read(&bio);
	uint64_t fileSize = bio.ReadValue<uint64_t>();
	CKademlia::GetIndexed()->SendValidNoteResult(target, ip, port, fileSize, senderKey);
}

// KADEMLIA_SEARCH_NOTES_RES
// Used only by Kad1.0
void CKademliaUDPListener::ProcessSearchNotesResponse(const uint8_t *packetData, uint32_t lenPacket, uint32_t ip)
{
	// Verify packet is expected size
	CHECK_PACKET_MIN_SIZE(37);
	CHECK_TRACKED_PACKET(KADEMLIA_SEARCH_NOTES_REQ);

	// What search does this relate to
	CBuffer bio(packetData, lenPacket, true);
	ProcessSearchResponse(bio);
}

// KADEMLIA2_PUBLISH_NOTES_REQ
// Used only by Kad2.0
void CKademliaUDPListener::Process2PublishNotesRequest(const uint8_t *packetData, uint32_t lenPacket, uint32_t ip, uint16_t port, const CKadUDPKey& senderKey)
{
	// check if we are UDP firewalled
	if (CUDPFirewallTester::IsFirewalledUDP(true)) {
		//We are firewalled. We should not index this entry and give publisher a false report.
		return;
	}

	CBuffer bio(packetData, lenPacket, true);
	CUInt128 target;
	target.Read(&bio);

	CUInt128 distance(CKademlia::GetPrefs()->GetKadID());
	distance.XOR(target);

	if (distance.Get32BitChunk(0) > SEARCHTOLERANCE && !IsLanIP(ip)) {
		return;
	}

	CUInt128 source;
	source.Read(&bio);

	Kademlia::CEntry* entry = new Kademlia::CEntry();
	try {
		entry->m_uIP = ip;
		entry->m_uUDPport = port;
		entry->m_uKeyID.SetValue(target);
		entry->m_uSourceID.SetValue(source);
		entry->m_bSource = false;
		uint32_t tags = bio.ReadValue<uint8_t>();
		while (tags > 0) {
			CTag *tag = CTag::FromBuffer(&bio);
			if(tag) {
				if (!tag->GetName().compare(TAG_FILENAME)) {
					if (entry->GetCommonFileName().empty()) {
						entry->SetFileName(tag->GetStr());
					}
					delete tag;
				} else if (!tag->GetName().compare(TAG_FILESIZE)) {
					if (entry->m_uSize == 0) {
						entry->m_uSize = tag->GetInt();
					}
					delete tag;
				} else {
					//TODO: Filter tags
					entry->AddTag(tag);
				}
			}
			tags--;
		}
	} catch(...) {
		ASSERT(0);
		delete entry;
		entry = NULL;
		throw;
	}

	uint8_t load = 0;
	if (CKademlia::GetIndexed()->AddNotes(target, source, entry, load)) {
		CBuffer packetdata(17);
		target.Write(&packetdata);;
		packetdata.WriteValue<uint8_t>(load);
		DebugSend(L"Kad2PublishRes", ip, port);
		SendPacket(packetdata, KADEMLIA2_PUBLISH_RES, ip, port, senderKey, NULL);
	} else {
		delete entry;
	}
}

// KADEMLIA_FIREWALLED_REQ
// Used by Kad1.0 and Kad2.0
void CKademliaUDPListener::ProcessFirewalledRequest(const uint8_t *packetData, uint32_t lenPacket, uint32_t ip, uint16_t port, const CKadUDPKey& senderKey)
{
	// Verify packet is expected size
	CHECK_PACKET_EXACT_SIZE(2);

	CBuffer bio(packetData, lenPacket, true);
	uint16_t tcpport = bio.ReadValue<uint16_t>();

	CUInt128 zero;
	CContact contact(zero, ip, port, tcpport, 0, 0, false, zero);
	if (!CKadHandler::Instance()->IssueFirewallCheck(contact, 0)) {
		return; // cancelled for some reason, don't send a response
	}

	// Send response
	CBuffer packetdata(4);
	packetdata.WriteValue<uint32_t>(ip);
	DebugSend(L"KadFirewalledRes", ip, port);
	SendPacket(packetdata, KADEMLIA_FIREWALLED_RES, ip, port, senderKey, NULL);
}

// KADEMLIA_FIREWALLED2_REQ
// Used by Kad2.0 Prot.Version 7+
void CKademliaUDPListener::ProcessFirewalled2Request(const uint8_t *packetData, uint32_t lenPacket, uint32_t ip, uint16_t port, const CKadUDPKey& senderKey)
{
	// Verify packet is expected size
	CHECK_PACKET_MIN_SIZE(19);

	CBuffer bio(packetData, lenPacket, true);
	uint16_t tcpPort = bio.ReadValue<uint16_t>();
	CUInt128 userID;
	userID.Read(&bio);
	uint8_t connectOptions = bio.ReadValue<uint8_t>();

	CUInt128 zero;
	CContact contact(userID, ip, port, tcpPort, 0, 0, false, zero);
	if (!CKadHandler::Instance()->IssueFirewallCheck(contact, connectOptions)) {
		return; // cancelled for some reason, don't send a response
	}

	// Send response
	CBuffer packetdata(4);
	packetdata.WriteValue<uint32_t>(ip);
	DebugSend(L"KadFirewalledRes", ip, port);
	SendPacket(packetdata, KADEMLIA_FIREWALLED_RES, ip, port, senderKey, NULL);
}

// KADEMLIA_FIREWALLED_RES
// Used by Kad1.0 and Kad2.0
void CKademliaUDPListener::ProcessFirewalledResponse(const uint8_t *packetData, uint32_t lenPacket, uint32_t ip, const CKadUDPKey& senderKey)
{
	// Verify packet is expected size
	CHECK_PACKET_EXACT_SIZE(4);

	if (!CKadHandler::Instance()->IsKadFirewallCheckIP(ip)) { /* KADEMLIA_FIREWALLED2_REQ + KADEMLIA_FIREWALLED_REQ */
		throw CException(LOG_ERROR, L"Received unrequested firewall response packet in %S", __FUNCTION__);
	}

	CBuffer bio(packetData, lenPacket, true);
	uint32_t firewalledIP = bio.ReadValue<uint32_t>();

	// Update con state only if something changes.
	if (CKademlia::GetPrefs()->GetIPAddress() != firewalledIP) {
		CKademlia::GetPrefs()->SetIPAddress(firewalledIP);
	}
	CKademlia::GetPrefs()->IncRecheckIP();
}

// KADEMLIA_FIREWALLED_ACK_RES
// Used by Kad1.0 and Kad2.0
void CKademliaUDPListener::ProcessFirewalledAckResponse(uint32_t lenPacket)
{
	// Deprecated since KadVersion 7+, the result is now sent per TCP instead of UDP, because this will fail if our intern UDP port is unreachable.
	// But we want the TCP testresult regardless if UDP is firewalled, the new UDP state and test takes care of the rest.

	// Verify packet is expected size
	CHECK_PACKET_EXACT_SIZE(0);

	CKademlia::GetPrefs()->IncFirewalled();
}

// KADEMLIA_FINDBUDDY_REQ
// Used by Kad1.0 and Kad2.0
void CKademliaUDPListener::ProcessFindBuddyRequest(const uint8_t *packetData, uint32_t lenPacket, uint32_t ip, uint16_t port, const CKadUDPKey& senderKey)
{
	// Verify packet is expected size
	CHECK_PACKET_MIN_SIZE(34);

	if (CKademlia::GetPrefs()->GetFirewalled() || CUDPFirewallTester::IsFirewalledUDP(true) || !CUDPFirewallTester::IsVerified()) {
		// We are firewalled but somehow we still got this packet.. Don't send a response..
		return;
	} else if (CKadHandler::Instance()->HasBuddy()) {
		// we already have a buddy
		return;
	}

	CBuffer bio(packetData, lenPacket, true);
	CUInt128 BuddyID;
	BuddyID.Read(&bio);
	CUInt128 userID;
	userID.Read(&bio);
	uint16_t tcpport = bio.ReadValue<uint16_t>();

	CUInt128 zero;
	CContact contact(userID, ip, port, tcpport, 0, 0, false, zero);
	if (!CKadHandler::Instance()->IncomingBuddy(&contact, &BuddyID)) {
		return; // cancelled for some reason, don't send a response
	}

	CBuffer packetdata(34);
	BuddyID.Write(&packetdata);
	CKadHandler::Instance()->GetEd2kHash().Write(&packetdata);
	packetdata.WriteValue<uint16_t>(CKadHandler::Instance()->GetTCPPort());
	if (!senderKey.IsEmpty()) { // remove check for later versions
		packetdata.WriteValue<uint8_t>(CKademlia::GetPrefs()->GetMyConnectOptions(true, false)); // new since 0.49a, old mules will ignore it  (hopefully ;) )
	}

	DebugSend(L"KadFindBuddyRes", ip, port);
	SendPacket(packetdata, KADEMLIA_FINDBUDDY_RES, ip, port, senderKey, NULL);
}

// KADEMLIA_FINDBUDDY_RES
// Used by Kad1.0 and Kad2.0
void CKademliaUDPListener::ProcessFindBuddyResponse(const uint8_t *packetData, uint32_t lenPacket, uint32_t ip, uint16_t port, const CKadUDPKey& senderKey)
{
	// Verify packet is expected size
	CHECK_PACKET_MIN_SIZE(34);
	CHECK_TRACKED_PACKET(KADEMLIA_FINDBUDDY_REQ);

	CBuffer bio(packetData, lenPacket, true);
	CUInt128 check;
	check.Read(&bio);
	check.XOR(CUInt128(true));
	if (CKademlia::GetPrefs()->GetKadID() == check) {
		CUInt128 userID;
		userID.Read(&bio);
		uint16_t tcpport = bio.ReadValue<uint16_t>();
		uint8_t connectOptions = 0;
		if (lenPacket > 34) {
			// 0.49+ (kad version 7) sends additionally its connect options so we know whether to use an obfuscated connection
			connectOptions = bio.ReadValue<uint8_t>();
		}

		CUInt128 zero;
		CContact contact(userID, ip, port, tcpport, 0, 0, false, zero);
		CKadHandler::Instance()->RequestBuddy(&contact, connectOptions);
	}
}

// KADEMLIA_CALLBACK_REQ
// Used by Kad1.0 and Kad2.0
void CKademliaUDPListener::ProcessCallbackRequest(const uint8_t *packetData, uint32_t lenPacket, uint32_t ip, uint16_t port, const CKadUDPKey& senderKey)
{
	// Verify packet is expected size
	CHECK_PACKET_MIN_SIZE(34);

	if (CKadHandler::Instance()->HasBuddy()) {
		CBuffer bio(packetData, lenPacket, true);
		CUInt128 check;
		check.Read(&bio);
		// JOHNTODO: Begin filtering bad buddy ID's..
		// CUInt128 bud(buddy->GetBuddyID());
		CUInt128 file;
		file.Read(&bio);
		uint16_t tcp = bio.ReadValue<uint16_t>();

		CKadHandler::Instance()->RelayKadCallback(ip, tcp, check, file);
	}
}

// KADEMLIA2_PING
void CKademliaUDPListener::Process2Ping(uint32_t ip, uint16_t port, const CKadUDPKey& senderKey)
{
	// can be used just as PING, currently it is however only used to determine one's external port
	CBuffer packetdata(2);
	packetdata.WriteValue<uint16_t>(port);
	DebugSend(L"Kad2Pong", ip, port);
	SendPacket(packetdata, KADEMLIA2_PONG, ip, port, senderKey, NULL); 
}

// KADEMLIA2_PONG
void CKademliaUDPListener::Process2Pong(const uint8_t *packetData, uint32_t lenPacket, uint32_t ip)
{
	CHECK_PACKET_MIN_SIZE(2);
	CHECK_TRACKED_PACKET(KADEMLIA2_PING);

	// Is this one of our legacy challenge packets?
	CUInt128 contactID;
	if (IsLegacyChallenge(CUInt128((uint32_t)0), ip, KADEMLIA2_PING, contactID)) {
		// yup it is, set the contact as verified
		if (!CKademlia::GetRoutingZone()->VerifyContact(contactID, ip)) {
			LogKadLine(LOG_DEBUG /*logKadRouting*/, L"Unable to find valid sender in routing table (sender: %s)", IPToStr(ip).c_str());
		} else {
			LogKadLine(LOG_DEBUG /*logKadRouting*/, L"Verified contact with legacy challenge (Kad2Ping) - %s", IPToStr(ip).c_str());
		}
		return;	// we do not actually care for its other content
	}

	if (CKademlia::GetPrefs()->FindExternKadPort(false)) {
		// the reported port doesn't always have to be our true external port, esp. if we used our intern port
		// and communicated recently with the client some routers might remember this and assign the intern port as source
		// but this shouldn't be a problem because we prefer intern ports anyway.
		// might have to be reviewed in later versions when more data is available
		CBuffer Buffer(packetData,2,true);
		CKademlia::GetPrefs()->SetExternKadPort(Buffer.ReadValue<uint16_t>(), ip);

		if (CUDPFirewallTester::IsFWCheckUDPRunning()) {
			CUDPFirewallTester::QueryNextClient();
		}
	}
}

// KADEMLIA2_FIREWALLUDP
void CKademliaUDPListener::Process2FirewallUDP(const uint8_t *packetData, uint32_t lenPacket, uint32_t ip)
{
	// Verify packet is expected size
	CHECK_PACKET_MIN_SIZE(3);

	CBuffer Buffer(packetData,3,true);
	uint8_t errorCode = Buffer.ReadValue<uint8_t>();
	uint16_t incomingPort = Buffer.ReadValue<uint16_t>();
	if ((incomingPort != CKademlia::GetPrefs()->GetExternalKadPort() && incomingPort != CKadHandler::Instance()->GetKadPort(true)) || incomingPort == 0) {
		LogKadLine(LOG_DEBUG /*logClientKadUDP*/, L"Received UDP FirewallCheck on unexpected incoming port %u from %s", incomingPort, IPToStr(ip).c_str());
		CUDPFirewallTester::SetUDPFWCheckResult(false, true, ip, 0);
	} else if (errorCode == 0) {
		LogKadLine(LOG_DEBUG /*logClientKadUDP*/, L"Received UDP FirewallCheck packet from %s with incoming port %u", IPToStr(ip).c_str(), incomingPort);
		CUDPFirewallTester::SetUDPFWCheckResult(true, false, ip, incomingPort);
	} else {
		LogKadLine(LOG_DEBUG /*logClientKadUDP*/, L"Received UDP FirewallCheck packet from %s with incoming port %u with remote errorcode %u - ignoring result"
			, IPToStr(ip).c_str(), incomingPort, errorCode);
		CUDPFirewallTester::SetUDPFWCheckResult(false, true, ip, 0);
	}
}

void CKademliaUDPListener::SendPacket(const CBuffer &data, uint8_t opcode, uint32_t destinationHost, uint16_t destinationPort, const CKadUDPKey& targetKey, const CUInt128* cryptTargetID)
{
	AddTrackedOutPacket(destinationHost, opcode);
	CPacket* packet = new CPacket(data, OP_KADEMLIAHEADER, opcode);
	if (packet->GetPacketSize() > 200) {
		packet->PackPacket();
	}
	uint8_t cryptData[16];
	uint8_t *cryptKey;
	if (cryptTargetID != NULL) {
		cryptKey = (uint8_t *)&cryptData;
		cryptTargetID->StoreCryptValue(cryptKey);
	} else {
		cryptKey = NULL;
	}
	CKadHandler::Instance()->SendPacket(packet, destinationHost, destinationPort, true, cryptKey, true, targetKey.GetKeyValue(ntohl(CKadHandler::Instance()->GetPublicIP())));
}

bool CKademliaUDPListener::FindNodeIDByIP(CKadClientSearcher* requester, uint32_t ip, uint16_t tcpPort, uint16_t udpPort)
{
	// send a hello packet to the given IP in order to get a HELLO_RES with the NodeID
	LogKadLine(LOG_DEBUG /*logClientKadUDP*/, L"FindNodeIDByIP: Requesting NodeID from %s by sending Kad2HelloReq", IPToStr(ip).c_str());
	DebugSend(L"Kad2HelloReq", ip, udpPort);
	SendMyDetails(KADEMLIA2_HELLO_REQ, ip, udpPort, 1, 0, NULL, false); // todo: we send this unobfuscated, which is not perfect, see this can be avoided in the future
	FetchNodeID_Struct sRequest = { ip, tcpPort, GetCurTick() + SEC2MS(60), requester };
	m_fetchNodeIDRequests.push_back(sRequest);
	return true;
}

void CKademliaUDPListener::ExpireClientSearch(CKadClientSearcher* expireImmediately)
{
	uint32_t now = GetCurTick();
	for (FetchNodeIDList::iterator it = m_fetchNodeIDRequests.begin(); it != m_fetchNodeIDRequests.end();) {
		FetchNodeIDList::iterator it2 = it++;
		if (it2->requester == expireImmediately) {
			m_fetchNodeIDRequests.erase(it2);
		}
		else if (it2->expire < now) {
			it2->requester->KadSearchNodeIDByIPResult(KCSR_TIMEOUT, NULL);
			m_fetchNodeIDRequests.erase(it2);
		}
	}
}

void CKademliaUDPListener::SendLegacyChallenge(uint32_t ip, uint16_t port, const CUInt128& contactID)
{
	// We want to verify that a pre-0.49a contact is valid and not sent from a spoofed IP.
	// Because those versions don't support any direct validating, we send a KAD_REQ with a random ID,
	// which is our challenge. If we receive an answer packet for this request, we can be sure the
	// contact is not spoofed
#ifdef _DEBUG
	CContact* contact = CKademlia::GetRoutingZone()->GetContact(contactID);
	if (contact != NULL) {
		if (contact->GetType() < 2) {
			LogKadLine(LOG_DEBUG /*logKadRouting*/, L"Sending challenge to a long known contact (should be verified already) - %s", IPToStr(ip).c_str());
		}
	} else {
		ASSERT(0);
	}
#endif

	if (HasActiveLegacyChallenge(ip)) {
		// don't send more than one challenge at a time
		return;
	}
	CBuffer packetdata(33);
	packetdata.WriteValue<uint8_t>(KADEMLIA_FIND_VALUE);
	CUInt128 challenge;
	challenge.FillRandom();
	if (challenge == 0) {
		// hey there is a 2^128 chance that this happens ;)
		ASSERT(0);
		challenge = 1;
	}
	// Put the target we want into the packet. This is our challenge
	challenge.Write(&packetdata);
	// Add the ID of the contact we are contacting for sanity checks on the other end.
	contactID.Write(&packetdata);
	DebugSend(L"Kad2Req(SendLegacyChallenge)", ip, port);
	// those versions we send those requests to don't support encryption / obfuscation
	SendPacket(packetdata, KADEMLIA2_REQ, ip, port, 0, NULL);
	AddLegacyChallenge(contactID, challenge, ip, KADEMLIA2_REQ);
}

// File_checked_for_headers
