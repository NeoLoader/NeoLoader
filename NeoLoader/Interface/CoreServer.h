#pragma once

#include "../../Framework/IPC/IPCSocket.h"
class CCoreBus;
class CWebRoot;
class CWebUI;
class CWebAPI;
class CFile;
class CTransfer;
class CWebTask;
class CDebugTask;
class CHtmlSession;
class CWebSession;
class CHtmlBrowser;
class CXMLElement;
class CNeoFS;
class CHosterLink;
class CArchiveSet;

class CCoreServer: public CIPCServer
{
	Q_OBJECT

public:
	CCoreServer(QObject* qObject = NULL);
	~CCoreServer();

	virtual QString		CheckLogin(const QString &UserName, const QString &Password);

	virtual QVariant	ProcessRequest(const QString& Command, const QVariant& Parameters);

	static QVariantList	DumpLog(const QList<CLog::SLine>& Log, uint64 uLast = 0);

protected:
	//CCoreBus*			m_Bus;

	CWebRoot*			m_WebRoot;
	CWebUI*				m_WebUI;
	CWebAPI*			m_WebAPI;
    CNeoFS*             m_NeoFS;

	friend class CWebUI;

	QVariantMap			Console(QVariantMap Request);
	QVariantMap			GetInfo(QVariantMap Request);
	QVariantMap			GetLog(QVariantMap Request);
	QVariantMap			Shutdown(QVariantMap Request);

	QVariantMap			FileList(QVariantMap Request);
	QVariantMap			GetProgress(QVariantMap Request);
	QVariantMap			GetFile(QVariantMap Request);
	QVariantMap			SetFile(QVariantMap Request);
	QVariantMap			AddFile(QVariantMap Request);
	QVariantMap			FileAction(QVariantMap Request);
	QVariantMap			FileIO(QVariantMap Request);
	QVariantMap			GetTransfers(QVariantMap Request);
	QVariantMap			TransferAction(QVariantMap Request);
	QVariantMap			GetClients(QVariantMap Request);
	QVariantMap			GetHosting(QVariantMap Request);

	QVariantMap			GetCore(QVariantMap Request);
	QVariantMap			SetCore(QVariantMap Request);
	QVariantMap			CoreAction(QVariantMap Request);

	QVariantMap			ServerList(QVariantMap Request);
	QVariantMap			ServerAction(QVariantMap Request);

	QVariantMap			ServiceList(QVariantMap Request);
	QVariantMap			GetService(QVariantMap Request);
	QVariantMap			SetService(QVariantMap Request);
	QVariantMap			ServiceAction(QVariantMap Request);

	QVariantMap			WebTaskList(QVariantMap Request);

	QVariantMap			GrabLinks(QVariantMap Request);
	QVariantMap			MakeLink(QVariantMap Request);

	QVariantMap			GrabberList(QVariantMap Request);
	QVariantMap			GrabberAction(QVariantMap Request);

#ifdef CRAWLER
	QVariantMap			CrawlerList(QVariantMap Request);
	QVariantMap			CrawlerAction(QVariantMap Request);
	QVariantMap			GetCrawler(QVariantMap Request);
	QVariantMap			SetCrawler(QVariantMap Request);
#endif

	QVariantMap			SearchList(QVariantMap Request);
	QVariantMap			StartSearch(QVariantMap Request);
	QVariantMap			StopSearch(QVariantMap Request);

	QVariantMap			DiscoverContent(QVariantMap Request);
	QVariantMap			FetchContents(QVariantMap Request);
	QVariantMap			GetContent(QVariantMap Request);
	QVariantMap			GetStream(QVariantMap Request);

	QVariantMap			GetCaptchas(QVariantMap Request);
	QVariantMap			SetSolution(QVariantMap Request);

	QVariantMap			Test(QVariantMap Request);

private:

	struct SStateCache
	{
		SStateCache() {}
		virtual ~SStateCache() {}
		uint64			LastUpdate;
	};

	QMap<uint64, SStateCache*>	m_StatusCache;

	UINT GetFileType(CFile* pFile);
	QString FileTypeToStr(UINT Type);
	UINT GetFileState(CFile* pFile);
	QString FileStateToStr(UINT State);
	UINT GetFileStatus(CFile* pFile);
	QString FileStatusToStr(UINT Status);
	UINT GetFileJobs(CFile* pFile);
	QStringList FileJobsToStr(UINT Jobs);
	QVariantMap DumpHosters(QMap<QString, QMap<QString, double> > HostingInfo, QMap<QString, QMap<QString, QList<CHosterLink*> > > &AllLinks, CFile* pFile, CArchiveSet* pArchive = NULL);

	struct SCachedFile
	{
		SCachedFile()
		{
			FileSize = 0;

			FileType = eUnknownFile;
			FileState = eUnknownState;
			FileStatus = eUnknownStatus;
			FileJobs = eNone;

			Progress = 0;
			Availability = 0;
			AuxAvailability = 0;
			Transfers = 0;
			ConnectedTransfers = 0;
			CheckedTransfers = 0;
			SeedTransfers = 0;
			UpRate = 0;
			Upload = 0;
			DownRate = 0;
			Download = 0;
			Uploaded = 0;
			Downloaded = 0;
			QueuePos = 0;
		}

		QString FileName;
		QString FileDir;

		uint64 FileSize;

		enum EType
		{
			eUnknownFile, 
			eFile,
			eArchive,
			eCollection,
			eMultiFile,
		};
		UINT FileType;

		enum EState
		{
			eUnknownState,
			eRemoved,
			eHalted,
			eDuplicate,
			ePending,
			eStarted,
			ePaused,
			eStopped
		};
		UINT FileState;

		enum EStatus
		{
			eUnknownStatus,
			eError,
			eEmpty,
			eIncomplete,
			eComplete
		};
		UINT FileStatus;

#ifndef NO_HOSTERS
		QString HosterStatus;
#endif

		enum EJob
		{
			eNone = 0x00,
			eAllocating = 0x01,
			eHashing = 0x02,
			ePacking = 0x04,
			eExtracting = 0x08,
			eSearching = 0x10
		};
		UINT FileJobs;

		int Progress;
		double Availability;
		double AuxAvailability;
		int Transfers;
		int ConnectedTransfers;
		int CheckedTransfers;
		int SeedTransfers;
		int UpRate;
		int Upload;
		int DownRate;
		int Download;
		uint64 Uploaded;
		uint64 Downloaded;
		int QueuePos;

		QVariantMap File;
	};

	struct SFileStateCache: SStateCache
	{
		virtual ~SFileStateCache()
		{
			foreach(SCachedFile* pFile, Map)
				delete pFile;
			Map.clear();
		}
		QMap<uint64, SCachedFile*>	Map;
	};

	UINT GetTransferStatus(CTransfer* pTransfer);
	QString TransferStatusToStr(UINT Status);
	UINT GetUploadState(CTransfer* pTransfer);
	UINT GetDownloadState(CTransfer* pTransfer);
	QString TransferStateToStr(UINT State);

	struct SCachedTransfer
	{
		SCachedTransfer()
		{
			TransferStatus = 0;
			UploadState = 0;
			DownloadState = 0;

			UpRate = 0;
			Upload = 0;

			DownRate = 0;
			Download = 0;
			Uploaded = 0;
			Downloaded = 0;

			FileSize = 0;
			Available = 0;
			Progress = 0;
			LastDownloaded = 0;
			LastUploaded = 0;
		}

		QString Url;
		QString Type;
		QString Found;

		enum EStatus
		{
			eUnknownStatus,
			eUnchecked,
			eConnected,
			eChecked,
			eError
		};
		UINT TransferStatus;

		enum EState
		{
			eUnknownState,
			eNone,
			eIdle,
			ePending,
			eWaiting, // eChoked
			eTansfering // eUnchoked
		};
		UINT UploadState;
		UINT DownloadState;

		int UpRate;
		int Upload;
		int DownRate;
		int Download;
		uint64 Downloaded;
		uint64 Uploaded;

		QString FileName;

		uint64 FileSize;
		uint64 Available;
		int Progress;
		QString Software;
		uint64 LastDownloaded;
		uint64 LastUploaded;

		QVariantMap Transfer;
	};

	struct STransferStateCache: SStateCache
	{
		virtual ~STransferStateCache()
		{
			foreach(SCachedTransfer* pTransfer, Map)
				delete pTransfer;
			Map.clear();
		}
		QMap<uint64, SCachedTransfer*>	Map;
	};

	struct SCachedContent
	{

	};
	
	struct SContentStateCache: SStateCache
	{
		virtual ~SContentStateCache()
		{
			foreach(SCachedContent* pContent, Map)
				delete pContent;
			Map.clear();
		}
		QMap<uint64, SCachedContent*>	Map;
	};
};
