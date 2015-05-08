#include "GlobalHeader.h"
#include "FilePublisher.h"
#include "../../../NeoCore.h"
#include "../../../FileTransfer/NeoShare/NeoKad.h"
#include "../../../FileTransfer/NeoShare/NeoManager.h"
#include "../../../Interface/InterfaceManager.h"

CFilePublisher::CFilePublisher(CKadAbstract* pItf)
: QObjectEx(pItf) 
{
}

CFilePublisher::~CFilePublisher()
{
	qDeleteAll(m_Publishments);
	qDeleteAll(m_Searches);
}

void CFilePublisher::Process(UINT Tick)
{
	CKadAbstract* pItf = Itf();

	foreach(SKadSearch* pSearch, m_Searches)
	{
		QByteArray LookupID = pSearch->LookupID;
		QVariantMap Request;
		Request["LookupID"] = LookupID;
		Request["AutoStop"] = true; // stop if the lookup is finished
		QVariantMap Response = theCore->m_Interfaces->RemoteProcedureCall("NeoKad", "QueryLookup", Request).toMap();

		//QVariantMap LookupInfo = Response["Info"].toMap();

		bool bDone = Response["Staus"] == "Finished";
		if(!EntrysFound(pSearch->FileID, Response["Results"].toList(), bDone))
		{
			pItf->StopLookup(pSearch->LookupID);	
			bDone = true;
		}
		if(bDone)
		{
			m_Searches.remove(pSearch->FileID);
			delete pSearch;
		}
	}

	foreach(SPub* pPub, m_Publishments)
	{
		int Pending = 0;
		time_t ExpirationTime = 0;
		int StoreCount = 0;
		foreach(SHPub* pRes, pPub->Lookups)
		{
			if(!pRes->LookupID.isEmpty())
			{
				if(pItf->HandleLookup(pRes->LookupID, pRes->ExpirationTime, pRes->StoreCount)) // true means finished
					pRes->LookupID.clear();
				else
					Pending++;
			}
			if(ExpirationTime == 0 || ExpirationTime > pRes->ExpirationTime)
				ExpirationTime = pRes->ExpirationTime;
			if(StoreCount == 0 || StoreCount > pRes->StoreCount)
				StoreCount = pRes->StoreCount;
		}

		if(Pending == 0)
		{
			uint64 FileID = m_Publishments.key(pPub);
			QMultiMap<EFileHashType, CFileHashPtr> Hashes;
			foreach(SHPub* pRes, pPub->Lookups)
				Hashes.insert(pRes->pHash->GetType(), pRes->pHash);
			pItf->Update(FileID, Index(), ExpirationTime, StoreCount, Hashes);
			delete m_Publishments.take(FileID);
		}
	}

	if(m_Publishments.size() > pItf->MaxLookups())
		return;

	uint64 FileID = 0;
	QMultiMap<EFileHashType, CFileHashPtr> Hashes;

	const QMap<uint64, SKadFile*> Files = pItf->GetFiles();
	QMap<uint64, SKadFile*>::const_iterator I = Files.find(m_LastFileID);
	if(I == Files.end())	I = Files.begin();
	else					I++;
	for(;I != Files.end(); I++)
	{
		if(m_Publishments.contains(I.key()))
			continue;
		FileID = I.value()->FileID;
		Hashes = pItf->IsOutdated(FileID, Index());
		if(!Hashes.isEmpty())
			break;
	}
	if(I == Files.end())
	{
		m_LastFileID = 0;
		return;
	}
	m_LastFileID = I.key();


	SPub* pPub = new SPub();
	foreach(CFileHashPtr pHash, Hashes) // we may publish under multiple target ID's as we have multiple file hashes
	{
		QVariant Request = PublishEntrys(FileID, pHash);
		if(Request.isValid()) // is there anythign to do?
		{
			QByteArray LookupID = pItf->StartLookup(Request);
			if(!LookupID.isEmpty())
				pPub->Lookups.append(new SHPub(LookupID, pHash));
		}
	}
	m_Publishments.insert(m_LastFileID, pPub);
}

bool CFilePublisher::Find(uint64 FileID, CFileHash* pFileHash, const QString& Function, const QString& FileName)
{
	CKadAbstract* pItf = Itf();

	EFileHashType HashType = pFileHash->GetType();
	QByteArray Hash = pFileHash->GetHash();

	QVariantMap Request = theCore->m_NeoManager->GetKad()->GetLookupCfg();
	Request["TargetID"] = pItf->MkTargetID(Hash);
	Request["CodeID"] = theCore->m_NeoManager->GetKad()->GetCodeID(CodeID());

	QVariantList Execute;
	QVariantMap Find;
	Find["Function"] = Function;
	//Find["ID"] =;
	QVariantMap Parameters;
	Parameters["HV"] = Hash; // HashValue
	Parameters["HF"] = CFileHash::HashType2Str(HashType); // HashType

	//Parameters["Count"] =  // K-ToDo

	Find["Parameters"] = Parameters;
	Execute.append(Find);
	Request["Execute"] = Execute;

	Request["GUIName"] = Function + ": " + FileName; // for GUI only

	QByteArray LookupID = pItf->StartLookup(Request);
	if(!LookupID.isEmpty())
	{
		m_Searches.insert(FileID, new SKadSearch(FileID, LookupID, Function));
		return true;
	}
	return false;
}

bool CFilePublisher::IsFinding(uint64 FileID, const QString& Function)
{
	for(QMultiMap<uint64, SKadSearch*>::iterator I = m_Searches.find(FileID); I != m_Searches.end() && I.key() == FileID; I++)
	{
		if((*I)->Function == Function)
			return true;
	}
	return false;
}