#pragma once

#include "kademlia/Kademlia.h"
#include "kademlia/SearchManager.h"

struct SFileInfo;

typedef std::deque<SFileInfo*> KnownFileArray;

///////////////////////////////////////////////////////////////////////////////
// CPublishKeyword

class CPublishKeyword
{
public:
	CPublishKeyword(const wstring& rstrKeyword)
	{
		m_strKeyword = rstrKeyword;
		// min. keyword char is allowed to be < 3 in some cases (see also 'CSearchManager::getWords')
		//ASSERT( rstrKeyword.GetLength() >= 3 );
		ASSERT( !rstrKeyword.empty() );
		KadGetKeywordHash(rstrKeyword, &m_nKadID);
		SetNextPublishTime(0);
		SetPublishedCount(0);
	}

	const Kademlia::CUInt128& GetKadID() const { return m_nKadID; }
	const wstring& GetKeyword() const { return m_strKeyword; }
	int GetRefCount() const { return m_aFiles.size(); }
	const KnownFileArray& GetReferences() const { return m_aFiles; }

	uint32 GetNextPublishTime() const { return m_tNextPublishTime; }
	void SetNextPublishTime(uint32 tNextPublishTime) { m_tNextPublishTime = tNextPublishTime; }

	uint32 GetPublishedCount() const { return m_uPublishedCount; }
	void SetPublishedCount(uint32 uPublishedCount) { m_uPublishedCount = uPublishedCount; }
	void IncPublishedCount() { m_uPublishedCount++; }

	bool AddRef(SFileInfo* pFile) {
		if (std::find(m_aFiles.begin(), m_aFiles.end(), pFile) != m_aFiles.end()) {
			ASSERT(0);
			return false;
		}
		m_aFiles.push_back(pFile);
		return true;
	}

	int RemoveRef(SFileInfo* pFile) {
		KnownFileArray::iterator it = std::find(m_aFiles.begin(), m_aFiles.end(), pFile);
		if (it != m_aFiles.end()) {
			m_aFiles.erase(it);
		}
		return m_aFiles.size();
	}

	void RemoveAllReferences() {
		m_aFiles.clear();
	}

	void RotateReferences(unsigned iRotateSize) {
		if(!m_aFiles.size())
		{
			ASSERT(0);
			return;
		}
		
		unsigned shift = (iRotateSize % m_aFiles.size());
		std::rotate(m_aFiles.begin(), m_aFiles.begin() + shift, m_aFiles.end());
	}

protected:
	wstring m_strKeyword;
	Kademlia::CUInt128 m_nKadID;
	uint32 m_tNextPublishTime;
	uint32 m_uPublishedCount;
	KnownFileArray m_aFiles;
};

///////////////////////////////////////////////////////////////////////////////
// CPublishKeywordList

class CPublishKeywordList
{
public:
	CPublishKeywordList();
	~CPublishKeywordList();

	void AddKeyword(const wstring& keyword, SFileInfo *file);
	void AddKeywords(SFileInfo* pFile);
	void RemoveKeyword(const wstring& keyword, SFileInfo *file);
	void RemoveKeywords(SFileInfo* pFile);
	void RemoveAllKeywords();

	void RemoveAllKeywordReferences();
	void PurgeUnreferencedKeywords();

	int GetCount() const { return m_lstKeywords.size(); }

	CPublishKeyword* GetNextKeyword();
	void ResetNextKeyword();

	uint32 GetNextPublishTime() const { return m_tNextPublishKeywordTime; }
	void SetNextPublishTime(uint32 tNextPublishKeywordTime) { m_tNextPublishKeywordTime = tNextPublishKeywordTime; }

protected:
	// can't use a CMap - too many disadvantages in processing the 'list'
	//CTypedPtrMap<CMapStringToPtr, CString, CPublishKeyword*> m_lstKeywords;
	typedef std::list<CPublishKeyword*> CKeyWordList;
	CKeyWordList m_lstKeywords;
	CKeyWordList::iterator m_posNextKeyword;
	uint32 m_tNextPublishKeywordTime;

	CPublishKeyword* FindKeyword(const wstring& rstrKeyword, CKeyWordList::iterator* ppos = NULL);
};
