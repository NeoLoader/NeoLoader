//
// This file is part of the MuleKad Project.
//
// Copyright (c) 2012 David Xanatos ( XanatosDavid@googlemail.com )
// Copyright (c) 2004-2011 Angel Vidal ( kry@amule.org )
// Copyright (c) 2004-2011 aMule Team ( admin@amule.org / http://www.amule.org )
// Copyright (c) 2003-2011 Barry Dunne (http://www.emule-project.net)
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
#include "Indexed.h"


#include "../Protocols.h"
#include "../Constants.h"
#include "../FileTags.h"
#include "../routing/Contact.h"
#include "../net/KademliaUDPListener.h"
#include "../utils/KadUDPKey.h"
#include "../../../Framework/Buffer.h"
#include "../UDPSocket.h"
#include "../KadHandler.h"

////////////////////////////////////////
using namespace Kademlia;
////////////////////////////////////////

CIndexed::CIndexed()
{
	m_lastClean = time(NULL) + (60*30);
	m_totalIndexSource = 0;
	m_totalIndexKeyword = 0;
	m_totalIndexNotes = 0;
	m_totalIndexLoad = 0;
}

CIndexed::~CIndexed()
{
	for (LoadMap::iterator it = m_Load_map.begin(); it != m_Load_map.end(); ++it ) {
		Load* load = it->second;
		ASSERT(load);
		delete load;
	}

	for (SrcHashMap::iterator itSrcHash = m_Sources_map.begin(); itSrcHash != m_Sources_map.end(); ++itSrcHash ) {
		SrcHash* currSrcHash = itSrcHash->second;
		CKadSourcePtrList& KeyHashSrcMap = currSrcHash->m_Source_map;

		for (CKadSourcePtrList::iterator itSource = KeyHashSrcMap.begin(); itSource != KeyHashSrcMap.end(); ++itSource) {
			Source* currSource = *itSource;
			CKadEntryPtrList& SrcEntryList = currSource->entryList;

			for (CKadEntryPtrList::iterator itEntry = SrcEntryList.begin(); itEntry != SrcEntryList.end(); ++itEntry) {
				Kademlia::CEntry* currName = *itEntry;
				delete currName;
			}
			delete currSource;
		}
		delete currSrcHash;
	}

	for (KeyHashMap::iterator itKeyHash = m_Keyword_map.begin(); itKeyHash != m_Keyword_map.end(); ++itKeyHash ) {
		KeyHash* currKeyHash = itKeyHash->second;
		CSourceKeyMap& KeyHashSrcMap = currKeyHash->m_Source_map;

		for (CSourceKeyMap::iterator itSource = KeyHashSrcMap.begin(); itSource != KeyHashSrcMap.end(); ++itSource ) {
			Source* currSource = itSource->second;
			CKadEntryPtrList& SrcEntryList = currSource->entryList;

			for (CKadEntryPtrList::iterator itEntry = SrcEntryList.begin(); itEntry != SrcEntryList.end(); ++itEntry) {
				Kademlia::CKeyEntry* currName = static_cast<Kademlia::CKeyEntry*>(*itEntry);
				ASSERT(currName->IsKeyEntry());
				currName->DirtyDeletePublishData();
				delete currName;
			}
			delete currSource;
		}
		CKeyEntry::ResetGlobalTrackingMap();
		delete currKeyHash;
	}

	for (SrcHashMap::iterator itNoteHash = m_Notes_map.begin(); itNoteHash != m_Notes_map.end(); ++itNoteHash) {
		SrcHash* currNoteHash = itNoteHash->second;
		CKadSourcePtrList& KeyHashNoteMap = currNoteHash->m_Source_map;

		for (CKadSourcePtrList::iterator itNote = KeyHashNoteMap.begin(); itNote != KeyHashNoteMap.end(); ++itNote) {
			Source* currNote = *itNote;
			CKadEntryPtrList& NoteEntryList = currNote->entryList;
			for (CKadEntryPtrList::iterator itNoteEntry = NoteEntryList.begin(); itNoteEntry != NoteEntryList.end(); ++itNoteEntry) {
				delete *itNoteEntry;
			}
			delete currNote;
		}
		delete currNoteHash;
	} 
}

void CIndexed::Clean()
{
	time_t tNow = time(NULL);
	if (m_lastClean > tNow) {
		return;
	}

	uint32_t k_Removed = 0;
	uint32_t s_Removed = 0;
	uint32_t s_Total = 0;
	uint32_t k_Total = 0;

	KeyHashMap::iterator itKeyHash = m_Keyword_map.begin();
	while (itKeyHash != m_Keyword_map.end()) {
		KeyHashMap::iterator curr_itKeyHash = itKeyHash++; // Don't change this to a ++it!
		KeyHash* currKeyHash = curr_itKeyHash->second;

		for (CSourceKeyMap::iterator itSource = currKeyHash->m_Source_map.begin(); itSource != currKeyHash->m_Source_map.end(); ) {
			CSourceKeyMap::iterator curr_itSource = itSource++; // Don't change this to a ++it!
			Source* currSource = curr_itSource->second;

			CKadEntryPtrList::iterator itEntry = currSource->entryList.begin();
			while (itEntry != currSource->entryList.end()) {
				k_Total++;

				Kademlia::CKeyEntry* currName = static_cast<Kademlia::CKeyEntry*>(*itEntry);
				ASSERT(currName->IsKeyEntry());
				if (!currName->m_bSource && currName->m_tLifeTime < tNow) {
					k_Removed++;
					itEntry = currSource->entryList.erase(itEntry);
					delete currName;
					continue;
				} else if (currName->m_bSource) {
					ASSERT(0);
				} else {
					currName->CleanUpTrackedPublishers();	// intern cleanup
				}
				++itEntry;
			}

			if (currSource->entryList.empty()) {
				currKeyHash->m_Source_map.erase(curr_itSource);
				delete currSource;
			}
		}

		if (currKeyHash->m_Source_map.empty()) {
			m_Keyword_map.erase(curr_itKeyHash);
			delete currKeyHash;
		}
	}

	SrcHashMap::iterator itSrcHash = m_Sources_map.begin();
	while (itSrcHash != m_Sources_map.end()) {
		SrcHashMap::iterator curr_itSrcHash = itSrcHash++; // Don't change this to a ++it!
		SrcHash* currSrcHash = curr_itSrcHash->second;

		CKadSourcePtrList::iterator itSource = currSrcHash->m_Source_map.begin();
		while (itSource != currSrcHash->m_Source_map.end()) {
			Source* currSource = *itSource;			

			CKadEntryPtrList::iterator itEntry = currSource->entryList.begin();
			while (itEntry != currSource->entryList.end()) {
				s_Total++;

				Kademlia::CEntry* currName = *itEntry;
				if (currName->m_tLifeTime < tNow) {
					s_Removed++;
					itEntry = currSource->entryList.erase(itEntry);
					delete currName;
				} else {
					++itEntry;
				}
			}

			if (currSource->entryList.empty()) {
				itSource = currSrcHash->m_Source_map.erase(itSource);
				delete currSource;
			} else {
				++itSource;
			}
		}

		if (currSrcHash->m_Source_map.empty()) {
			m_Sources_map.erase(curr_itSrcHash);
			delete currSrcHash;
		}
	}

	m_totalIndexSource = s_Total - s_Removed;
	m_totalIndexKeyword = k_Total - k_Removed;
	LogKadLine(LOG_DEBUG /*logKadIndex*/, L"Removed %u keyword out of %u and %u source out of %u", k_Removed, k_Total, s_Removed, s_Total);
	m_lastClean = tNow + MIN2S(30);
}

bool CIndexed::AddKeyword(const CUInt128& keyID, const CUInt128& sourceID, Kademlia::CKeyEntry* entry, uint8_t& load)
{
	if (!entry) {
		return false;
	}

	if(!entry->IsKeyEntry())
	{
		ASSERT(0);
		return false;
	}

	if (m_totalIndexKeyword > KADEMLIAMAXENTRIES) {
		load = 100;
		return false;
	}

	if (entry->m_uSize == 0 || entry->GetCommonFileName().empty() || entry->GetTagCount() == 0 || entry->m_tLifeTime < time(NULL)) {
		return false;
	}

	KeyHashMap::iterator itKeyHash = m_Keyword_map.find(keyID); 
	KeyHash* currKeyHash = NULL;
	if (itKeyHash == m_Keyword_map.end()) {
		Source* currSource = new Source;
		currSource->sourceID.SetValue(sourceID);
		entry->MergeIPsAndFilenames(NULL); // IpTracking init
		currSource->entryList.push_front(entry);
		currKeyHash = new KeyHash;
		currKeyHash->keyID.SetValue(keyID);
		currKeyHash->m_Source_map[currSource->sourceID] = currSource;
		m_Keyword_map[currKeyHash->keyID] = currKeyHash;
		load = 1;
		m_totalIndexKeyword++;
		return true;
	} else {
		currKeyHash = itKeyHash->second; 
		size_t indexTotal = currKeyHash->m_Source_map.size();
		if (indexTotal > KADEMLIAMAXINDEX) {
			load = 100;
			//Too many entries for this Keyword..
			return false;
		}
		Source* currSource = NULL;
		CSourceKeyMap::iterator itSource = currKeyHash->m_Source_map.find(sourceID);
		if (itSource != currKeyHash->m_Source_map.end()) {
			currSource = itSource->second;
			if (currSource->entryList.size() > 0) {
				if (indexTotal > KADEMLIAMAXINDEX - 5000) {
					load = 100;
					//We are in a hot node.. If we continued to update all the publishes
					//while this index is full, popular files will be the only thing you index.
					return false;
				}
				// also check for size match
				CKeyEntry *oldEntry = NULL;
				for (CKadEntryPtrList::iterator itEntry = currSource->entryList.begin(); itEntry != currSource->entryList.end(); ++itEntry) {
					CKeyEntry *currEntry = static_cast<Kademlia::CKeyEntry*>(*itEntry);
					ASSERT(currEntry->IsKeyEntry());
					if (currEntry->m_uSize == entry->m_uSize) {
						oldEntry = currEntry;
						currSource->entryList.erase(itEntry);
						break;
					}
				}
				entry->MergeIPsAndFilenames(oldEntry);	// oldEntry can be NULL, that's ok and we still need to do this call in this case
				if (oldEntry == NULL) {
					m_totalIndexKeyword++;
					LogKadLine(LOG_DEBUG /*logKadIndex*/, L"Multiple sizes published for file %s", entry->m_uSourceID.ToHexString().c_str());
				}
				delete oldEntry;
				oldEntry = NULL;
			} else {
				m_totalIndexKeyword++;
				entry->MergeIPsAndFilenames(NULL); // IpTracking init
			}
			load = (uint8_t)((indexTotal * 100) / KADEMLIAMAXINDEX);
			currSource->entryList.push_front(entry);
			return true;
		} else {
			currSource = new Source;
			currSource->sourceID.SetValue(sourceID);
			entry->MergeIPsAndFilenames(NULL); // IpTracking init
			currSource->entryList.push_front(entry);
			currKeyHash->m_Source_map[currSource->sourceID] = currSource;
			m_totalIndexKeyword++;
			load = (indexTotal * 100) / KADEMLIAMAXINDEX;
			return true;
		}
	}
}


bool CIndexed::AddSources(const CUInt128& keyID, const CUInt128& sourceID, Kademlia::CEntry* entry, uint8_t& load)
{
	if (!entry) {
		return false;
	}

	if( entry->m_uIP == 0 || entry->m_uTCPport == 0 || entry->m_uUDPport == 0 || entry->GetTagCount() == 0 || entry->m_tLifeTime < time(NULL)) {
		return false;
	}
		
	SrcHash* currSrcHash = NULL;
	SrcHashMap::iterator itSrcHash = m_Sources_map.find(keyID);
	if (itSrcHash == m_Sources_map.end()) {
		Source* currSource = new Source;
		currSource->sourceID.SetValue(sourceID);
		currSource->entryList.push_front(entry);
		currSrcHash = new SrcHash;
		currSrcHash->keyID.SetValue(keyID);
		currSrcHash->m_Source_map.push_front(currSource);
		m_Sources_map[currSrcHash->keyID] =  currSrcHash;
		m_totalIndexSource++;
		load = 1;
		return true;
	} else {
		currSrcHash = itSrcHash->second;
		size_t size = currSrcHash->m_Source_map.size();

		for (CKadSourcePtrList::iterator itSource = currSrcHash->m_Source_map.begin(); itSource != currSrcHash->m_Source_map.end(); ++itSource) {
			Source* currSource = *itSource;
			if (currSource->entryList.size()) {
				CEntry* currEntry = currSource->entryList.front();
				ASSERT(currEntry != NULL);
				if (currEntry->m_uIP == entry->m_uIP && (currEntry->m_uTCPport == entry->m_uTCPport || currEntry->m_uUDPport == entry->m_uUDPport)) {
					CEntry* currName = currSource->entryList.front();
					currSource->entryList.pop_front();
					delete currName;
					currSource->entryList.push_front(entry);
					load = (size * 100) / KADEMLIAMAXSOURCEPERFILE;
					return true;
				}
			} else {
				//This should never happen!
				currSource->entryList.push_front(entry);
				ASSERT(0);
				load = (size * 100) / KADEMLIAMAXSOURCEPERFILE;
				return true;
			}
		}
		if (size > KADEMLIAMAXSOURCEPERFILE) {
			Source* currSource = currSrcHash->m_Source_map.back();
			currSrcHash->m_Source_map.pop_back();
			ASSERT(currSource != NULL);
			Kademlia::CEntry* currName = currSource->entryList.back();
			currSource->entryList.pop_back();
			ASSERT(currName != NULL);
			delete currName;
			currSource->sourceID.SetValue(sourceID);
			currSource->entryList.push_front(entry);
			currSrcHash->m_Source_map.push_front(currSource);
			load = 100;
			return true;
		} else {
			Source* currSource = new Source;
			currSource->sourceID.SetValue(sourceID);
			currSource->entryList.push_front(entry);
			currSrcHash->m_Source_map.push_front(currSource);
			m_totalIndexSource++;
			load = (size * 100) / KADEMLIAMAXSOURCEPERFILE;
			return true;
		}
	}
	
	return false;
}

bool CIndexed::AddNotes(const CUInt128& keyID, const CUInt128& sourceID, Kademlia::CEntry* entry, uint8_t& load)
{
	if (!entry) {
		return false;
	}

	if (entry->m_uIP == 0 || entry->GetTagCount() == 0) {
		return false;
	}

	SrcHash* currNoteHash = NULL;
	SrcHashMap::iterator itNoteHash = m_Notes_map.find(keyID);
	if (itNoteHash == m_Notes_map.end()) {
		Source* currNote = new Source;
		currNote->sourceID.SetValue(sourceID);
		currNote->entryList.push_front(entry);
		currNoteHash = new SrcHash;
		currNoteHash->keyID.SetValue(keyID);
		currNoteHash->m_Source_map.push_front(currNote);
		m_Notes_map[currNoteHash->keyID] = currNoteHash;
		load = 1;
		m_totalIndexNotes++;
		return true;
	} else {
		currNoteHash = itNoteHash->second;
		size_t size = currNoteHash->m_Source_map.size();

		for (CKadSourcePtrList::iterator itSource = currNoteHash->m_Source_map.begin(); itSource != currNoteHash->m_Source_map.end(); ++itSource) {			
			Source* currNote = *itSource;			
			if( currNote->entryList.size() ) {
				CEntry* currEntry = currNote->entryList.front();
				ASSERT(currEntry!=NULL);
				if (currEntry->m_uIP == entry->m_uIP || currEntry->m_uSourceID == entry->m_uSourceID) {
					CEntry* currName = currNote->entryList.front();
					currNote->entryList.pop_front();
					delete currName;
					currNote->entryList.push_front(entry);
					load = (size * 100) / KADEMLIAMAXNOTESPERFILE;
					return true;
				}
			} else {
				//This should never happen!
				currNote->entryList.push_front(entry);
				ASSERT(0);
				load = (size * 100) / KADEMLIAMAXNOTESPERFILE;
				m_totalIndexNotes++;
				return true;
			}
		}
		if (size > KADEMLIAMAXNOTESPERFILE) {
			Source* currNote = currNoteHash->m_Source_map.back();
			currNoteHash->m_Source_map.pop_back();
			ASSERT(currNote != NULL);
			CEntry* currName = currNote->entryList.back();
			currNote->entryList.pop_back();
			ASSERT(currName != NULL);
			delete currName;
			currNote->sourceID.SetValue(sourceID);
			currNote->entryList.push_front(entry);
			currNoteHash->m_Source_map.push_front(currNote);
			load = 100;
			return true;
		} else {
			Source* currNote = new Source;
			currNote->sourceID.SetValue(sourceID);
			currNote->entryList.push_front(entry);
			currNoteHash->m_Source_map.push_front(currNote);
			load = (size * 100) / KADEMLIAMAXNOTESPERFILE;
			m_totalIndexNotes++;
			return true;
		}
	}
}

bool CIndexed::AddLoad(const CUInt128& keyID, uint32_t timet)
{
	Load* load = NULL;

	if ((uint32_t)time(NULL) > timet) {
		return false;
	}

	LoadMap::iterator it = m_Load_map.find(keyID);
	if (it != m_Load_map.end()) {
		ASSERT(0);
		return false;
	}

	load = new Load();
	load->keyID.SetValue(keyID);
	load->time = timet;
	m_Load_map[load->keyID] = load;
	m_totalIndexLoad++;
	return true;
}

void CIndexed::SendValidKeywordResult(const CUInt128& keyID, const SSearchTerm* pSearchTerms, uint32_t ip, uint16_t port, bool oldClient, uint16_t startPosition, const CKadUDPKey& senderKey)
{
	KeyHash* currKeyHash = NULL;
	KeyHashMap::iterator itKeyHash = m_Keyword_map.find(keyID);
	if (itKeyHash != m_Keyword_map.end()) {
		currKeyHash = itKeyHash->second;
		CBuffer packetdata(1024*50);
		Kademlia::CKademlia::GetPrefs()->GetKadID().Write(&packetdata);
		keyID.Write(&packetdata);
		packetdata.WriteValue<uint16_t>(50);
		const uint16_t maxResults = 300;
		int count = 0 - startPosition;

		// we do 2 loops: In the first one we ignore all results which have a trustvalue below 1
		// in the second one we then also consider those. That way we make sure our 300 max results are not full
		// of spam entries. We could also sort by trustvalue, but we would risk to only send popular files this way
		// on very hot keywords
		bool onlyTrusted = true;
#ifdef _DEBUG
		uint32_t dbgResultsTrusted = 0;
		uint32_t dbgResultsUntrusted = 0;
#endif

		do {
			for (CSourceKeyMap::iterator itSource = currKeyHash->m_Source_map.begin(); itSource != currKeyHash->m_Source_map.end(); ++itSource) {
				Source* currSource =  itSource->second;

				for (CKadEntryPtrList::iterator itEntry = currSource->entryList.begin(); itEntry != currSource->entryList.end(); ++itEntry) {
					Kademlia::CKeyEntry* currName = static_cast<Kademlia::CKeyEntry*>(*itEntry);
					ASSERT(currName->IsKeyEntry());
					if ((onlyTrusted ^ (currName->GetTrustValue() < 1.0)) && (!pSearchTerms || currName->SearchTermsMatch(pSearchTerms))) {
						if (count < 0) {
							count++;
						} else if ((uint16_t)count < maxResults) {
							if (!oldClient || currName->m_uSize <= OLD_MAX_FILE_SIZE) {
								count++;
#ifdef _DEBUG
								if (onlyTrusted) {
									dbgResultsTrusted++;
								} else {
									dbgResultsUntrusted++;
								}
#endif
								currName->m_uSourceID.Write(&packetdata);
								currName->WriteTagListWithPublishInfo(&packetdata);
								if (count % 50 == 0) {
									DebugSend(L"Kad2SearchRes", ip, port);
									CKademlia::GetUDPListener()->SendPacket(packetdata, KADEMLIA2_SEARCH_RES, ip, port, senderKey, NULL);
									// Reset the packet, keeping the header (Kad id, key id, number of entries)
									packetdata.SetSize(16 + 16 + 2);
								}
							}
						} else {
							itSource = currKeyHash->m_Source_map.end();
							--itSource;
							break;
						}
					}
				}
			}

			if (onlyTrusted && count < (int)maxResults) {
				onlyTrusted = false;
			} else {
				break;
			}
		} while (!onlyTrusted);

#ifdef _DEBUG
		LogKadLine(LOG_DEBUG /*logKadIndex*/, L"Kad keyword search result request: Sent %u trusted and %u untrusted results", dbgResultsTrusted, dbgResultsUntrusted);
#endif

		if (count > 0) {
			uint16_t countLeft = (uint16_t)count % 50;
			if (countLeft) {
				packetdata.SetPosition(16 + 16);
				packetdata.WriteValue<uint16_t>(countLeft);
				DebugSend(L"Kad2SearchRes", ip, port);
				CKademlia::GetUDPListener()->SendPacket(packetdata, KADEMLIA2_SEARCH_RES, ip, port, senderKey, NULL);
			}
		}
	}
	Clean();
}

void CIndexed::SendValidSourceResult(const CUInt128& keyID, uint32_t ip, uint16_t port, uint16_t startPosition, uint64_t fileSize, const CKadUDPKey& senderKey)
{
	SrcHash* currSrcHash = NULL;
	SrcHashMap::iterator itSrcHash = m_Sources_map.find(keyID);
	if (itSrcHash != m_Sources_map.end()) {
		currSrcHash = itSrcHash->second;
		CBuffer packetdata(1024*50);
		Kademlia::CKademlia::GetPrefs()->GetKadID().Write(&packetdata);
		keyID.Write(&packetdata);
		packetdata.WriteValue<uint16_t>(50);
		uint16_t maxResults = 300;
		int count = 0 - startPosition;

		for (CKadSourcePtrList::iterator itSource = currSrcHash->m_Source_map.begin(); itSource != currSrcHash->m_Source_map.end(); ++itSource) {
			Source* currSource = *itSource;	
			if (currSource->entryList.size()) {
				Kademlia::CEntry* currName = currSource->entryList.front();
				if (count < 0) {
					count++;
				} else if (count < maxResults) {
					if (!fileSize || !currName->m_uSize || currName->m_uSize == fileSize) {
						currName->m_uSourceID.Write(&packetdata);
						currName->WriteTagList(&packetdata);
						count++;
						if (count % 50 == 0) {
							DebugSend(L"Kad2SearchRes", ip, port);
							CKademlia::GetUDPListener()->SendPacket(packetdata, KADEMLIA2_SEARCH_RES, ip, port, senderKey, NULL);
							// Reset the packet, keeping the header (Kad id, key id, number of entries)
							packetdata.SetSize(16 + 16 + 2);
						}
					}
				} else {
					break;
				}
			}
		}

		if (count > 0) {
			uint16_t countLeft = (uint16_t)count % 50;
			if (countLeft) {
				packetdata.SetPosition(16 + 16);
				packetdata.WriteValue<uint16_t>(countLeft);
				DebugSend(L"Kad2SearchRes", ip, port);
				CKademlia::GetUDPListener()->SendPacket(packetdata, KADEMLIA2_SEARCH_RES, ip, port, senderKey, NULL);
			}
		}
	}
	Clean();
}

void CIndexed::SendValidNoteResult(const CUInt128& keyID, uint32_t ip, uint16_t port, uint64_t fileSize, const CKadUDPKey& senderKey)
{
	SrcHash* currNoteHash = NULL;
	SrcHashMap::iterator itNote = m_Notes_map.find(keyID);
	if (itNote != m_Notes_map.end()) {
		currNoteHash = itNote->second;		
		CBuffer packetdata(1024*50);
		Kademlia::CKademlia::GetPrefs()->GetKadID().Write(&packetdata);
		keyID.Write(&packetdata);
		packetdata.WriteValue<uint16_t>(50);
		uint16_t maxResults = 150;
		uint16_t count = 0;

		for (CKadSourcePtrList::iterator itSource = currNoteHash->m_Source_map.begin(); itSource != currNoteHash->m_Source_map.end(); ++itSource ) {
			Source* currNote = *itSource;
			if (currNote->entryList.size()) {
				Kademlia::CEntry* currName = currNote->entryList.front();
				if (count < maxResults) {
					if (!fileSize || !currName->m_uSize || fileSize == currName->m_uSize) {
						currName->m_uSourceID.Write(&packetdata);
						currName->WriteTagList(&packetdata);
						count++;
						if (count % 50 == 0) {
							DebugSend(L"Kad2SearchRes", ip, port);
							CKademlia::GetUDPListener()->SendPacket(packetdata, KADEMLIA2_SEARCH_RES, ip, port, senderKey, NULL);
							// Reset the packet, keeping the header (Kad id, key id, number of entries)
							packetdata.SetSize(16 + 16 + 2);
						}
					}
				} else {
					break;
				}
			}
		}

		uint16_t countLeft = count % 50;
		if (countLeft) {
			packetdata.SetPosition(16 + 16);
			packetdata.WriteValue<uint16_t>(countLeft);
			DebugSend(L"Kad2SearchRes", ip, port);
			CKademlia::GetUDPListener()->SendPacket(packetdata, KADEMLIA2_SEARCH_RES, ip, port, senderKey, NULL);
		}
	}
}

bool CIndexed::SendStoreRequest(const CUInt128& keyID)
{
	Load* load = NULL;
	LoadMap::iterator it = m_Load_map.find(keyID);
	if (it != m_Load_map.end()) {
		load = it->second;
		if (load->time < (uint32_t)time(NULL)) {
			m_Load_map.erase(it);
			m_totalIndexLoad--;
			delete load;
			return true;
		}
		return false;
	}
	return true;
}

SSearchTerm::SSearchTerm()
{
	type = AND;
	tag = NULL;
	left = NULL;
	right = NULL;
}

SSearchTerm::~SSearchTerm()
{
	if (type == String) {
		delete astr;
	}
	delete tag;
}
// File_checked_for_headers
