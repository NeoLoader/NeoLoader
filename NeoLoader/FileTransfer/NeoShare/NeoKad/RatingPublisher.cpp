#include "GlobalHeader.h"
#include "RatingPublisher.h"
#include "../../../NeoCore.h"
#include "../../../FileTransfer/NeoShare/NeoKad.h"
#include "../../../FileTransfer/NeoShare/NeoManager.h"
#include "../../../../Framework/Cryptography/HashFunction.h"
#include "../../../FileList/Hashing/FileHash.h"

CRatingPublisher::CRatingPublisher(CKadAbstract* pItf)
: CFilePublisher(pItf) 
{
}

QVariant CRatingPublisher::PublishEntrys(uint64 FileID, CFileHashPtr pHash)
{
	CKadAbstract* pItf = Itf();

	// Note: we publish ratings only under the neo hash soa file must have that hash to be elegable for rating publishment
	ASSERT(pHash->GetType() == HashNeo || pHash->GetType() == HashXNeo);

	QVariantMap Request = theCore->m_NeoManager->GetKad()->GetLookupCfg(CNeoKad::ePublish);
	Request["TargetID"] = pItf->MkTargetID(pHash->GetHash());
	Request["CodeID"] = theCore->m_NeoManager->GetKad()->GetCodeID(CodeID());


	QVariantList Execute;
	QVariantMap Call;
	Call["Function"] = "publishRating";
	//Call["ID"] =; 

	QVariantMap Parameters;
	Parameters["PID"] = pItf->MkPubID(FileID); // PubID
	Parameters["HV"] = pHash->GetHash(); // HashValue
	Parameters["HF"] = CFileHash::HashType2Str(pHash->GetType()); // HashType

	//Parameters["Expiration"] = // K-ToDo-Now: set a proeprly short time, base it on scpirience in how long the client is online

	QVariantMap Rating = pItf->GetRating(FileID);
	Parameters["FN"] = Rating["FileName"]; // FileName
	Parameters["FD"] = Rating["Description"]; // FileDescription
	Parameters["FR"] = Rating["Rating"]; // FileRating
	if(Rating.contains("CoverUrl"))
		Parameters["CU"] = Rating["CoverUrl"]; // CoverUrl

	Call["Parameters"] = Parameters;
	Execute.append(Call);
	Request["Execute"] = Execute;

	Request["GUIName"] = "publishRating: " + Rating["FileName"].toString(); // for GUI only

	return Request;
}

bool CRatingPublisher::EntrysFound(uint64 FileID, const QVariantList& Results, bool bDone)
{
	CKadAbstract* pItf = Itf();

	QVariantList Notes;
	foreach(const QVariant& vResult, Results)
	{
		QVariantMap Result = vResult.toMap();
		if(Result.isEmpty())
			continue;

		//Result["ID"]
		QString Function = Result["Function"].toString();
		QVariantMap Return = Result["Return"].toMap();
		// K-ToDo: check if hash is really ok

		if(Function == "findRatings")
			Notes = Return["RL"].toList();
	}

	pItf->AddRatings(FileID, Notes, bDone);
	return true;
}