//
// This file is part of the MuleKad Project.
//
// Copyright (c) 2012 David Xanatos ( XanatosDavid@googlemail.com )
// Copyright (c) 2008-2011 Dévai Tamás ( gonosztopi@amule.org )
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

#include "Entry.h"
#include "../FileTags.h"
#include "../Constants.h"
#include "Indexed.h"
#include "../../../Framework/Buffer.h"
#include "../../../Framework/Strings.h"
#include "../net/KademliaUDPListener.h"
#include "../UDPSocket.h"
#include "../KadHandler.h"

using namespace Kademlia;

CKeyEntry::GlobalPublishIPMap	CKeyEntry::s_globalPublishIPs;


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////// CEntry
CEntry::~CEntry()
{
	deleteTagPtrListEntries(&m_taglist);
}

CEntry* CEntry::Copy() const
{
	CEntry* entry = new CEntry();
	for (FileNameList::const_iterator it = m_filenames.begin(); it != m_filenames.end(); ++it) {
		entry->m_filenames.push_back(*it);
	}
	entry->m_uIP = m_uIP;
	entry->m_uKeyID.SetValue(m_uKeyID);
	entry->m_tLifeTime = m_tLifeTime;
	entry->m_uSize = m_uSize;
	entry->m_bSource = m_bSource;
	entry->m_uSourceID.SetValue(m_uSourceID);
	entry->m_uTCPport = m_uTCPport;
	entry->m_uUDPport = m_uUDPport;
	for (TagPtrList::const_iterator it = m_taglist.begin(); it != m_taglist.end(); ++it) {
		entry->m_taglist.push_back((*it)->CloneTag());
	}
	return entry;
}

bool CEntry::GetIntTagValue(const wstring& tagname, uint64_t& value, bool includeVirtualTags) const
{
	for (TagPtrList::const_iterator it = m_taglist.begin(); it != m_taglist.end(); ++it) {
		if ((*it)->IsInt() && ((*it)->GetName() == tagname)) {
			value = (*it)->GetInt();
			return true;
		}
	}

	if (includeVirtualTags) {
		// SizeTag is not stored anymore, but queried in some places
		if (tagname == TAG_FILESIZE) {
			value = m_uSize;
			return true;
		}
	}
	value = 0;
	return false;
}

wstring CEntry::GetStrTagValue(const wstring& tagname) const
{
	for (TagPtrList::const_iterator it = m_taglist.begin(); it != m_taglist.end(); ++it) {
		if (((*it)->GetName() == tagname) && (*it)->IsStr()) {
			return (*it)->GetStr();
		}
	}
	return L"";
}

void CEntry::SetFileName(const wstring& name)
{
	if (!m_filenames.empty()) {
		ASSERT(0);
		m_filenames.clear();
	}
	sFileNameEntry sFN = { name, 1 };
	m_filenames.push_front(sFN);
}

wstring CEntry::GetCommonFileName() const
{
	// return the filename on which most publishers seem to agree on
	// due to the counting, this doesn't has to be excact, we just want to make sure to not use a filename which just
	// a few bad publishers used and base or search matching and answering on this, instead of the most popular name
	// Note: The Index values are not the acutal numbers of publishers, but just a relativ number to compare to other entries
	FileNameList::const_iterator result = m_filenames.end();
	uint32_t highestPopularityIndex = 0;
	for (FileNameList::const_iterator it = m_filenames.begin(); it != m_filenames.end(); ++it) {
		if (it->m_popularityIndex > highestPopularityIndex) {
			highestPopularityIndex = it->m_popularityIndex;
			result = it;
		}
	}
	wstring strResult(result != m_filenames.end() ? result->m_filename : L"");
	ASSERT(!strResult.empty() || m_filenames.empty());
	return strResult;
}

wstring CEntry::GetCommonFileNameLowerCase() const	
{ 
	return MkLower(GetCommonFileName()); 
}

void CEntry::WriteTagListInc(CBuffer* data, uint32_t increaseTagNumber)
{
	// write taglist and add name + size tag
	ASSERT(data);

	uint32_t count = GetTagCount() + increaseTagNumber;	// will include name and size tag in the count if needed
	ASSERT(count <= 0xFF);
	data->WriteValue<uint8_t>(count);

	if (!GetCommonFileName().empty()){
		ASSERT(count > m_taglist.size());
		CTagString(TAG_FILENAME, GetCommonFileName()).ToBuffer(data);
	}
	if (m_uSize != 0){
		ASSERT(count > m_taglist.size());
		CTagVarInt(TAG_FILESIZE, m_uSize).ToBuffer(data);
	}

	for (TagPtrList::const_iterator it = m_taglist.begin(); it != m_taglist.end(); ++it) {
		(**it).ToBuffer(data);
	}
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////// CKeyEntry
CKeyEntry::CKeyEntry()
{
	m_publishingIPs = NULL;
	m_trustValue = 0;
	m_lastTrustValueCalc = 0;
}

CKeyEntry::~CKeyEntry()
{
	if (m_publishingIPs != NULL) {
		for (PublishingIPList::const_iterator it = m_publishingIPs->begin(); it != m_publishingIPs->end(); ++it) {
			AdjustGlobalPublishTracking(it->m_ip, false, L"instance delete");
		}
		delete m_publishingIPs;
		m_publishingIPs = NULL;
	}
}

bool CKeyEntry::SearchTermsMatch(const SSearchTerm* searchTerm) const
{
	// boolean operators
	if (searchTerm->type == SSearchTerm::AND) {
		return SearchTermsMatch(searchTerm->left) && SearchTermsMatch(searchTerm->right);
	}

	if (searchTerm->type == SSearchTerm::OR) {
		return SearchTermsMatch(searchTerm->left) || SearchTermsMatch(searchTerm->right);
	}

	if (searchTerm->type == SSearchTerm::NOT) {
		return SearchTermsMatch(searchTerm->left) && !SearchTermsMatch(searchTerm->right);
	}

	// word which is to be searched in the file name (and in additional meta data as done by some ed2k servers???)
	if (searchTerm->type == SSearchTerm::String) {
		int strSearchTerms = searchTerm->astr->size();
		if (strSearchTerms == 0) {
			return false;
		}
		// if there are more than one search strings specified (e.g. "aaa bbb ccc") the entire string is handled
		// like "aaa AND bbb AND ccc". search all strings from the string search term in the tokenized list of
		// the file name. all strings of string search term have to be found (AND)
		wstring commonFileNameLower(GetCommonFileNameLowerCase());
		for (int i = 0; i < strSearchTerms; i++) {
			// this will not give the same results as when tokenizing the filename string, but it is 20 times faster.
			if (commonFileNameLower.find((*(searchTerm->astr))[i]) == -1) {
				return false;
			}
		}
		return true;
	}

	if (searchTerm->type == SSearchTerm::MetaTag) {
		if (searchTerm->tag->GetType() == 2) {	// meta tags with string values
			if (searchTerm->tag->GetName() == TAG_FILEFORMAT) {
				// 21-Sep-2006 []: Special handling for TAG_FILEFORMAT which is already part
				// of the filename and thus does not need to get published nor stored explicitly,
				wstring commonFileName(GetCommonFileName());
				int ext = commonFileName.find(L'.', true);
				if (ext != -1) {
					return CompareStr(commonFileName.substr(ext + 1), searchTerm->tag->GetStr());
				}
			} else {
				for (TagPtrList::const_iterator it = m_taglist.begin(); it != m_taglist.end(); ++it) {
					if ((*it)->IsStr() && searchTerm->tag->GetName() == (*it)->GetName()) {
						return CompareStr((*it)->GetStr(),searchTerm->tag->GetStr());
					}
				}
			}
		}
	} else if (searchTerm->type == SSearchTerm::OpGreaterEqual) {
		if (searchTerm->tag->IsInt()) {	// meta tags with integer values
			uint64_t value;
			if (GetIntTagValue(searchTerm->tag->GetName(), value, true)) {
				return value >= searchTerm->tag->GetInt();
			}
		} else if (searchTerm->tag->IsFloat()) {	// meta tags with float values
			for (TagPtrList::const_iterator it = m_taglist.begin(); it != m_taglist.end(); ++it) {
				if ((*it)->IsFloat() && searchTerm->tag->GetName() == (*it)->GetName()) {
					return (*it)->GetFloat() >= searchTerm->tag->GetFloat();
				}
			}
		}
	} else if (searchTerm->type == SSearchTerm::OpLessEqual) {
		if (searchTerm->tag->IsInt()) {	// meta tags with integer values
			uint64_t value;
			if (GetIntTagValue(searchTerm->tag->GetName(), value, true)) {
				return value <= searchTerm->tag->GetInt();
			}
		} else if (searchTerm->tag->IsFloat()) {	// meta tags with float values
			for (TagPtrList::const_iterator it = m_taglist.begin(); it != m_taglist.end(); ++it) {
				if ((*it)->IsFloat() && searchTerm->tag->GetName() == (*it)->GetName()) {
					return (*it)->GetFloat() <= searchTerm->tag->GetFloat();
				}
			}
		}
	} else if (searchTerm->type == SSearchTerm::OpGreater) {
		if (searchTerm->tag->IsInt()) {	// meta tags with integer values
			uint64_t value;
			if (GetIntTagValue(searchTerm->tag->GetName(), value, true)) {
				return value > searchTerm->tag->GetInt();
			}
		} else if (searchTerm->tag->IsFloat()) {	// meta tags with float values
			for (TagPtrList::const_iterator it = m_taglist.begin(); it != m_taglist.end(); ++it) {
				if ((*it)->IsFloat() && searchTerm->tag->GetName() == (*it)->GetName()) {
					return (*it)->GetFloat() > searchTerm->tag->GetFloat();
				}
			}
		}
	} else if (searchTerm->type == SSearchTerm::OpLess) {
		if (searchTerm->tag->IsInt()) {	// meta tags with integer values
			uint64_t value;
			if (GetIntTagValue(searchTerm->tag->GetName(), value, true)) {
				return value < searchTerm->tag->GetInt();
			}
		} else if (searchTerm->tag->IsFloat()) {	// meta tags with float values
			for (TagPtrList::const_iterator it = m_taglist.begin(); it != m_taglist.end(); ++it) {
				if ((*it)->IsFloat() && searchTerm->tag->GetName() == (*it)->GetName()) {
					return (*it)->GetFloat() < searchTerm->tag->GetFloat();
				}
			}
		}
	} else if (searchTerm->type == SSearchTerm::OpEqual) {
		if (searchTerm->tag->IsInt()) {	// meta tags with integer values
			uint64_t value;
			if (GetIntTagValue(searchTerm->tag->GetName(), value, true)) {
				return value == searchTerm->tag->GetInt();
			}
		} else if (searchTerm->tag->IsFloat()) {	// meta tags with float values
			for (TagPtrList::const_iterator it = m_taglist.begin(); it != m_taglist.end(); ++it) {
				if ((*it)->IsFloat() && searchTerm->tag->GetName() == (*it)->GetName()) {
					return (*it)->GetFloat() == searchTerm->tag->GetFloat();
				}
			}
		}
	} else if (searchTerm->type == SSearchTerm::OpNotEqual) {
		if (searchTerm->tag->IsInt()) {	// meta tags with integer values
			uint64_t value;
			if (GetIntTagValue(searchTerm->tag->GetName(), value, true)) {
				return value != searchTerm->tag->GetInt();
			}
		} else if (searchTerm->tag->IsFloat()) {	// meta tags with float values
			for (TagPtrList::const_iterator it = m_taglist.begin(); it != m_taglist.end(); ++it) {
				if ((*it)->IsFloat() && searchTerm->tag->GetName() == (*it)->GetName()) {
					return (*it)->GetFloat() != searchTerm->tag->GetFloat();
				}
			}
		}
	}

	return false;
}

void CKeyEntry::AdjustGlobalPublishTracking(uint32_t ip, bool increase, const wstring& DEBUG_ONLY(dbgReason))
{
	uint32_t count = 0;
	bool found = false;
	GlobalPublishIPMap::const_iterator it = s_globalPublishIPs.find(ip & 0xFFFFFF00 /* /24 netmask, take care of endian if needed */ );
	if (it != s_globalPublishIPs.end()) {
		count = it->second;
		found = true;
	}

	if (increase) {
		count++;
	} else {
		count--;
	}

	if (count > 0) {
		s_globalPublishIPs[ip & 0xFFFFFF00] = count;
	} else if (found) {
		s_globalPublishIPs.erase(ip & 0xFFFFFF00);
	} else {
		ASSERT(0);
	}
#ifdef _DEBUG
	if (!dbgReason.empty()) {
		LogKadLine(LOG_DEBUG /*logKadEntryTracking*/, L"%s %s (%s) - (%s), new count %u"
			, (increase ? L"Adding" : L"Removing"), IPToStr(ip & 0xFFFFFF00).c_str(), IPToStr(ip).c_str(), dbgReason.c_str(), count);
	}
#endif
}

void CKeyEntry::MergeIPsAndFilenames(CKeyEntry* fromEntry)
{
	// this is called when replacing a stored entry with a refreshed one. 
	// we want to take over the tracked IPs and the different filenames from the old entry, the rest is still
	// "overwritten" with the refreshed values. This might be not perfect for the taglist in some cases, but we can't afford
	// to store hundreds of taglists to figure out the best one like we do for the filenames now
	if (m_publishingIPs != NULL) { // This instance needs to be a new entry, otherwise we don't want/need to merge
		ASSERT(fromEntry == NULL);
		ASSERT(!m_publishingIPs->empty());
		ASSERT(!m_filenames.empty());
		return;
	}

	ASSERT( m_aAICHHashs.size() <= 1 );
	//fetch the "new" AICH hash if any
	SAICHHash* pNewAICHHash = NULL;
	if ( !m_aAICHHashs.empty() )
	{
		pNewAICHHash = new SAICHHash(m_aAICHHashs[0]);
		m_aAICHHashs.clear();
		m_anAICHHashPopularity.clear();
	}

	bool refresh = false;
	if (fromEntry == NULL || fromEntry->m_publishingIPs == NULL) {
		ASSERT(fromEntry == NULL);
		// if called with NULL, this is a complete new entry and we need to initalize our lists
		if (m_publishingIPs == NULL) {
			m_publishingIPs = new PublishingIPList();
		}
		// update the global track map below
	} else {
		//  copy over the existing ones.
		m_aAICHHashs.insert(m_aAICHHashs.end(), fromEntry->m_aAICHHashs.begin(), fromEntry->m_aAICHHashs.end());
		m_anAICHHashPopularity.insert(m_anAICHHashPopularity.end(), fromEntry->m_anAICHHashPopularity.begin(), fromEntry->m_anAICHHashPopularity.end());

		// merge the tracked IPs, add this one if not already on the list
		m_publishingIPs = fromEntry->m_publishingIPs;
		fromEntry->m_publishingIPs = NULL;	
		bool fastRefresh = false;
		for (PublishingIPList::iterator it = m_publishingIPs->begin(); it != m_publishingIPs->end(); ++it) {
			if (it->m_ip == m_uIP) {
				refresh = true;
				if ((time(NULL) - it->m_lastPublish) < (KADEMLIAREPUBLISHTIMES - HR2S(1))) {
					LogKadLine(LOG_DEBUG /*logKadEntryTracking*/, L"FastRefresh publish, ip: %s", IPToStr(m_uIP).c_str());
					fastRefresh = true; // refreshed faster than expected, will not count into filenamepopularity index
				}
				it->m_lastPublish = time(NULL);

				// Has the AICH Hash this publisher reported changed?
				if (pNewAICHHash != NULL)
				{
					if (it->m_byAICHHashIdx != 0xFFFF && m_aAICHHashs[it->m_byAICHHashIdx] != *pNewAICHHash)
					{
						LogKadLine(LOG_DEBUG /*logKadEntryTracking*/, L"KadEntryTracking: AICH Hash changed, publisher ip: %s", IPToStr(m_uIP).c_str());
						AddRemoveAICHHash(m_aAICHHashs[it->m_byAICHHashIdx].Hash, false);
						it->m_byAICHHashIdx = AddRemoveAICHHash(pNewAICHHash->Hash, true);
					}
					else if (it->m_byAICHHashIdx == 0xFFFF)
					{
						LogKadLine(LOG_DEBUG /*logKadEntryTracking*/, L"KadEntryTracking: New AICH Hash during publishing (publisher reported none before), publisher ip: %s", IPToStr(m_uIP).c_str());
						it->m_byAICHHashIdx = AddRemoveAICHHash(pNewAICHHash->Hash, true);
					}
				}
				else if (it->m_byAICHHashIdx != 0xFFFF)
				{
					LogKadLine(LOG_DEBUG /*logKadEntryTracking*/, L"KadEntryTracking: AICH Hash removed, publisher ip: %s", IPToStr(m_uIP).c_str());
					AddRemoveAICHHash(m_aAICHHashs[it->m_byAICHHashIdx].Hash, false);
					it->m_byAICHHashIdx = 0xFFFF;
				}

				m_publishingIPs->push_back(*it);
				m_publishingIPs->erase(it);
				break;
			}
		}

		// copy over trust value, in case we don't want to recalculate
		m_trustValue = fromEntry->m_trustValue;
		m_lastTrustValueCalc = fromEntry->m_lastTrustValueCalc;

		// copy over the different names, if they are different the one we have right now
		ASSERT(m_filenames.size() == 1); // we should have only one name here, since it's the entry from one single source
		sFileNameEntry currentName = { L"", 0 };
		if (m_filenames.size() != 0) {
			currentName = m_filenames.front();
			m_filenames.pop_front();
		}

		bool duplicate = false;
		for (FileNameList::iterator it = fromEntry->m_filenames.begin(); it != fromEntry->m_filenames.end(); ++it) {
			sFileNameEntry nameToCopy = *it;
			if (CompareStr(currentName.m_filename, nameToCopy.m_filename)) {
				// the filename of our new entry matches with our old, increase the popularity index for the old one
				duplicate = true;
				if (!fastRefresh) {
					nameToCopy.m_popularityIndex++;
				}
			}
			m_filenames.push_back(nameToCopy);
		}
		if (!duplicate) {
			m_filenames.push_back(currentName);
		}
	}

	// if this was a refresh done, otherwise update the global track map
	if (!refresh) {
		ASSERT(m_uIP != 0);
		uint16 nAICHHashIdx;
		if (pNewAICHHash != NULL)
			nAICHHashIdx = AddRemoveAICHHash(pNewAICHHash->Hash, true);
		else
			nAICHHashIdx = 0xFFFF;
		sPublishingIP add = { m_uIP, time(NULL), nAICHHashIdx };
		m_publishingIPs->push_back(add);

		// add the publisher to the tacking list
		AdjustGlobalPublishTracking(m_uIP, true, L"new publisher");

		// we keep track of max 100 IPs, in order to avoid too much time for calculation/storing/loading.
		if (m_publishingIPs->size() > 100) {
			sPublishingIP curEntry = m_publishingIPs->front();
			m_publishingIPs->pop_front();
			if (curEntry.m_byAICHHashIdx != 0xFFFF)
				VERIFY( AddRemoveAICHHash(m_aAICHHashs[curEntry.m_byAICHHashIdx].Hash, false) == curEntry.m_byAICHHashIdx );
			AdjustGlobalPublishTracking(curEntry.m_ip, false, L"more than 100 publishers purge");
		}

		// since we added a new publisher, we want to (re)calculate the trust value for this entry		
		ReCalculateTrustValue();
	}
	delete pNewAICHHash;
	LogKadLine(LOG_DEBUG /*logKadEntryTracking*/, L"Indexed Keyword, Refresh: %s, Current Publisher: %s, Total Publishers: %u, Total different Names: %u, TrustValue: %.2f, file: %s"
		, (refresh ? L"Yes" : L"No"), IPToStr(m_uIP).c_str(), m_publishingIPs->size(), m_filenames.size(), m_trustValue, m_uSourceID.ToHexString().c_str());
}

void CKeyEntry::ReCalculateTrustValue()
{
#define PUBLISHPOINTSSPERSUBNET	10.0
	// The trustvalue is supposed to be an indicator how trustworthy/important (or spammy) this entry is and lies between 0 and ~10000,
	// but mostly we say everything below 1 is bad, everything above 1 is good. It is calculated by looking at how many different
	// IPs/24 have published this entry and how many entries each of those IPs have.
	// Each IP/24 has x (say 3) points. This means if one IP publishes 3 different entries without any other IP publishing those entries,
	// each of those entries will have 3 / 3 = 1 Trustvalue. Thats fine. If it publishes 6 alone, each entry has 3 / 6 = 0.5 trustvalue - not so good
	// However if there is another publisher for entry 5, which only publishes this entry then we have 3/6 + 3/1 = 3.5 trustvalue for this entry
	//
	// What's the point? With this rating we try to avoid getting spammed with entries for a given keyword by a small IP range, which blends out
	// all other entries for this keyword do to its amount as well as giving an indicator for the searcher. So if we are the node to index "Knoppix", and someone
	// from 1 IP publishes 500 times "knoppix casino 500% bonus.txt", all those entries will have a trustvalue of 0.006 and we make sure that
	// on search requests for knoppix, those entries are only returned after all entries with a trustvalue > 1 were sent (if there is still space).
	//
	// Its important to note that entry with < 1 do NOT get ignored or singled out, this only comes into play if we have 300 more results for
	// a search request rating > 1
	if(m_publishingIPs == NULL)
	{
		ASSERT(0);
		return;
	}

	m_lastTrustValueCalc = GetCurTick();
	m_trustValue = 0;
	ASSERT(!m_publishingIPs->empty());
	for (PublishingIPList::iterator it = m_publishingIPs->begin(); it != m_publishingIPs->end(); ++it) {
		sPublishingIP curEntry = *it;
		uint32_t count = 0;
		GlobalPublishIPMap::const_iterator itMap = s_globalPublishIPs.find(curEntry.m_ip & 0xFFFFFF00 /* /24 netmask, take care of endian if needed*/);
		if (itMap != s_globalPublishIPs.end()) {
			count = itMap->second;
		}
		if (count > 0) {
			m_trustValue += PUBLISHPOINTSSPERSUBNET / count;
		} else {
			LogKadLine(LOG_DEBUG /*logKadEntryTracking*/, L"Inconsistency in RecalcualteTrustValue()");
			ASSERT(0);
		}
	}
}

double CKeyEntry::GetTrustValue()
{
	// update if last calcualtion is too old, will assert if this entry is not supposed to have a trustvalue
	if (GetCurTick() - m_lastTrustValueCalc > MIN2MS(10)) {
		ReCalculateTrustValue();
	}
	return m_trustValue;
}

void CKeyEntry::CleanUpTrackedPublishers()
{
	if (m_publishingIPs == NULL) {
		return;
	}

	time_t now = time(NULL);
	while (!m_publishingIPs->empty()) {
		// entries are ordered, older ones first
		sPublishingIP curEntry = m_publishingIPs->front();
		if (now - curEntry.m_lastPublish > KADEMLIAREPUBLISHTIMEK) {
			AdjustGlobalPublishTracking(curEntry.m_ip, false, L"cleanup");
			m_publishingIPs->pop_front();
		} else {
			break;
		}
	}
}

void CKeyEntry::WritePublishTrackingDataToFile(CBuffer* data)
{
	// format: <AICH HashCount 2><{AICH Hash Indexed} HashCount> <Names_Count 4><{<Name string><PopularityIndex 4>} Names_Count>
	//		   <PublisherCount 4><{<IP 4><Time 4><AICH Idx 2>} PublisherCount>

	// Write AICH Hashes and map them to a new cleaned up index without unreferenced hashes
	uint16 nNewIdxPos = 0;
	vector<uint16_t> aNewIndexes;
	for (int i = 0; i < m_aAICHHashs.size(); i++)
	{
		if (m_anAICHHashPopularity[i] > 0)
		{
			aNewIndexes.push_back(nNewIdxPos);
			nNewIdxPos++;
		}
		else
			aNewIndexes.push_back(0xFFFF);
	}
	data->WriteValue<uint16_t>(nNewIdxPos);
	for (int i = 0; i < m_aAICHHashs.size(); i++)
	{
		if (m_anAICHHashPopularity[i] > 0)
			data->WriteData(m_aAICHHashs[i].Hash, 20);
	}

	data->WriteValue<uint32_t>(m_filenames.size());
	for (FileNameList::const_iterator it = m_filenames.begin(); it != m_filenames.end(); ++it) {
		data->WriteString(it->m_filename, CBuffer::eUtf8, CBuffer::e16Bit);
		data->WriteValue<uint32_t>(it->m_popularityIndex);
	}

	if (m_publishingIPs != NULL) {
		data->WriteValue<uint32_t>(m_publishingIPs->size());
		for (PublishingIPList::const_iterator it = m_publishingIPs->begin(); it != m_publishingIPs->end(); ++it) {
			ASSERT(it->m_ip != 0);
			data->WriteValue<uint32_t>(it->m_ip);
			data->WriteValue<uint32_t>(it->m_lastPublish);
			uint16_t nIdx = 0xFFFF;
			if (it->m_byAICHHashIdx != 0xFFFF)
			{
				nIdx = aNewIndexes[it->m_byAICHHashIdx];
				ASSERT( nIdx != 0xFFFF );
			}
			data->WriteValue<uint16_t>(nIdx);
		}
	} else {
		ASSERT(0);
		data->WriteValue<uint32_t>(0);
	}
}

void CKeyEntry::ReadPublishTrackingDataFromFile(CBuffer* data, bool bIncludesAICH)
{
	// format: <AICH HashCount 2><{AICH Hash Indexed} HashCount> <Names_Count 4><{<Name string><PopularityIndex 4>} Names_Count>
	//		   <PublisherCount 4><{<IP 4><Time 4><AICH Idx 2>} PublisherCount>	    
	ASSERT( m_aAICHHashs.empty() );
	ASSERT( m_anAICHHashPopularity.empty() );
	if (bIncludesAICH)
	{
		uint16_t nAICHHashCount = data->ReadValue<uint16_t>();
		for (uint16_t i = 0; i < nAICHHashCount; i++)
		{
			SAICHHash hash;
			memcpy(hash.Hash, data->ReadData(20), 20);
			m_aAICHHashs.push_back(hash);
			m_anAICHHashPopularity.push_back(0);
		}
	}

	ASSERT(m_filenames.empty());
	uint32_t nameCount = data->ReadValue<uint32_t>();
	for (uint32_t i = 0; i < nameCount; i++) {
		sFileNameEntry toAdd;
		toAdd.m_filename = data->ReadString(CBuffer::eUtf8, CBuffer::e16Bit);
		toAdd.m_popularityIndex = data->ReadValue<uint32_t>();
		m_filenames.push_back(toAdd);
	}

	ASSERT(m_publishingIPs == NULL);
	m_publishingIPs = new PublishingIPList();
	uint32_t ipCount = data->ReadValue<uint32_t>();
#ifdef _DEBUG
	uint32_t dbgLastTime = 0;
#endif
	for (uint32_t i = 0; i < ipCount; i++) {
		sPublishingIP toAdd;
		toAdd.m_ip = data->ReadValue<uint32_t>();
		ASSERT(toAdd.m_ip != 0);
		toAdd.m_lastPublish = data->ReadValue<uint32_t>();
#ifdef _DEBUG
		ASSERT(dbgLastTime <= (uint32_t)toAdd.m_lastPublish); // should always be sorted oldest first
		dbgLastTime = toAdd.m_lastPublish;
#endif

		// read hash index and update popularity index
		if (bIncludesAICH)
		{
			toAdd.m_byAICHHashIdx = data->ReadValue<uint16_t>();
			if (toAdd.m_byAICHHashIdx != 0xFFFF)
			{
				if (toAdd.m_byAICHHashIdx >= m_aAICHHashs.size())
				{
					// should never happen
					ASSERT( false );
					LogKadLine(LOG_DEBUG /*logKadEntryTracking*/, L"CKeyEntry::ReadPublishTrackingDataFromFile - Out of Index AICH Hash index value while loading keywords");
					toAdd.m_byAICHHashIdx = 0xFFFF;
				}
				else
					m_anAICHHashPopularity[toAdd.m_byAICHHashIdx]++;
			}
		}
		else
			toAdd.m_byAICHHashIdx = 0xFFFF;

		AdjustGlobalPublishTracking(toAdd.m_ip, true, L"");

		m_publishingIPs->push_back(toAdd);
	}
	ReCalculateTrustValue();
#ifdef _DEBUG
//	if (m_aAICHHashs.size() == 1)
//		LogKadLine(LOG_DEBUG /*logKadEntryTracking*/, L"Loaded 1 AICH Hash (%s, publishers %u of %u) for file %s"
//		, m_aAICHHashs[0].GetString(), m_anAICHHashPopularity[0], m_pliPublishingIPs->GetCount(), m_uSourceID.ToHexString());
//	else if (m_aAICHHashs.size() > 1)
//	{
//		LogKadLine(LOG_DEBUG /*logKadEntryTracking*/, L"Loaded multiple (%u) AICH Hashs for file %s, dumping..."
//			, m_aAICHHashs.size(), m_uSourceID.ToHexString());
//		for (int i = 0; i < m_aAICHHashs.size(); i++)
//			LogKadLine(LOG_DEBUG /*logKadEntryTracking*/, L"%s - %u out of %u publishers"
//			, m_aAICHHashs[i].GetString(), m_anAICHHashPopularity[i], m_pliPublishingIPs->GetCount());
//	}
// 	if (GetTrustValue() < 1.0) {
// 		LogLine(logKadEntryTracking, "Loaded %u different names, %u different publishIPs (trustvalue = %.2f) for file %s"
// 			, nameCount, ipCount, GetTrustValue(), m_uSourceID.ToHexString().c_str());
// 	}
#endif
}

void CKeyEntry::DirtyDeletePublishData()
{
	// instead of deleting our publishers properly in the destructor with decreasing the count in the global map 
	// we just remove them, and trust that the caller in the end also resets the global map, so the
	// kad shutdown is speed up a bit
	delete m_publishingIPs;
	m_publishingIPs = NULL;
}

void CKeyEntry::WriteTagListWithPublishInfo(CBuffer* data)
{
	if (m_publishingIPs == NULL || m_publishingIPs->size() == 0) {
		ASSERT(0);
		WriteTagList(data);
		return;
	}

	// here we add a tag including how many publishers this entry has, the trustvalue and how many different names are known
	// this is supposed to get used in later versions as an indicator for the user how valid this result is (of course this tag
	// alone cannot be trusted 100%, because we could be a bad node, but it's a part of the puzzle)

	uint32_t nAdditionalTags = 1;
	if (!m_aAICHHashs.empty())
		nAdditionalTags++;
	WriteTagListInc(data, nAdditionalTags); // write the standard taglist but increase the tagcount by the count we wan to add

	uint32_t trust = (uint16_t)(GetTrustValue() * 100);
	uint32_t publishers = m_publishingIPs->size() & 0xFF /*% 256*/;
	uint32_t names = m_filenames.size() & 0xFF /*% 256*/;
	// 32 bit tag: <namecount uint8><publishers uint8><trustvalue*100 uint16>
	uint32_t tagValue = (names << 24) | (publishers << 16) | trust;
	CTagVarInt(TAG_PUBLISHINFO, tagValue).ToBuffer(data);

	// Last but not least the AICH Hash tag, containing all reported (hopefulley exactly 1) AICH hashes for this file together
	// with the count of publishers who reported it
	if (!m_aAICHHashs.empty()) 
	{
		CBuffer fileAICHTag;
		uint8 byCount = 0;
		// get count of AICH tags with popularity > 0
		for (int i = 0; i < m_aAICHHashs.size(); i++)
		{
			if (m_anAICHHashPopularity[i] > 0)
				byCount++;
			// bobs tags in kad are limited to 255 bytes, so no more than 12 AICH hashes can be written
			// that shouldn't be an issue however, as the normal AICH hash count is 1, if we have more than
			// 10 for some reason we can't use it most likely anyway
			if (1 + (20 * (byCount + 1)) + (1 * (byCount + 1)) > 250)
			{
				LogKadLine(LOG_DEBUG /*logKadEntryTracking*/, L"More than 12(!) AICH Hashs to send for search answer, have to truncate, entry: %s", m_uSourceID.ToHexString().c_str());
				break;
			}
						
		}
		// write tag even on 0 count now
		fileAICHTag.WriteValue<uint8>(byCount);
		uint8 nWritten = 0;
		uint8 j;
		for (j = 0; nWritten < byCount && j < m_aAICHHashs.size(); j++)
		{
			if (m_anAICHHashPopularity[j] > 0)
			{
				fileAICHTag.WriteValue<uint8>(m_anAICHHashPopularity[j]);
				fileAICHTag.WriteData(m_aAICHHashs[j].Hash, 20);
				nWritten++;
			}
		}
		ASSERT( nWritten == byCount && nWritten <= j );
		ASSERT(fileAICHTag.GetSize() <= 255 );
		uint8 nSize = (uint8)fileAICHTag.GetSize();
		byte* byBuffer = fileAICHTag.GetBuffer();
		CTagBsob tag(TAG_KADAICHHASHRESULT, byBuffer, nSize);
		tag.ToBuffer(data);
	}
}


uint16 CKeyEntry::AddRemoveAICHHash(const byte* hash, bool bAdd)
{
	ASSERT( m_aAICHHashs.size() == m_anAICHHashPopularity.size() );
	for (int i = 0; i < m_aAICHHashs.size(); i++)
	{
		if (memcmp(m_aAICHHashs[i].Hash, hash, 20) == 0)
		{
			if (bAdd)
			{
				m_anAICHHashPopularity[i] += 1;
				return (uint16)i;
			}
			else
			{
				if (m_anAICHHashPopularity[i] >= 1)
					m_anAICHHashPopularity[i] -= 1;
				else
					ASSERT( false );
				return (uint16)i;
			}
		}
	}
	if (bAdd)
	{
		m_aAICHHashs.push_back(SAICHHash(hash));
		m_anAICHHashPopularity.push_back(1);
		return (uint16)m_aAICHHashs.size() - 1;
	}
	else
	{
		ASSERT( false );
		return 0xFFFF;
	}
}
