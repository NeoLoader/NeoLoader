//
// This file is part of the MuleKad Project.
//
// Copyright (c) 2012 David Xanatos ( XanatosDavid@googlemail.com )
// Copyright (c) 2003-2011 aMule Team ( admin@amule.org / http://www.amule.org )
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
#include "KadHandler.h"
#include "KeywordPublisher.h"

CPublishKeywordList::CPublishKeywordList()
{
	ResetNextKeyword();
	SetNextPublishTime(0);
}

CPublishKeywordList::~CPublishKeywordList()
{
	RemoveAllKeywords();
}

CPublishKeyword* CPublishKeywordList::GetNextKeyword()
{
	if (m_posNextKeyword == m_lstKeywords.end()) {
		m_posNextKeyword = m_lstKeywords.begin();
		if (m_posNextKeyword == m_lstKeywords.end()) {
			return NULL;
		}
	}
	return *m_posNextKeyword++;
}

void CPublishKeywordList::ResetNextKeyword()
{
	m_posNextKeyword = m_lstKeywords.begin();
}

CPublishKeyword* CPublishKeywordList::FindKeyword(const wstring& rstrKeyword, CKeyWordList::iterator* ppos)
{
	CKeyWordList::iterator it = m_lstKeywords.begin();
	for (; it != m_lstKeywords.end(); ++it) {
		CPublishKeyword* pPubKw = *it;
		if (pPubKw->GetKeyword() == rstrKeyword) {
			if (ppos) {
				(*ppos) = it;
			}

			return pPubKw;
		}
	}
	
	return NULL;
}

void CPublishKeywordList::AddKeyword(const wstring& keyword, SFileInfo *file)
{
	CPublishKeyword* pubKw = FindKeyword(keyword);
	if (pubKw == NULL) {
		pubKw = new CPublishKeyword(keyword);
		m_lstKeywords.push_back(pubKw);
		SetNextPublishTime(0);
	}
	pubKw->AddRef(file);
}

void CPublishKeywordList::AddKeywords(SFileInfo* pFile)
{
	Kademlia::WordList wordlist;
	Kademlia::CSearchManager::GetWords(pFile->sName, &wordlist);
	Kademlia::WordList::const_iterator it;
	for (it = wordlist.begin(); it != wordlist.end(); ++it) {
		AddKeyword(*it, pFile);
	}
}

void CPublishKeywordList::RemoveKeyword(const wstring& keyword, SFileInfo *file)
{
	CKeyWordList::iterator pos;
	CPublishKeyword* pubKw = FindKeyword(keyword, &pos);
	if (pubKw != NULL) {
		if (pubKw->RemoveRef(file) == 0) {
			if (pos == m_posNextKeyword) {
				++m_posNextKeyword;
			}
			m_lstKeywords.erase(pos);
			delete pubKw;
			SetNextPublishTime(0);
		}
	}
}

void CPublishKeywordList::RemoveKeywords(SFileInfo* pFile)
{
	Kademlia::WordList wordlist;
	Kademlia::CSearchManager::GetWords(pFile->sName, &wordlist);
	Kademlia::WordList::const_iterator it;
	for (it = wordlist.begin(); it != wordlist.end(); ++it) {
		RemoveKeyword(*it, pFile);
	}
}


void CPublishKeywordList::RemoveAllKeywords()
{
	CKeyWordList::iterator it = m_lstKeywords.begin();
	for (; it != m_lstKeywords.end(); ++it) {
		delete (*it);
	}
	ResetNextKeyword();
	SetNextPublishTime(0);
}


void CPublishKeywordList::RemoveAllKeywordReferences()
{
	CKeyWordList::iterator it = m_lstKeywords.begin();
	for (; it != m_lstKeywords.end(); ++it) {
		(*it)->RemoveAllReferences();
	}
}


void CPublishKeywordList::PurgeUnreferencedKeywords()
{
	CKeyWordList::iterator it = m_lstKeywords.begin();
	while (it != m_lstKeywords.end()) {
		CPublishKeyword* pPubKw = *it;
		if (pPubKw->GetRefCount() == 0) {
			if (it == m_posNextKeyword) {
				++m_posNextKeyword;
			}
			m_lstKeywords.erase(it++);
			delete pPubKw;
			SetNextPublishTime(0);
		} else {
			++it;
		}
	}
}