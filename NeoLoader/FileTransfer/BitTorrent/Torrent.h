#pragma once

#include "../../../Framework/ObjectEx.h"
#include "../../FileList/File.h"
#include "../../FileTransfer/BitTorrent/TorrentPeer.h"
class CTorrentInfo;

#define EMPTY_INFO_HASH QByteArray(CFileHash::GetSize(HashTorrent), 0)

class CTorrent: public QObjectEx
{
	Q_OBJECT

public:
	CTorrent(CFile* pFile);
	~CTorrent();

	virtual bool					AddTorrentFromFile(const QByteArray& Torrent);
	virtual void					SetupPartMap();
	virtual void					AddEmptyTorrent(const QString& FileName, const QByteArray& InfoHash);
	virtual	bool					InstallMetadata();
	virtual bool					MakeTorrent(uint64 uPieceLength, bool bMerkle = false, const QString& Name = "", bool bPrivate = false);
	virtual bool					ImportTorrent(const QByteArray &TorrentData);

	virtual	bool					CompareSubFiles(CPartMap* pPartMap);

	virtual CTorrentInfo*			GetInfo()							{ASSERT(m_TorrentInfo); return m_TorrentInfo;}
	virtual QByteArray				GetInfoHash();
	virtual CFileHashPtr			GetHash();

	virtual CFile*					GetFile()							{return qobject_cast<CFile*>(parent());}

	virtual int						NextMetadataBlock(const QByteArray& ID);
	virtual void					AddMetadataBlock(int Index, const QByteArray& MetadataBlock, uint64 uTotalSize, const QByteArray& ID);
	virtual void					ResetMetadataBlock(int Index, const QByteArray& ID);

	virtual QString					GetSubFileName(int Index);
	virtual QString					GetSubFilePath(int Index);

	virtual bool					LoadTorrentFromFile(const QByteArray& InfoHash);
	virtual void					RemoveTorrentFile();
	virtual bool					SaveTorrentToFile();
	
	virtual bool					TryInstallMetadata();

signals:
	void							MetadataLoaded();

public slots:
	virtual void					OnFileHashed();

protected:
	virtual bool					LoadPieceHashes();

	virtual QByteArray				TryAssemblyMetadata();

	CTorrentInfo*					m_TorrentInfo;
	CFileHashPtr					m_pHash;

	struct SMetadata
	{
		SMetadata() : MetadataSize(0) {}
		uint64 MetadataSize; 
		QMap<int, QByteArray>	Blocks;
	};
	struct SMetadataExchange
	{
		SMetadataExchange() : NewData(false) {}
		bool NewData;
		QMap<QByteArray, SMetadata> Metadata;
	};
	SMetadataExchange*				m_MetadataExchange;
};
