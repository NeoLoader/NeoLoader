#include "GlobalHeader.h"
#include "KeywordPublisher.h"
#include "../../../NeoCore.h"
#include "../../../FileTransfer/NeoShare/NeoKad.h"
#include "../../../FileTransfer/NeoShare/NeoManager.h"
#include "../../../../Framework/Cryptography/HashFunction.h"
#include "../../../FileList/Hashing/FileHash.h"


CKeywordPublisher::CKeywordPublisher(CKadAbstract* pItf)
: QObjectEx(pItf) 
{
}

CKeywordPublisher::~CKeywordPublisher()
{
	qDeleteAll(m_Publishments);
}

void CKeywordPublisher::Process(UINT Tick)
{
	CKadAbstract* pItf = Itf();

	foreach(SPub* pPub, m_Publishments)
	{
		if(pItf->HandleLookup(pPub->LookupID, pPub->ExpirationTime, pPub->StoreCount)) // true means finished
		{
			QString Keyword = m_Publishments.key(pPub);
			foreach(uint64 FileID, pPub->Files)
			{
				if(SKadFile* pFile = pItf->GetFiles().value(FileID))
				{
					pFile->Keywords[Keyword].first = pPub->ExpirationTime;
					pFile->Keywords[Keyword].second = pPub->StoreCount;
					// Note: we set worst result as file result, its used only to resume after the client was closed befoure all was published
					time_t ExpirationTime = 0;
					int StoreCount = 0;
					for(QMap<QString, QPair<time_t, int> >::iterator I = pFile->Keywords.begin(); I != pFile->Keywords.end(); I++)
					{
						if(ExpirationTime == 0 || ExpirationTime > I.value().first)
							ExpirationTime = I.value().first;
						if(StoreCount == 0 || StoreCount > I.value().second)
							StoreCount = I.value().second;
					}
					if(ExpirationTime != 0) // 0 means not every keyword was published, dont update in that case
						pItf->Update(FileID, CKadAbstract::eKeyword, ExpirationTime, StoreCount);
				}
			}
			delete m_Publishments.take(m_Publishments.key(pPub));
		}
	}

	if(m_Publishments.size() > pItf->MaxLookups())
		return;

	SKadKeyword* pKeyword = NULL;
	QList<uint64> Files;

	const QMap<QString, SKadKeyword*> Keywords =  pItf->GetKeywords();
	QMap<QString, SKadKeyword*>::const_iterator I = Keywords.find(m_LastKeyword);
	if(I == Keywords.end())	I = Keywords.begin();
	else					I++;
	time_t uNow = GetTime();
	for(;I != Keywords.end(); I++)
	{
		if(m_Publishments.contains(I.key()))
			continue;
		pKeyword = I.value();
		int Count = 0;
		foreach(SKadFile* pFile, pKeyword->Files)
		{
			time_t ExpirationTime = pFile->Keywords[pKeyword->Keyword].first;
			if(uNow > ExpirationTime || ExpirationTime - uNow < HR2S(1))  // K-ToDo-Now: customise
			{
				Files.append(pFile->FileID);
				if(++Count >= 100) // Note: we never publish more than 100 files per batch, 
					break; // if we abort here during the next run of this keyword we weil take the next files as this one wil have got updated dates already
			}
		}
		if(!Files.isEmpty())
			break;
	}
	if(I == Keywords.end())
	{
		m_LastKeyword.clear();
		return;
	}
	m_LastKeyword = I.key();
	

	QVariant Request = PublishEntrys(pKeyword->Keyword, Files);
	if(Request.isValid()) // is there anythign to do?
	{
		QByteArray LookupID = pItf->StartLookup(Request);
		if(!LookupID.isEmpty())
		{
			SPub* pPub = new SPub(LookupID);
			pPub->Files = Files;
			m_Publishments.insert(m_LastKeyword, pPub);
		}
	}
}

QVariant CKeywordPublisher::PublishEntrys(const QString& Keyword, const QList<uint64>& Files)
{
	CKadAbstract* pItf = Itf();

	QByteArray Hash = CHashFunction::Hash(Keyword.toUtf8(), CHashFunction::eSHA256);

	QVariantMap Request = theCore->m_NeoManager->GetKad()->GetLookupCfg(CNeoKad::ePublish);
	Request["TargetID"] = pItf->MkTargetID(Hash);
	Request["CodeID"] = theCore->m_NeoManager->GetKad()->GetCodeID("KeywordIndex");

	//Request["Expiration"] = // K-ToDo-Now: set a proeprly long time

	QVariantList Execute;
	QVariantMap Call;
	Call["Function"] = "publishKeyword";
	//Call["ID"] =; 
	QVariantMap Parameters;
	Parameters["KW"] = Keyword;
	QVariantList FileList;
	foreach(uint64 FileID, Files)
	{
		SFileInto sFile = pItf->GetFileInfo(FileID);

		if(sFile.HashMap.isEmpty())
			continue; // we may have a file without a hash if its a torretn subfile

		QVariantMap File;
		File["PID"] = pItf->MkPubID(FileID); // PubID

		// Note: we try to keep trace of the initial release ID
		//File["RID"] = pFile->GetProperty("ReleaseID", pFileRef->PubID);

		File["FN"] = sFile.FileName; // FileName
		File["FS"] = sFile.FileSize; // FileSize

		File["HM"] = sFile.HashMap; // HashMap

		// X-TODO-P: add availability info
		// K-ToDo-Now: add more informations

		FileList.append(File);
	}
	if(FileList.isEmpty())
		return QVariant(); // Nothing todo here :/

	Parameters["FL"] = FileList;
	Call["Parameters"] = Parameters;
	Execute.append(Call);
	Request["Execute"] = Execute;

	Request["GUIName"] = "publishKeyword: " + Keyword; // for GUI only

	return Request;
}
