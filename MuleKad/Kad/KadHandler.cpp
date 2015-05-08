//
// This file is part of the MuleKad Project.
//
// Copyright (c) 2012 David Xanatos ( XanatosDavid@googlemail.com )
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
#include "Types.h"
#include "KadHandler.h"
#include "../../Framework/Buffer.h"
#include "kademlia/Prefs.h"
#include "kademlia/Kademlia.h"
#include "kademlia/Search.h"
#include "kademlia/Entry.h"
#include "kademlia/SearchManager.h"
#include "routing/Contact.h"
#include "routing/RoutingZone.h"
#include "UDPSocket.h"
#include "kademlia/UDPFirewallTester.h"
#include "utils/KadUDPKey.h"
#include "../Framework/Settings.h"
#include "../Framework/Xml.h"
#include "FileTags.h"
#include "KeywordPublisher.h"
#include "UDPSocket.h"
#include "Protocols.h"
#include "../../Framework/Exception.h"
#include "Packet.h"
#include "../../zlib/zlib.h"
#include "../MuleKad.h"

CKadHandler* CKadHandler::m_Instance = NULL;

CKadHandler::CKadHandler(const QByteArray& KadID, uint16_t ClientPort, const QByteArray& Ed2kHash)
{
	ASSERT(m_Instance == NULL);
	m_Instance = this;

	ASSERT(KadID.size() == 16);
	Kademlia::CUInt128 ID((byte*)KadID.data());
	m_KadPrefs = new Kademlia::CPrefs(ID);

	m_ClientPort = ClientPort;
	m_UDPPort = 0;

	if(!Ed2kHash.isEmpty())
	{
		ASSERT(Ed2kHash.size() == 16);
		m_Ed2kHash.SetValueBE((byte*)Ed2kHash.data());
	}

	m_LanMode = false;
	m_SupportsCryptLayer = true;
	m_RequestsCryptLayer = false;
	m_RequiresCryptLayer = false;
	m_SupportsNatTraversal = false;

	m_PublicIP = 0;
	m_Firewalled = true;

	m_BuddyIP = 0;
	m_BuddyPort = 0;

	m_currFileSrc = 0;
	m_currFileNotes = 0;
	m_currFileKey = 0;
	m_lastPublishKadSrc = 0;
	m_lastPublishKadNotes = 0;

	Kademlia::CKademlia::Start(m_KadPrefs);

	m_Keywords = new CPublishKeywordList();

	m_UDPSocket = NULL;

	LoadNodes();
}

CKadHandler::~CKadHandler()
{
	SaveNodes();

	Kademlia::CKademlia::Stop();

	for (map<Kademlia::CUInt128, SFileInfo*>::iterator I = m_FileList.begin(); I != m_FileList.end(); I++)
		delete I->second;

	for (map<uint32_t, SSearch*>::iterator I = m_Searches.begin(); I != m_Searches.end(); I++)
		delete I->second;

	delete m_Keywords;
	m_Instance = NULL;

	delete m_UDPSocket;
}

void CKadHandler::SetupProxy(uint16_t UDPPort, QObject* pProxy)
{
	ASSERT(m_UDPPort == 0);
	m_UDPPort = UDPPort;

	connect(this, SIGNAL(SendPacket(QByteArray, quint32, quint16, bool, QByteArray, quint32)), pProxy, SLOT(SendPacket(QByteArray, quint32, quint16, bool, QByteArray, quint32)));
	connect(pProxy, SIGNAL(ProcessPacket(QByteArray, quint32, quint16, bool, quint32)), this, SLOT(ProcessPacket(QByteArray, quint32, quint16, bool, quint32)));
}

void CKadHandler::SetupSocket(uint16_t UDPPort, uint32_t UDPKey)
{
	ASSERT(m_UDPPort == 0);
	m_UDPPort = UDPPort;

	m_UDPSocket = new CUDPSocket(UDPPort, UDPKey);
}

void CKadHandler::LoadNodes()
{
	QFile File(CSettings::GetSettingsDir() + "/nodes.dat");
	if(!File.open(QFile::ReadOnly))
		return;
	CBuffer Buffer(File.size());
	Buffer.SetSize(File.read((char*)Buffer.GetBuffer(), Buffer.GetLength()));
	File.close();

	LoadNodes(Buffer);
}

void CKadHandler::LoadNodes(CBuffer& Buffer)
{
	try
	{
		uint32_t numContacts = Buffer.ReadValue<uint32_t>();
		uint32_t fileVersion = 0;
		if (numContacts == 0) {
			fileVersion = Buffer.ReadValue<uint32_t>();
			if (fileVersion == 3) {
				uint32_t bootstrapEdition = Buffer.ReadValue<uint32_t>();
				if (bootstrapEdition == 1) {
					// this is a special bootstrap-only nodes.dat, handle it in a separate reading function
					return;
				}
			}
			if (fileVersion >= 1 && fileVersion <= 3) {
				numContacts = Buffer.ReadValue<uint32_t>();
			}
		}

		for (uint32_t i = 0; i < numContacts; i++) 
		{
			Kademlia::CUInt128 id;
			id.Read(&Buffer);
			uint32_t ip = Buffer.ReadValue<uint32_t>();
			uint16_t udpPort = Buffer.ReadValue<uint16_t>();
			uint16_t tcpPort = Buffer.ReadValue<uint16_t>();
			uint8_t contactVersion = 0;
			contactVersion = Buffer.ReadValue<uint8_t>();
			Kademlia::CKadUDPKey kadUDPKey;
			bool verified = false;
			if (fileVersion >= 2) {
				kadUDPKey.ReadFromFile(Buffer);
				verified = Buffer.ReadValue<uint8_t>() != 0;
			}
			// IP appears valid
			if (contactVersion > 1) 
			{
				// This was not a dead contact, inc counter if add was successful
				Kademlia::CKademlia::GetRoutingZone()->AddUnfiltered(id, ip, udpPort, tcpPort, contactVersion, kadUDPKey, verified, false, false);
			}
		}
	}
	catch(const CException&)
	{
	}
}

void CKadHandler::SaveNodes()
{
	if(!Kademlia::CKademlia::GetRoutingZone())
		return; // not connected

	CBuffer Buffer;

	Kademlia::ContactList contacts;
	Kademlia::CKademlia::GetRoutingZone()->GetBootstrapContacts(&contacts, 500);

	Buffer.WriteValue<uint32_t>(0); // old num set to 0
	Buffer.WriteValue<uint32_t>(2); // version
	Buffer.WriteValue<uint32_t>(contacts.size()); // entry count

	for (Kademlia::ContactList::iterator I = contacts.begin(); I != contacts.end(); I++)
	{
		Kademlia::CContact* Contact = *I;

		Contact->GetClientID().Write(&Buffer);
		Buffer.WriteValue<uint32_t>(Contact->GetIPAddress());
		Buffer.WriteValue<uint16_t>(Contact->GetUDPPort());
		Buffer.WriteValue<uint16_t>(Contact->GetTCPPort());
		Buffer.WriteValue<uint8_t>(Contact->GetVersion());
		Contact->GetUDPKey().StoreToFile(Buffer);
		Buffer.WriteValue<uint8_t>(Contact->IsIPVerified());
	}

	QFile File(CSettings::GetSettingsDir() + "/nodes.dat");
	if(!File.open(QFile::WriteOnly))
		return;
	File.write((char*)Buffer.GetBuffer(), Buffer.GetSize());
	File.close();
}

bool CKadHandler::IsRunning()
{
	return Kademlia::CKademlia::IsRunning();
}

void CKadHandler::Bootstrap(const QHostAddress& Address, uint16 Port)
{
	Kademlia::CKademlia::Bootstrap(Address.toIPv4Address(), Port);
}

void CKadHandler::Process()
{
	if(m_UDPSocket)
		m_UDPSocket->Process();

	if(!Kademlia::CKademlia::IsRunning())
		return;

	Kademlia::CKademlia::Process();
	if(Kademlia::CKademlia::GetPrefs()->HasLostConnection())
		Kademlia::CKademlia::Stop();
	
	// Buddy Handling
	if (m_ClientPort != 0 && Kademlia::CKademlia::IsConnected())
	{
		//we only need a buddy if direct callback is not available
		if(Kademlia::CKademlia::IsFirewalled() && Kademlia::CUDPFirewallTester::IsFirewalledUDP(true))
		{
			//TODO 0.49b: Kad buddies won'T work with RequireCrypt, so it is disabled for now but should (and will)
			//be fixed in later version
			// Update: Buddy connections itself support obfuscation properly since 0.49a (this makes it work fine if our buddy uses require crypt)
			// ,however callback requests don't support it yet so we wouldn't be able to answer callback requests with RequireCrypt, protocolchange intended for the next version
			if(!HasBuddy() && Kademlia::CKademlia::GetPrefs()->GetFindBuddy() && !RequiresCryptLayer())
			{
				//We are a firewalled client with no buddy. We have also waited a set time 
				//to try to avoid a false firewalled status.. So lets look for a buddy..
				Kademlia::CSearch* Search = Kademlia::CSearchManager::PrepareLookup(Kademlia::CSearch::FINDBUDDY, true, Kademlia::CUInt128(true).XOR(Kademlia::CKademlia::GetPrefs()->GetKadID()));
				if(Search)
				{
					LogKadLine(LOG_DEBUG ,L"Starting Buddysearch");

					Search->SetName(L"Find Buddy");
				}
				else
				{
					//This search ID was already going. Most likely reason is that
					//we found and lost our buddy very quickly and the last search hadn't
					//had time to be removed yet. Go ahead and set this to happen again
					//next time around.
					Kademlia::CKademlia::GetPrefs()->SetFindBuddy();
				}
			}
		}
	}

	// Publishing
	time_t tNow = time(NULL);
	if( Kademlia::CKademlia::IsConnected() && !m_FileList.empty() && Kademlia::CKademlia::GetPublish()) 
	{ 
		//We are connected to Kad. We are either open or have a buddy. And Kad is ready to start publishing.

		if( Kademlia::CKademlia::GetTotalStoreKey() < KADEMLIATOTALSTOREKEY) 
		{
			//We are not at the max simultaneous keyword publishes 
			if (tNow >= m_Keywords->GetNextPublishTime()) {

				//Enough time has passed since last keyword publish

				//Get the next keyword which has to be (re)-published
				CPublishKeyword* pPubKw = m_Keywords->GetNextKeyword();
				if (pPubKw) {

					//We have the next keyword to check if it can be published

					//Debug check to make sure things are going well.
					ASSERT( pPubKw->GetRefCount() != 0 );

					if (tNow >= pPubKw->GetNextPublishTime()) {
						//This keyword can be published.
						Kademlia::CSearch* pSearch = Kademlia::CSearchManager::PrepareLookup(Kademlia::CSearch::STOREKEYWORD, false, pPubKw->GetKadID());
						if (pSearch) {
							pSearch->SetName(pPubKw->GetKeyword());
							//pSearch was created. Which means no search was already being done with this HashID.
							//This also means that it was checked to see if network load wasn't a factor.

							//Add all file IDs which relate to the current keyword to be published
							const KnownFileArray& aFiles = pPubKw->GetReferences();
							uint32 count = 0;
							for (unsigned int f = 0; f < aFiles.size(); ++f) {

								//Only publish complete files as someone else should have the full file to publish these keywords.
								//As a side effect, this may help reduce people finding incomplete files in the network.
								if( aFiles[f]->uCompleteSourcesCount > 0 ) {
									count++;
									pSearch->AddFileID(aFiles[f]->FileID);
									if( count > 150 ) {
										//We only publish up to 150 files per keyword publish then rotate the list.
										pPubKw->RotateReferences(f);
										break;
									}
								}
							}

							if( count ) {
								//Start our keyword publish
								pPubKw->SetNextPublishTime(tNow+(KADEMLIAREPUBLISHTIMEK));
								pPubKw->IncPublishedCount();
								Kademlia::CSearchManager::StartSearch(pSearch);
							} else {
								//There were no valid files to publish with this keyword.
								delete pSearch;
							}
						}
					}
				}
				m_Keywords->SetNextPublishTime(KADEMLIAPUBLISHTIME+tNow);
			}
		}
		
		if( Kademlia::CKademlia::GetTotalStoreSrc() < KADEMLIATOTALSTORESRC && m_ClientPort != 0 ) {
			if(tNow >= m_lastPublishKadSrc) {
				if(m_currFileSrc > m_FileList.size()) {
					m_currFileSrc = 0;
				}
				SFileInfo* pCurKnownFile = GetFileByIndex(m_currFileSrc);
				if(pCurKnownFile && pCurKnownFile->bShared) {
					if((!IsFirewalled() || HasBuddy() || HasIPv6()) && (pCurKnownFile->lastPublishTimeKadSrc == 0 || pCurKnownFile->lastPublishTimeKadSrc + KADEMLIAREPUBLISHTIMES <= (uint32)time(NULL) || GetBuddyIP() != pCurKnownFile->lastBuddyIP)) {
							
						pCurKnownFile->lastBuddyIP = GetBuddyIP();
						Kademlia::CSearch* Search = Kademlia::CSearchManager::PrepareLookup(Kademlia::CSearch::STOREFILE, true, pCurKnownFile->FileID);
						if(Search)
						{
							Search->SetName(pCurKnownFile->sName);
							pCurKnownFile->lastPublishTimeKadSrc = time(NULL);
						}
					}	
				}
				m_currFileSrc++;

				// even if we did not publish a source, reset the timer so that this list is processed
				// only every KADEMLIAPUBLISHTIME seconds.
				m_lastPublishKadSrc = KADEMLIAPUBLISHTIME+tNow;
			}
		}

		if( Kademlia::CKademlia::GetTotalStoreNotes() < KADEMLIATOTALSTORENOTES) 
		{
			if(tNow >= m_lastPublishKadNotes) {
				if(m_currFileNotes > m_FileList.size()) {
					m_currFileNotes = 0;
				}
				SFileInfo* pCurKnownFile = GetFileByIndex(m_currFileNotes);
				if(pCurKnownFile) {
					if(pCurKnownFile->lastPublishTimeKadNotes + KADEMLIAREPUBLISHTIMEN <= time(NULL) && (!pCurKnownFile->sComment.empty() || pCurKnownFile->uRating != 0)) {
						
						Kademlia::CSearch* Search = Kademlia::CSearchManager::PrepareLookup(Kademlia::CSearch::STORENOTES, true, pCurKnownFile->FileID);
						if(Search)
						{
							Search->SetName(pCurKnownFile->sName);
							pCurKnownFile->lastPublishTimeKadNotes = (uint32)time(NULL);
						}
					}	
				}
				m_currFileNotes++;

				// even if we did not publish a source, reset the timer so that this list is processed
				// only every KADEMLIAPUBLISHTIME seconds.
				m_lastPublishKadNotes = KADEMLIAPUBLISHTIME+tNow;
			}
		}
	}

	// cleanup forgoten searches
	for(map<uint32_t, SSearch*>::iterator I = m_Searches.begin();I != m_Searches.end();)
	{
		if(I->second->Stopped && I->second->Stopped + MIN2MS(10) < GetCurTick())
		{
			delete I->second;
			m_Searches.erase(I++);
		}
		else
			I++;
	}
}

SFileInfo* CKadHandler::GetFileByIndex(unsigned int index)
{
	if ( index >= m_FileList.size() )
		return NULL;
	map<Kademlia::CUInt128, SFileInfo*>::const_iterator pos = m_FileList.begin();
	std::advance(pos, index);
	return pos->second;
}

uint32_t CKadHandler::GetPublicIP()
{
	if (m_PublicIP == 0)
		return GetKadIP();
	return m_PublicIP;
}

bool CKadHandler::IsFirewalled()
{
	if(!m_Firewalled)
		return false;
	if(!IsFirewalledTCP())
		return false;
	return true;
}

uint32_t CKadHandler::GetKadIP()
{
	return Kademlia::CKademlia::GetIPAddress();
}

bool CKadHandler::IsFirewalledTCP()
{
	return Kademlia::CKademlia::IsFirewalled();
}

bool CKadHandler::IsFirewalledUDP()
{
	return Kademlia::CUDPFirewallTester::IsFirewalledUDP(true);
}

uint16_t CKadHandler::GetKadPort(bool bForceInternal)
{
	if(!bForceInternal && Kademlia::CKademlia::GetPrefs() && Kademlia::CKademlia::GetPrefs()->GetUseExternKadPort() && Kademlia::CKademlia::GetPrefs()->GetExternalKadPort() != 0)
		return Kademlia::CKademlia::GetPrefs()->GetExternalKadPort();
	return m_UDPPort;
}

void CKadHandler::SendPacket(CPacket* packet, uint32_t ip, uint16_t port, bool bEncrypt, const uint8_t* clientHashOrKadID, bool bKad, uint32_t nReceiverVerifyKey)
{
	size_t len = packet->GetPacketSize() + 2;
	char* buf = new char [len];
	memcpy(buf, packet->GetUDPHeader(), 2);
	memcpy(buf + 2, packet->GetDataBuffer(), packet->GetPacketSize());

	ASSERT(bKad);
	if((clientHashOrKadID == NULL || (*((uint64*)&clientHashOrKadID) == 0 && *(((uint64*)&clientHashOrKadID)+1) == 0)) || nReceiverVerifyKey == 0)
		bEncrypt = false;

	if(m_UDPSocket)
		m_UDPSocket->SendPacket(buf, len, ip, port, bEncrypt, clientHashOrKadID, bKad, nReceiverVerifyKey);
	else
	{
		QByteArray CryptoID;
		if(!(clientHashOrKadID == NULL || (*((uint64*)&clientHashOrKadID) == 0 && *(((uint64*)&clientHashOrKadID)+1) == 0)))
			CryptoID = QByteArray((const char*)clientHashOrKadID, 16);
		emit SendPacket(QByteArray(buf, len), ip, port, bEncrypt, CryptoID, nReceiverVerifyKey);
	}

	delete [] buf;
	delete packet;
}

void CKadHandler::ProcessPacket(QByteArray Data, quint32 IPv4, quint16 UDPPort, bool validKey, quint32 UDPKey)
{
	if(Data.size() < 2)
		return;

	if(IsFiltered(IPv4))
		return;

	uint8_t Prot = Data.data()[0];
	if(Prot == OP_KADEMLIAPACKEDPROT)
	{
		uint8_t opcode = Data.data()[1];
		uint32_t newSize = Data.size() * 10 + 300; // Should be enough...
		std::vector<uint8_t> unpack(newSize);
		uLongf unpackedsize = newSize - 2;
		uint16_t result = uncompress(&(unpack[2]), &unpackedsize,  (const Bytef*)Data.data() + 2, Data.size() - 2);
		if (result == Z_OK) 
		{
			unpack[0] = OP_KADEMLIAHEADER;
			unpack[1] = opcode;
			Data = QByteArray((char*)&(unpack[0]), unpackedsize + 2);
		}
	}
	else if(Prot != OP_KADEMLIAHEADER)
		return;

	try
	{
		Kademlia::CKademlia::ProcessPacket((const uint8_t*)Data.data(), Data.size(), IPv4, UDPPort, validKey, Kademlia::CKadUDPKey(UDPKey, ntohl(CKadHandler::Instance()->GetPublicIP())));
	} 
	catch(const CException& Exception){
		LogKadLine(LOG_DEBUG /*logClientUDP*/, L"Error while parsing UDP packet: %s", Exception.GetLine().c_str());
	}
}

void CKadHandler::ClearBuddy()
{
	if(HasBuddy())
		Kademlia::CKademlia::GetPrefs()->SetFindBuddy();
	m_BuddyIP = 0;
	m_BuddyPort = 0;
}

void CKadHandler::AddKadFirewallRequest(uint32_t uIP)
{
	SPendingFWCheck PendingFWCheck;
	PendingFWCheck.uIP = uIP;
	PendingFWCheck.uTime = GetCurTick();
	m_PendingFWChecks.push_back(PendingFWCheck);
	while(!m_PendingFWChecks.empty())
	{
		if (GetCurTick() - m_PendingFWChecks.front().uTime > SEC2MS(180)) 
			m_PendingFWChecks.erase(m_PendingFWChecks.begin());
		else
			break;
	}
}

bool CKadHandler::IsKadFirewallCheckIP(uint32_t uIP)
{
	for(list<SPendingFWCheck>::iterator I = m_PendingFWChecks.begin(); I != m_PendingFWChecks.end(); I++)
	{
		if((*I).uIP == uIP && GetCurTick() - m_PendingFWChecks.front().uTime < SEC2MS(180))
			return true;
	}
	return false;
}

bool CKadHandler::IssueFirewallCheck(const Kademlia::CContact& Contact, uint8 uConOpts, bool bTestUDP)
{
	// first make sure we don't know this IP already from somewhere
	//if (FindClientByIP(contact.GetIPAddress()) != NULL)
	//	return false;

	// EK-ToDo-Now: return false if the client is known

	if(m_ClientPort == 0)
		return false; // sorry we are running in kad only mode no TCP at all
	
	if(CKadHandler::Instance()->RequiresCryptLayer() && (Contact.GetClientID() == 0 || (uConOpts & 0x1) == 0))
		return false;

	SQueuedFWCheck QueuedFWCheck;
	QueuedFWCheck.uIP = Contact.GetIPAddress();
	QueuedFWCheck.uTCPPort = Contact.GetTCPPort();
	Contact.GetClientID().ToByteArray(QueuedFWCheck.UserHash);
	QueuedFWCheck.uConOpts = uConOpts;
	QueuedFWCheck.bTestUDP = bTestUDP;
	m_QueuedFWChecks.push_back(QueuedFWCheck);
	return true;
}

void CKadHandler::FWCheckUDPRequested(uint32_t IP, uint16 IntPort, uint16 ExtPort, uint32 UDPKey, bool Biased)
{
	if (Kademlia::CKademlia::GetRoutingZone()->GetContact(IP, 0, false) != NULL)
		Biased = true;

	CBuffer Data1 (1 + 2);
	Data1.WriteValue<uint8>(Biased ? 1 : 0);
	Data1.WriteValue<uint16>(IntPort);
	Kademlia::CKademlia::GetUDPListener()->SendPacket(Data1, KADEMLIA2_FIREWALLUDP, IP, IntPort
		, Kademlia::CKadUDPKey(UDPKey, ntohl(CKadHandler::Instance()->GetPublicIP())), NULL);

	// if the client has a router with PAT (and therefore a different extern port than intern), test this port too
	if (ExtPort != 0 && ExtPort != IntPort)
	{
		CBuffer Data2 (1 + 2);
		Data2.WriteValue<uint8>(Biased ? 1 : 0);
		Data2.WriteValue<uint16>(ExtPort);
		Kademlia::CKademlia::GetUDPListener()->SendPacket(Data1, KADEMLIA2_FIREWALLUDP, IP , ExtPort
			, Kademlia::CKadUDPKey(UDPKey, ntohl(CKadHandler::Instance()->GetPublicIP())), NULL);
	}
}

void CKadHandler::FWCheckACKRecived(uint32_t IP)
{
	if (Kademlia::CKademlia::IsRunning())
	{
		bool bOK = IsKadFirewallCheckIP(IP);
		LogKadLine(LOG_NOTE, L"Incomming, FW ACK: %s (%s)", IPToStr(IP).c_str(), bOK ? L"ok" : L"unexpected");
		if(bOK)
			Kademlia::CKademlia::GetPrefs()->IncFirewalled();
	}
}

void CKadHandler::SendFWCheckACK(uint32_t IP, uint16 KadPort)
{
	Kademlia::CKademlia::GetUDPListener()->SendNullPacket(KADEMLIA_FIREWALLED_ACK_RES, IP, KadPort, 0, NULL);
}

void CKadHandler::RequestBuddy(Kademlia::CContact* Contact, uint8_t uConOpts) // tels us who agreed to become or buddy, we have to connect to him
{
	if(IsKadFirewallCheckIP(Contact->GetIPAddress()))
		return;

	SPendingBuddy PendingBuddy;
	PendingBuddy.uIP = Contact->GetIPAddress();
	PendingBuddy.uTCPPort = Contact->GetUDPPort();
	PendingBuddy.uKadPort = Contact->GetTCPPort();
	PendingBuddy.uConOpts = uConOpts;
	Contact->GetClientID().ToByteArray(PendingBuddy.UserHash);
	PendingBuddy.bIncoming = false;
	m_PendingBuddys.push_back(PendingBuddy);
}

bool CKadHandler::IncomingBuddy(Kademlia::CContact* Contact, Kademlia::CUInt128* BuddyID) // restests us to become a budy, expect conenction
{
	SPendingBuddy PendingBuddy;
	PendingBuddy.uIP = Contact->GetIPAddress();
	PendingBuddy.uTCPPort = Contact->GetUDPPort();
	PendingBuddy.uKadPort = Contact->GetTCPPort();
	PendingBuddy.uConOpts = 0;
	Contact->GetClientID().ToByteArray(PendingBuddy.UserHash);
	BuddyID->ToByteArray(PendingBuddy.BuddyID);
	PendingBuddy.bIncoming = true;
	m_PendingBuddys.push_back(PendingBuddy);
	return true;
}


void CKadHandler::RelayKadCallback(uint32_t uIP, uint16_t uUDPPort, const Kademlia::CUInt128& BuddyID, const Kademlia::CUInt128& FileID)
{
	SBufferedCallback BufferedCallback;
	BufferedCallback.uIP = uIP;
	BufferedCallback.uTCPPort = uUDPPort;
	BuddyID.ToByteArray(BufferedCallback.BuddyID);
	FileID.ToByteArray(BufferedCallback.FileID);
	m_BufferedCallbacks.push_back(BufferedCallback);
}

bool CKadHandler::RequestKadCallback(const QByteArray& BuddyID, const QByteArray& FileID)
{
	Kademlia::CUInt128 buddyID((byte*)BuddyID.data());
	Kademlia::CUInt128 fileID((byte*)FileID.data());

	if( Kademlia::CKademlia::GetPrefs()->GetTotalSource() > 5 || Kademlia::CSearchManager::AlreadySearchingFor(buddyID))
	{
		//There are too many source lookups already or we are already searching this key.
		// bad luck, as lookups aren't supposed to hapen anyway, we just let it fail, if we want
		// to actually really use lookups (so buddies without known IPs), this should be reworked
		// for example by adding a queuesystem for queries
		//LogKadLine(LOG_DEBUG ,L"There are too many source lookups already or we are already searching this key.");
		return false;
	}

	Kademlia::CSearch* FindSource = new Kademlia::CSearch;
	FindSource->SetSearchTypes(Kademlia::CSearch::FINDSOURCE);
	FindSource->SetTargetID(buddyID);
	FindSource->AddFileID(fileID);
	if(!Kademlia::CSearchManager::StartSearch(FindSource))
	{
		ASSERT(0); //This should never happen..
		return false;
	}
	//Started lookup..
	return true;
}

void CKadHandler::DirectCallback(uint32_t uIP, uint16_t uTCPPort, const QByteArray& UserHash, uint8_t uConOpts)
{
	SPendingCallback PendingCallback;
	PendingCallback.uIP = uIP;
	PendingCallback.uTCPPort = uTCPPort;
	memcpy(PendingCallback.UserHash,UserHash.data(),16);
	PendingCallback.uConOpts = uConOpts;
	m_PendingCallbacks.push_back(PendingCallback);
}

void CKadHandler::RelayUDPPacket(uint32_t uIP, uint16_t uUDPPort, byte* pData, size_t uLen)
{
	SBufferedPacket BufferedPacket;
	BufferedPacket.uIP = uIP;
	BufferedPacket.uUDPPort = uUDPPort;
	BufferedPacket.Data = QByteArray((char*)pData, uLen);
	m_BufferedPackets.push_back(BufferedPacket);
}

void CKadHandler::AddFile(SFileInfo* File)
{
	m_Keywords->AddKeywords(File);

	m_FileList.insert(pair<Kademlia::CUInt128, SFileInfo*>(File->FileID, File));
}

void CKadHandler::RemoveFile(SFileInfo* File)
{
	m_Keywords->RemoveKeywords(File);

	map<Kademlia::CUInt128, SFileInfo*>::iterator I = m_FileList.find(File->FileID);
	if(I != m_FileList.end())
	{
		delete I->second;
		m_FileList.erase(I);
	}
}

SFileInfo* CKadHandler::GetFile(const Kademlia::CUInt128& target)
{
	map<Kademlia::CUInt128, SFileInfo*>::iterator I = m_FileList.find(target);
	if(I == m_FileList.end())
		return NULL;
	return I->second;
}

uint64_t CKadHandler::GetSearchedSize(uint32_t searchID)
{
	// Note: source and note searched use not only the file hash but also the file size to identify the file
	map<uint32_t, SSearch*>::iterator I = m_Searches.find(searchID);
	if(I == m_Searches.end())
		return 0;
	SSearch* Search = I->second;
	return Search->SearchedSize;
}


uint32 CKadHandler::FindSources(const QByteArray& FileID, uint64 uFileSize, const wstring& sName)
{
	Kademlia::CUInt128 fileID((byte*)FileID.data());

	Kademlia::CSearch* Search = Kademlia::CSearchManager::PrepareLookup(Kademlia::CSearch::FILE, true, fileID);
	if (!Search)
		return 0;
	Search->SetName(sName);
	ASSERT(m_Searches[Search->GetSearchID()] == NULL);
	m_Searches[Search->GetSearchID()] = new SSearch(SSearch::eSource, uFileSize);
	return Search->GetSearchID();
}

void CKadHandler::SourceFound(uint32_t searchID, const Kademlia::CUInt128* pcontactID, const Kademlia::CUInt128* pbuddyID, uint8_t type, uint32_t ip, uint16_t tcp, uint16_t udp, uint32_t dwBuddyIP, uint16_t dwBuddyPort, uint8_t byCryptOptions, const Kademlia::CUInt128* pIPv6)
{
	map<uint32_t, SSearch*>::iterator I = m_Searches.find(searchID);
	if(I == m_Searches.end())
		return;
	SSearch* Search = I->second;
	ASSERT(Search->Type == SSearch::eSource);
	
	map<Kademlia::CUInt128, SSearch::SSource*>::iterator J = Search->Result.Sources->find(*pcontactID);
	if(J == Search->Result.Sources->end())
		J = Search->Result.Sources->insert(pair<Kademlia::CUInt128, SSearch::SSource*>(*pcontactID, new SSearch::SSource)).first;
	SSearch::SSource* Source = J->second;

	//Source->UserHash = *pcontactID;
	Source->BuddyID = *pbuddyID;
	switch( type )
	{
		case 4:
		case 1:
			//NonFirewalled users
			Source->eType = SSearch::SSource::eOpen; 
			break;
		case 2:
			//Don't use this type... Some clients will process it wrong..
			break;
		case 5:
		case 3:
			//This will be a firewaled client connected to Kad only.
			Source->eType = SSearch::SSource::eFirewalled;
			break;
		case 6:
			// firewalled source which supports direct udp callback
			if ((byCryptOptions & 0x08) == 0){
				LogKadLine(LOG_DEBUG ,_T("Received Kad source type 6 (direct callback) which has the direct callback flag not set"));
				break;
			}
			Source->eType = SSearch::SSource::eUDPOpen;
			break;
		default: ASSERT(0);
	}
	Source->uIP = ip;
	Source->uTCPPort = tcp;
	Source->uKADPort = udp;
	Source->uBuddyIP = dwBuddyIP;
	Source->uBuddyPort = dwBuddyPort;
	Source->uCryptOptions = byCryptOptions;
	Source->IPv6 = *pIPv6;
}

bool CKadHandler::GetFoundSources(uint32 searchID, map<Kademlia::CUInt128, SSearch::SSource*>& Sources)
{
	map<uint32_t, SSearch*>::iterator I = m_Searches.find(searchID);
	if(I == m_Searches.end())
		return true;
	SSearch* Search = I->second;
	ASSERT(Search->Type == SSearch::eSource);

	Sources = *I->second->Result.Sources;
	I->second->Result.Sources->clear();
	if(I->second->Stopped)
	{
		delete I->second;
		m_Searches.erase(I);
		return true;
	}
	return false;
}


uint32 CKadHandler::FindFiles(const SSearchRoot& SearchRoot, QString& ErrorStr)
{
	CBuffer data;
	WriteSearchTree(data, SearchRoot);
	wstring searchString = PrintSearchTree(SearchRoot.pSearchTree);
	wstring Error;
	Kademlia::CSearch* Search = Kademlia::CSearchManager::PrepareFindKeywords(searchString, data.GetSize(), data.GetBuffer(), 0, Error);
	if (!Search)
	{
		ErrorStr = QString::fromStdWString(Error);
		return 0;
	}
	Search->SetName(searchString);
	ASSERT(m_Searches[Search->GetSearchID()] == NULL);
	m_Searches[Search->GetSearchID()] = new SSearch(SSearch::eFile);
	return Search->GetSearchID();
}

void CKadHandler::FilesFound(uint32_t searchID, const Kademlia::CUInt128 *fileID, const wstring& name, uint64_t size, const wstring& type, uint32_t kadPublishInfo, uint32_t availability, const TagPtrList& taglist)
{
	map<uint32_t, SSearch*>::iterator I = m_Searches.find(searchID);
	if(I == m_Searches.end())
		return;
	SSearch* Search = I->second;
	ASSERT(Search->Type == SSearch::eFile);
	
	map<Kademlia::CUInt128, SSearch::SFile*>::iterator J = Search->Result.Files->find(*fileID);
	bool bUpdate = false;
	if(J == Search->Result.Files->end())
		J = Search->Result.Files->insert(pair<Kademlia::CUInt128, SSearch::SFile*>(*fileID, new SSearch::SFile)).first;
	else
		bUpdate = true;
	SSearch::SFile* File = J->second;

	File->sName = name;
	File->uSize = size;
	File->sType = type;
	if(bUpdate)
	{
		for(TagPtrList::const_iterator K = taglist.begin(); K != taglist.end(); K++)
		{
			CTag* Tag = *K;
			for(TagPtrList::iterator J = File->TagList.begin(); J != File->TagList.end(); J++)
			{
				CTag* KnownTag = *J;
				if(Tag->GetName() == KnownTag->GetName() || Tag->GetNameID() == KnownTag->GetNameID())
				{
					delete Tag;
					Tag = NULL;
					break;
				}
			}	
			if(Tag)
				File->TagList.push_back(Tag);
		}

		File->uAvailability = Max(File->uAvailability, availability);
		File->uDifferentNames = Max(File->uDifferentNames,((kadPublishInfo & 0xFF000000) >> 24));
		File->uPublishersKnown =  Max(File->uPublishersKnown,((kadPublishInfo & 0x00FF0000) >> 16));
		File->uTrustValue += (kadPublishInfo & 0x0000FFFF);
	}
	else
	{
		File->TagList = taglist;

		File->uAvailability = availability;
		File->uDifferentNames = (kadPublishInfo & 0xFF000000) >> 24;
		File->uPublishersKnown = (kadPublishInfo & 0x00FF0000) >> 16;
		File->uTrustValue = kadPublishInfo & 0x0000FFFF;
	}
}

bool CKadHandler::GetFoundFiles(uint32 searchID, map<Kademlia::CUInt128, SSearch::SFile*>& Files)
{
	map<uint32_t, SSearch*>::iterator I = m_Searches.find(searchID);
	if(I == m_Searches.end())
		return true;
	SSearch* Search = I->second;
	ASSERT(Search->Type == SSearch::eFile);

	Files = *I->second->Result.Files;
	I->second->Result.Files->clear();
	if(I->second->Stopped)
	{
		delete I->second;
		m_Searches.erase(I);
		return true;
	}
	return false;
}


uint32 CKadHandler::FindNotes(const QByteArray& FileID, uint64 uFileSize, const wstring& sName)
{
	Kademlia::CUInt128 fileID((byte*)FileID.data());

	Kademlia::CSearch* Search = Kademlia::CSearchManager::PrepareLookup(Kademlia::CSearch::NOTES, true, fileID);
	if (!Search)
		return 0;
	Search->SetName(sName);
	ASSERT(m_Searches[Search->GetSearchID()] == NULL);
	m_Searches[Search->GetSearchID()] = new SSearch(SSearch::eNote, uFileSize);
	return Search->GetSearchID();
}

bool CKadHandler::NoteFounds(uint32 searchID, Kademlia::CEntry* entry)
{
	map<uint32_t, SSearch*>::iterator I = m_Searches.find(searchID);
	if(I == m_Searches.end())
		return false;
	SSearch* Search = I->second;
	ASSERT(Search->Type == SSearch::eNote);
	
	bool bRet = false;
	map<Kademlia::CUInt128, SSearch::SNote*>::iterator J = Search->Result.Notes->find(entry->m_uSourceID);
	if(J == Search->Result.Notes->end())
	{
		J = Search->Result.Notes->insert(pair<Kademlia::CUInt128, SSearch::SNote*>(entry->m_uSourceID, new SSearch::SNote)).first;
		bRet = true;
	}
	SSearch::SNote* Note = J->second;

	Note->sName = entry->GetCommonFileName();
	uint64_t Value;
	entry->GetIntTagValue(TAG_FILERATING, Value);
	Note->uRating = Value;
	Note->sComment = entry->GetStrTagValue(TAG_DESCRIPTION);

	return bRet;
}

bool CKadHandler::GetFoundNotes(uint32 searchID, map<Kademlia::CUInt128, SSearch::SNote*>& Notes)
{
	map<uint32_t, SSearch*>::iterator I = m_Searches.find(searchID);
	if(I == m_Searches.end())
		return true;
	SSearch* Search = I->second;
	ASSERT(Search->Type == SSearch::eNote);

	Notes = *I->second->Result.Notes;
	I->second->Result.Notes->clear();
	if(I->second->Stopped)
	{
		delete I->second;
		m_Searches.erase(I);
		return true;
	}
	return false;
}

void CKadHandler::StopSearch(uint32_t searchID)
{
	Kademlia::CSearchManager::StopSearch(searchID, false);
}

void CKadHandler::SearchFinished(uint32_t searchID)
{
	map<uint32_t, SSearch*>::iterator I = m_Searches.find(searchID);
	if(I == m_Searches.end())
		return;

	I->second->Stopped = GetCurTick();
}

// EM-ToDo: add ip filter
bool CKadHandler::IsFiltered(uint32_t ip)
{
	return false;
}

void CKadHandler::FilterIP(uint32_t ip)
{

}

bool g_bLogKad = true;
void LogKadLine(uint32 uFlag, const wchar_t* sLine, ...)
{
	if(!g_bLogKad && ((uFlag & LOG_DEBUG) != 0))
		return;
		
	ASSERT(sLine != NULL);

	const size_t bufferSize = 10241;
	wchar_t bufferline[bufferSize];

	va_list argptr;
	va_start(argptr, sLine);
#ifndef WIN32
	if (vswprintf_l(bufferline, bufferSize, sLine, argptr) == -1)
#else
	if (vswprintf(bufferline, bufferSize, sLine, argptr) == -1)
#endif
		bufferline[bufferSize - 1] = L'\0';
	va_end(argptr);

	QString sBufferLine = QString::fromWCharArray(bufferline);
	LogLine(uFlag, "MuleKad :: " + sBufferLine);
}