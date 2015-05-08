#include "GlobalHeader.h"
#include "LinkPublisher.h"
#include "../../../NeoCore.h"
#include "../../../FileTransfer/NeoShare/NeoKad.h"
#include "../../../FileTransfer/NeoShare/NeoManager.h"
#include "../../../../Framework/Cryptography/HashFunction.h"
#include "../../../FileList/Hashing/FileHash.h"

#ifndef NO_HOSTERS
#include "../../../FileTransfer/HosterTransfer/HosterLink.h"

CLinkPublisher::CLinkPublisher(CKadAbstract* pItf)
: CFilePublisher(pItf) 
{
}

QVariant CLinkPublisher::PublishEntrys(uint64 FileID, CFileHashPtr pHash)
{
	CKadAbstract* pItf = Itf();

	QVariantMap Request = theCore->m_NeoManager->GetKad()->GetLookupCfg(CNeoKad::ePublish);
	Request["TargetID"] = pItf->MkTargetID(pHash->GetHash());
	Request["CodeID"] = theCore->m_NeoManager->GetKad()->GetCodeID(CodeID());

	//Request["Expiration"] = (uint64)(GetTime() + DAY2S(1)); // K-ToDo-Now: set a proeprly short time, base it on scpirience in how long the client is online


	QVariantList Execute;

	QVariantMap Call;
	Call["Function"] = "secureLinks";
	//Call["ID"] =; 
	QVariantMap Parameters;
	Parameters["PID"] = pItf->MkPubID(FileID); // PubID
	Parameters["HV"] = pHash->GetHash(); // HashValue
	Parameters["HF"] = CFileHash::HashType2Str(pHash->GetType()); // HashType

	//Parameters["EXP"] = // K-ToDo-Now: set a proeprly short time, base it on scpirience in how long the client is online

	Parameters["LL"] = pItf->GetLinks(FileID, pHash); // LinkList

	Call["Parameters"] = Parameters;

	Execute.append(Call);
	Request["Execute"] = Execute;

	Request["GUIName"] = "secureLinks: " + pItf->GetFileInfo(FileID).FileName; // for GUI only

	return Request;
}

bool CLinkPublisher::EntrysFound(uint64 FileID, const QVariantList& Results, bool bDone)
{
	CKadAbstract* pItf = Itf();

	QStringList Urls;
	foreach(const QVariant& vResult, Results)
	{
		QVariantMap Result = vResult.toMap();
		if(Result.isEmpty())
			continue;

		//Result["ID"]
		QString Function = Result["Function"].toString();
		QVariantMap Return = Result["Return"].toMap();
		// K-ToDo: check if hash is really ok

		if(Function == "findLinks")
		{
			foreach(const QVariant& Link, Return["LL"].toList())
			{
				QString Url = Link.toString();
				if(Url.left(8) == "magnet:?")
					Url = CHosterLink::DecryptLinks(Url).first();
				Urls.append(Url);
			}
		}
	}

	pItf->AddLinks(FileID, Urls);
	return true;
}
#endif