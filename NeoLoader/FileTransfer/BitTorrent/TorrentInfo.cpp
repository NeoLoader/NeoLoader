#include "GlobalHeader.h"
#include "TorrentInfo.h"
#include "TorrentManager.h"
#include "../../NeoCore.h"
#include "../../../qbencode/lib/bencode.h"
#include "../../../Framework/Cryptography/HashFunction.h"
#include "../../../Framework/Strings.h"
#include "../../../Framework/Xml.h"
#include "../../FileList/Hashing/HashingThread.h"

// http://fileformats.wikia.com/wiki/Torrent_file

QString fromUtf8(QByteArray arr, bool bPath = false)
{
    string temp = arr.data();
	verify_encoding(temp, bPath);
	wchar_t* res = UTF8toWCHAR(temp.c_str());
	QString Ret = QString::fromWCharArray(res);
	delete [] res;
	return Ret;
}

CTorrentInfo::CTorrentInfo(QObject* qObject)
: QObjectEx(qObject) 
{
	m_TotalLength = 0;
	m_PieceLength = 0;
	m_Private = false;
}

CTorrentInfo::~CTorrentInfo()
{
}

bool CTorrentInfo::LoadTorrentFile(const QString& TorrentFile)
{
	QFile File(TorrentFile);
    if (!File.open(QIODevice::ReadOnly))
		return false;
	return LoadTorrentFile(File.readAll());
}

bool CTorrentInfo::LoadTorrentFile(const QByteArray& Torrent)
{
    Bdecoder Decoder(Torrent);
    BencodedMap Dict;
    Decoder.read(&Dict);
	if(Decoder.error())
		return false;

	QByteArray Metadata = Dict["info"].buffer();
	m_InfoHash = CHashFunction::Hash(Metadata, CAbstractKey::eSHA1);
	if(!LoadMetadata(Metadata))
		return false;

	//if(Dict.contains("hoster-list"))
	//	m_Properties["hoster-list"] = Dict.get("hoster-list");

	int Tier = 0;
	foreach(const QVariant& TrackerEntry, Dict.get("announce-list").toList())
	{
		foreach(const QVariant& TrackerSubEntry, TrackerEntry.toList())
			m_Trackers.insert(Tier,TrackerSubEntry.toString());
		Tier++;
	}
	if(m_Trackers.isEmpty() && Dict.contains("announce"))
		m_Trackers.insert(0,Dict.get("announce").toString());

	if(Dict.contains("creation date"))
		m_Properties["CreationTime"] = QDateTime::fromTime_t(Dict.get("creation date").toULongLong());
	if(Dict.contains("comment"))
		m_Properties["Description"] = fromUtf8((Dict.contains("comment.utf-8") ? Dict.get("comment.utf-8") : Dict.get("comment")).toByteArray());
	if(Dict.contains("created by"))
		m_Properties["Creator"] = Dict.get("created by").toString();
	
	return !Decoder.error();
}

bool CTorrentInfo::SaveTorrentFile(const QString& TorrentFile)
{
	QByteArray Buffer = SaveTorrentFile();

	QFile File(TorrentFile);
    if (File.open(QIODevice::ReadWrite))
	{
		File.write(Buffer);
		return true;
	}
	return false;
}

QByteArray CTorrentInfo::SaveTorrentFile()
{
	BencodedMap Dict;
	Dict["info"] = BencodedBuffer(m_Metadata);

	//if(m_Properties.contains("hoster-list"))
	//	Dict.set("hoster-list", m_Properties.value("hoster-list"));

	if(!m_Trackers.isEmpty())
	{
		QVariantList Trackers;
		foreach(int Tier, m_Trackers.uniqueKeys())
		{
			QVariantList SubTrackers;
			foreach(const QString& Tracker, m_Trackers.values(Tier))
				SubTrackers.append(Tracker);
			Trackers.append((QVariant)SubTrackers);
		}
		if(!Trackers.isEmpty())
			Dict.set("announce-list",Trackers);
		if(!m_Trackers.value(0).isEmpty())
			Dict.set("announce", m_Trackers.value(0)); 
	}

	if(m_Properties.contains("CreationTime"))
		Dict.set("creation date", (quint64)m_Properties["CreationTime"].toDateTime().toTime_t());
	if(m_Properties.contains("Description"))
		Dict.set("comment", m_Properties["Description"].toString());
	if(m_Properties.contains("Creator"))
		Dict.set("created by", m_Properties["Creator"].toString());

	return Bencoder::encode(Dict).buffer();
}

bool CTorrentInfo::LoadMetadata(const QByteArray& Metadata)
{
    Bdecoder Decoder(Metadata);
    QVariantMap Info;
    Decoder.read(&Info);
	if(Decoder.error())
		return false;

	m_TorrentName = fromUtf8((Info.contains("name.utf-8") ? Info["name.utf-8"] : Info["name"]).toByteArray(), true);

	uint64 TotalLength = 0;
    if (Info.contains("files")) 
	{
		QVariantList Files = Info["files"].toList();
        for (int i = 0; i < Files.size(); i++) 
		{
			QVariantMap File = Files.at(i).toMap();
			SFileInfo FileInfo;
			QVariantList Path = File.contains("path.utf-8") ? File["path.utf-8"].toList() : File["path"].toList();
			if(!Path.isEmpty())
			{
				FileInfo.FileName = fromUtf8(Path.takeLast().toByteArray(), true);
				foreach(const QVariant vName, Path)
					FileInfo.FilePath.append(fromUtf8(vName.toByteArray(), true));
			}
			else
				FileInfo.FileName = "unknown";
			FileInfo.Length = File["length"].toULongLong();
			m_Files.append(FileInfo);

			TotalLength += FileInfo.Length;
        }

		if(m_Files.size() == 1) // thats a pseudo multifile (only one file)
			m_TorrentName = m_Files.first().FileName;
    } 
	else if (Info.contains("length")) 
		TotalLength = Info["length"].toULongLong();
	else
		return false;
	uint64 PieceLength = Info["piece length"].toULongLong();
	if(Info.contains("pieces"))
	{
		QByteArray PieceHashes = Info["pieces"].toByteArray();
		m_PieceHashes.clear();
		for (int i = 0; i < PieceHashes.size(); i += 20)
			m_PieceHashes.append(PieceHashes.mid(i, 20));
	}

	if(Info.contains("root hash"))
		m_RootHash = Info["root hash"].toByteArray();

	if(Info.contains("private"))
		m_Private = Info["private"].toInt() != 0;

	if(m_RootHash.isEmpty() && DivUp(TotalLength,PieceLength) != m_PieceHashes.size())
		return false;

	m_Metadata = Metadata;
	m_TotalLength = TotalLength;
	m_PieceLength = PieceLength;
	return true;
}

void CTorrentInfo::MakeMetadata(const QList<SFileInfo>& FileList, uint64 PieceLength, const QList<QByteArray>& PieceHashes, const QByteArray& RootHash)
{
	m_Files = FileList;
	m_PieceLength = PieceLength;
	m_PieceHashes = PieceHashes;
	m_RootHash = RootHash;

	QVariantMap Info;

	Info["name"] = m_TorrentName.toUtf8();

	if(!m_Files.isEmpty())
	{
		QVariantList Files;
		for(int i=0; i < m_Files.size(); i++)
		{
			m_Files.at(i);
			QVariantMap File;
			QStringList Path = m_Files.at(i).FilePath;
			Path.append(m_Files.at(i).FileName);
			File["path"] = Path;
			File["length"] = m_Files.at(i).Length;
			Files.append(File);
		}
		Info["files"] = Files;
	}
	else
	{
		ASSERT(m_TotalLength != 0);
		Info["length"] = m_TotalLength;
	}

	Info["piece length"] = m_PieceLength;

	if(!m_PieceHashes.isEmpty())
	{
		QByteArray Pieces;
		foreach(const QByteArray& Piece, m_PieceHashes)
			Pieces.append(Piece);
		Info["pieces"] = Pieces;
	}
	else if(!RootHash.isEmpty())
		Info["root hash"] = RootHash;
	else {ASSERT(0);
	}

	if(m_Private)
		Info["private"] = 1;

	ASSERT(!RootHash.isEmpty() || DivUp(m_TotalLength,m_PieceLength) == m_PieceHashes.size());

	m_Metadata = Bencoder::encode(Info).buffer();
	m_InfoHash = CHashFunction::Hash(m_Metadata, CAbstractKey::eSHA1);
}

QByteArray CTorrentInfo::GetMetadataBlock(int Index)
{
	return m_Metadata.mid(Index*META_DATA_BLOCK_SIZE,META_DATA_BLOCK_SIZE);
}
