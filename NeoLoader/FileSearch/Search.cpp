#include "GlobalHeader.h"
#include "Search.h"
#include "SearchManager.h"
#include "../FileList/File.h"
#include "../FileList/FileDetails.h"
#include "../FileList/FileStats.h"
#include "../FileTransfer/HashInspector.h"
#include "../FileList/FileManager.h"
#include "../FileList/Hashing/UntrustedFileHash.h"
#include "../NeoCore.h"
#include "../FileTransfer/BitTorrent/Torrent.h"
#include "../FileTransfer/BitTorrent/TorrentInfo.h"
#ifndef NO_HOSTERS
#include "../FileTransfer/HosterTransfer/WebManager.h"
#include "../FileTransfer/HosterTransfer/WebCrawler.h"
#endif
#include "Collection.h"

CSearch::CSearch(ESearchNet SearchNet, QObject* qObject)
 : CFileList(qObject)
{
	m_SearchNet = SearchNet;
}

CSearch::CSearch(ESearchNet SearchNet, const QString& Expression, const QVariantMap& Criteria, QObject* qObject)
 : CFileList(qObject)
{
	m_Expression = Expression;
	m_Criteria = Criteria;

	m_SearchNet = SearchNet;
}

void CSearch::Process(UINT Tick)
{
	CFileList::Process(Tick);

	if((Tick & EPerSec) == 0)
		return;
	
	foreach(CFile* pFile, m_FileMap)
	{
		if(!pFile->IsStarted())
			continue;

		CTorrent* pTorrent = pFile->GetTopTorrent();
		if((!pFile->MetaDataMissing()
#ifndef NO_HOSTERS
		 || pFile->IsRawArchive()
#endif
		 ) && (!pTorrent || !pTorrent->GetInfo()->IsEmpty()))
			pFile->Stop();
	}
}

bool CSearch::AddFoundFile(CFile* pFile)
{
	QList<CFile*> KnownFiles = theCore->m_FileManager->FindDuplicates(pFile, true);
	if(!KnownFiles.isEmpty())
	{
		//ASSERT(KnownFiles.count() == 1);
		CFile* pFoundFile = KnownFiles.at(0);

		if(pFoundFile->IsRemoved())
			pFile->SetProperty("KnownStatus", "Removed");
		else if(pFoundFile->IsComplete())
			pFile->SetProperty("KnownStatus", "Complete");
		else
			pFile->SetProperty("KnownStatus", "Incomplete");
	}

	QList<CFile*> FoundFiles = FindDuplicates(pFile, true);
	if(FoundFiles.size() == 1)
	{
		// Note: here we prefer to delete the new file to prevent unnececery list updated in the gui
		MergeFiles(pFile, FoundFiles.first());

		delete pFile;
		return false;
	}
	
	// Note: if there are more than 1 files mathcing the hash of the current file
	//			we take the enw file and drop the duplicates
	foreach(CFile* pFoundFile, FoundFiles)
	{
		MergeFiles(pFoundFile, pFile);

		// handle parrents if present
		foreach(uint64 FileID, pFile->GetParentFiles())
		{
			if(CFile* pParentFile = GetFileByID(FileID))
				pParentFile->ReplaceSubFile(pFoundFile, pFile);
		}

		// remove old file
		pFoundFile->Remove(true);
	}

#ifndef NO_HOSTERS
	pFile->GetDetails()->Collect();
#endif

	connect(pFile->GetDetails(), SIGNAL(Update()), this, SLOT(OnUpdate()));
	CFileList::AddFile(pFile);
	
	return true;
}

void CSearch::MergeFiles(CFile* pFromFile, CFile* pToFile)
{
	pToFile->GetDetails()->Merge(pFromFile->GetDetails());

	// Update Hashes
	foreach(CFileHashPtr pHash, pFromFile->GetAllHashes(true))
		pToFile->GetInspector()->AddAuxHash(pHash);
	pToFile->GetInspector()->AddHashesToFile();
	pToFile->SelectMasterHash();
}

uint64 CSearch::GrabbFile(CFile* pFile)
{
	pFile->Stop();
	CFile* pNewFile = new CFile();

	// Note: the search is not like the grabber it has its own files and thay stay in search, 
	//			thay also ar not allowed to be started due to possible torrent info hash conflicts in the manager
	//			so we make an exact copy and grab thise

	QVariantMap File = pFile->Store();
	File["ID"] = 0; // ensure new unique file ID will be assigned
	pNewFile->Load(File);
	pFile->SetFileDir("");

	if(theCore->m_FileManager->GrabbFile(pNewFile))
	{
		pFile->SetProperty("KnownStatus", "Incomplete");
		return pNewFile->GetFileID();
	}

	delete pNewFile;
	return 0;
}

//QVariantMap CSearch::GetAllCriterias()
//{
//	QVariantMap Criteria;
//	foreach(const QByteArray& Name, dynamicPropertyNames())
//		Criteria[Name] = property(Name);
//	return Criteria;
//}

//void CSearch::SetAllCriterias(const QVariantMap& Criteria)
//{
//	foreach(const QString& Name, Criteria.keys())
//		setProperty(Name.toLatin1(), Criteria[Name]);
//}

QString CSearch::GetSearchNetStr(ESearchNet SearchNet)
{
	switch(SearchNet)
	{
		case eSmartAgent:	return "SmartAgent";
		case eNeoKad:		return "NeoKad";
		case eMuleKad:		return "MuleKad";
		case eEd2kServer:	return "Ed2kServer";
		case eWebSearch:	return "WebSearch";
	}
	return "";
}

ESearchNet CSearch::GetSearchNet(const QString& SearchNetStr)
{
	if(SearchNetStr == "NeoKad")		return eNeoKad;
	if(SearchNetStr == "MuleKad")		return eMuleKad;
	if(SearchNetStr == "Ed2kServer")	return eEd2kServer;
	if(SearchNetStr == "WebSearch")		return eWebSearch;
	if(SearchNetStr == "SmartAgent")	return eSmartAgent;
	return eInvalid;
}

// Load/Store
QVariant CSearch::Store()
{
	QVariantMap Data;
	Data["SearchNet"] = CSearch::GetSearchNetStr(GetSearchNet());
	Data["Expression"] = m_Expression;
	Data["Criterias"] = m_Criteria;
	Data["Files"] = CFileList::Store();
	return Data;		
}

int CSearch::Load(const QVariantMap& Data)
{
	m_Expression = Data["Expression"].toString();
	m_Criteria = Data["Criterias"].toMap();
	return CFileList::Load(Data["Files"].toList());
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

CWebSearch::CWebSearch(ESearchNet SearchNet, QObject* qObject)
 : CSearch(SearchNet, qObject)
{
}

CWebSearch::CWebSearch(ESearchNet SearchNet, const QString& Expression, const QVariantMap& Criteria, QObject* qObject)
 : CSearch(SearchNet, Expression, Criteria, qObject)
{
}

#ifndef NO_HOSTERS
void CWebSearch::AddCollection(const QString& SourceID, const QVariantMap& Collection, int Priority)
{
	CrawlUrls(Collection["crawlUrls"].toStringList());
}

//void CWebSearch::AddUrls(const QStringList& Urls)
//{
//	CrawlUrls(Urls);
//}

void CWebSearch::CrawlUrls(const QStringList& Urls, CCollection* pCollection)
{
	foreach(const QString& Url, Urls)
	{
		if(!m_CrawledUrls.contains(Url))
		{
			// Note: we crawl collections imminetly we dont queue this requests
			theCore->m_WebManager->GetCrawler()->CrawlSite(Url, this, pCollection != NULL, pCollection);
			m_CrawledUrls.insert(Url);
		}
	}
}
#endif