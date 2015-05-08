#pragma once

#include "../../Framework/ObjectEx.h"
#include "PartMap.h"
#include "FileList.h"

class CFileList;
class CDownload;
class CUpload;
class CFile;
class CFileHash;
class CPartMap;
class CTransfer;
class CBandwidthLimit;
class CHosterLink;
class CTorrent;
class CHashInspector;
class CPartDownloader;
class CFileStats;
class CArchiveSet;
class CFileDetails;

class CFile: public QObjectEx
{
	Q_OBJECT

public:
	CFile(QObject* qObject = NULL);
	~CFile();

	CFileList*						GetList()							{return qobject_cast<CFileList*>(parent());}
	virtual uint64					GetFileID()							{return m_FileID;}
	virtual void					SetFileID(uint64 FileID);

	// File creation function
	virtual bool					AddFromFile(const QString& FilePath);				// Add file as cmpleted file
	virtual void					AddEmpty(EFileHashType MasterHash, const QString& FileName, uint64 uFileSize, bool bPending = true);	// Add file as incmpleted file
	virtual bool					AddNewMultiFile(const QString& FileName, const QList<uint64>& SubFiles);

	virtual bool					AddTorrentFromFile(const QByteArray& Torrent);

	virtual bool					MakeTorrent(uint64 uPieceLength = 0, bool bMerkle = false, const QString& Name = "", bool bPrivate = false);
	virtual bool					AddTorrent(const QByteArray &TorrentData);
	virtual void					TorrentHashed(CTorrent* pTorrent, bool bSuccess);
	virtual CTorrent*				GetTorrent(const QByteArray& InfoHash);
	virtual const QMap<QByteArray, CTorrent*>& GetTorrents() {return m_Torrents;}
	virtual CTorrent*				GetTopTorrent();

	// File Processing
	virtual void					Process(UINT Tick);

	// File Operations
	virtual void					Start(bool bPaused = false, bool bOnError = false);
	virtual void					Pause()	{Start(true);}
	virtual void					Stop(bool bOnError = false);
	virtual void					Resume();
	virtual void					Suspend();
	virtual void					Rehash();
	virtual bool					Remove(bool bDeleted = false, bool bDeleteComplete = false, bool bForce = false, bool bCleanUp = true);
	virtual void					UnRemove(CFileHashPtr pFileHash = CFileHashPtr());

	virtual void					SetPending(bool bPending = true);

	// File properties
	virtual	void					SetFileSize(const uint64 FileSize);
	virtual	uint64					GetFileSize()						{return m_FileSize;}
	virtual	uint64					GetFileSizeEx();

	virtual QList<uint64>			GetSubFiles();
	virtual QList<uint64>			GetParentFiles();
	virtual bool					ReplaceSubFile(CFile* pOldFile, CFile* pNewFile);
	virtual bool					IsMultiFile();
	virtual bool					IsSubFile();
	virtual bool					MetaDataMissing();

	virtual QMap<EFileHashType, CFileHashPtr>& GetHashMap()				{return m_HashMap;}
	virtual void					AddHash(CFileHashPtr pFileHash);
	virtual CFileHash*				GetHash(EFileHashType Type)			{return GetHashPtr(Type).data();}
	virtual QList<CFileHashPtr>		GetHashes(EFileHashType Type);
	virtual QList<CFileHashPtr>		GetAllHashes(bool bAll = false);
	virtual CFileHashPtr			GetHashPtr(EFileHashType Type);
	virtual CFileHashPtr			GetHashPtrEx(EFileHashType Type, const QByteArray& Hash);
	virtual bool					CompareHash(const CFileHash* pFileHash);
	virtual void					DelHash(CFileHashPtr pFileHash);
	virtual void					Purge(CFileHashPtr pFileHash);
	virtual QList<CFileHashPtr>		GetListForHashing(bool bEmpty = false);

	virtual bool					SelectMasterHash();
	virtual void					SetMasterHash(CFileHashPtr pMasterHash) {m_pMasterHash = pMasterHash;}
	virtual CFileHashPtr&			GetMasterHash()						{return m_pMasterHash;}

	virtual void					CleanUpHashes();

#ifndef NO_HOSTERS
	virtual const QMap<QByteArray, CArchiveSet*>& GetArchives()			{return m_Archives;}
	virtual CArchiveSet*			GetArchive(const QByteArray& Hash)	{return m_Archives.value(Hash, NULL);}
#endif

	virtual int 					IsAutoShare();
	virtual int						IsTorrent();	// BitTorrent 0 off 1 all torrent 2 one torrent 
	virtual bool					IsEd2kShared();	// eDonkey / eMule
	virtual bool					IsNeoShared();	// NeoShare
#ifndef NO_HOSTERS
	virtual int						IsHosterDl();	// Hoster Part Download 0 off 1 on 2 on with captcha
	virtual bool					IsHosterUl();	// Hoster Part Auto Upload (Hoster Cache)
	virtual bool					IsArchive();	// Hoster Archive Download, no P2P
	virtual bool					IsRawArchive();	// This Archive does not have a verification hash
#endif

	virtual	void					SetFileName(const QString& FileName);
	virtual	QString					GetFileName()						{return m_FileName;}

	virtual CFileDetails*			GetDetails()						{return m_FileDetails;}

	virtual void					GrabDescription();

	virtual void					SetProperty(const QString& Name, const QVariant& Value);
	virtual QVariant				GetProperty(const QString& Name, const QVariant& Default = QVariant());
	virtual bool					HasProperty(const QString& Name)						{return m_Properties.contains(Name);}
	//virtual void					SetProperty(const QString& Name, const QVariant& Value)	{setProperty(Name.toLatin1(), Value);}
	//virtual QVariant				GetProperty(const QString& Name)						{return property(Name.toLatin1());}
	virtual QList<QString>			GetAllProperties()										{return m_Properties.uniqueKeys();}
	virtual QVariantMap&			GetProperties()											{return m_Properties;}

	// File Status
	virtual	void					SetFilePath(const QString& FilePath);
	virtual	void					SetFilePath(bool bComplete = false);
	virtual	QString					GetFilePath()						{return m_FilePath;}
	virtual void					SetFileDir(const QString& Dir)		{m_FileDir = Dir; if(!m_FileDir.isEmpty() && m_FileDir.right(1) != "/") m_FileDir += "/";}
	virtual	QString					GetFileDir()						{return m_FileDir;}

	virtual time_t					GetActiveTime();

	virtual bool					IsStarted();
	virtual bool					IsPaused(bool bIgnore = false);
	virtual bool					IsRemoved()							{return m_Status == eRemoved;} 
	virtual bool					IsComplete(bool bAux = false);
	virtual bool					IsIncomplete();
	virtual bool					IsHashing();
	virtual bool					IsAllocating();
	virtual bool					IsHalted()							{return m_Halted;}
	virtual void					SetHalted(bool bSet);

	virtual bool					IsPending()							{return m_Status == ePending;}
	virtual bool					IsDuplicate()						{return m_Status == eDuplicate;}

	virtual CPartMap*				GetPartMap()						{return m_Parts.data();}
	virtual CPartMapPtr&			GetPartMapPtr()						{return m_Parts;}

	virtual CPartDownloader*		GetDownloader()						{ASSERT(m_Downloader); return m_Downloader;}

	virtual	uint64					GetStatusStats(uint32 eStatus, bool bInverse = false);

	virtual void					SetError(const QString& Error)		{m_Error = Error; Stop(true);}
	virtual bool					HasError()							{return !m_Error.isEmpty();} 
	virtual const QString&			GetError()							{return m_Error;}
	virtual void					ClearError()						{m_Error.clear();}

	virtual qint64					UploadedBytes()						{return m_UploadedBytes;}
	virtual qint64					DownloadedBytes()					{return m_DownloadedBytes;}

	virtual void					SetDownloads(int Active, int Waiting) {m_ActiveDownloads = Active; m_WaitingDownloads = Waiting;}
	virtual int						GetActiveDownloads()				{return m_ActiveDownloads;}
	virtual int						GetWaitingDownloads()				{return m_WaitingDownloads;}
	virtual void					SetUploads(int Active, int Waiting) {m_ActiveUploads = Active; m_WaitingUploads = Waiting;}
	virtual int						GetActiveUploads()					{return m_ActiveUploads;}
	virtual int						GetWaitingUploads()					{return m_WaitingUploads;}

	// Transfers
	virtual QSet<CTransfer*>		GetTransfers()						{return m_Transfers;}
	virtual void					AddTransfer(CTransfer* pTransfer);
	virtual void					RemoveTransfer(CTransfer* pTransfer, bool bDelete = true);
	virtual CTransfer*				GetTransfer(uint64 ID);

	virtual	void					CancelRequest(uint64 uBegin, uint64 uEnd);

	virtual	CFileStats*				GetStats()							{return m_FileStats;}

	virtual int						GetQueuePos()						{return m_QueuePos;}
	virtual void					SetQueuePos(int QueuePos, bool bAdjust = true);

	virtual CBandwidthLimit*		GetUpLimit()						{return m_UpLimit;}
	virtual CBandwidthLimit*		GetDownLimit()						{return m_DownLimit;}

	virtual bool					CheckModification(bool bDateOnly = false);
	virtual bool					CheckDeleted();

	virtual CHashInspector*			GetInspector()						{ASSERT(m_Inspector); return m_Inspector;}

	virtual void					InitEmpty();
	virtual void					SetPartMap(CPartMapPtr pMap);
	virtual CSharedPartMap*			SharePartMap();

	// File Operations
	virtual void					Enable();
	virtual void					Disable();
	virtual void					Hold(bool bDelete = false);
	virtual bool					OpenIO();
	virtual void					ProtectIO(bool bSetReadOnly = true);
	virtual void					CloseIO();
	virtual void					UpdateBC();

#ifndef NO_HOSTERS
	virtual bool					CompleteFromSolid(const QString& FileName);
#endif

	// Load/Store
	virtual QVariantMap				Store();
	virtual bool					Load(const QVariantMap& File);

	virtual void					AddLogLine(time_t uStamp, uint32 uFlag, const CLogMsg& Line);

public slots:
	virtual void					OnAllocation(uint64 Progress, bool Finished);
	virtual void					OnDataWriten(uint64 Offset, uint64 Length, bool bOk, void* Aux);

	// File Processing
	virtual void					OnFileHashed();
	virtual void					OnCompleteFile();

	// File Status
	virtual void 					OnBytesWritten(qint64 Bytes)		{m_UploadedBytes += Bytes;}
    virtual void 					OnBytesReceived(qint64 Bytes)		{m_DownloadedBytes += Bytes;}

	virtual void					MediaInfoRead(int Index);

signals:
	void							Update();
	void							HavePiece(int Index); // torretn only
	void							FileVerified();
	void							MetadataLoaded();

protected:
	friend class CFileList;
	friend class CFileManager;
	friend class CArchiveDownloader;
	friend class CTorrent;
	friend class CHashInspector;
	friend class CPartDownloader;
	friend class CCoreServer;

	// File creation function
	virtual void					CalculateHashes(bool bAll = false);
	// File Processing
	virtual void					EmitHavePiece(int Index)			{emit HavePiece(Index);}

	virtual void					StartAllocate();

	// File Status
	virtual bool					CheckDuplicate();

	virtual void					PurgeFile();

	virtual void					Merge(CFile* pFile);
	
	virtual bool					SubFilesComplete();

	virtual void					UpdateSelectionPriority();

	// member variables
	uint64							m_FileID; // global unique fileID

	QString							m_FileName;
	QString							m_FileDir;
	uint64							m_FileSize;
	QMap<EFileHashType,CFileHashPtr>m_HashMap;
	CFileHashPtr					m_pMasterHash; // Note: the m_MasterHash variable also indicates wather its a compelte file or not, complete fiels dont have a masterhash
	QString							m_FilePath;

	CPartDownloader*				m_Downloader;

	CHashInspector*					m_Inspector;

	enum EState
	{
		eStarted,
		ePaused,
		eStopped,
		eUnknown
	}								m_State;
	enum EStatus
	{
		eNone,
		eDuplicate,		// Note: a duplicate is always a compelte file, incomplete files get set into an error state
		eComplete,
		eIncomplete,
		ePending,
		eRemoved,
	}								m_Status;
	bool							m_Halted;
	QString							m_Error;

	QSet<CTransfer*>				m_Transfers;

	CPartMapPtr						m_Parts;

	QVariantMap						m_Properties;

	int								m_QueuePos;

	CBandwidthLimit*				m_UpLimit;
	CBandwidthLimit*				m_DownLimit;

	QMap<QByteArray, CTorrent*>		m_Torrents;

#ifndef NO_HOSTERS
	QMap<QByteArray, CArchiveSet*>	m_Archives;
#endif

	CFileStats*						m_FileStats;
	CFileDetails*					m_FileDetails;

	uint64							m_UploadedBytes;
	uint64							m_DownloadedBytes;

	int								m_ActiveDownloads;
	int								m_WaitingDownloads;
	int								m_ActiveUploads;
	int								m_WaitingUploads;

	struct SStatusStat
	{
		SStatusStat() 
		 : uLength(0),uRevision(0),uInvalidate(0) {}
		uint64 uLength;
		uint32 uRevision;
		uint64 uInvalidate;
	};
	QMap<int, SStatusStat>			m_StatusStats;
};
