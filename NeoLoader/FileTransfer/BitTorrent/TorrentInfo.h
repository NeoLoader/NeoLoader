#pragma once

#include "../../../Framework/ObjectEx.h"
#include "../../FileList/Hashing/FileHash.h"

#define META_DATA_BLOCK_SIZE	16384

class CTorrentInfo: public QObjectEx
{
	Q_OBJECT

public:
	CTorrentInfo(QObject* qObject = NULL);
	~CTorrentInfo();

	bool					LoadTorrentFile(const QString& TorrentFile);
	bool					LoadTorrentFile(const QByteArray& Torrent);
	bool					LoadMetadata(const QByteArray& Metadata);
	QByteArray				SaveTorrentFile();
	bool					SaveTorrentFile(const QString& TorrentFile);

	struct SFileInfo
	{
		qint64						Length;
		QStringList					FilePath;
		QString						FileName;
	};

	void					MakeMetadata(const QList<SFileInfo>& FileList, uint64 PieceLength, const QList<QByteArray>& PieceHashes, const QByteArray& RootHash = QByteArray());

	QByteArray				GetInfoHash()												{return m_InfoHash;}
	void					SetInfoHash(const QByteArray& InfoHash)						{m_InfoHash = InfoHash;}
	quint64					GetTotalLength()											{return m_TotalLength;}
	void					SetTotalLength(uint64 TotalLength)							{m_TotalLength = TotalLength;}

	const QString&			GetTorrentName()											{return m_TorrentName;}
	void					SetTorrentName(const QString& TorrentName)					{m_TorrentName = TorrentName;}
	bool					IsMultiFile()												{return m_Files.count() > 1;}

	bool					IsEmpty()													{return m_PieceLength == 0;}
	bool					IsMerkle()													{return !m_RootHash.isEmpty();}
	void					SetPrivate()												{m_Private = true;}
	bool					IsPrivate()													{return m_Private;}

	const QList<QByteArray>&GetPieceHashes()											{ASSERT(!m_PieceHashes.isEmpty()); return m_PieceHashes;}
	const QByteArray&		GetRootHash()												{ASSERT(!m_RootHash.isEmpty()); return m_RootHash;}
	quint64					GetPieceLength()											{return m_PieceLength;}

	const QList<SFileInfo>& GetFiles()													{ASSERT(!m_Files.isEmpty()); return m_Files;}

	const QMultiMap<int, QString>& GetTrackerList()										{return m_Trackers;}
	void					SetTrackerList(const QMultiMap<int, QString>& Trackers)		{m_Trackers = Trackers;}

	void					SetProperty(const QString& Name, const QVariant& Value)		{if(Value.isValid()) m_Properties.insert(Name, Value); else m_Properties.remove(Name);}
	QVariant				GetProperty(const QString& Name)							{return m_Properties.value(Name);}
	bool					HasProperty(const QString& Name)							{return m_Properties.contains(Name);}
	//void					SetProperty(const QString& Name, const QVariant& Value)		{setProperty(Name.toLatin1(), Value);}
	//QVariant				GetProperty(const QString& Name)							{return property(Name.toLatin1());}
	QList<QString>			GetAllProperties()											{return m_Properties.uniqueKeys();}

	QByteArray				GetMetadataBlock(int Index);
	uint64					GetMetadataSize()											{return m_Metadata.size();}
	
protected:
	QByteArray				m_InfoHash;
	quint64					m_TotalLength;

	QString					m_TorrentName;
	QVariantMap				m_Properties;

	QList<SFileInfo>		m_Files;
	QMultiMap<int, QString>	m_Trackers;

	QList<QByteArray>		m_PieceHashes;
	QByteArray				m_RootHash;
	quint64					m_PieceLength;
	bool					m_Private;

	QByteArray				m_Metadata;
};
