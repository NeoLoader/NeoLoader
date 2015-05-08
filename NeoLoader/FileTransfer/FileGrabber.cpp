#include "GlobalHeader.h"
#include "FileGrabber.h"
#include "../FileList/File.h"
#include "Transfer.h"
#include "../FileList/FileList.h"
#include "../FileList/FileStats.h"
#include "../NeoCore.h"
#include "../../Framework/Cryptography/SymmetricKey.h"
#include "../../Framework/Cryptography/HashFunction.h"
#include "../../Framework/RequestManager.h"
#include "../../Framework/qzlib.h"
#ifndef NO_HOSTERS
#include "HosterTransfer/HosterLink.h"
#include "./HosterTransfer/LinkGrabber.h"
#include "./HosterTransfer/ContainerDecoder.h"
#include "./HosterTransfer/ArchiveDownloader.h"
#include "./HosterTransfer/WebManager.h"
#include "./HosterTransfer/ArchiveSet.h"
#endif
#include "../FileList/Hashing/FileHashSet.h"
#include "../FileList/Hashing/FileHashTree.h"
#include "../FileList/Hashing/HashingThread.h"
#include "./ed2kMule/MuleSource.h"
#include "../FileList/FileManager.h"
#include "../FileTransfer/BitTorrent/TorrentInfo.h"
#include "../FileTransfer/BitTorrent/Torrent.h"
#include "../../qbencode/lib/bencode.h"
#include "../FileTransfer/BitTorrent/TorrentManager.h"
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif
#include "../../Framework/Xml.h"

/*#define LINK_KEY_URL	"http://link.neoloader.to/getkey.php?id="
#define AGENT_KEY		"opWAE034AS3dsfs43pAx"*/

CFileGrabber::CFileGrabber(QObject* qObject)
 : CFileList(qObject)
{
#ifndef NO_HOSTERS
	m_pLinkGrabber = new CLinkGrabber(this);
	m_pContainerDecoder = new CContainerDecoder(this);
#endif
}

CFileGrabber::~CFileGrabber()
{
	foreach(SGrabTask* pTask, m_GrabList)
		delete pTask;
	m_GrabList.clear();

	/*foreach(SDecryptEntry* pDecrypt, m_DecryptCache)
		delete pDecrypt;
	m_DecryptCache.clear();*/
}

void CFileGrabber::Process(UINT Tick)
{
#ifndef NO_HOSTERS
	m_pLinkGrabber->Process(Tick);
#endif

	//ProcessKeyCache();

	CFileList::Process(Tick);

	if((Tick & EPerSec) == 0)
		return;
	
	foreach(CFile* pFile, m_FileMap)
	{
		if(!pFile->IsStarted())
			continue;

		CFileStats::STemp& Temp = pFile->GetStats()->GetTemp(eHosterLink);
		CTorrent* pTorrent = pFile->GetTopTorrent();
		if((!pFile->MetaDataMissing() 
#ifndef NO_HOSTERS
			|| pFile->IsRawArchive()
#endif
			) && (!pTorrent || !pTorrent->GetInfo()->IsEmpty()) && Temp.Checked == Temp.All)
		{
			pFile->Stop();
#ifndef NO_HOSTERS
			if(pFile->IsArchive())
			{
				const QMap<QByteArray, CArchiveSet*>& Archives = pFile->GetArchives();
				if(Archives.count() > 1 && pFile->IsRawArchive())
				{
					m_pLinkGrabber->InspectLinks(pFile); // this may delete this
					pFile->Remove(true); // this deletes pFile
				}
			}
#endif
		}
	}
}

uint64 CFileGrabber::GrabFile(CFile* pFile)
{
	uint64 GrabberID = 0;

	// Note: this function is invoced by grabbed torrents during sub file adding
	//			assotiate the sub files with the same tast as teh aprent torrent
	if(CLinkedPartMap* pLinkedParts = qobject_cast<CLinkedPartMap*>(pFile->GetPartMap()))
	{
		ASSERT(pLinkedParts->GetLinks().count() == 1);

		foreach(SGrabTask* pTask, m_GrabList)
		{
			if(pTask->Files.contains((*pLinkedParts->GetLinks().begin())->ID))
			{
				GrabberID = m_GrabList.key(pTask);
				break;
			}
		}
	}

	if(GrabberID == 0)
		GrabberID = GrabUris(QStringList("file://" + pFile->GetFileName()));
	AddFile(pFile, GrabberID); // this must not fail

	return GrabberID;
}

void CFileGrabber::AddFile(CFile* pFile)
{
	GrabFile(pFile);
}

bool CFileGrabber::AddFile(CFile* pFile, uint64 GrabberID)
{
	if(!m_GrabList.contains(GrabberID))
	{
		delete pFile; // this grabber task doe snot longer exist drop the file
		return false;
	}
	
	QList<CFile*> Files = theCore->m_FileManager->FindDuplicates(pFile, true);
	if(!Files.isEmpty())
	{
		ASSERT(Files.count() == 1);
		CFile* pFoundFile = Files.at(0);

		delete pFile;
		m_GrabList[GrabberID]->Files.insert(pFoundFile->GetFileID());
	}
	else
	{
		CFileList::AddFile(pFile);
		m_GrabList[GrabberID]->Files.insert(pFile->GetFileID());
	}
	return true;
}

void CFileGrabber::AssociateFile(CFile* pFile, uint64 GrabberID)
{
	ASSERT(m_GrabList.contains(GrabberID));
	uint64 FileID = pFile->GetFileID();
	m_GrabList[GrabberID]->Files.insert(FileID);
}

uint64 CFileGrabber::FindGrabberID(CFile* pFile)
{
	foreach(SGrabTask* pTask, m_GrabList)
	{
		if(pTask->Files.contains(pFile->GetFileID()))
			return m_GrabList.key(pTask);
	}
	return 0;
}

QStringList CFileGrabber::SplitUris(QString Links)
{
	QStringList LinkList;
	/*foreach(const QString& Link, Links.split("\n"))
	{
		if(!Link.trimmed().isEmpty())
			LinkList.append(Link.trimmed());
	}*/
	for(int Begin = 0;;)
	{
		Begin = Links.indexOf(QRegExp("(magnet:\\?)|(https?://)|(ftp://)|(ed2k://)|(jd://)|(jdlist://)|(dlc://)"), Begin);
		if(Begin == -1)
			break;

		int End = Links.indexOf(QRegExp("[ \t\r\n]+"), Begin);
		if(End == -1)
			End = Links.length();

		LinkList.append(Links.mid(Begin, End-Begin));
		Begin = End;
	}
	return LinkList;
}

QStringList CFileGrabber::AllQueryItemValue(const QUrl& Url, const QString& key)
{
	/*Group settings 
	Allows to include several files and their URNs, names and hashes in the Magnet 
	link by adding a count number preceded by a dot (".") to each link parameter.
		magnet:?xt.1=[ URN of the first file]&xt.2=[ URN of the second file]*/

	QStringList QueryItemValue;
	typedef QPair<QString, QString> StrPair;
#if QT_VERSION < 0x050000
	foreach(const StrPair& ItemPair, Url.queryItems())
#else
	QUrlQuery Query(Url);
	foreach(const StrPair& ItemPair, Query.queryItems())
#endif
	{
		if(ItemPair.first.indexOf(QRegExp(key + "\\.?[0-9]*")) == 0)
			QueryItemValue.append(QUrl::fromPercentEncoding(ItemPair.second.toLatin1()));
	}
	return QueryItemValue;
}

uint64 CFileGrabber::GrabUris(const QStringList& Uris)
{
	uint64 GrabberID = 0;
#ifdef _DEBUG
	do GrabberID++;
#else
	do GrabberID = GetRand64() & MAX_FLOAT;
#endif
	while(m_GrabList.contains(GrabberID)); // statisticaly almost impossible but in case
	m_GrabList.insert(GrabberID,new SGrabTask(Uris));

	foreach(const QString& sUri, Uris)
		GrabUri(sUri, GrabberID);

	return GrabberID;
}

bool CFileGrabber::GrabUri(const QString& sUri, uint64 GrabberID)
{
	// first unwrap links like // http://link.neoloader.to/magnet:?dn=file&xl=0&xt=urn:neo:HASH
	int Pos = sUri.lastIndexOf("magnet:?");
	QUrl Uri(sUri.mid(Pos > 0 ? Pos : 0));

	if(sUri.left(7) == "file://")
		return true; // thts a dummy for some file re wead for grabbing
#ifndef NO_HOSTERS
	else if(Uri.scheme().indexOf(QRegExp("(http|https|ftp)"), Qt::CaseInsensitive) == 0)
	{
		return m_pLinkGrabber->AddUrl(sUri, NULL, GrabberID);
	}
#endif
	else if(sUri.left(7).compare("ed2k://", Qt::CaseInsensitive) == 0) // ed2k link
	{
		if(CFile* pFile = AddEd2k(sUri))
		{
			pFile->Start();
			AddFile(pFile, GrabberID);
			return true;
		}
	}
	else if(Uri.scheme().indexOf("magnet", Qt::CaseInsensitive) == 0) // magnet link
	{
		if(CFile* pFile = AddMagnet(sUri))
		{
			pFile->Start();
			AddFile(pFile, GrabberID);
			return true;
		}
	}
#ifndef NO_HOSTERS
	else if(Uri.scheme().indexOf("jd", Qt::CaseInsensitive) == 0 || Uri.scheme().indexOf("dlc", Qt::CaseInsensitive) == 0)
	{
		Uri.setScheme("http");
		return m_pLinkGrabber->AddUrl(Uri.toString(), NULL, GrabberID);
	}
	else if(sUri.left(9) == "jdlist://")
	{
		QStringList Links = QString(QByteArray::fromBase64(sUri.mid(9).toLatin1())).split(',') ;//.replace("-","+").replace("_","/");
		foreach(const QString Link, Links)
			m_pLinkGrabber->AddUrl(Link, NULL, GrabberID);
		return true;
	}
#endif
	else
		LogLine(LOG_WARNING, tr("Unknown Uri: %1").arg(sUri));

	return false;
}

CFile* CFileGrabber::AddMagnet(QString sUri, QString FileName)
{
	QString Domain;
	// first unwrap links like // http://link.neoloader.to/magnet:?dn=file&xl=0&xt=urn:neo:HASH
	int Pos = sUri.lastIndexOf("magnet:?");
	if(Pos > 0)
	{
		Domain = sUri.left(Pos);
		sUri.remove(0, Pos);
	}

	QUrl Uri(sUri);

	//http://en.wikipedia.org/wiki/Magnet_URI_scheme
	//magnet:? xl = [Size in Bytes] & dn = [file name (URL encoded)] & xt = urn: neo: [ SHA2 hash tree ]

#if QT_VERSION < 0x050000
	/*dn (Display Name) - Filename*/
	QString dn = QUrl::fromPercentEncoding(Uri.queryItemValue("dn").replace("+", " ").toUtf8());
	/*xl (eXact Length) - Size in bytes*/
	uint64 xl = Uri.queryItemValue("xl").toLongLong();
#else
	QUrlQuery Query(Uri);

	/*dn (Display Name) - Filename*/
	QString dn = QUrl::fromPercentEncoding(Query.queryItemValue("dn").replace("+", " ").toUtf8());
	/*xl (eXact Length) - Size in bytes*/
	uint64 xl = Query.queryItemValue("xl").toLongLong();
#endif

	CFile* pFile = new CFile(this);

	EFileHashType MasterHash = HashUnknown;
	/*xt (eXact Topic) - URN containing file hash - stands for "exact topic". 
		This token is the most important part of Magnet links. It is used to find and verify files included in the Magnet link.*/
	int xt_c = 0;
	QStringList xt = AllQueryItemValue(Uri, "xt");
	foreach(const QString& t, xt)
	{
		if(CFileHashPtr pHash = CFileHashPtr(CFileHash::FromString(t.toLatin1(),HashNone,xl)))
		{
			if(pHash->GetType() < MasterHash) // Hash Values are sorted by thair priority
				MasterHash = pHash->GetType();
			pFile->AddHash(pHash); // this function wil dispose of duplicates
			xt_c++;
		}
		else 
			LogLine(LOG_WARNING, tr("Magnet link %1 has an invalid file hash %2").arg(dn, t));
	}

	if(xt_c == 0 && !xt.isEmpty())
	{
		LogLine(LOG_ERROR, tr("Magnet link %1 has onyl invalid file hashes").arg(dn));
		delete pFile;
		return NULL;
	}

	/*Tracker URL. Used to obtain resources for BitTorrent downloads without a need for DHT support.*/
	QStringList tr = AllQueryItemValue(Uri, "tr");

	/*"xs" stands for "eXact source". It is either an Web download for the file linked to by the 
		Magnet link or the address of a P2P source for the file*/
	QStringList xs = AllQueryItemValue(Uri, "xs");
	/*"as" stands for "acceptable source". This type of source refers to a direct download from a web server.*/
	xs.append(AllQueryItemValue(Uri, "as")); 

#ifndef NO_HOSTERS
	// as source is a source
	foreach(const QString& s, xs)
		theCore->m_WebManager->AddToFile(pFile, s, eGrabber);
#endif

	if(MasterHash == HashUnknown && !xs.isEmpty())
		MasterHash = HashArchive;

	if(MasterHash != HashUnknown)
		pFile->AddEmpty(MasterHash, FileName.isEmpty() ? dn : FileName, xl);

	QStringList Trackers;
	QStringList Servers;
	foreach(const QString& Url, tr)
	{
		if(Url.left(7).compare("ed2k://", Qt::CaseInsensitive)  == 0)
			Servers.append(Url);
		else if(!Url.isEmpty())
			Trackers.append(Url);
	}
	if(!Servers.isEmpty())
		pFile->SetProperty("Ed2kServers", Servers);
	if(CTorrent* pTorrent = pFile->GetTopTorrent()) // Note: here we have only one torrent anyways
	{
		QMultiMap<int,QString> TrackerList;
		foreach(const QString& Tracker, Trackers)
			TrackerList.insert(0, Tracker);
		pTorrent->GetInfo()->SetTrackerList(TrackerList);
	}

#ifndef NO_HOSTERS
	/*encrypted sources*/
#if QT_VERSION < 0x050000
	QString es = Uri.queryItemValue("es");
#else
    QString es = Query.queryItemValue("es");
#endif
	if(!es.isEmpty())
	{
		QVariant Variant;
		Bdecoder::decode(&Variant, QByteArray::fromBase64(es.replace("-","+").replace("_","/").toLatin1()));
		if(Variant.type() == QVariant::Map)
			m_pLinkGrabber->DecryptLinks(Variant.toMap(), Domain, pFile);
		else
		{
			QByteArray Links = Unpack(Variant.toByteArray());
			foreach(const QString& Link, QString::fromUtf8(Links).split("\r\n"))
				theCore->m_WebManager->AddToFile(pFile, Link, eGrabber);
		}

		//	DecryptES(pFile,es,Uri.queryItemValue("ct").toLongLong());
	}
#endif

	/*This field specifies a string of search keywords to search for in P2P networks.*/
	//QStringList kt = AllQueryItemValue(Uri, "kt");
	// ToDo: start kad search

	/*This is a link to a list of links (see list). Perhaps as a web link...*/
	//QStringList mt = AllQueryItemValue(Uri, "mt");
	// ToDo: whats that

	return pFile;
}

CFile* CFileGrabber::AddEd2k(QString sUri, QString FileName)
{
	uint64 uFileSize;
	CFileHashPtr pEd2kHash;
	CFileHashPtr pAICHHash;
	QStringList Sources;
	QStringList Servers;

	if(sUri.left(8).compare("ed2k://|",Qt::CaseInsensitive) != 0)
		sUri = QString::fromUtf8(QByteArray::fromPercentEncoding(sUri.toUtf8()));

	// Note: QUrl messes up ed2k links must parse string
	QStringList Segments = sUri.split("/", QString::SkipEmptyParts);
	if(Segments.size() < 1)
	{
		LogLine(LOG_ERROR, tr("Invalid Ed2kLink"));
		return NULL;
	}

	foreach(const QString& Segment, Segments)
	{
		QStringList Sections = Segment.split("|");
		if(Sections.size() >=6 && Sections[1].compare("file", Qt::CaseInsensitive) == 0)
		{
			if(FileName.isEmpty())
				FileName = QString::fromUtf8(QByteArray::fromPercentEncoding(Sections[2].toUtf8()));
			uFileSize = Sections[3].toULongLong();
			pEd2kHash = CFileHashPtr(CFileHash::FromString(Sections[4].toLatin1(), HashEd2k, uFileSize));
			if(!pEd2kHash)
				return NULL;
			for (int i = 5; i < Sections.size(); i++)
			{
				if(Sections[i].left(2).compare("h=") == 0) // aich hash
				{
					pAICHHash = CFileHashPtr(CFileHash::FromString(Sections[i].mid(2).toLatin1(), HashMule, uFileSize));
				}
				else if(Sections[i].left(2).compare("p=") == 0) // hash set
				{
					QList<QByteArray> HashSet;
					foreach(const QString& Hash, Sections[i].mid(2).split(":"))
						HashSet.append(QByteArray::fromHex(Hash.toLatin1()));
					if(((CFileHashSet*)pEd2kHash.data())->SetHashSet(HashSet))
						theCore->m_Hashing->SaveHash(pEd2kHash.data());
				}
			}
		}
		else if(Sections.size() >= 3 && Sections[1].left(7).compare("sources", Qt::CaseInsensitive) == 0)
		{
			Sources = Sections[1].split(",", QString::SkipEmptyParts);
			Sources.removeFirst();
		}
		else if(Sections.size() >= 3 && Sections[1].left(7).compare("servers", Qt::CaseInsensitive) == 0)
		{
			Servers = Sections[1].split(",", QString::SkipEmptyParts);
			Servers.removeFirst();
		}
	}

	if(!pEd2kHash)
	{
		LogLine(LOG_ERROR, tr("Invalid Ed2kLink"));
		return NULL;
	}

	CFile* pFile = new CFile();
	pFile->AddHash(pEd2kHash);
	if(pAICHHash)
		pFile->AddHash(pAICHHash);
	pFile->AddEmpty(HashEd2k, FileName, uFileSize);
	if(!Servers.isEmpty())
		pFile->SetProperty("Ed2kServers", Servers);

	/*foreach(const QString& Source, Sources)
	{
		StrPair IpPort = Split2(Source, ":");
		if(IpPort.second.isEmpty())
			continue;

		SMuleSource Mule;
		Mule.SetIP(CAddress(IpPort.first));
		Mule.TCPPort = IpPort.second.toUInt();

		if(pFile->GetTransfer(QString("ed2k://|source,%1:%2|").arg(Mule.GetIP().ToQString(true)).arg(Mule.TCPPort)) == NULL)
		{
			CMuleSource* pSource = new CMuleSource(Mule);
			pSource->SetFoundBy(eGrabber);
			pFile->AddTransfer(pSource);
			pSource->AttacheClient();
		}
	}*/

	return pFile;
}

QString CFileGrabber::GetUri(CFile* pFile, ELinks Links, EEncoding Encoding)
{
	ASSERT(pFile);

	if(Encoding == eed2k)
	{
		if(CFileHash* pHash = pFile->GetHash(HashEd2k))
		{
			QString Addon;
			if(CFileHash* pAICH = pFile->GetHash(HashMule))
				Addon = QString("h=%1|").arg(QString(pAICH->ToString()));

			QStringList Servers = pFile->GetProperty("Ed2kServers").toStringList();
			if(!Servers.isEmpty())
			{
				Addon += "servers";
				foreach(const QString& Url, Servers)
					Addon += "," + QString::fromLatin1(Url.toLatin1().toPercentEncoding());
				Addon += "|";
			}

			QString sUri = "ed2k://|file|" + QString::fromLatin1(pFile->GetFileName().toUtf8().toPercentEncoding()) + QString("|%1|%2|%3/").arg(pFile->GetFileSize()).arg(QString::fromLatin1(pHash->ToString())).arg(Addon);
			return sUri;
		}
		return "";
	}

	/*quint64 CryptoID = 0;
	if(Encoding == eEncrypted)
	{
		CryptoID = GetCryptoID(pFile->GetFileID());
		if(CryptoID == 0)
			return "";
	}*/

	// assembly URL
	QString Domain = theCore->Cfg()->GetString("Content/MagnetDomain");
	if(Domain.right(1) != "/")
		Domain.append("/");

	QString sUri = (Encoding == eMagnet ? "" : Domain)  + "magnet:";

	int Count = 0;

	if (Encoding == eMagnet)
	{
		if (CTorrent* pTorrent = pFile->GetTopTorrent()) // Note: many torrent clients expect the inforhash to be the first entry
		{
			if (Count++ == 0)	sUri += "?";
			else				sUri += "&";
			sUri += QString("xt=urn:%1:%2").arg(CFileHash::HashType2Str(HashTorrent), QString(pTorrent->GetInfoHash().toHex()));
		}
	}
	else
	{
		foreach(CTorrent* pTorrent, pFile->GetTorrents())
		{
			if (pTorrent)
			{
				if (Count++ == 0)	sUri += "?";
				else				sUri += "&";
				sUri += QString("xt=urn:%1:%2").arg(CFileHash::HashType2Str(HashTorrent), QString(pTorrent->GetInfoHash().toHex()));
			}
		}
	}
	
	EFileHashType HashPrio[4] = {HashNeo,HashXNeo,HashEd2k,HashMule};
	for(int i=0; i<ARRSIZE(HashPrio); i++)
	{
		if(CFileHash* pHash = pFile->GetHash(HashPrio[i]))
		{
			if(Count++ == 0)	sUri += "?";
			else				sUri += "&";
			sUri += QString("xt=urn:%1:%2").arg(CFileHash::HashType2Str(pHash->GetType()),QString(pHash->ToString()));
		}
	}

	if(Count++ == 0)	sUri += "?";
	else				sUri += "&";
	sUri  += "dn=" + QString::fromLatin1(pFile->GetFileName().toUtf8().toPercentEncoding());

	if(pFile->GetFileSize() != 0)
		sUri += QString("&xl=%1").arg(pFile->GetFileSize());
	
	if (Encoding == eMagnet)
	{
		if (CTorrent* pTorrent = pFile->GetTopTorrent())
		{
			foreach(const QString& Url, pTorrent->GetInfo()->GetTrackerList())
				sUri += "&tr=" + QString::fromLatin1(Url.toLatin1().toPercentEncoding());
		}
	}

	foreach(const QString& Url, pFile->GetProperty("Ed2kServers").toStringList())
		sUri += "&tr=%1" + QString::fromLatin1(Url.toLatin1().toPercentEncoding());

#ifndef NO_HOSTERS
	QStringList LinkList;
#endif
	switch(Links)
	{
		case eNone:
			break;
		case eArchive:
			foreach(CTransfer* pTransfer, pFile->GetTransfers())
			{
				if(!pTransfer->IsDownload())
					continue;
				if(pTransfer->GetPartMap() != NULL)
					continue;
#ifndef NO_HOSTERS
				if(CHosterLink* pHosterLink = qobject_cast<CHosterLink*>(pTransfer))
				{
					if(pHosterLink->GetProtection())
						continue;
					LinkList.append(pHosterLink->GetUrl());
				}
#endif
			}
			break;
	}

#ifndef NO_HOSTERS
	if(!LinkList.isEmpty())
	{
		if(Encoding == eEncrypted || Encoding == eCompressed)
		{
			sUri += QString("&es=");

			QVariant Variant;
			if(Encoding == eEncrypted)
			{
				QVariantMap Map = m_pLinkGrabber->EncryptLinks(Domain, LinkList);
				if(Map.contains("Error"))
					LogLine(LOG_ERROR, tr("Link encryption for %1 failed %2").arg(pFile->GetFileName()).arg(Map["Error"].toString()));
				else
					Variant = Map;
			}
			else
			{
				QByteArray Links = LinkList.join("\r\n").toLatin1();
				Variant = Pack(Links);
			}
			QByteArray es = Bencoder::encode(Variant).buffer();

			/*time_t CreationTime = GetTime();
			sUri += QString("&ct=%1").arg(CreationTime);

			QByteArray es = EncryptES(CryptoID,LinkList,CreationTime);

			if(Encoding == eEncrypted)
				sUri += QString("&es=%1:").arg(CryptoID);
			else // if(Hosts == eCompressed)
				sUri += QString("&es=");*/

			// Note: we need URL compatible encoding: http://en.wikipedia.org/wiki/Base64#URL_applications
			sUri +=  QString(es.toBase64().replace("+","-").replace("/","_").replace("=",""));
		}
		else
		{
			int Counter = 1;
			foreach(const QString& Link, LinkList)
				sUri += QString("&xs.%1=%2").arg(Counter++).arg(QString(QUrl::toPercentEncoding(Link)));
		}
	}
#endif
	return sUri;
}

/*uint64 CFileGrabber::GetCryptoID(uint64 FileID)
{
	uint64 CryptoID = m_IDCache.value(FileID,0);
	if(CryptoID == 0)
		m_KeyOrders.insert(FileID, 0);
	return CryptoID;
}

QByteArray CFileGrabber::EncryptES(uint64 CryptoID, QStringList LinkList, time_t CreationTime)
{
	QByteArray Key = m_KeyCache.value(CryptoID);
	ASSERT(!Key.isEmpty());

	QByteArray LinkData = LinkList.join("\r\n").toLatin1();
	QByteArray PackedData = Pack(LinkData);
	if(CryptoID == 0)
		return PackedData;
	QByteArray CryptoLinks = CEncryptionKey::Process(PackedData, CAbstractKey::eAES | CAbstractKey::eCFB, Key, QByteArray((char*)&CreationTime,sizeof(CreationTime)));
	return CryptoLinks;
}

int CFileGrabber::DecryptES(CFile* pFile, QString es, time_t CreationTime)
{
	int Sep = es.indexOf(":");
	uint64 CryptoID = Sep != -1 ? es.left(Sep).toULongLong() : 0;
	QByteArray CryptoLinks = QByteArray::fromBase64(es.mid(Sep+1).replace("-","+").replace("_","/").toLatin1());
	return DecryptES(pFile, CryptoID, CryptoLinks, CreationTime);
}

int CFileGrabber::DecryptES(CFile* pFile, uint64 CryptoID, QByteArray CryptoLinks, time_t CreationTime)
{
	ASSERT(pFile);

	QByteArray PackedData;
	if(CryptoID)
	{
		QByteArray Key = m_KeyCache.value(CryptoID);
		if(Key.isEmpty())
		{
			m_DecryptCache.insert(pFile->GetFileID(),new SDecryptEntry(CryptoID, CryptoLinks, CreationTime));
			m_KeyOrders.insert(pFile->GetFileID(), CryptoID);
			return 2;
		}

		PackedData = CDecryptionKey::Process(CryptoLinks, CAbstractKey::eAES | CAbstractKey::eCFB, Key, QByteArray((char*)&CreationTime,sizeof(time_t)));
	}
	else
		PackedData = CryptoLinks;

	QByteArray LinkData = Unpack(PackedData);
	
	foreach(const QString& Link, QString::fromUtf8(LinkData).split("\r\n"))
		pFile->AddTransfer(Link, CryptoID != 0);
	return !LinkData.isEmpty();
}

void CFileGrabber::ProcessKeyCache()
{
	if(!m_KeyOrders.isEmpty())
	{
		uint64 FileID = m_KeyOrders.uniqueKeys().first();
		quint64 CryptoID = m_KeyOrders.value(FileID);
		if(!m_KeyCache.contains(CryptoID))
		{
			QString sUrl = LINK_KEY_URL;
			sUrl += QString::number(CryptoID);
			QNetworkRequest Request = QNetworkRequest(sUrl);
			Request.setHeader(QNetworkRequest::ContentTypeHeader, "text/plain; charset=UTF-8");
			Request.setRawHeader("User-Agent", AGENT_KEY);
			QNetworkReply* pReply = theCore->m_RequestManager->get(Request);
			connect(pReply, SIGNAL(finished()), this, SLOT(OnFinished()));
		}
		else
		{
			m_KeyOrders.remove(FileID);
			if(m_DecryptCache.contains(FileID))
			{
				SDecryptEntry* pDecrypt = m_DecryptCache.take(FileID);
				if(CFile* pFile = CFileList::GetFile(FileID))
					DecryptES(pFile, pDecrypt->CryptoID, pDecrypt->CryptoLinks, pDecrypt->CreationTime);
				delete pDecrypt;
			}
		}
	}
}

void CFileGrabber::OnFinished()
{
	QNetworkReply* pReply = (QNetworkReply*)sender();

	QByteArray Reply = pReply->readAll();
	pReply->deleteLater();

	ASSERT(!m_KeyOrders.isEmpty());
	uint64 FileID = m_KeyOrders.uniqueKeys().first();
	quint64 CryptoID = m_KeyOrders.value(FileID);
	
	int Sep = Reply.indexOf(":");
	if(Sep == -1)
	{
		LogLine(LOG_ERROR, tr("Server gave an invalid key"));
		return;
	}

	if(CryptoID == 0)
		CryptoID = Reply.left(Sep).toULongLong(0,16);
	m_KeyOrders.remove(FileID);
	m_IDCache.insert(FileID,CryptoID);
	m_KeyCache.insert(CryptoID, QByteArray::fromHex(Reply.mid(Sep+1)));
}

int CFileGrabber::KeysPending(uint64 GrabberID)
{
	int Count = 0;
	if(m_GrabList.contains(GrabberID))
	{
		foreach(uint64 FileID, m_GrabList.value(GrabberID)->Files)
		{
			if(m_DecryptCache.contains(FileID))
				Count++;
		}
	}
	return Count;
}
*/

QList<CFile*> CFileGrabber::GetFiles(uint64 GrabberID)
{
	if(GrabberID == 0)
		return CFileList::GetFiles();

	QList<CFile*> Files;
	if(m_GrabList.contains(GrabberID))
	{
		foreach(uint64 FileID, m_GrabList.value(GrabberID)->Files)
		{
			if(CFile* pFile = CFileList::GetFile(FileID))
				Files.append(pFile);
		}
	}
	return Files;
}

int CFileGrabber::CountFiles(uint64 GrabberID)
{
	if(GrabberID == 0)
		return m_FileMap.size();
	if(SGrabTask* pTask = m_GrabList.value(GrabberID))
		return pTask->Files.size();
	return 0;
}

void CFileGrabber::Clear(bool bRemove, uint64 GrabberID)
{
#ifndef NO_HOSTERS
	m_pLinkGrabber->ClearTasks(bRemove, GrabberID);
#endif

	foreach(uint64 CurID, m_GrabList.keys())
	{
		if(GrabberID && GrabberID != CurID)
			continue;

		SGrabTask* pCurTask = m_GrabList.value(CurID);

		if(bRemove)
		{
			// remove all files no other task is using
			foreach(uint64 FileID, pCurTask->Files)
			{
				foreach(SGrabTask* pTask, m_GrabList)
				{
					if(pTask->Files.contains(FileID) && pTask != pCurTask)
					{
						FileID = 0;
						break;
					}
				}
				if(!FileID)
					continue;

				if(CFile* pFile = GetFileByID(FileID))
					RemoveFile(pFile);
			}
			pCurTask->Files.clear();
		}

#ifndef NO_HOSTERS
		if(bRemove || (m_pLinkGrabber->GetTasks(CurID).isEmpty() && GetFiles(CurID).isEmpty()))
#else
		if(bRemove)
#endif
		{
			m_GrabList.remove(CurID);
			delete pCurTask;
		}
	}
}

void CFileGrabber::DownloadPacket(const QString& Url, uint64 GrabebrID)
{
	QNetworkRequest Request = QNetworkRequest(Url);
	QNetworkReply* pReply = theCore->m_RequestManager->get(Request);
	connect(pReply, SIGNAL(finished()), this, SLOT(OnFinished()));
	m_PendingPackets.insert(pReply, GrabebrID);
}

void CFileGrabber::OnFinished()
{
	QNetworkReply* pReply = (QNetworkReply*)sender();
	uint64 GrabberID = m_PendingPackets.take(pReply);
	if(!GrabberID)
		return;

	QByteArray Reply = pReply->readAll();
	QString Url = pReply->url().toString();
	pReply->deleteLater();

	QString FileName = Url2FileName(Url);
	QString FileExt = GetFileExt(FileName);
	if(FileExt.compare("torrent", Qt::CaseInsensitive) == 0)
	{
		CFile* pFile = new CFile();
		pFile->SetPending();
		if(!AddFile(pFile, GrabberID))
			return; // task was canceled in the mean time
		if(pFile->AddTorrentFromFile(Reply))
			pFile->Start();
		else
		{
			LogLine(LOG_ERROR, tr("The torrent file %1 cannot not be parsed.").arg(FileName));
			
			pFile->Remove(true);
		}
	}
#ifndef NO_HOSTERS
	else if(FileExt.compare("DLC", Qt::CaseInsensitive) == 0)
		m_pContainerDecoder->AddContainer(Reply,FileExt,GrabberID);
#endif
	else
		LogLine(LOG_WARNING, tr("Downloaded Packet: %1 cannot be handled").arg(FileName));
}


/////////////////////////////////////////////////////////////////////////////////////
// Load/Store

void CFileGrabber::StoreToFile()
{
	QVariantMap Grabber;
	Grabber["Files"] = Store();

	QVariantList Tasks;
	foreach(uint64 ID, m_GrabList.keys())
	{
		SGrabTask* pCurTask = m_GrabList.value(ID);
		QVariantMap Task;
		Task["ID"] = ID;
		QVariantList Files;
		foreach(uint64 FileID, pCurTask->Files)
			Files.append(FileID);
		Task["Files"] = Files;
		Task["Uris"] = pCurTask->Uris;
		Tasks.append(Task);
	}
	Grabber["Tasks"] = Tasks;

	CXml::Write(Grabber, theCore->Cfg()->GetSettingsDir() + "/GrabberList.xml");
}

void CFileGrabber::LoadFromFile()
{
	QVariantMap Grabber = CXml::Read(theCore->Cfg()->GetSettingsDir() + "/GrabberList.xml").toMap();
	Load(Grabber["Files"].toList());

	foreach(CFile* pFile, m_FileMap.values())
	{
		if(pFile->IsStarted())
			pFile->Enable();
	}

	foreach(const QVariant& vTask, Grabber["Tasks"].toList())
	{
		QVariantMap Task = vTask.toMap();
		SGrabTask* pCurTask = new SGrabTask(Task["Uris"].toStringList());
		foreach(const QVariant& vID, Task["Files"].toList())
			pCurTask->Files.insert(vID.toULongLong());
		m_GrabList.insert(Task["ID"].toUInt(), pCurTask);
	}
}
