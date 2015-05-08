#include "GlobalHeader.h"
#include "MuleCollection.h"
#include "../../../Framework/Buffer.h"
#include "../../../Framework/Exception.h"
#include "../../../Framework/Cryptography/AsymmetricKey.h"
#include "../../../Framework/Scope.h"
#include "../../FileList/File.h"
#include "../../FileList/FileDetails.h"
#include "MuleTags.h"
#include "../../FileList/Hashing/FileHashSet.h"
#include "../../FileList/Hashing/FileHashTree.h"

CMuleCollection::CMuleCollection(QObject* qObject)
 : QObjectEx(qObject)
{
	m_TotalLength = 0;
}

void CMuleCollection::AddFile(const SFileInfo& File)
{
	m_Files.append(File); 
	m_TotalLength += File.FileSize;
}
	
bool CMuleCollection::LoadFromFile(const QString& FilePath)
{
	QFile File(FilePath);
	if(!File.open(QFile::ReadOnly))
		return false;
	return LoadFromData(File.readAll(), FilePath);
}

bool CMuleCollection::LoadFromData(const QByteArray& Data, const QString& FileName)
{
	if(Data.at(0) == '#')
	{
		m_CollectionName = Split2(Split2(FileName, "/", true).second, ".").first;
		QBuffer Buffer((QByteArray*)&Data);
		Buffer.open(QBuffer::ReadOnly);
		while(Buffer.canReadLine())
		{
			QByteArray Line = Buffer.readLine();
			if(Line.at(0) == '#')
				continue;
			
			QList<QByteArray> Segments = Line.split('/');
			if(Segments.size() < 1)
				continue;

			SFileInfo File;
			foreach(const QByteArray& Segment, Segments)
			{
				QList<QByteArray> Sections = Segment.split('|');
				if(Sections.size() >=6 && Sections[1].toLower() == "file")
				{
					File.FileName = QString::fromUtf8(QByteArray::fromPercentEncoding(Sections[2]));
					File.FileSize = Sections[3].toULongLong();
					File.HashEd2k = QByteArray::fromHex(Sections[4]);
					for (int i = 5; i < Sections.size(); i++)
					{
						if(Sections[i].left(2) == "h=") // aich hash
							File.HashAICH = Sections[i].mid(2);
						/*else if(Sections[i].left(2) == "p=") // hash set
						{
							
						}*/
					}
				}
			}
			AddFile(File);
		}
		return true;
	}

	try
	{
		CBuffer Buffer(Data);

		uint32 uVersion = Buffer.ReadValue<uint32>();
		if(uVersion != COLLECTION_FILE_VERSION1_INITIAL && uVersion != COLLECTION_FILE_VERSION2_LARGEFILES)
			return false;

		QVariantMap HeaderTags = CMuleTags::ReadTags(&Buffer);
		for(QVariantMap::iterator I = HeaderTags.begin(); I != HeaderTags.end(); ++I)
		{
			const QVariant& Value = I.value();
			switch(FROM_NUM(I.key()))
			{
				case FT_FILENAME:
					m_CollectionName = Value.toString();
					break;
				case FT_COLLECTIONAUTHOR:
					m_Properties["Creator"] = Value;
					break;
				case FT_COLLECTIONAUTHORKEY:
					m_CreatorKey = Value.toByteArray();
					break;
				default:
					m_Properties.insert(I.key(), Value);
			}
		}

		for(uint32 uFileCount = Buffer.ReadValue<uint32>(); uFileCount; uFileCount--)
		{
			SFileInfo File;
			QVariantMap Tags = CMuleTags::ReadTags(&Buffer);
			for(QVariantMap::iterator I = Tags.begin(); I != Tags.end(); ++I)
			{
				const QVariant& Value = I.value();
				switch(FROM_NUM(I.key()))
				{
					case FT_FILENAME:
						File.FileName = Value.toString();
						break;
					case FT_FILESIZE:
						File.FileSize = Value.toULongLong();
						break;
					case FT_FILEHASH:
						File.HashEd2k = Value.value<CMuleHash>().ToByteArray();
						break;
					case FT_AICH_HASH:
						File.HashAICH = Value.toString().toLatin1(); // Note: this is still base 32 encoded
						break;

					case FT_FILECOMMENT:
						File.Properties.insert(PROP_DESCRIPTION, Value);
						break;
					case FT_FILERATING:
						File.Properties.insert(PROP_RATING, Value);
						break;

					//case FT_FILETYPE:
					//	break;
					default:
						File.Properties.insert(I.key(), Value);
				}
			}
			AddFile(File);
		}

		if(m_CreatorKey.isEmpty())
			return true;
	
		bool bVerifyed = false;
		if(Buffer.GetSizeLeft())
		{
			CPublicKey PubKey;
			if(PubKey.SetKey(m_CreatorKey))
				bVerifyed = PubKey.Verify(Buffer.GetBuffer(), Buffer.GetPosition(), Buffer.GetData(0), Buffer.GetSizeLeft());
		}

		if(!bVerifyed)
			LogLine(LOG_WARNING, tr("Signature of collection %1 is invalid").arg(FileName));

		return bVerifyed;
	}
	catch(CException& Exception)
	{
		LogLine(Exception.GetFlag(), tr("malformated collection; %1").arg(QString::fromStdWString(Exception.GetLine())));
		return false;
	}
}

QByteArray CMuleCollection::SaveToData(CPrivateKey* pPrivKey)
{
	if(pPrivKey && (pPrivKey->GetAlgorithm() & CAbstractKey::eAsymCipher) != CAbstractKey::eRSA)
	{
		LogLine(LOG_ERROR, tr("Can not save eMule sollection with a Private Key other than RSA"));
		return QByteArray();
	}

	CBuffer Buffer;

	Buffer.WriteValue<uint32>(COLLECTION_FILE_VERSION2_LARGEFILES);

	QVariantMap HeaderTags = m_Properties;
	HeaderTags[TO_NUM(FT_FILENAME)] = m_CollectionName;
	if(HeaderTags.contains("Creator"))
		HeaderTags[TO_NUM(FT_COLLECTIONAUTHOR)] = HeaderTags.take("Creator").toString();
	if(pPrivKey)
	{
		CScoped<CPublicKey> pPubKey = pPrivKey->PublicKey();
		m_CreatorKey = pPubKey->ToByteArray();
		HeaderTags[TO_NUM(FT_COLLECTIONAUTHORKEY)] = m_CreatorKey;
	}
	CMuleTags::WriteTags(HeaderTags, &Buffer, true);

	Buffer.WriteValue<uint32>(m_Files.count());
	foreach(const SFileInfo& File, m_Files)
	{
		QVariantMap Tags = m_Properties;
		Tags[TO_NUM(FT_FILENAME)] = File.FileName;
		Tags[TO_NUM(FT_FILESIZE)] = File.FileSize;
		CMuleHash Hash((byte*)File.HashEd2k.data());
		Tags[TO_NUM(FT_FILEHASH)] = QVariant(CMuleHash::GetVariantType(), &Hash);
		if(!File.HashAICH.isEmpty())
			Tags[TO_NUM(FT_AICH_HASH)] = QString(File.HashAICH);
		if(Tags.contains(PROP_DESCRIPTION))
			Tags[TO_NUM(FT_FILECOMMENT)] = Tags.take(PROP_DESCRIPTION).toString();
		if(Tags.contains(PROP_RATING))
			Tags[TO_NUM(FT_FILERATING)] = Tags.take(PROP_RATING).toInt();
		CMuleTags::WriteTags(Tags, &Buffer, true);
	}

	if(pPrivKey)
	{
		CBuffer Sign;
		pPrivKey->Sign(&Buffer, &Sign);
		Buffer.AppendData(Sign.GetBuffer(), Sign.GetSize());
	}

	return Buffer.ToByteArray();
}

bool CMuleCollection::SaveToFile(const QString& FilePath, CPrivateKey* pPrivKey)
{
	QFile qFile(FilePath);
	if(!qFile.open(QFile::WriteOnly))
		return false;
	QByteArray Data = SaveToData(pPrivKey);
	if(Data.isEmpty())
		return false;
	qFile.write(Data);
	return true;
}

bool CMuleCollection::SaveToSimpleFile(const QString& FilePath)
{
	QFile qFile(FilePath);
	if(!qFile.open(QFile::WriteOnly))
		return false;
	qFile.write("# eMule collection (simple text format)\r\n");
	foreach(const SFileInfo& File, m_Files)
	{
		QString AICH;
		if(!File.HashAICH.isEmpty())
			AICH = QString("h=%1|").arg(QString(File.HashAICH));
		qFile.write(QString("ed2k://|file|%1|%2|%3|%4/\r\n").arg(QString(File.FileName.toUtf8().toPercentEncoding())).arg(File.FileSize).arg(QString(File.HashEd2k.toHex())).arg(AICH).toUtf8());
	}
	return true;
}

void CMuleCollection::Populate(CFile* pFile)
{
	uint64 FileID = pFile->GetFileID();
	ASSERT(FileID);

	CFileList* pList = pFile->GetList();

	pFile->SetFileName(m_CollectionName);
	pFile->SetFileSize(m_TotalLength);
	pFile->InitEmpty();
	pFile->SetMasterHash(CFileHashPtr(new CFileHash(HashXNeo)));
	CJoinedPartMap* pParts = new CJoinedPartMap(pFile->GetFileSize());

	uint64 Offset = 0;
	foreach(const SFileInfo& File, m_Files)
	{
		if(File.FileSize == 0)
		{
			LogLine(LOG_WARNING, tr("Ignoring empty file '%1' in collection '%2'").arg(File.FileName).arg(pFile->GetFileName()));
			continue;
		}

		QString Dir = pFile->GetFileDir();
		Dir += pFile->GetFileName() + "/";

		CFileHashPtr pHashSet = CFileHashPtr(new CFileHashSet(HashEd2k, File.FileSize));
		pHashSet->SetHash(File.HashEd2k);

		// Find known file
		CFile* pSubFile = pList->GetFileByHash(pHashSet.data(), true);
		
		// if we have a patologic collection, ignore it, make a new file
		if(pSubFile && pSubFile->GetParentFiles().contains(pFile->GetFileID()))
			pSubFile = NULL;

		if(pSubFile)
		{
			if(pSubFile->IsMultiFile())
			{
				ASSERT(0);
				pSubFile = NULL;
			}
			else if(pSubFile->IsRemoved())
			{
				pSubFile->SetFileDir(Dir);
				pSubFile->UnRemove(pFile->GetMasterHash());
				if(!pSubFile->IsPending())
					pSubFile->Resume();
			}
		}
		
		if(!pSubFile)
		{
			pSubFile = new CFile();
			if(pFile->GetProperty("Temp").toBool())
				pSubFile->SetProperty("Temp", true);
			pSubFile->SetFileDir(Dir);
		
			pSubFile->AddHash(pHashSet);
			if(!File.HashAICH.isEmpty())
			{
				if(CFileHashPtr pHashTree = CFileHashPtr(CFileHashTree::FromString(File.HashEd2k, HashMule, File.FileSize)))
					pSubFile->AddHash(pHashTree);
			}

			pSubFile->AddEmpty(HashEd2k, File.FileName, File.FileSize, pFile->IsPending());

			pList->AddFile(pSubFile);

			if(!pSubFile->IsPending())
				pSubFile->Resume();
		}

		if(!File.Properties.isEmpty())
			pSubFile->GetDetails()->Add("emulecollection://" + m_CollectionName, File.Properties);

		uint64 uBegin = Offset; 
		uint64 uEnd = Offset + File.FileSize;
		Offset += File.FileSize;
		
		CSharedPartMap* pSubParts = pSubFile->SharePartMap(); 
		ASSERT(pSubParts); // this must not fail - we did all checks above

		pParts->SetupLink(uBegin, uEnd, pSubFile->GetFileID());
		pSubParts->SetupLink(uBegin, uEnd, pFile->GetFileID());

		if(pFile->IsPaused(true))
			pSubFile->Pause();
		else if(pFile->IsStarted())
			pSubFile->Start();
	}

	pFile->SetPartMap(CPartMapPtr(pParts));
}

bool CMuleCollection::Import(CFile* pFile)
{
	if(!pFile->IsMultiFile())
	{
		LogLine(LOG_ERROR, tr("A colection can only be created form a multi file"));
		return false;
	}

	m_CollectionName = pFile->GetFileName();
	foreach(uint64 SubFileID, pFile->GetSubFiles())
	{
		CFile* pSubFile = pFile->GetList()->GetFileByID(SubFileID);
		if(!pSubFile)
			continue;

		CFileHash* pEd2kHash = pSubFile->GetHash(HashEd2k);
		if(!pEd2kHash)
		{
			LogLine(LOG_ERROR, tr("File %1 doe snot have an ed2k hash").arg(pSubFile->GetFileName()));
			return false;
		}

		SFileInfo File;
		File.FileName = pSubFile->GetFileName();
		File.FileSize = pSubFile->GetFileSize();
		File.HashEd2k = pEd2kHash->GetHash();
		CFileHash* pAICHHash = pSubFile->GetHash(HashMule);
		if(pAICHHash)
			File.HashAICH = pAICHHash->ToString();

		if(pSubFile->HasProperty(PROP_DESCRIPTION))
			File.Properties.insert(PROP_DESCRIPTION, pSubFile->GetProperty(PROP_DESCRIPTION));
		if(pSubFile->HasProperty(PROP_RATING))
			File.Properties.insert(PROP_RATING, pSubFile->GetProperty(PROP_RATING));

		AddFile(File);
	}
	return false;
}