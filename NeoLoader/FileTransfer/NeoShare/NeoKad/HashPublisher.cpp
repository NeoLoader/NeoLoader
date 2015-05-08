#include "GlobalHeader.h"
#include "HashPublisher.h"
#include "../../../NeoCore.h"
#include "../../../FileTransfer/NeoShare/NeoKad.h"
#include "../../../FileTransfer/NeoShare/NeoManager.h"
#include "../../../../Framework/Cryptography/HashFunction.h"
#include "../../../FileList/Hashing/FileHashSet.h"
#include "../../../FileList/Hashing/FileHashTree.h"
#include "../../../FileList/Hashing/FileHashTreeEx.h"
#include "../../../FileList/Hashing/HashingThread.h"
#include "../../../../Framework/Exception.h"
#include "../../../../Framework/Xml.h"

CHashPublisher::CHashPublisher(CKadAbstract* pItf)
: CFilePublisher(pItf) 
{
}

QVariant CHashPublisher::PublishEntrys(uint64 FileID, CFileHashPtr pHash)
{
	CKadAbstract* pItf = Itf();

	QStringList Functions;

	uint64 PubID = pItf->MkPubID(FileID);

	QVariantMap Request = theCore->m_NeoManager->GetKad()->GetLookupCfg(CNeoKad::ePublish);
	Request["TargetID"] = pItf->MkTargetID(pHash->GetHash());
	Request["CodeID"] = theCore->m_NeoManager->GetKad()->GetCodeID(CodeID());

	Request["Expiration"] = (uint64)(GetTime() + DAY2S(1)); // K-ToDo-Now: set a proeprly short time, base it on scpirience in how long the client is online

	SFileInto FileInfo = pItf->GetFileInfo(FileID);

	QVariantList Execute;
	
	//////////////////////////////////////////////////////////////////////////////////////


	if(pItf->IsComplete(FileID) && FileInfo.HashMap.size() > 1) // alias publishing, if the file has more than one valid hash publish the other hashes
	{
		QVariantMap Call;

		Call["Function"] = "publishAlias";
		//Call["ID"] =; 
		QVariantMap Parameters;
		Parameters["PID"] = PubID; // PubID
		
		Parameters["FN"] = FileInfo.FileName; // FileName
		Parameters["FS"] = FileInfo.FileSize; // FileSize

		Parameters["HM"] = FileInfo.HashMap; // HashMap

		Call["Parameters"] = Parameters;
		Execute.append(Call);

		Functions.append("publishAlias");
	}

	//////////////////////////////////////////////////////////////////////////////////////

	QVariantList ParentFiles;
	foreach(uint64 ParentFileID, pItf->GetParentFiles(FileID))
	{
		if(!pItf->IsComplete(ParentFileID))
			continue;

		SFileInto ParentFile = pItf->GetFileInfo(ParentFileID);
		if(ParentFile.HashMap.isEmpty())
			continue;

		QVariantMap File;
		File["FN"] = ParentFile.FileName; // FileName
		File["FS"] = ParentFile.FileSize; // FileSize

		File["HM"] = ParentFile.HashMap; // HashMap

		ParentFiles.append(File);
	}

	if(!ParentFiles.isEmpty()) // check if we actualy have aprrent files
	{
		QVariantMap Call;

		Call["Function"] = "publishNesting";
		//Call["ID"] =; 
		QVariantMap Parameters;
		Parameters["PID"] = PubID; // PubID

		Parameters["FN"] = FileInfo.FileName; // FileName
		Parameters["FS"] = FileInfo.FileSize; // FileSize

		Parameters["HM"] = FileInfo.HashMap; // HashMap

		Parameters["FL"] = ParentFiles; // Parent File List

		Call["Parameters"] = Parameters;
		Execute.append(Call);

		Functions.append("publishNesting");
	}

	//////////////////////////////////////////////////////////////////////////////////////

	if(pHash->GetType() == HashXNeo) // Thats a Neo Multi File
	{
		uint64 Offset = 0;
		QVariantList SubFiles;
		foreach(uint64 SubFileID, pItf->GetSubFiles(FileID))
		{
			SFileInto SubFile = pItf->GetFileInfo(SubFileID, FileID);

			QVariantMap File;
			File["OF"] = Offset; // FileOffset
			File["FS"] = SubFile.FileSize; // FileSize
			Offset += SubFile.FileSize;

			File["FN"] = SubFile.FileName; // FileName
			if(!SubFile.FileDirectory.isEmpty())
				File["FD"] = SubFile.FileDirectory; // FileDirectory
			File["HM"] = SubFile.HashMap; // HashMap

			// Sanity check
			if(!SubFile.HashMap.contains("neo"))
			{
				ASSERT(0); // this should not happen
				SubFiles.clear();
				break;
			}

			SubFiles.append(File);
		}

		if(!SubFiles.isEmpty()) // make sure the MultiFile is still compalte and not missign anything
		{
			QVariantMap Call;

			Call["Function"] = "publishIndex";
			//Call["ID"] =; 
			QVariantMap Parameters;
			Parameters["PID"] = PubID; // PubID

			Parameters["FN"] = FileInfo.FileName; // FileName
			Parameters["FS"] = FileInfo.FileSize; // FileSize

			Parameters["HM"] = FileInfo.HashMap; // HashMap

			Parameters["SF"] = SubFiles;

			if(CFileHashTreeEx* pHashTreeEx = qobject_cast<CFileHashTreeEx*>(pHash.data()))
			{
				Parameters["MH"] = pHashTreeEx->GetMetaHash();
				Parameters["RH"] = pHashTreeEx->GetRootHash();
			}

			Call["Parameters"] = Parameters;
			Execute.append(Call);

			Functions.append("publishIndex");
		}
	}

	//////////////////////////////////////////////////////////////////////////////////////

	CFileHashEx* pHashEx = qobject_cast<CFileHashEx*>(pHash.data());
	if((pHash->GetType() == HashXNeo || pHash->GetType() == HashNeo) && pHashEx && pHashEx->CanHashParts()) // Publish Neo Hash Sets
	{
		QVariantMap Call;

		Call["Function"] = "publishHash";
		//Call["ID"] =; 
		QVariantMap Parameters;
		Parameters["PID"] = PubID; // PubID
		Parameters["HF"] = CFileHash::HashType2Str(pHash->GetType()); // HashType
		Parameters["HV"] = pHash->GetHash(); // HashValue
		Parameters["FS"] = FileInfo.FileSize; // FileSize

		if(CFileHashTree* pHashTree = qobject_cast<CFileHashTree*>(pHash.data()))
		{
			Parameters["HT"] = pHashTree->SaveBin();
			
			if(CFileHashTreeEx* pHashTreeEx = qobject_cast<CFileHashTreeEx*>(pHash.data()))
				Parameters["MH"] = pHashTreeEx->GetMetaHash(); // MetaHash
		}
		else if(CFileHashSet* pHashSet = qobject_cast<CFileHashSet*>(pHash.data())) // Note this code could also handle ed2k hashsets
			Parameters["HS"] = pHashSet->SaveBin(); // HashSet

		Call["Parameters"] = Parameters;
		Execute.append(Call);

		Functions.append("publishHash");
	}
		
	//////////////////////////////////////////////////////////////////////////////////////

	Request["GUIName"] = Functions.join(";") + ": " + FileInfo.FileName; // for GUI only

	if(Execute.isEmpty())
		return QVariant(); // Nothing todo here :/

	Request["Execute"] = Execute;

	return Request;
}

bool CHashPublisher::EntrysFound(uint64 FileID, const QVariantList& Results, bool bDone)
{
	CKadAbstract* pItf = Itf();

	foreach(const QVariant& vResult, Results)
	{
		QVariantMap Result = vResult.toMap();
		if(Result.isEmpty())
			continue;

		//Result["ID"]
		QString Function = Result["Function"].toString();
		QVariantMap Return = Result["Return"].toMap();
		// K-ToDo: check if hash is really ok

		if(Function == "findAliases")
		{
			uint64 uFileSize = Return["FS"].toULongLong(); // FileSize
			QVariantMap HashMap = Return["HM"].toMap(); // HashMap

			pItf->AddAuxHashes(FileID, uFileSize, HashMap);
		}
		else if(Function == "findIndex")
		{
			QMultiMap<EFileHashType, CFileHashPtr> Hashes = pItf->GetHashes(FileID);
			CFileHashTreeEx* pHashTreeEx = qobject_cast<CFileHashTreeEx*>(Hashes.value(HashXNeo).data());
			if(!pHashTreeEx)
				continue;
			
			if(!pHashTreeEx->Validate(Return["MH"].toByteArray(), Return["RH"].toByteArray()))
			{
				LogLine(LOG_ERROR | LOG_DEBUG, tr("Recived invalid Neo Meta Hash"));
				continue;
			}

			QString FileName = Return["FN"].toString(); // FileName
			uint64 uFileSize = Return["FS"].toULongLong(); // FileSize

			QList<SFileInto> Files;
			uint64 uTotalSize = 0;
			CBuffer MetaData;
			foreach(const QVariant vFile, Return["SF"].toList()) // SubFiles
			{
				QVariantMap File = vFile.toMap();

				SFileInto FileInfo;
				//				 File["OF"].toULongLong();  // FileOffset
				FileInfo.FileSize = File["FS"].toULongLong(); // FileSize

				FileInfo.FileName = File["FN"].toString(); // FileName
				FileInfo.FileDirectory = File["FD"].toString(); // FileDirectory

				FileInfo.HashMap = File["HM"].toMap(); // HashMap
				Files.append(FileInfo);

				uTotalSize += FileInfo.FileSize;


				QByteArray FileHash = FileInfo.HashMap[CFileHash::HashType2Str(HashNeo)].toByteArray();
				MetaData.WriteValue<uint64>(FileInfo.FileSize);
				MetaData.WriteQData(FileHash);
			}
			if(pHashTreeEx->HashMetaData(MetaData.ToByteArray()) != Return["MH"].toByteArray())
			{
				LogLine(LOG_ERROR | LOG_DEBUG, tr("Recived invalid Neo Meta Data"));
				continue;
			}
			if(uFileSize != uTotalSize)
			{
				LogLine(LOG_ERROR | LOG_DEBUG, tr("Recived inconsistent Neo Meta Data"));
				continue;
			}

			pItf->SetupIndex(FileID, FileName, uFileSize, Files);
		}
		else if(Function == "findHash")
		{
			EFileHashType HashType = CFileHash::Str2HashType(Return["HF"].toString());
			QMultiMap<EFileHashType, CFileHashPtr> Hashes = pItf->GetHashes(FileID);
			
			CFileHash* pFileHash = Hashes.value(HashType).data();
			if(!pFileHash)
				continue;
			if(pFileHash->GetHash() != Return["HV"].toByteArray()) // HashValue
			{
				LogLine(LOG_ERROR | LOG_DEBUG, tr("Recived hash is wrong"));
				continue;
			}

			if(pFileHash->IsComplete())
				continue; // hash set/tree is already complete

			try
			{
				if(CFileHashTree* pHashTree = qobject_cast<CFileHashTree*>(pFileHash))
				{
					if(CFileHashTreeEx* pHashTreeEx = qobject_cast<CFileHashTreeEx*>(pFileHash))
						pHashTreeEx->SetMetaHash(Return["MH"].toByteArray()); // MetaHash

					// Note: MetaData must be set befoure loading tree, or else load will fail!!
					if(!pHashTree->IsComplete())
					{
						if(!pHashTree->LoadBin(Return["HT"].toByteArray())) // HashTree
							throw CException(LOG_DEBUG, L"HashTree couldn't be loaded");
					}
				}
				else if(CFileHashSet* pHashSet = qobject_cast<CFileHashSet*>(pFileHash))
				{
					if(!pHashSet->LoadBin(Return["HS"].toByteArray())) // HashSet
						throw CException(LOG_DEBUG, L"HashSet couldn't be loaded");
				}
				else {
					ASSERT(0); // we shouldn't done that for this
				}

				theCore->m_Hashing->SaveHash(pFileHash); // X-TODO-P: this is misplaced here
			}
			catch(const CException& Exception) 
			{
				LogLine(Exception.GetFlag(), tr("recived malformated packet; %1").arg(QString::fromStdWString(Exception.GetLine())));
			}
		}
	}
	return true;
}
