#include "GlobalHeader.h"
#include "ProgressBar.h"
#include "FileListView.h"
#include "FileListModel.h"
#include "LinkDialog.h"
#include "TorrentCreator.h"
#include "HosterUploader.h"
#include "SearchWindow.h"
#include "GrabberWindow.h"
#include "../NeoGUI.h"
#include "../Common/TreeWidgetEx.h"
#include "../Common/TreeViewEx.h"
#include "../Common/FitnessBars.h"
#include "SourceCell.h"
#include <QWidgetAction>
#include "GUI/SettingsWidgets.h"
#include "../Common/MultiLineInput.h"
#include "../Common/Common.h"
#include "../Common/Dialog.h"

QString Mode2Str(UINT Mode)
{
	switch(Mode)
	{
	case CFileListView::eUndefined:		return "FileList";
	case CFileListView::eDownload:		return "Download";
	case CFileListView::eSharedFiles:	return "SharedFiles";
	case CFileListView::eFileArchive:	return "FileArchive";
	case CFileListView::eFilesSearch:	return "FilesSearch";
	case CFileListView::eFilesGrabber:	return "FilesGrabber";
	case CFileListView::eSubFiles:		return "SubFiles";
	default:							return "";
	}
}

class CSortFilterProxyModel: public QSortFilterProxyModel
{
public:
	CSortFilterProxyModel(bool bAlternate, QObject* parrent = 0) : QSortFilterProxyModel(parrent) {m_bAlternate = bAlternate;}

	bool filterAcceptsRow(int source_row, const QModelIndex & source_parent) const
	{
		// custom behaviour :
		if(!filterRegExp().isEmpty())
		{
			// get source-model index for current row
			QModelIndex source_index = sourceModel()->index(source_row, this->filterKeyColumn(), source_parent) ;
			if(source_index.isValid())
			{
				// if any of children matches the filter, then current index matches the filter as well
				int nb = sourceModel()->rowCount(source_index) ;
				for(int i = 0; i < nb; i++)
				{
					if(filterAcceptsRow(i, source_index))
						return true;
				}
				// check current index itself :
				QString key = sourceModel()->data(source_index, filterRole()).toString();
				return key.contains(filterRegExp()) ;
			}
		}
		// parent call for initial behaviour
		return QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent) ;
	}

	QVariant data(const QModelIndex &index, int role) const
	{
		QVariant Data = QSortFilterProxyModel::data(index, role);
		if(m_bAlternate && role == Qt::BackgroundRole && !Data.isValid())
		{
			if (0 == index.row() % 2)
				return QColor(226, 237, 253);
			else
				return QColor(Qt::white);
		}
		return Data;
	}

protected:
	bool		m_bAlternate;
};

CFileListView::CFileListView(UINT Mode, QWidget *parent)
:QWidget(parent)
{
	m_pFileSyncJob = 0;
	m_Ops = Mode2Str(Mode);
	m_Mode = Mode;
	m_SubMode = 0;
	m_ID = 0;
	m_PendingExpand = false;
	m_FindingIndex = false;
	m_SyncToken = 0;
	m_NextFullUpdate = 0;

	m_Hold = false;

	m_pMainLayout = new QVBoxLayout(this);
	m_pMainLayout->setMargin(0);

	m_pMenu = NULL;

	m_pFileTree = new QTreeViewEx();
	m_pFileTree->setItemDelegate(new QStyledItemDelegate16(this));
	m_pFileModel = new CFilesModel();
	m_pFileModel->SetMode(Mode);
	connect(m_pFileModel, SIGNAL(CheckChanged(quint64, bool)), this, SLOT(OnCheckChanged(quint64, bool)));
	connect(m_pFileModel, SIGNAL(Updated()), this, SLOT(OnUpdated()));
	m_pSortProxy = new CSortFilterProxyModel(theGUI->Cfg()->GetBool("Gui/Alternate"), this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pFileModel);
	m_pSortProxy->setDynamicSortFilter(theGUI->Cfg()->GetString("Gui/AutoSort") == "true" 
		|| theGUI->Cfg()->GetString("Gui/AutoSort").contains(Mode == eFilesSearch ? "S" : "F"));
	m_pFileTree->setModel(m_pSortProxy);

	m_pFileTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
#ifdef WIN32
	m_pFileTree->setStyle(QStyleFactory::create("windowsxp"));
#endif
	m_pFileTree->setSortingEnabled(true);

	m_pFileTree->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pFileTree, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenuRequested(const QPoint &)));

	//connect(m_pFileTree, SIGNAL(clicked(const QModelIndex&)), this, SLOT(OnClicked(const QModelIndex&)));
	connect(m_pFileTree->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(OnSelectionChanged(QItemSelection,QItemSelection)));
	connect(m_pFileTree, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(OnDoubleClicked(const QModelIndex&)));

	connect(m_pFileTree->verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(OnScroll()));
	connect(m_pFileTree, SIGNAL(expanded(const QModelIndex &)), this, SLOT(OnScroll()));

	m_pMainLayout->addWidget(m_pFileTree);
	setLayout(m_pMainLayout);

	m_pFileTree->header()->restoreState(theGUI->Cfg()->GetBlob("Gui/Widget_" + m_Ops + "_Columns"));
	m_pFileTree->hideColumn(m_pFileModel->columnCount() - 1);

	ChangeMode(Mode);

	m_TimerId = startTimer(250);
}

CFileListView::~CFileListView()
{
	theGUI->Cfg()->SetBlob("Gui/Widget_" + m_Ops + "_Columns",m_pFileTree->header()->saveState());

	if(m_TimerId != 0)
		killTimer(m_TimerId);
}

void CFileListView::ChangeMode(UINT Mode, UINT SubMode)
{
	m_SubMode = SubMode;
	if(m_Mode == Mode && m_pMenu)
		return;

	m_Mode = Mode;
	m_pFileModel->SetMode(m_Mode);
	m_pFileModel->Clear();
	m_SyncToken = 0;
	m_NextFullUpdate = 0;
	m_pFileSyncJob = NULL;

	// load proepr view config for this panel
	theGUI->Cfg()->SetBlob("Gui/Widget_" + m_Ops + "_Columns",m_pFileTree->header()->saveState());
	m_Ops = Mode2Str(Mode);
	m_pFileTree->header()->restoreState(theGUI->Cfg()->GetBlob("Gui/Widget_" + m_Ops + "_Columns"));
	m_pFileTree->hideColumn(m_pFileModel->columnCount() - 1);


	delete m_pMenu;
	m_pMenu = new QMenu(this);

	m_pGrab = m_pIndex = m_pRefresh = m_pAlias = m_pStart = m_pForce = m_pPause = m_pStop = m_pDisable = m_pDelete = m_pRehashFile = m_pAddSource = m_pStream = m_pMakeMulti = m_pMakeTorrent = m_pNeoShare = m_pEd2kShare  =
	m_pGetLinks =
#ifdef CRAWLER
	m_pArchiveFile = 
#endif
	m_pUploadParts = m_pRemoveArchive = m_pCheckLinks = m_pCleanupLinks = m_pResetLinks = m_pInspectLinks = m_pSetPasswords = m_pReUploadOff = m_pReUploadDef = m_pReUploadOn =
	m_pPrioAuto = m_pPrioHighest = m_pPrioHigh = m_pPrioMedium = m_pPrioLow = m_pPrioLowest = NULL;
	m_pPrioMenu = m_pQueueMenu = m_pHosterMenu = m_pKadMenu = NULL;
	m_pPriority = m_pReUpload = NULL;
	m_pUpLimit = m_pDownLimit = NULL;
	m_pQueuePos = NULL;
	m_pShareRatio = NULL;

	if(Mode == eFilesSearch || Mode == eFilesGrabber)
	{
		m_pGrab = MakeAction(m_pMenu, tr("Add to Downloads"), ":/Icons/Downloads");
		connect(m_pGrab, SIGNAL(triggered()), this, SLOT(OnGrab()));

		m_pMenu->addSeparator();
	}

	if(Mode == eFilesSearch)
	{
		m_pRefresh = MakeAction(m_pMenu, tr("Refresh Results"), ":/Icons/Refresh");
		m_pRefresh->setShortcut(QKeySequence::Refresh);
		m_pRefresh->setShortcutContext(Qt::WidgetShortcut);
		m_pFileTree->addAction(m_pRefresh);
		connect(m_pRefresh, SIGNAL(triggered()), this, SLOT(OnRefresh()));

		m_pMenu->addSeparator();
	}

	if(Mode == eDownload || Mode == eSharedFiles || Mode == eFileArchive || Mode == eSubFiles)
	{
		m_pStart = MakeAction(m_pMenu, tr("Start File"), ":/Icons/Start");
		connect(m_pStart, SIGNAL(triggered()), this, SLOT(OnStart()));

		if(Mode != eFileArchive)
		{
			if(theGUI->Cfg()->GetInt("Gui/AdvancedControls"))
			{
				m_pPause = MakeAction(m_pMenu, tr("Pause File"), ":/Icons/Pause");
				connect(m_pPause, SIGNAL(triggered()), this, SLOT(OnPause()));
			}

			m_pStop = MakeAction(m_pMenu, tr("Stop File"), ":/Icons/Stop");
			connect(m_pStop, SIGNAL(triggered()), this, SLOT(OnStop()));
		}
	}
	else if(Mode == eFilesGrabber)
	{
		m_pStart = MakeAction(m_pMenu, tr("Fetch File"), ":/Icons/Start");
		connect(m_pStart, SIGNAL(triggered()), this, SLOT(OnStart()));
	}

	if(Mode == eDownload || Mode == eSharedFiles || Mode == eFileArchive || Mode == eFilesGrabber)
	{
		m_pDelete = MakeAction(m_pMenu, tr("Delete File"), ":/Icons/Delete");
		m_pDelete->setShortcut(QKeySequence::Delete);
		m_pDelete->setShortcutContext(Qt::WidgetShortcut);
		m_pFileTree->addAction(m_pDelete);
		connect(m_pDelete, SIGNAL(triggered()), this, SLOT(OnDelete()));
	}

	if(Mode == eSubFiles)
	{
		m_pDisable = MakeAction(m_pMenu, tr("Disable File"));
		m_pDisable->setCheckable(true);
		connect(m_pDisable, SIGNAL(triggered()), this, SLOT(OnDisable()));
	}


	if(Mode == eSharedFiles || Mode == eDownload)
	{
		m_pStream = MakeAction(m_pMenu, tr("Play"), ":/Icons/Playback");
		connect(m_pStream, SIGNAL(triggered()), this, SLOT(OnStream()));

		m_pMenu->addSeparator();

		m_pQueueMenu = MakeMenu(m_pMenu, tr("Sharing Queue"), ":/Icons/Queue");


		m_pForce = MakeAction(m_pQueueMenu, tr("Force Start")/*, ":/Icons/Force"*/);
		m_pForce->setCheckable(true);
		connect(m_pForce, SIGNAL(triggered()), this, SLOT(OnForce()));

		m_pQueuePos = new QSpinBox(this);
		m_pQueuePos->setRange(0, INT_MAX);
		CMenuAction* pQueuePos = new CMenuAction(m_pQueuePos, tr("Queue Position:")/*, ":/Icons/UpDown"*/);
		connect(m_pQueuePos, SIGNAL(valueChanged(int)), this, SLOT(OnQueuePos(int)));
		m_pQueueMenu->addAction(pQueuePos);

		m_pQueueMenu->addSeparator();

		m_pShareRatio = new QDoubleSpinBox(this);
		m_pShareRatio->setRange(0, 100);
		m_pShareRatio->setSingleStep(0.01);
		m_pShareRatio->setSpecialValueText(tr("Default"));
		CMenuAction* pShareRatio = new CMenuAction(m_pShareRatio, tr("Share Ratio:"));
		connect(m_pShareRatio, SIGNAL(valueChanged(double)), this, SLOT(OnShareRatio(double)));
		m_pQueueMenu->addAction(pShareRatio);



		m_pPrioMenu = MakeMenu(m_pMenu, tr("Bandwidth Priority"), ":/Icons/Prio");

		m_pPriority = new QActionGroup(m_pPrioMenu);
		m_pPrioAuto = MakeAction(m_pPriority, m_pPrioMenu, tr("Default"),0);	// Default
																				// Extreme
		m_pPrioHighest = MakeAction(m_pPriority, m_pPrioMenu, tr("Highest"), 9);// Highest
																				// Higher
		m_pPrioHigh = MakeAction(m_pPriority, m_pPrioMenu, tr("High"), 7);		// High
																				// Above
		m_pPrioMedium = MakeAction(m_pPriority, m_pPrioMenu, tr("Medium"), 5);	// Medium
																				// Below
		m_pPrioLow = MakeAction(m_pPriority, m_pPrioMenu, tr("Low"), 3);		// Low
																				// Lower
		m_pPrioLowest = MakeAction(m_pPriority, m_pPrioMenu, tr("Lowest"), 1);	// Lowest
		connect(m_pPriority, SIGNAL(triggered(QAction*)), this, SLOT(OnPriority(QAction*)));

		m_pPrioMenu->addSeparator();

		m_pUpLimit = new QSpinBoxEx(this, tr("Unlimited"), " kb/s", true);
		CMenuAction* pUpLimit = new CMenuAction(m_pUpLimit, tr("Upload Limit"), ":/Icons/Upload");
		connect(m_pUpLimit, SIGNAL(valueChanged(int)), this, SLOT(OnUpLimit(int)));
		m_pPrioMenu->addAction(pUpLimit);
		m_pDownLimit = new QSpinBoxEx(this, tr("Unlimited"), " kb/s", true);
		CMenuAction* pDownLimit = new CMenuAction(m_pDownLimit, tr("Download Limit"), ":/Icons/Download");
		connect(m_pDownLimit, SIGNAL(valueChanged(int)), this, SLOT(OnDownLimit(int)));
		m_pPrioMenu->addAction(pDownLimit);

		m_pMenu->addSeparator();
	}

	m_pMenu->addSeparator();

	if(Mode == eDownload || Mode == eFilesGrabber)
	{
		m_pPasteLink = MakeAction(m_pMenu, tr("Paste Links"), ":/Icons/Links");
		m_pPasteLink->setShortcut(QKeySequence::Paste);
		m_pPasteLink->setShortcutContext(Qt::WidgetShortcut);
		m_pFileTree->addAction(m_pPasteLink);
		connect(m_pPasteLink, SIGNAL(triggered()), this, SLOT(OnPasteLinks()));
	}
	else
		m_pPasteLink = NULL;

	m_pLinksMenu = MakeMenu(m_pMenu,tr("Get Links"), ":/Icons/MakeLink");
	if(theGUI->Cfg()->GetInt("Gui/AdvancedControls"))
	{
		m_pGetLinks = MakeAction(m_pLinksMenu, tr("Advanced Link Options"));
		connect(m_pGetLinks, SIGNAL(triggered()), this, SLOT(OnGetLinks()));
		m_pLinksMenu->addSeparator();
	}
	m_pCopyNeo = MakeAction(m_pLinksMenu, tr("Copy Neo Link"));
	m_pCopyNeo->setShortcut(QKeySequence::Copy);
	m_pCopyNeo->setShortcutContext(Qt::WidgetShortcut);
	m_pFileTree->addAction(m_pCopyNeo);
	connect(m_pCopyNeo, SIGNAL(triggered()), this, SLOT(OnGetLinks()));
	m_pCopyMagnet = MakeAction(m_pLinksMenu, tr("Copy magnet Link"));
	connect(m_pCopyMagnet, SIGNAL(triggered()), this, SLOT(OnGetLinks()));
	m_pCopyEd2k = MakeAction(m_pLinksMenu, tr("Copy ed2k Link"));
	connect(m_pCopyEd2k, SIGNAL(triggered()), this, SLOT(OnGetLinks()));

	m_pLinksMenu->addSeparator();

	m_pSaveTorrent = MakeAction(m_pLinksMenu, tr("Save torrent File"));
	connect(m_pSaveTorrent, SIGNAL(triggered()), this, SLOT(OnGetLinks()));

	m_pMenu->addSeparator();

	if(Mode == eSharedFiles)
	{
		m_pMakeMulti = MakeAction(m_pMenu, tr("Make Multi File"), ":/Icons/MakeMulti");
		connect(m_pMakeMulti, SIGNAL(triggered()), this, SLOT(OnMakeMulti()));

		m_pMakeTorrent = MakeAction(m_pMenu, tr("Create Torrent"), ":/Icons/BitTorrent");
		connect(m_pMakeTorrent, SIGNAL(triggered()), this, SLOT(OnMakeTorrent()));

		m_pMenu->addSeparator();
	}

	if(Mode != eFileArchive)
	{
		m_pAddSource = MakeAction(m_pMenu, tr("Add Source"), ":/Icons/AddSource");
		connect(m_pAddSource, SIGNAL(triggered()), this, SLOT(OnAddSource()));

		m_pKadMenu = MakeMenu(m_pMenu, tr("Lookup / Pubishment"), ":/Icons/NeoShare");

		m_pIndex = MakeAction(m_pKadMenu, tr("Get Meta Data"));
		connect(m_pIndex, SIGNAL(triggered()), this, SLOT(OnIndex()));
		m_pAlias = MakeAction(m_pKadMenu, tr("Find Other Hashes"));
		connect(m_pAlias, SIGNAL(triggered()), this, SLOT(OnAlias()));
		m_pKadMenu->addSeparator();
		m_pAnnounce = MakeAction(m_pKadMenu, tr("Get Sources (Announce)"));
		connect(m_pAnnounce, SIGNAL(triggered()), this, SLOT(OnAnnounce()));
		m_pPublish = MakeAction(m_pKadMenu, tr("Publish File Info"));
		connect(m_pPublish, SIGNAL(triggered()), this, SLOT(OnPublish()));

		m_pMenu->addSeparator();
	}

	//
	m_pShare = MakeMenu(m_pMenu, tr("Downloading / Sharing"), ":/Icons/P2PClients");

	m_pShare->addSeparator();

	m_pAutoShare = MakeAction(m_pShare, tr("Auto Download (default)"));
	m_pAutoShare->setCheckable(true);
	connect(m_pAutoShare, SIGNAL(triggered()), this, SLOT(OnAutoShare()));

	m_pShare->addSeparator();

	if(theGUI->Cfg()->GetInt("Gui/AdvancedControls") == 1)
	{
		m_pNeoShare = MakeAction(m_pShare, tr("Neo Share"));
		m_pNeoShare->setCheckable(true);
		connect(m_pNeoShare, SIGNAL(triggered()), this, SLOT(OnNeoShare()));
		
		m_pShare->addSeparator();
	}

	m_pShare->addSeparator();

	m_pTorrent = new QActionGroup(m_pShare);
	m_pTorrentOff = MakeAction(m_pTorrent, m_pShare, "No Torrents", 0);
	m_pTorrentOne = MakeAction(m_pTorrent, m_pShare, "One Torrents", 2);
	m_pTorrentOn = MakeAction(m_pTorrent, m_pShare, "All Torrents", 1);
	connect(m_pTorrent, SIGNAL(triggered(QAction*)), this, SLOT(OnTorrent(QAction*)));

	m_pShare->addSeparator();

	m_pEd2kShare = MakeAction(m_pShare, tr("Ed2k Share"));
	m_pEd2kShare->setCheckable(true);
	connect(m_pEd2kShare, SIGNAL(triggered()), this, SLOT(OnEd2kShare()));

	m_pShare->addSeparator();

	m_pHosterDl = new QActionGroup(m_pShare);
	m_pHosterDlOff = MakeAction(m_pHosterDl, m_pShare, "No Hosters", 0);
	m_pHosterDlOn = MakeAction(m_pHosterDl, m_pShare, "Hoster Hosters", 1);
	m_pHosterDlEx = MakeAction(m_pHosterDl, m_pShare, "Hoster Downloads with Captcha", 2);
	m_pHosterDlArch = MakeAction(m_pHosterDl, m_pShare, "Archive Download with Captcha", 3);
	connect(m_pHosterDl, SIGNAL(triggered(QAction*)), this, SLOT(OnHosterDl(QAction*)));

	m_pShare->addSeparator();

	m_pHosterUp = MakeAction(m_pShare, tr("Auto Hoster Upload"));
	m_pHosterUp->setCheckable(true);
	connect(m_pHosterUp, SIGNAL(triggered()), this, SLOT(OnHosterUl()));
	m_pShare->addAction(m_pHosterUp);

	//

	if(Mode == eSharedFiles || Mode == eDownload)
	{
		m_pHosterMenu = MakeMenu(m_pMenu, tr("Hoster Tools"), ":/Icons/Hosters");

		m_pReUpload = new QActionGroup(m_pHosterMenu);
		m_pReUploadOn = MakeAction(m_pReUpload, m_pHosterMenu, "ReUpload Links", 1);
		m_pReUploadDef = MakeAction(m_pReUpload, m_pHosterMenu, "Default Links", 0);
		m_pReUploadOff = MakeAction(m_pReUpload, m_pHosterMenu, "No Links", 2);
		connect(m_pReUpload, SIGNAL(triggered(QAction*)), this, SLOT(OnReUpload(QAction*)));

		m_pHosterMenu->addSeparator();

		if(Mode == eSharedFiles)
		{
			m_pUploadParts = MakeAction(m_pHosterMenu, tr("Upload to Hoster"), ":/Icons/UploadToHoster");
			connect(m_pUploadParts, SIGNAL(triggered()), this, SLOT(OnUploadParts()));
		}

		m_pRemoveArchive = MakeAction(m_pHosterMenu, tr("Remove Archives"), ":/Icons/RemoveArch");
		connect(m_pRemoveArchive, SIGNAL(triggered()), this, SLOT(OnRemoveArchive()));

		m_pCheckLinks = MakeAction(m_pHosterMenu, tr("Check Links"), ":/Icons/CheckLinks");
		connect(m_pCheckLinks, SIGNAL(triggered()), this, SLOT(OnCheckLinks()));

		m_pCleanupLinks = MakeAction(m_pHosterMenu, tr("Cleanup Links"), ":/Icons/CleanUpLinks");
		connect(m_pCleanupLinks, SIGNAL(triggered()), this, SLOT(OnCleanupLinks()));

		m_pResetLinks = MakeAction(m_pHosterMenu, tr("Reset Links"), ":/Icons/ResetLinks");
		connect(m_pResetLinks, SIGNAL(triggered()), this, SLOT(OnResetLinks()));

		m_pInspectLinks = MakeAction(m_pHosterMenu, tr("Inspect Links"), ":/Icons/Inspect");
		connect(m_pInspectLinks, SIGNAL(triggered()), this, SLOT(OnInspectLinks()));

		m_pSetPasswords = MakeAction(m_pHosterMenu, tr("Set Password(s)"), ":/Icons/ArchPW");
		connect(m_pSetPasswords, SIGNAL(triggered()), this, SLOT(OnSetPasswords()));		
	}

	if(theGUI->Cfg()->GetInt("Gui/AdvancedControls"))
	{
		m_pMenu->addSeparator();

		m_pToolsMenu = MakeMenu(m_pMenu, tr("Advanced Tools"), ":/Icons/Tools");

		if(Mode == eSharedFiles || Mode == eDownload)
		{
			m_pRehashFile = MakeAction(m_pToolsMenu, tr("ReHash File"), ":/Icons/Hash");
			connect(m_pRehashFile, SIGNAL(triggered()), this, SLOT(OnRehashFile()));

#ifdef CRAWLER
			m_pArchiveFile = MakeAction(m_pMenu, tr("Archive File"));
			connect(m_pArchiveFile, SIGNAL(triggered()), this, SLOT(OnArchiveFile()));
#endif
		}

		if(m_pToolsMenu->isEmpty())
			m_pToolsMenu->setEnabled(false);
	}

	foreach(QAction* pAction, m_AuxMenus)
		m_pMenu->addAction(pAction);
}

void CFileListView::SetID(uint64 ID)
{
	m_FindingIndex = false;
	if(m_ID != ID && m_Mode == eSubFiles)
		m_PendingExpand = true;
	m_ID = ID;

	m_pFileModel->Clear();
	m_SyncToken = 0;
	m_NextFullUpdate = 0;
}

void CFileListView::SetFilter(const QRegExp& Exp)
{
	m_pSortProxy->setFilterRegExp(Exp);
	m_pSortProxy->setFilterKeyColumn(CFilesModel::eFileName);
}

void CFileListView::OnUpdated()
{
	if(m_PendingExpand)
	{
		m_PendingExpand = false;
		m_pFileTree->expandAll();
	}
}

class CFileSyncJob: public CInterfaceJob
{
public:
	CFileSyncJob(CFileListView* pView)
	{
		m_pView = pView;
		m_Mode = pView->m_Mode;
		m_SubMode = pView->m_SubMode;
		m_ID = pView->m_ID;
		m_Full = true;

		switch(m_Mode)
		{
		case CFileListView::eDownload:
		case CFileListView::eSharedFiles:
			{
				if(m_SubMode == CFileListView::eCompleted)
					m_Request["FileStatus"] = "Completed";
				else
				{
					if(m_Mode == CFileListView::eDownload)
						m_Request["FileStatus"] = "Incomplete"; // includes "Completed"
					else
						m_Request["FileStatus"] = "Complete";

					switch(m_SubMode)
					{
						case CFileListView::eStarted:	m_Request["FileState"] = "Started";	break;
						case CFileListView::ePaused:	m_Request["FileState"] = "Paused";	break;
						case CFileListView::eStopped:	m_Request["FileState"] = "Stopped";	break;
					}
				}
				break;
			}
		case CFileListView::eFileArchive:	m_Request["FileState"] = "Removed";	break;
		case CFileListView::eFilesSearch:	m_Request["SearchID"] = m_ID;		break;
		case CFileListView::eFilesGrabber:	m_Request["GrabberID"] = m_ID;		break;
		case CFileListView::eSubFiles:		m_Request["RootID"] = m_ID;			break;
		}

		QVariantList ExtendedList;
		foreach(uint64 FileID, pView->m_VisibleFiles)
			ExtendedList.append(FileID);

		m_Request["ExtendedList"] = ExtendedList; // Note: if this is not set all files are extended by default
		m_Request["Token"] = pView->m_SyncToken;
	}

	CFileSyncJob(CFileListView* pView, const QSet<uint64>& VisibleFiles)
	{
		m_pView = pView;
		m_Mode = pView->m_Mode;
		m_SubMode = pView->m_SubMode;
		m_ID = pView->m_ID;
		m_Full = false;

		QVariantList SelectedList;
		foreach(uint64 FileID, VisibleFiles)
			SelectedList.append(FileID);

		m_Request["SelectedList"] = SelectedList;
		m_Request["Token"] = pView->m_SyncToken;
	}

	virtual QString			GetCommand()	{return "FileList";}
	virtual void			HandleResponse(const QVariantMap& Response)
	{
		if(m_pView && m_pView->m_pFileSyncJob == this)
		{
			if(m_Full) // schedule next full update
			{
				uint64 FullUpdateDelay = Min(Max(GetDuration() * 10, 1000), 5000);
				m_pView->m_NextFullUpdate = GetCurTick() + FullUpdateDelay;
			}

			if(m_Mode == m_pView->m_Mode && m_SubMode == m_pView->m_SubMode && m_ID == m_pView->m_ID)
				m_pView->SyncFiles(Response, m_Full);
		}
	}
	virtual void			Finish(bool bOK)
	{
		if(m_pView && m_pView->m_pFileSyncJob == this)
			m_pView->m_pFileSyncJob = NULL;
		CInterfaceJob::Finish(bOK);
	}

protected:
	UINT					m_Mode;
	UINT					m_SubMode;
	uint64 					m_ID;
	bool					m_Full;
	QPointer<CFileListView>	m_pView; // Note: this can be deleted at any time
};

void CFileListView::timerEvent(QTimerEvent *e)
{
    if (e && e->timerId() != m_TimerId) 
	{
        QObject::timerEvent(e);
		return;
    }

	if(m_Mode == eSubFiles && m_ID == 0)
		return;

	if(m_pFileSyncJob == NULL)
	{
		m_pFileSyncJob = m_NextFullUpdate < GetCurTick() ? new CFileSyncJob(this) : new CFileSyncJob(this, m_VisibleFiles);
		theGUI->ScheduleJob(m_pFileSyncJob);
	}
}

class CIndexgetJob: public CInterfaceJob
{
public:
	CIndexgetJob(uint64 ID)
	{
		m_Request["ID"] = ID;
		m_Request["Action"] = "FindIndex";
		m_Request["Log"] = true;
	}

	virtual QString			GetCommand()	{return "FileAction";}
	virtual void			HandleResponse(const QVariantMap& Response) {}
};

void CFileListView::SyncFiles(const QVariantMap& Response, bool bFull)
{
	QVariantList Files = Response["Files"].toList();

	bool bReSync = false;

	if(Response.contains("Token"))
	{
		if(m_SyncToken != Response["Token"].toULongLong())
		{
			bReSync = true;
			m_pFileModel->Clear();
			m_SyncToken = Response["Token"].toULongLong();
		}
		m_pFileModel->IncrSync(Files);
	}
	else
	{
		bReSync = true;
		m_pFileModel->Sync(Files);
	}

	if(bFull)
		m_pFileModel->CountFiles();

	if(m_Mode == eSubFiles && Files.isEmpty() && m_FindingIndex == false && bReSync)
	{
		CIndexgetJob* pIndexgetJob = new CIndexgetJob(m_ID);
		theGUI->ScheduleJob(pIndexgetJob);
		m_FindingIndex = true;
	}

	if(bFull)
		OnScroll();
}


void CFileListView::OnScroll()
{
	QSet<uint64> VisibleFiles;
	QSet<uint64> NewVisibleFiles;
	for(QModelIndex Index = m_pFileTree->indexAt(QPoint(0,0)); ; Index = m_pFileTree->indexBelow(Index))
	{
		if(!m_pFileTree->viewport()->rect().intersects(m_pFileTree->visualRect(Index)))
			break;
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		uint64 ID = m_pFileModel->data(ModelIndex, Qt::UserRole, CFilesModel::eFileName).toULongLong();
		if(ID == 0)
			continue; // just a directory not a real file

		VisibleFiles.insert(ID);
		if(!m_VisibleFiles.contains(ID))
			NewVisibleFiles.insert(ID);
	}
	m_VisibleFiles = VisibleFiles;

	if(m_pFileSyncJob == NULL)
	{
		m_pFileSyncJob = new CFileSyncJob(this, NewVisibleFiles);
		theGUI->ScheduleJob(m_pFileSyncJob);
	}

	uint64 uCurTick = GetCurTick();
	if(!m_pFileTree->isColumnHidden(CFilesModel::eProgress))
	{
		QMap<uint64, QPair<QPointer<QWidget>, QPersistentModelIndex> > OldProgressMap;
		m_pFileTree->StartUpdatingWidgets(OldProgressMap, m_ProgressMap);
		for(QModelIndex Index = m_pFileTree->indexAt(QPoint(0,0)); ; Index = m_pFileTree->indexBelow(Index))
		{
			Index = Index.sibling(Index.row(), CFilesModel::eProgress);
			if(!m_pFileTree->viewport()->rect().intersects(m_pFileTree->visualRect(Index)))
				break;
			QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
			uint64 ID = m_pFileModel->data(ModelIndex, Qt::UserRole, CFilesModel::eFileName).toULongLong();
			if(ID == 0 || m_pFileModel->Data(ModelIndex).value("FileType") == "Collection")
				continue; // just a directory not a real file

			QWidget* pWidget = OldProgressMap.take(ID).first;
			if(!theGUI->Cfg()->GetBool("Gui/SimpleBars"))
			{
				CProgressBar* pProgress = qobject_cast<CProgressBar*>(pWidget);
				if(!pProgress)
				{
					pProgress = new CProgressBar(ID);
					m_ProgressMap.insert(ID, qMakePair((QPointer<QWidget>)pProgress, QPersistentModelIndex(Index)));
					m_pFileTree->setIndexWidget(Index, pProgress);
				}
				pProgress->SetProgress(m_pFileModel->data(ModelIndex, Qt::EditRole, CFilesModel::eProgress).toInt());
				if(uCurTick > pProgress->GetNextUpdate())
					pProgress->Update();
			}
			else
			{
				QProgressBar* pProgress = qobject_cast<QProgressBar*>(pWidget);
				if(!pProgress)
				{
					pProgress = new QProgressBar();
					pProgress->setMaximumHeight(16);
					m_ProgressMap.insert(ID, qMakePair((QPointer<QWidget>)pProgress, QPersistentModelIndex(Index)));
					m_pFileTree->setIndexWidget(Index, pProgress);
					pProgress->setMaximum(100);
				}
				pProgress->setValue(m_pFileModel->data(ModelIndex, Qt::EditRole, CFilesModel::eProgress).toInt());

				QString Color;
				if(m_pFileModel->Data(ModelIndex).value("FileStatus") == "Error")
					Color = "Red";
				else if(m_pFileModel->Data(ModelIndex).value("FileJobs").toStringList().contains("Allocating"))
					Color = "Yellow";
				else if(m_pFileModel->Data(ModelIndex).value("FileJobs").toStringList().contains("Hashing"))
					Color = "Blue";
				else
					Color = "Green";

				QString StyleSheet = QString("QProgressBar:horizontal {"
										"border: 1px solid gray;"
										//"border-radius: 3px;"
										"background: white;"
										"padding: 1px;"
										"text-align: right;"
										"margin-right: 10ex;"
										"}"
										"QProgressBar::chunk:horizontal {"
										//"margin-right: 0px; /* space */"
										//"width: 1px;"
										"background-color: %1"
										"}").arg(Color);

				pProgress->setStyleSheet(StyleSheet);

			}
		}
		m_pFileTree->EndUpdatingWidgets(OldProgressMap, m_ProgressMap);
	}

	if(!m_pFileTree->isColumnHidden(CFilesModel::eAvailability))
	{
		QMap<uint64, QPair<QPointer<CFitnessBars>, QPersistentModelIndex> > OldFitnessMap;
		m_pFileTree->StartUpdatingWidgets(OldFitnessMap, m_FitnessMap);
		for(QModelIndex Index = m_pFileTree->indexAt(QPoint(0,0)); ; Index = m_pFileTree->indexBelow(Index))
		{
			Index = Index.sibling(Index.row(), CFilesModel::eAvailability);
			if(!m_pFileTree->viewport()->rect().intersects(m_pFileTree->visualRect(Index)))
				break;
			QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
			uint64 ID = m_pFileModel->data(ModelIndex, Qt::UserRole, CFilesModel::eFileName).toULongLong();
			if(ID == 0 || m_pFileModel->Data(ModelIndex).value("FileType") == "Collection")
				continue; // just a directory not a real file

			int Value = m_pFileModel->data(ModelIndex, Qt::UserRole, CFilesModel::eAvailability).toInt();
			if(Value)
			{
				CFitnessBars* pFitness = OldFitnessMap.take(ID).first;
				if(!pFitness)
				{
					pFitness = new CFitnessBars();
					m_FitnessMap.insert(ID, qMakePair((QPointer<CFitnessBars>)pFitness, QPersistentModelIndex(Index)));
					m_pFileTree->setIndexWidget(Index, pFitness);
				}
				pFitness->SetValue(Value, QString::number(m_pFileModel->data(ModelIndex, Qt::EditRole, CFilesModel::eAvailability).toDouble(), 'f', 3));
			}
		}
		m_pFileTree->EndUpdatingWidgets(OldFitnessMap, m_FitnessMap);
	}

	if(!m_pFileTree->isColumnHidden(CFilesModel::eSources))
	{
		QMap<uint64, QPair<QPointer<CSourceCell>, QPersistentModelIndex> > OldSourceMap;
		m_pFileTree->StartUpdatingWidgets(OldSourceMap, m_SourceMap);
		for(QModelIndex Index = m_pFileTree->indexAt(QPoint(0,0)); ; Index = m_pFileTree->indexBelow(Index))
		{
			Index = Index.sibling(Index.row(), CFilesModel::eSources);
			if(!m_pFileTree->viewport()->rect().intersects(m_pFileTree->visualRect(Index)))
				break;
			QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
			uint64 ID = m_pFileModel->data(ModelIndex, Qt::UserRole, CFilesModel::eFileName).toULongLong();
			if(ID == 0 || m_pFileModel->Data(ModelIndex).value("FileType") == "Collection")
				continue; // just a directory not a real file

			CSourceCell* pSources = OldSourceMap.take(ID).first;
			if(!pSources)
			{
				pSources = new CSourceCell();
				m_SourceMap.insert(ID, qMakePair((QPointer<CSourceCell>)pSources, QPersistentModelIndex(Index)));
				m_pFileTree->setIndexWidget(Index, pSources);
			}
			pSources->SetValue(m_pFileModel->Data(ModelIndex));
		}
		m_pFileTree->EndUpdatingWidgets(OldSourceMap, m_SourceMap);
	}
}


void CFileListView::OnClicked(const QModelIndex& Index)
{
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	uint64 ID = m_pFileModel->data(ModelIndex, Qt::UserRole, CFilesModel::eFileName).toULongLong();
	emit FileItemClicked(ID, m_pFileTree->selectedRows().count() <= 1);
}

void CFileListView::OnSelectionChanged(const QItemSelection& Selected, const QItemSelection& Deselected)
{
	QModelIndex Index = m_pFileTree->currentIndex();
	if(Index.isValid())
		OnClicked(Index);
}

void CFileListView::OnDoubleClicked(const QModelIndex& Index)
{
	if(m_Mode == eFilesSearch || m_Mode == eFilesGrabber)
		OnGrab();
	else if(m_Mode == eSharedFiles || m_Mode == eDownload) 
		OnOpen();
}

QVariantMap CFileListView::GetFile(uint64 ID)
{
	QModelIndex Index = m_pFileModel->FindIndex(ID);
	return m_pFileModel->Data(Index);
}

QList<uint64> CFileListView::GetSelectedFilesIDs()
{
	QList<uint64> FileIDs;
	foreach(const QModelIndex& Index, m_pFileTree->selectedRows())
	{
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		FileIDs.append(m_pFileModel->data(ModelIndex, Qt::UserRole, CFilesModel::eFileName).toULongLong());
	}
	return FileIDs;
}

QModelIndexList GetAllSubTreeItems(CFilesModel* pModel, const QModelIndex& Parent)
{
	QModelIndexList IndexList;
	QList<QPair<QModelIndex, int> > ItemStack;
	ItemStack.append(QPair<QModelIndex, int> (Parent, 0));
	while(!ItemStack.isEmpty())
	{
		QModelIndex Index = ItemStack.last().first;
		if(ItemStack.last().second == 0)
		{
			uint64 ID = pModel->data(Index, Qt::UserRole, CFilesModel::eFileName).toULongLong();
			if(ID)
				IndexList.append(Index);
		}
		if(ItemStack.last().second < pModel->rowCount(Index))
			ItemStack.append(QPair<QModelIndex, int> (pModel->index(ItemStack.last().second++, 0, Index), 0));
		else
			ItemStack.removeLast();
	}
	return IndexList;
}

QModelIndexList GetCurTreeSelection(QTreeViewEx* pTree, QSortFilterProxyModel* pProxy, CFilesModel* pModel, bool bWithSub = false, QString* pFileName = NULL)
{
	QModelIndex Index = pTree->currentIndex();
	QModelIndexList IndexList;
	foreach(const QModelIndex& Index, pTree->selectedRows())
	{
		QModelIndex ModelIndex = pProxy->mapToSource(Index);
		bool bFile = pModel->data(ModelIndex, Qt::UserRole, CFilesModel::eFileName).toULongLong() != 0;

		if(bFile)
			IndexList.append(ModelIndex);
		else if(pFileName && pFileName->isEmpty())
			*pFileName = pModel->data(ModelIndex, Qt::DisplayRole, CFilesModel::eFileName).toString();

		if(!bFile || bWithSub)
			IndexList.append(GetAllSubTreeItems(pModel, ModelIndex));
	}
	return IndexList;
}

void CFileListView::OnMenuRequested(const QPoint &point)
{
	m_Hold = true;

	QModelIndexList SelectedItems = GetCurTreeSelection(m_pFileTree, m_pSortProxy, m_pFileModel);

	bool bCanStart = false;
	bool bCanPause = false;
	bool bCanStop = false;
	bool bNoMulti = true;
	int Count = 0;
	int CompleteCount = 0;
	QVariantMap File;
	int Ed2k = 0;
	int Neo = 0;
	int NeoX= 0;
	int Torrent = 0;
	int Priority = -1;
	int MaxUpload = -1;
	int MaxDownload = -1;
	int QueuePos = 0;
	foreach(const QModelIndex& ModelIndex, SelectedItems)
	{
		//uint64 ID = m_pFileModel->data(ModelIndex, Qt::UserRole, CFilesModel::eFileName).toULongLong();
		File = m_pFileModel->Data(ModelIndex);
		if(File.isEmpty())
			continue;

		Count ++;
		if(File["FileType"] == "MultiFile")
			bNoMulti = false;

		QString FileState = File["FileState"].toString();
		if(FileState != "Started")
			bCanStart = true;
		if(FileState != "Paused")
			bCanPause = true;
		if(FileState != "Stopped")
			bCanStop = true;
		QString FileStatus = File["FileStatus"].toString();
		if(FileStatus == "Complete")
			CompleteCount++;

		QStringList Hashes = File["Hashes"].toStringList();
		if(Hashes.contains("ed2k"))
			Ed2k++;
		if(Hashes.contains("neo"))
			Neo++;
		if(Hashes.contains("neox"))
			NeoX++;
		if(Hashes.contains("btih"))
			Torrent++;

		if(QueuePos == 0 || QueuePos < File["Queue"].toInt())
			QueuePos = File["QueuePos"].toInt();

		if(Priority == -1)
			Priority = File["Priority"].toInt();
		else if(Priority != -2 && Priority != File["Priority"].toInt())
			Priority = -2;

		if(MaxUpload == -1)
			MaxUpload = File["MaxUpload"].toInt();
		else if(MaxUpload != -2 && MaxUpload != File["MaxUpload"].toInt())
			MaxUpload = -2;
		if(MaxDownload == -1)
			MaxDownload = File["MaxDownload"].toInt();
		else if(MaxDownload != -2 && MaxDownload != File["MaxDownload"].toInt())
			MaxDownload = -2;
	}

	bool bHalted = File["FileState"].toString() == "Halted";

	if(m_pMakeMulti)	m_pMakeMulti->setEnabled(Count > 0 && bNoMulti);
	if(m_pMakeTorrent)	m_pMakeTorrent->setEnabled(Count == 1 && CompleteCount != 0);

	if(m_pStart)		m_pStart->setEnabled(!bHalted && bCanStart);
	if(m_pPause)		m_pPause->setEnabled(!bHalted && bCanPause);
	if(m_pStop)			m_pStop->setEnabled(!bHalted && bCanStop);

	if(m_pGrab)			m_pGrab->setEnabled(Count > 0);

	if(m_pDisable)		m_pDisable->setEnabled(Count > 0);
	if(m_pDisable)		m_pDisable->setChecked(bHalted);

	if(m_pDelete)		m_pDelete->setEnabled(Count > 0);
	if(m_pRehashFile)	m_pRehashFile->setEnabled(Count > 0);
#ifdef CRAWLER
	if(m_pArchiveFile)	m_pArchiveFile->setEnabled(CompleteCount > 0);
#endif

	if(m_pPasteLink)	m_pPasteLink->setEnabled(!QApplication::clipboard()->text().isEmpty());

	if(m_pLinksMenu)	m_pLinksMenu->setEnabled(Count > 0);
	if(m_pGetLinks)		m_pGetLinks->setEnabled(Count > 0);
	if(m_pCopyNeo)		m_pCopyNeo->setEnabled(Count > 0);
	if(m_pCopyMagnet)	m_pCopyMagnet->setEnabled(Torrent > 0);
	if(m_pCopyEd2k)		m_pCopyEd2k->setEnabled(Ed2k > 0);
	if(m_pSaveTorrent)	m_pSaveTorrent->setEnabled(Torrent > 0);

	if(m_pAddSource)	m_pAddSource->setEnabled(Count > 0);

	if(m_pStream)		m_pStream->setEnabled(Count == 1 && File["Streamable"].toBool());

	//
	if(m_pQueueMenu)	m_pQueueMenu->setEnabled(Count > 0);
	if(m_pForce)		m_pForce->setChecked(File["Force"].toBool());
	if(m_pQueuePos)		m_pQueuePos->setValue(QueuePos);
	if(m_pShareRatio)	m_pShareRatio->setValue(File["ShareRatio"].toDouble()/100.0);
	//

	//
	if(m_pPrioMenu)
	{
		switch(Priority)
		{
		case 0:		m_pPrioAuto->setChecked(true); break;		// Default
		case 10:												// Extreme
		case 9:		m_pPrioHighest->setChecked(true); break;	// Highest
		case 8:													// Higher
		case 7:		m_pPrioHigh->setChecked(true); break;		// High
		case 6:													// Above
		case 5:		m_pPrioMedium->setChecked(true); break;		// Medium
		case 4:													// Below
		case 3:		m_pPrioLow->setChecked(true); break;		// Low
		case 2:													// Lower
		case 1:		m_pPrioLowest->setChecked(true); break;		// Lowest
		default:
					m_pPrioAuto->setChecked(false);
					m_pPrioHighest->setChecked(false);
					m_pPrioHigh->setChecked(false);
					m_pPrioMedium->setChecked(false);
					m_pPrioLow->setChecked(false);
					m_pPrioLowest->setChecked(false);
		}
		m_pUpLimit->setValue(MaxUpload >= 0 ? MaxUpload / 1024 : 0);
		m_pDownLimit->setValue(MaxDownload >= 0 ? MaxDownload / 1024 : 0);
		m_pPrioMenu->setEnabled(Count > 0);
	}
	//

	//
	if(m_pQueueMenu) m_pQueueMenu->setEnabled(Count > 0);
	//

	//
	bool bAutoShare = File["AutoShare"].toBool();
	m_pAutoShare->setChecked(bAutoShare);
	m_pAutoShare->setEnabled(Count > 0);

	if(m_pNeoShare) m_pNeoShare->setChecked(File["NeoShare"].toBool());
	if(m_pNeoShare) m_pNeoShare->setEnabled(!bAutoShare && Count > 0);

	switch(File["Torrent"].toInt())
	{
	case 0: m_pTorrentOff->setChecked(true); break;
	case 1: m_pTorrentOn->setChecked(true); break;
	case 2: m_pTorrentOne->setChecked(true); break;
	}
	m_pTorrent->setEnabled(!bAutoShare && Count > 0);

	m_pEd2kShare->setChecked(File["Ed2kShare"].toBool());
	m_pEd2kShare->setEnabled(!bAutoShare && Count > 0);
	
	switch(File["HosterDl"].toInt())
	{
	case 0: m_pHosterDlOff->setChecked(true); break;
	case 1: m_pHosterDlOn->setChecked(true); break;
	case 2: m_pHosterDlEx->setChecked(true); break;
	case 3: m_pHosterDlArch->setChecked(true); break;
	}
	m_pHosterDl->setEnabled(!bAutoShare && Count > 0);

	m_pHosterUp->setChecked(File["HosterUl"].toBool());
	m_pHosterUp->setEnabled(!bAutoShare && Count > 0);

	if(m_pReUpload)
	{
		switch(File["ReUpload"].toInt())
		{
		case 1: m_pReUploadOn->setChecked(true); break;
		case 0: m_pReUploadDef->setChecked(true); break;
		case 2: m_pReUploadOff->setChecked(true); break;
		}
		m_pReUpload->setEnabled(Count > 0);
	}

	m_pShare->setEnabled(Count > 0);
	//

	if(m_pHosterMenu)	m_pHosterMenu->setEnabled(Count > 0);

	if(m_pIndex)		m_pIndex->setEnabled(NeoX > 0);
	if(m_pAlias)		m_pAlias->setEnabled(Count == 1 && bNoMulti);
	if(m_pAnnounce)		m_pAnnounce->setEnabled(Count > 0);
	if(m_pPublish)		m_pPublish->setEnabled(Count > 0);

	m_Hold = false;

	m_pMenu->popup(QCursor::pos());
}

class CSetPropertyJob: public CInterfaceJob
{
public:
	CSetPropertyJob(uint64 ID, const QString& Key, const QVariant& Value)
	{
		m_Request["ID"] = ID;

		QVariantMap Properties;
		Properties[Key] = Value;
		m_Request["Properties"] = Properties;
	}

	virtual QString			GetCommand()	{return "SetFile";}
	virtual void			HandleResponse(const QVariantMap& Response) {}
};

void CFileListView::OnStart()
{
	OnAction("Start", theGUI->Cfg()->GetBool("Gui/SubControl"));
}

void CFileListView::OnPause()
{ 
	OnAction("Pause", theGUI->Cfg()->GetBool("Gui/SubControl"));
}

void CFileListView::OnStop()
{ 
	OnAction("Stop", theGUI->Cfg()->GetBool("Gui/SubControl"));
}

void CFileListView::OnDisable()
{
	foreach(uint64 ID, GetIDs())
		OnCheckChanged(ID, !m_pDisable->isChecked());
}

void CFileListView::OnCheckChanged(quint64 ID, bool State)
{
	DoAction(State ? "Enable" : "Disable", ID);
}

class CDeleteDialog : public QDialogEx
{
	//Q_OBJECT

public:
	CDeleteDialog(const QStringList& Files, bool bWithMulti, bool bCanKeep, QWidget *pMainWindow = NULL)
		: QDialogEx(pMainWindow)
	{
		setWindowTitle(CFileListView::tr("Delete File(s)"));

		int i=0;
		m_pMainLayout = new QFormLayout(this);
		m_pMainLayout->setWidget(i++, QFormLayout::SpanningRole, new QLabel(CFileListView::tr("Delete File(s)/Directory(s)")));

		m_pFiles = new QPlainTextEdit();
		m_pFiles->setReadOnly(true);
		m_pFiles->setPlainText(Files.join("\r\n"));
		m_pMainLayout->setWidget(i++, QFormLayout::SpanningRole, m_pFiles);
		
		m_pWithSubFiles = new QCheckBox(CFileListView::tr("Delete also Complete Sub Files"));
		if(bWithMulti)
			m_pWithSubFiles->setChecked(theGUI->Cfg()->GetBool("Gui/DeleteWithSubFiles"));
		else
			m_pWithSubFiles->setEnabled(false);
		m_pMainLayout->setWidget(i++, QFormLayout::FieldRole, m_pWithSubFiles);

		if(theGUI->Cfg()->GetInt("Gui/AdvancedControls"))
		{
			m_pKeep = new QCheckBox(CFileListView::tr("Keep Files Archived"));
			if(bCanKeep)
				m_pKeep->setChecked(theGUI->Cfg()->GetBool("Gui/KeepFilesArchived"));
			else
				m_pKeep->setEnabled(false);
			m_pMainLayout->setWidget(i++, QFormLayout::FieldRole, m_pKeep);
		}
		else
			m_pKeep = NULL;

		m_pButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
		QObject::connect(m_pButtonBox, SIGNAL(accepted()), this, SLOT(accept()));
		QObject::connect(m_pButtonBox, SIGNAL(rejected()), this, SLOT(reject()));
		m_pButtonBox->button(QDialogButtonBox::Cancel)->setFocus();
		m_pMainLayout->setWidget(i++, QFormLayout::FieldRole, m_pButtonBox);
	}
	~CDeleteDialog()
	{
		if(m_pWithSubFiles->isEnabled())
			theGUI->Cfg()->SetSetting("Gui/DeleteWithSubFiles", m_pWithSubFiles->isChecked());
		if(m_pKeep && m_pKeep->isEnabled())
			theGUI->Cfg()->SetSetting("Gui/KeepFilesArchived", m_pKeep->isChecked());
	}

	bool				IsWithSubFiles()	{return m_pWithSubFiles->isChecked();}
	bool				IsKeep()			{return m_pKeep ? m_pKeep->isChecked() : true;}

protected:
	QFormLayout*		m_pMainLayout;
	QPlainTextEdit*		m_pFiles;
	QCheckBox*			m_pWithSubFiles;
	QCheckBox*			m_pKeep;
	QDialogButtonBox*	m_pButtonBox;
};

void CFileListView::OnDelete()
{
	QStringList Files;
	bool bWithMulti = false;
	foreach(const QModelIndex& Index, m_pFileTree->selectedRows())
	{
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);

		QVariantMap File = m_pFileModel->Data(ModelIndex);
		if(File["FileType"] == "MultiFile")
			bWithMulti = true;

		Files.append(File["FileName"].toString());
	}

	bool bCanKeep = m_Mode != eFileArchive && m_Mode != eFilesGrabber && m_Mode != eFilesSearch;
	CDeleteDialog DeleteDialog(Files, bWithMulti, bCanKeep);
	if(!DeleteDialog.exec())
		return;

	QVariantMap Options;
	if(bWithMulti && DeleteDialog.IsWithSubFiles())
		Options["Recursive"] = true;

	if(bCanKeep && DeleteDialog.IsKeep())
		OnAction("Delete", false, Options);
	else
		OnAction("Remove", false, Options);
}

class CFileAddSourceJob: public CInterfaceJob
{
public:
	CFileAddSourceJob(const QString& Url, uint64 ID)
	{
		m_Request["ID"] = ID;
		m_Request["Action"] = "AddSource";
		m_Request["Url"] = Url;
		m_Request["Log"] = true;
	}

	virtual QString			GetCommand()	{return "FileAction";}
	virtual void			HandleResponse(const QVariantMap& Response) {}
};

void CFileListView::AddSource(uint64 ID)
{
	QString Url = QInputDialog::getText(NULL, tr("Enter Source Url"), tr("Enter the URL of a new source to be added"), QLineEdit::Normal);
	if(Url.isEmpty())
		return;

	CFileAddSourceJob* pFileAddSourceJob = new CFileAddSourceJob(Url, ID);
	theGUI->ScheduleJob(pFileAddSourceJob);
}

void CFileListView::OnAddSource()
{
	QString Url = QInputDialog::getText(NULL, tr("Enter Source Url"), tr("Enter the URL of a new source to be added"), QLineEdit::Normal);
	if(Url.isEmpty())
		return;
	
	QModelIndexList SelectedItems = GetCurTreeSelection(m_pFileTree, m_pSortProxy, m_pFileModel);
	foreach(const QModelIndex& ModelIndex, SelectedItems)
	{
		uint64 ID = m_pFileModel->data(ModelIndex, Qt::UserRole, CFilesModel::eFileName).toULongLong();

		CFileAddSourceJob* pFileAddSourceJob = new CFileAddSourceJob(Url, ID);
		theGUI->ScheduleJob(pFileAddSourceJob);
	}
}

class CFileShareJob: public CInterfaceJob
{
public:
	CFileShareJob(uint64 ID, const QString& Network, int Share)
	{
		m_Request["ID"] = ID;
		m_Request["Action"] = "SetShare";
		m_Request["Network"] = Network;
		m_Request["Share"] = Share;
		m_Request["Log"] = true;
	}

	virtual QString			GetCommand()	{return "FileAction";}
	virtual void			HandleResponse(const QVariantMap& Response) {}
};

void CFileListView::SetShare(const QString& Network, int Share)
{
	if(m_Hold)
		return;

	QModelIndex Index = m_pFileTree->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);

	QModelIndexList SelectedItems = GetCurTreeSelection(m_pFileTree, m_pSortProxy, m_pFileModel);
	foreach(const QModelIndex& ModelIndex, SelectedItems)
	{
		uint64 ID = m_pFileModel->data(ModelIndex, Qt::UserRole, CFilesModel::eFileName).toULongLong();

		CFileShareJob* pFileShareJob = new CFileShareJob(ID, Network, Share);
		theGUI->ScheduleJob(pFileShareJob);
	}
}

void CFileListView::SetProperty(const QString& Name, const QVariant& Value)
{
	if(m_Hold)
		return;

	QModelIndex Index = m_pFileTree->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);

	QModelIndexList SelectedItems = GetCurTreeSelection(m_pFileTree, m_pSortProxy, m_pFileModel);
	foreach(const QModelIndex& ModelIndex, SelectedItems)
	{
		uint64 ID = m_pFileModel->data(ModelIndex, Qt::UserRole, CFilesModel::eFileName).toULongLong();
	
		CSetPropertyJob* pSetPropertyJob = new CSetPropertyJob(ID, Name, Value);
		theGUI->ScheduleJob(pSetPropertyJob);
	}
}

class CGetPasswordsJob: public CInterfaceJob
{
public:
	CGetPasswordsJob(CFileListView* pView, uint64 ID)
	{
		m_pView = pView;
		m_Request["ID"] = ID;
	}

	virtual QString			GetCommand()	{return "GetFile";}
	virtual void			HandleResponse(const QVariantMap& Response) 
	{
		if(m_pView)
		{
			QVariantMap Properties = Response["Properties"].toMap();
			m_pView->SetPasswords(Properties["Passwords"].toStringList());
		}
	}
protected:
	QPointer<CFileListView>	m_pView; // Note: this can be deleted at any time
};

void CFileListView::OnSetPasswords()
{
	QModelIndex Index = m_pFileTree->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	uint64 ID = m_pFileModel->data(ModelIndex, Qt::UserRole, CFilesModel::eFileName).toULongLong();

	CGetPasswordsJob* pGetPasswordsJob = new CGetPasswordsJob(this, ID);
	theGUI->ScheduleJob(pGetPasswordsJob);
}

class CSetPasswordsJob: public CInterfaceJob
{
public:
	CSetPasswordsJob(uint64 ID, const QStringList& Passwords)
	{
		m_Request["ID"] = ID;
		QVariantMap Properties;
		Properties["Passwords"] = Passwords;
		m_Request["Properties"] = Properties;
	}

	virtual QString			GetCommand()	{return "SetFile";}
	virtual void			HandleResponse(const QVariantMap& Response) {}
};

void CFileListView::SetPasswords(const QStringList& OldPasswords)
{
	bool bOK = true;
	QStringList Passwords = CMultiLineInput::GetInput(0, 
		tr("Enter Password(s) for archive decryption,\r\nOne Password to be tried per line,\r\nFor upload first password will be used."),
		OldPasswords.join("\r\n"), &bOK).split(QRegExp("\r?\n"));
	if(!bOK)
		return;

	QModelIndex Index = m_pFileTree->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);

	QModelIndexList SelectedItems = GetCurTreeSelection(m_pFileTree, m_pSortProxy, m_pFileModel);
	foreach(const QModelIndex& ModelIndex, SelectedItems)
	{
		uint64 ID = m_pFileModel->data(ModelIndex, Qt::UserRole, CFilesModel::eFileName).toULongLong();

		CSetPasswordsJob* pSetPasswordsJob = new CSetPasswordsJob(ID, Passwords);
		theGUI->ScheduleJob(pSetPasswordsJob);
	}
}

void CFileListView::OnRefresh()
{
	CSearchWindow::UpdateSearch(m_ID);
}

void CFileListView::OnStream()
{
	QModelIndex Index = m_pFileTree->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	uint64 ID = m_pFileModel->data(ModelIndex, Qt::UserRole, CFilesModel::eFileName).toULongLong();

	emit StreamFile(ID, m_pFileModel->Data(ModelIndex).value("FullPath").toString(), m_pFileModel->Data(ModelIndex).value("FileStatus") == "Complete");
}

void CFileListView::OnOpen()
{
	QModelIndex Index = m_pFileTree->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	uint64 ID = m_pFileModel->data(ModelIndex, Qt::UserRole, CFilesModel::eFileName).toULongLong();

	if (theGUI->Cfg()->GetString("Gui/OnDblClick") == "Open")
	{
		if (m_pFileModel->Data(ModelIndex).value("FileStatus") == "Complete")
		{
			if (theGUI->IsLocal())
				QDesktopServices::openUrl("file:///" + m_pFileModel->Data(ModelIndex).value("FullPath").toString());
			else
				QDesktopServices::openUrl(QString("http://%1:%2/Repository/Data/id:%3/").arg(theGUI->Cfg()->GetString("Core/RemoteHost")).arg(theGUI->Cfg()->GetString("HttpServer/Port")).arg(ID));
		}
	}
	else if (theGUI->Cfg()->GetString("Gui/OnDblClick") == "Tabs")
	{
		emit TogleDetails();
	}
	else if (theGUI->Cfg()->GetString("Gui/OnDblClick") == "Togle")
	{
		if (m_pFileModel->Data(ModelIndex).value("FileState") != "Started")
			OnStart();
		else
			OnStop();
	}
}

class CMakeMultiJob: public CInterfaceJob
{
public:
	CMakeMultiJob(const QString& FileName, const QVariantList& SubFiles)
	{
		m_Request["FileName"] = FileName;
		m_Request["SubFiles"] = SubFiles;
	}

	virtual QString			GetCommand()	{return "AddFile";}
	virtual void			HandleResponse(const QVariantMap& Response) {}
};

void CFileListView::OnMakeMulti()
{
	QString FileName;
	QModelIndexList SelectedItems = GetCurTreeSelection(m_pFileTree, m_pSortProxy, m_pFileModel, false, &FileName);
	QVariantList SubFiles;
	foreach(const QModelIndex& ModelIndex, SelectedItems)
		SubFiles.append(m_pFileModel->data(ModelIndex, Qt::UserRole, CFilesModel::eFileName).toULongLong());

	FileName = QInputDialog::getText(this, tr("File Name"), tr("Enter Name for new Multi File"), QLineEdit::Normal, FileName);
	if(FileName.isEmpty())
		return;

	CMakeMultiJob* pMakeMultiJob = new CMakeMultiJob(FileName, SubFiles);
	theGUI->ScheduleJob(pMakeMultiJob);
}

void CFileListView::OnMakeTorrent()
{
	QModelIndex Index = m_pFileTree->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	uint64 ID = m_pFileModel->data(ModelIndex, Qt::UserRole, CFilesModel::eFileName).toULongLong();

	QVariantMap File = m_pFileModel->Data(ModelIndex);

	CTorrentCreator TorrentCreator(ID);
	TorrentCreator.SetName(File["FileName"].toString());
	TorrentCreator.exec();
}

void CFileListView::OnGetLinks()
{
	QList<uint64> FileIDs = GetSelectedFilesIDs();
	if(sender() == m_pCopyNeo)
		CLinkDialog::CopyLinks(FileIDs, CLinkDialog::eNeo);
	else if(sender() == m_pCopyMagnet)
		CLinkDialog::CopyLinks(FileIDs, CLinkDialog::eMagnet);
	else if(sender() == m_pCopyEd2k)
		CLinkDialog::CopyLinks(FileIDs, CLinkDialog::ed2k);
	else if(sender() == m_pSaveTorrent)
		CLinkDialog::CopyLinks(FileIDs, CLinkDialog::eTorrent);
	else
	{
		CLinkDialog* pLinkDialog = new CLinkDialog(FileIDs, this);
		pLinkDialog->exec();
	}
}

void CFileListView::OnPasteLinks()
{
	QString Links = QApplication::clipboard()->text();
	if(!Links.isEmpty())
		CGrabberWindow::AddLinks(Links, m_Mode == eFilesGrabber);
}

class CFileActionJob: public CInterfaceJob
{
public:
	CFileActionJob(const QString& Action, uint64 ID, const QVariantMap& Options = QVariantMap())
	{
		m_Request = Options;
		m_Request["ID"] = ID;
		m_Request["Action"] = Action;
		m_Request["Log"] = true;
	}

	virtual QString			GetCommand()	{return "FileAction";}
	virtual void			HandleResponse(const QVariantMap& Response) {}
};

void CFileListView::DoAction(const QString& Action, uint64 ID, const QVariantMap& Options)
{
	CFileActionJob* pFileActionJob = new CFileActionJob(Action, ID, Options);
	theGUI->ScheduleJob(pFileActionJob);
}

QList<uint64> CFileListView::GetIDs(bool bWithSub)
{
	QList<uint64> IDs;
	QModelIndexList SelectedItems = GetCurTreeSelection(m_pFileTree, m_pSortProxy, m_pFileModel, bWithSub);
	foreach(const QModelIndex& ModelIndex, SelectedItems)
	{
		QVariantMap File = m_pFileModel->Data(ModelIndex);
		uint64 ID = File["ID"].toULongLong();
		if(IDs.contains(ID))
			continue;

		if(File["FileType"] == "MultiFile")
			IDs.prepend(ID);
		else
			IDs.append(ID);
	}

	return IDs;
}

void CFileListView::OnAction(const QString& Action, bool bWithSub, const QVariantMap& Options)
{
	if(m_Hold)
		return;

	foreach(uint64 ID, GetIDs(bWithSub))
		DoAction(Action, ID, Options);
}

void CFileListView::OnUploadParts()
{
	CHosterUploader HosterUploader;
	if(!HosterUploader.exec())
		return;

	QStringList Hosters = HosterUploader.GetHosters();
	if(Hosters.isEmpty())
	{
		QMessageBox::warning(NULL, tr("Hoster Upload"), tr("No Hosters Selected"));
		return;
	}
	QString Action = HosterUploader.GetAction(); // "UploadParts" "UploadArchive" "UploadSolid"

	QVariantMap Options;
	Options["Hosters"] = Hosters;
	OnAction(Action, false, Options); 
}

void CFileListView::Suspend(bool bSet)
{
	if(bSet)
	{
		if(m_TimerId != 0)
		{
			killTimer(m_TimerId);
			m_TimerId = 0;
		}
	}
	else
	{
		if(m_TimerId == 0)
			m_TimerId = startTimer(250);
		timerEvent(NULL);
	}
}
