#pragma once

class CFileSyncJob;
class CProgressBar;
class CFileItem;
class CFitnessBars;
class CSourceCell;
class CFilesModel;
class QTreeViewEx;
class QSpinBoxEx;

class CFileListView: public QWidget
{
	Q_OBJECT

public:
	enum EMode
	{
		eUndefined = 0,
		eDownload,		// StateString
		eSharedFiles,	// StateString
		eFileArchive,	// StateString
		eFilesSearch,	// SearchID 
		eFilesGrabber,	// GrabberID
		eSubFiles,		// RootID
	};

	enum ESubMode
	{
		eAll = 0,
		eStarted,
		ePaused,
		eStopped,
		eCompleted,
	};

	CFileListView(UINT Mode = 0, QWidget *parent = 0);
	~CFileListView();

	void				SetID(uint64 ID);
	uint64				GetID()				{return m_ID;}

	void				ChangeMode(UINT Mode, UINT SubMode = 0);
	UINT				GetMode()			{return m_Mode;}

	void				AppendMenu(QAction* pAction)	{m_AuxMenus.append(pAction);}

	void				Suspend(bool bSet);

	QVariantMap			GetFile(uint64 ID);
	QList<uint64>		GetSelectedFilesIDs();

	static void			AddSource(uint64 ID);
	static void			DoAction(const QString& Action, uint64 ID, const QVariantMap& Options = QVariantMap());

	void				SetPasswords(const QStringList& Passwords);

private slots:
	void				OnMenuRequested(const QPoint &point);
	void				OnClicked(const QModelIndex& Index);
	void				OnDoubleClicked(const QModelIndex& Index);
	void				OnSelectionChanged(const QItemSelection& Selected, const QItemSelection& Deselected);

	void				SetFilter(const QRegExp& Exp);

	void				OnScroll();

	void				OnGrab()			{OnAction("Grab");}

	void				OnStart();
	void				OnPause();
	void				OnStop();
	void				OnForce()			{ SetProperty("Force", m_pForce->isChecked()); }
	void				OnDisable();
	void				OnDelete();
	void				OnRehashFile()		{OnAction("RehashFile");}
#ifdef CRAWLER
	void				OnArchiveFile()		{OnAction("ArchiveFile");}
#endif
	void				OnAddSource();

	void				OnStream();
	void				OnOpen();

	void				OnFindRating()		{OnAction("FindRating");}
	void				OnClearRating()		{OnAction("ClearRating");}
	void				OnMakeMulti();
	void				OnMakeTorrent();

	void				OnIndex()			{OnAction("FindIndex");}
	void				OnAlias()			{OnAction("FindAliases");}
	void				OnAnnounce()		{OnAction("Announce");}
	void				OnPublish()			{OnAction("Republish");}
	
	void				OnPriority(QAction* pAction)	{SetProperty("Priority", pAction->data());}
	void				OnUpLimit(int Limit)			{SetProperty("Upload", Limit * 1024);}
	void				OnDownLimit(int Limit)			{SetProperty("Download", Limit * 1024);}

	void				OnQueuePos(int Pos)				{SetProperty("QueuePos", Pos);}
	void				OnShareRatio(double Ratio)		{SetProperty("ShareRatio", int(Ratio*100));}

	//
	void				OnAutoShare()		{SetShare("AutoShare", m_pAutoShare->isChecked() ? 1 : 0);}
	void				OnNeoShare()		{SetShare("NeoShare", m_pEd2kShare->isChecked() ? 1 : 0);}
	void				OnTorrent(QAction* pAction)	{SetShare("Torrent", pAction->data().toInt());}
	void				OnEd2kShare()		{SetShare("Ed2kShare", m_pEd2kShare->isChecked() ? 1 : 0);}

	void				OnHosterDl(QAction* pAction)	{SetShare("HosterDl", pAction->data().toInt());}
	void				OnHosterUl()		{SetShare("HosterUl", m_pHosterUp->isChecked() ? 1 : 0);}

	void				OnReUpload(QAction* pAction)	{SetShare("ReUpload", pAction->data().toInt());}
	//

	void				OnGetLinks();
	void				OnPasteLinks();

	void				OnUploadParts();
	void				OnRemoveArchive()	{OnAction("RemoveArchive");}
	void				OnCheckLinks()		{OnAction("CheckLinks");}
	void				OnCleanupLinks()	{OnAction("CleanupLinks");}
	void				OnResetLinks()		{OnAction("ResetLinks");}
	void				OnInspectLinks()	{OnAction("InspectLinks");}
	void				OnSetPasswords();

	void				OnAction(const QString& Action, bool bWithSub = false, const QVariantMap& Options = QVariantMap());

	void				OnRefresh();

	void				OnCheckChanged(quint64 ID, bool State);
	void				OnUpdated();

signals:
	void				FileItemClicked(uint64 ID, bool bOnly);
	void				StreamFile(uint64 ID, const QString& FileName, bool bComplete);
	void				TogleDetails();

protected:
	friend class CFileSyncJob;
	friend class CProgressJob;
	friend class CFileItem;

	void				SyncFiles(const QVariantMap& Response, bool bFull);

	QList<uint64>		GetIDs(bool bWithSub = false);

	virtual void		timerEvent(QTimerEvent *e);

	void				SetShare(const QString& Network, int Share);
	void				SetProperty(const QString& Name, const QVariant& Value);

	int					m_TimerId;
	QString				m_Ops;
	UINT				m_Mode;
	UINT				m_SubMode;
	uint64				m_ID;
	bool				m_PendingExpand;
	bool				m_FindingIndex;
	uint64				m_SyncToken;
	QSet<uint64>		m_VisibleFiles;
	uint64				m_NextFullUpdate;

	bool				m_Hold;

	QList<QAction*>		m_AuxMenus;

	CFileSyncJob*		m_pFileSyncJob;

	QVBoxLayout*		m_pMainLayout;

	QMenu*				m_pMenu;

	QAction*			m_pGrab;
	QAction*			m_pRefresh;

	QAction*			m_pStart;
	QAction*			m_pPause;
	QAction*			m_pStop;
	QAction*			m_pDisable;
	QAction*			m_pDelete;
#ifdef CRAWLER
	QAction*			m_pArchiveFile;
#endif

	QMenu*				m_pPrioMenu;
	QActionGroup*		m_pPriority;
	QAction*			m_pPrioAuto;
	QAction*			m_pPrioHighest;
	QAction*			m_pPrioHigh;
	QAction*			m_pPrioMedium;
	QAction*			m_pPrioLow;
	QAction*			m_pPrioLowest;
	QSpinBoxEx*			m_pUpLimit;
	QSpinBoxEx*			m_pDownLimit;

	QMenu*				m_pQueueMenu;
	QAction*			m_pForce;
	QSpinBox*			m_pQueuePos;
	QDoubleSpinBox*		m_pShareRatio;

	QAction*			m_pAddSource;
	QMenu*				m_pKadMenu;
	QAction*			m_pIndex;
	QAction*			m_pAlias;
	QAction*			m_pAnnounce;
	QAction*			m_pPublish;

	QAction*			m_pStream;

	QAction*			m_pMakeMulti;
	QAction*			m_pMakeTorrent;

	QAction*			m_pPasteLink;
	QMenu*				m_pLinksMenu;
	QAction*			m_pGetLinks;
	QAction*			m_pCopyNeo;
	QAction*			m_pCopyMagnet;
	QAction*			m_pCopyEd2k;
	QAction*			m_pSaveTorrent;

	//
	QMenu*				m_pShare;

	QAction*			m_pAutoShare;

	QAction*			m_pNeoShare;

	QActionGroup*		m_pTorrent;
	QAction*			m_pTorrentOff;
	QAction*			m_pTorrentOne;
	QAction*			m_pTorrentOn;

	QAction*			m_pEd2kShare;

	QActionGroup*		m_pHosterDl;
	QAction*			m_pHosterDlOff;
	QAction*			m_pHosterDlOn;
	QAction*			m_pHosterDlEx;
	QAction*			m_pHosterDlArch;

	QAction*			m_pHosterUp;

	QActionGroup*		m_pReUpload;
	QAction*			m_pReUploadOff;
	QAction*			m_pReUploadDef;
	QAction*			m_pReUploadOn;
	//

	QMenu*				m_pHosterMenu;
	QAction*			m_pUploadParts;
	QAction*			m_pRemoveArchive;
	QAction*			m_pCheckLinks;
	QAction*			m_pCleanupLinks;
	QAction*			m_pResetLinks;
	QAction*			m_pInspectLinks;
	QAction*			m_pSetPasswords;

	QMenu*				m_pToolsMenu;
	QAction*			m_pRehashFile;

	QAction*			m_pShowDetails;

	QTreeViewEx*		m_pFileTree;
	CFilesModel*		m_pFileModel;
	QSortFilterProxyModel* m_pSortProxy;
	QMap<uint64, QPair<QPointer<QWidget>, QPersistentModelIndex> > m_ProgressMap;
	QMap<uint64, QPair<QPointer<CFitnessBars>, QPersistentModelIndex> > m_FitnessMap;
	QMap<uint64, QPair<QPointer<CSourceCell>, QPersistentModelIndex> > m_SourceMap;
};

QString Mode2Str(UINT Mode);
