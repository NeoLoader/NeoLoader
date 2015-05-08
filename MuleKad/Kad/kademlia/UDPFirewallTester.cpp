//
// This file is part of the MuleKad Project.
//
// Copyright (c) 2012 David Xanatos ( XanatosDavid@googlemail.com )
// Copyright (c) 2008-2011 Dévai Tamás ( gonosztopi@amule.org )
// Copyright (c) 2008-2011 aMule Team ( admin@amule.org / http://www.amule.org )
// Copyright (c) 2002-2011 Merkur ( devs@emule-project.net / http://www.emule-project.net )
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
#include "GlobalHeader.h"
#include "UDPFirewallTester.h"
#include "../utils/UInt128.h"
#include "../utils/KadUDPKey.h"
#include "../routing/RoutingZone.h"
#include "Prefs.h"
#include "SearchManager.h"
#include "../KadHandler.h"
#include "../UDPSocket.h"

using namespace Kademlia;

bool	CUDPFirewallTester::m_firewalledUDP				= false;
bool	CUDPFirewallTester::m_firewalledLastStateUDP	= false;
bool	CUDPFirewallTester::m_isFWVerifiedUDP			= false;
bool	CUDPFirewallTester::m_nodeSearchStarted			= false;
bool	CUDPFirewallTester::m_timedOut					= false;
uint8_t	CUDPFirewallTester::m_fwChecksRunningUDP		= 0;
uint8_t	CUDPFirewallTester::m_fwChecksFinishedUDP		= 0;
uint32_t CUDPFirewallTester::m_testStart				= 0;
uint32_t CUDPFirewallTester::m_lastSucceededTime		= 0;
CUDPFirewallTester::PossibleClientList	CUDPFirewallTester::m_possibleTestClients;
CUDPFirewallTester::UsedClientList	CUDPFirewallTester::m_usedTestClients;


bool CUDPFirewallTester::IsFirewalledUDP(bool lastStateIfTesting)
{
	if (CKademlia::IsRunningInLANMode()) {
		return false;
	}
	if (!m_timedOut && IsFWCheckUDPRunning()) {
		if (!m_firewalledUDP && CKademlia::IsFirewalled() && m_testStart != 0 && GetCurTick() - m_testStart > MIN2MS(6)
			&& !m_isFWVerifiedUDP /*For now we don't allow to get firewalled by timeouts if we have succeded a test before, might be changed later*/)
		{
			LogKadLine(LOG_DEBUG /*logKadUdpFwTester*/, L"Timeout: Setting UDP status to firewalled after being unable to get results for 6 minutes");
			m_timedOut = true;
		}
	}
	else if (m_timedOut && IsFWCheckUDPRunning()) {
		return true; // firewallstate by timeout
	}
	else if (m_timedOut) {
		ASSERT(0);
	}

	if (lastStateIfTesting && IsFWCheckUDPRunning()) {
		return m_firewalledLastStateUDP;
	} else {
		return m_firewalledUDP;
	}
}

void CUDPFirewallTester::SetUDPFWCheckResult(bool succeeded, bool testCancelled, uint32_t fromIP, uint16_t incomingPort)
{
	// can be called on shutdown after KAD has been stopped
	if (!CKademlia::IsRunning()) {
		return;
	}

	// check if we actually requested a firewallcheck from this client
	bool requested = false;
	for (UsedClientList::iterator it = m_usedTestClients.begin(); it != m_usedTestClients.end(); ++it) {
		if (it->contact.GetIPAddress() == fromIP) {
			if (!IsFWCheckUDPRunning() && !m_firewalledUDP && m_isFWVerifiedUDP && m_lastSucceededTime + SEC2MS(10) > GetCurTick()
			    && incomingPort == CKadHandler::Instance()->GetKadPort(true) && CKademlia::GetPrefs()->GetUseExternKadPort()) {
				// our test finished already in the last 10 seconds with being open because we received a proper result packet before
				// however we now receive another answer packet on our incoming port (which is not unusal as both resultpackets are sent
				// nearly at the same time and UDP doesn't cares if the order stays), while the one before was received on our extern port
				// Because a proper forwarded intern port is more reliable to stay open than an extern port set by the NAT, we prefer
				// intern ports and change the setting.
				CKademlia::GetPrefs()->SetUseExternKadPort(false);
				LogKadLine(LOG_DEBUG /*logKadUdpFwTester*/, L"Corrected UDP firewall result: Using open internal (%u) instead of open external port", incomingPort);
				return;
			} else if (it->answered) {
				// we already received an answer. This may happen since all tests contain of two answer packets,
				// but the answer could also be too late and we already counted it as failure.
				return;
			} else {
				it->answered = true;
			}
			requested = true;
			break;
		}
	}

	if (!requested){
		LogKadLine(LOG_DEBUG /*logKadUdpFwTester*/, L"Unrequested UDPFWCheckResult from %s", IPToStr(fromIP).c_str());
		return;
	}

	if (!IsFWCheckUDPRunning()) {
		// it's all over already
		return;
	}

	if (m_fwChecksRunningUDP == 0) {
		ASSERT(0);
	} else {
		m_fwChecksRunningUDP--;
	}

	if (!testCancelled){
		m_fwChecksFinishedUDP++;
		if (succeeded) {	//one positive result is enough
			m_testStart = 0;
			m_firewalledUDP = false;
			m_isFWVerifiedUDP = true;
			m_timedOut = false;
			m_fwChecksFinishedUDP = UDP_FIREWALLTEST_CLIENTSTOASK; // don't do any more tests
			m_fwChecksRunningUDP = 0; // all other tests are cancelled
			m_possibleTestClients.clear(); // clear list, keep used clients list though
			CSearchManager::CancelNodeFWCheckUDPSearch(); // cancel firewallnode searches if any are still active
			// if this packet came to our internal port, explict set the interal port as used port from now on
			if (incomingPort == CKadHandler::Instance()->GetKadPort(true)) {
				CKademlia::GetPrefs()->SetUseExternKadPort(false);
				LogKadLine(LOG_DEBUG /*logKadUdpFwTester*/, L"New Kad Firewallstate (UDP): Open, using intern port");
			} else if (incomingPort == CKademlia::GetPrefs()->GetExternalKadPort() && incomingPort != 0) {
				CKademlia::GetPrefs()->SetUseExternKadPort(true);
				LogKadLine(LOG_DEBUG /*logKadUdpFwTester*/, L"New Kad Firewallstate (UDP): Open, using extern port");
			}
			return;
		} else if (m_fwChecksFinishedUDP >= UDP_FIREWALLTEST_CLIENTSTOASK) {
			// seems we are firewalled
			m_testStart = 0;
			LogKadLine(LOG_DEBUG /*logKadUdpFwTester*/, L"New KAD Firewallstate (UDP): Firewalled");
			m_firewalledUDP = true;
			m_isFWVerifiedUDP = true;
			m_timedOut = false;
			m_possibleTestClients.clear(); // clear list, keep used clients list though
			CSearchManager::CancelNodeFWCheckUDPSearch(); // cancel firewallnode searches if any are still active
			return;
		} else {
			LogKadLine(LOG_DEBUG /*logKadUdpFwTester*/, L"Kad UDP firewalltest from %s result: Firewalled, continue testing", IPToStr(fromIP).c_str());
		}
	} else {
		LogKadLine(LOG_DEBUG /*logKadUdpFwTester*/, L"Kad UDP firewalltest from %s cancelled", IPToStr(fromIP).c_str());
	}
	QueryNextClient();
}

void CUDPFirewallTester::ReCheckFirewallUDP(bool setUnverified)
{
	ASSERT(m_fwChecksRunningUDP == 0);
	m_fwChecksRunningUDP = 0;
	m_fwChecksFinishedUDP = 0;
	m_lastSucceededTime = 0;
	m_testStart = GetCurTick();
	m_timedOut = false;
	m_firewalledLastStateUDP = m_firewalledUDP;
	m_isFWVerifiedUDP = (m_isFWVerifiedUDP && !setUnverified);
	CSearchManager::FindNodeFWCheckUDP(); // start a lookup for a random node to find suitable IPs
	m_nodeSearchStarted = true;
	CKademlia::GetPrefs()->FindExternKadPort(true);
}

void CUDPFirewallTester::Connected()
{
	if (!m_nodeSearchStarted && IsFWCheckUDPRunning()) {
		if(CKadHandler::Instance()->GetTCPPort() != 0)
			CSearchManager::FindNodeFWCheckUDP(); // start a lookup for a random node to find suitable IPs
		m_nodeSearchStarted = true;
		m_testStart = GetCurTick();
		m_timedOut = false;
	}
}

void CUDPFirewallTester::Reset()
{
	m_firewalledUDP = false;
	m_firewalledLastStateUDP = false;
	m_isFWVerifiedUDP = false;
	m_nodeSearchStarted = false;
	m_timedOut = false;
	m_fwChecksRunningUDP = 0;
	m_fwChecksFinishedUDP = 0;
	m_testStart = 0;
	m_lastSucceededTime = 0;
	CSearchManager::CancelNodeFWCheckUDPSearch(); // cancel firewallnode searches if any are still active
	m_possibleTestClients.clear();
	CKademlia::GetPrefs()->SetUseExternKadPort(true);
	// keep the list of used clients
}

void CUDPFirewallTester::QueryNextClient()
{	// try the next available client for the firewallcheck
	if (!IsFWCheckUDPRunning() || !GetUDPCheckClientsNeeded() || CKademlia::GetPrefs()->FindExternKadPort(false)) {
		return; // check if more tests are needed and wait till we know our extern port
	}

	if (!CKademlia::IsRunning() || CKademlia::GetRoutingZone() == NULL) {
		ASSERT(0);
		return;
	}

	while (!m_possibleTestClients.empty()) {
		CContact curContact = m_possibleTestClients.front();
		m_possibleTestClients.pop_front();
		// udp firewallchecks are not supported by clients with kadversion < 6
		if (curContact.GetVersion() <= 5) {
			continue;
		}

		// sanity - do not test ourself
		if (curContact.GetIPAddress() == CKadHandler::Instance()->GetPublicIP() || curContact.GetClientID() == CKademlia::GetPrefs()->GetKadID()) {
			continue;
		}

		// check if we actually requested a firewallcheck from this client at some point
		bool alreadyRequested = false;
		for (UsedClientList::const_iterator it = m_usedTestClients.begin(); it != m_usedTestClients.end(); ++it) {
			if (it->contact.GetIPAddress() == curContact.GetIPAddress()) {
				alreadyRequested = true;
				break;
			}
		}

		// check if we know its IP already from kademlia - we need an IP which was never used for UDP yet
		if (!alreadyRequested && CKademlia::GetRoutingZone()->GetContact(curContact.GetIPAddress(), 0, false) == NULL) {
			// ok, tell the clientlist to do the same search and start the check if ok
			CContact tmpContact = curContact;
			tmpContact.SetClientID(CUInt128()); // we dont want to wrongly set the node ID as user hash
			if (CKadHandler::Instance()->IssueFirewallCheck(tmpContact, 0, true)) {
				UsedClient_Struct sAdd = { curContact, false };
				m_usedTestClients.push_front(sAdd);
				m_fwChecksRunningUDP++;
				break;
			}
		}
	}
}
