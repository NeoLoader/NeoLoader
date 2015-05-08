#include "GlobalHeader.h"
#include "TransfersView.h"
#include "TransfersModel.h"
#include "ProgressBar.h"
#include "../NeoGUI.h"
#include "../Common/TreeViewEx.h"

QString TrMode2Str(UINT Mode)
{
	switch(Mode)
	{
	case CTransfersView::eTransfers:	return "Transfers";
	case CTransfersView::eActive:		return "ActiveTransfers";
	case CTransfersView::eDownloads:	return "ActiveDownload";
	case CTransfersView::eUploads:		return "ActiveUploads";
	case CTransfersView::eClients:		return "Clients";
	default:							return "";
	}
}

CTransfersView::CTransfersView(UINT Mode, QWidget *parent)
:QWidget(parent)
{
	m_pTransferSyncJob = NULL;
	m_ID = 0;
	m_Ops = TrMode2Str(Mode);
	m_Mode = Mode;
	m_SyncToken = 0;
	m_NextFullUpdate = 0;

	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);

	m_pTransferTree = new QTreeViewEx();
	m_pTransferTree->setItemDelegate(new QStyledItemDelegate16(this));
	m_pTransferModel = new CTransfersModel();
	m_pSortProxy = new QSortFilterProxyModel(this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pTransferModel);
	m_pSortProxy->setDynamicSortFilter(theGUI->Cfg()->GetString("Gui/AutoSort") == "true" 
		|| theGUI->Cfg()->GetString("Gui/AutoSort").contains(Mode == eTransfers ? "L" : "T"));
	m_pTransferTree->setModel(m_pSortProxy);

	m_pTransferTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
#ifdef WIN32
	m_pTransferTree->setStyle(QStyleFactory::create("windowsxp"));
#endif
	m_pTransferTree->setSortingEnabled(true);

	m_pTransferTree->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pTransferTree, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenuRequested(const QPoint &)));

	connect(m_pTransferTree, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(OnDoubleClicked(const QModelIndex&)));

	connect(m_pTransferTree->verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(OnScroll()));

	m_pMenu = new QMenu();

	if (theGUI->Cfg()->GetInt("Gui/AdvancedControls"))
	{
		m_StartDownload = new QAction(tr("Start Download"), m_pMenu);
		connect(m_StartDownload, SIGNAL(triggered()), this, SLOT(OnStartDownload()));
		m_pMenu->addAction(m_StartDownload);

		m_StartUpload = new QAction(tr("Start Upload"), m_pMenu);
		connect(m_StartUpload, SIGNAL(triggered()), this, SLOT(OnStartUpload()));
		m_pMenu->addAction(m_StartUpload);

		m_CancelTransfer = new QAction(tr("Cancel Transfer"), m_pMenu);
		connect(m_CancelTransfer, SIGNAL(triggered()), this, SLOT(OnCancelTransfer()));
		m_pMenu->addAction(m_CancelTransfer);
	}
	else
	{
		m_StartDownload = m_StartUpload = m_CancelTransfer = NULL;
	}

	m_ClearError = new QAction(tr("Clear Error"), m_pMenu);
	connect(m_ClearError, SIGNAL(triggered()), this, SLOT(OnClearError()));
	m_pMenu->addAction(m_ClearError);

	m_RemoveTransfer = new QAction(tr("Remove Transfer"), m_pMenu);
	connect(m_RemoveTransfer, SIGNAL(triggered()), this, SLOT(OnRemoveTransfer()));
	m_pMenu->addAction(m_RemoveTransfer);

	m_pMenu->addSeparator();

	m_CopyUrls = new QAction(tr("Copy URLs"), m_pMenu);
	connect(m_CopyUrls, SIGNAL(triggered()), this, SLOT(OnCopyUrl()));
	m_pMenu->addAction(m_CopyUrls);

	m_pMainLayout->addWidget(m_pTransferTree);

	setLayout(m_pMainLayout);

	m_pTransferTree->header()->restoreState(theGUI->Cfg()->GetBlob("Gui/Widget_" + m_Ops + "_Columns"));
	m_pTransferTree->hideColumn(m_pTransferModel->columnCount() - 1);

	m_TimerId = startTimer(250);
}

CTransfersView::~CTransfersView()
{
	theGUI->Cfg()->SetBlob("Gui/Widget_" + m_Ops + "_Columns",m_pTransferTree->header()->saveState());

	killTimer(m_TimerId);
}

void CTransfersView::ShowTransfers(uint64 ID)
{
	m_ID = ID;
	m_SyncToken = 0;
	timerEvent(NULL);
}

void CTransfersView::ChangeMode(UINT Mode)
{
	m_ID = -1;
	if(m_Mode == Mode)
		return;
	m_SyncToken = 0;

	// load proepr view config for this panel
	theGUI->Cfg()->SetBlob("Gui/Widget_" + m_Ops + "_Columns",m_pTransferTree->header()->saveState());
	m_Ops = TrMode2Str(Mode);
	m_pTransferTree->header()->restoreState(theGUI->Cfg()->GetBlob("Gui/Widget_" + m_Ops + "_Columns"));
	m_pTransferTree->hideColumn(m_pTransferModel->columnCount() - 1);

	m_Mode = Mode;

	timerEvent(NULL);
}

class CTransferSyncJob: public CInterfaceJob
{
public:
	CTransferSyncJob(CTransfersView* pView)
	{
		m_pView = pView;
		m_Mode = pView->m_Mode;
		m_ID = pView->m_ID;
		m_Full = true;

		if(m_Mode == CTransfersView::eClients)
		{
			m_Command = "GetClients";
		}
		else
		{
			m_Command = "GetTransfers";

			if(m_ID != -1)
				m_Request["ID"] = m_ID;
			else
			{
				switch(m_Mode)
				{
					case CTransfersView::eActive:		m_Request["Type"]  = "Active";		break;
					case CTransfersView::eDownloads:	m_Request["Type"]  = "Downloads";	break;
					case CTransfersView::eUploads:		m_Request["Type"]  = "Uploads";		break;
				}
			}
		}

		QVariantList ExtendedList;
		foreach(uint64 FileID, pView->m_VisibleTransfers)
			ExtendedList.append(FileID);

		m_Request["ExtendedList"] = ExtendedList; // Note: if this is not set all files are extended by default
		m_Request["Token"] = pView->m_SyncToken;
	}

	CTransferSyncJob(CTransfersView* pView, const QSet<uint64>& VisibleTransfers)
	{
		m_pView = pView;
		m_Mode = pView->m_Mode;
		m_ID = pView->m_ID;
		m_Full = false;

		m_Command = m_Mode == CTransfersView::eClients ? "GetClients" : "GetTransfers";

		QVariantList SelectedList;
		foreach(uint64 FileID, VisibleTransfers)
			SelectedList.append(FileID);

		m_Request["SelectedList"] = SelectedList;
		m_Request["Token"] = pView->m_SyncToken;
	}

	virtual QString			GetCommand()	{return m_Command;}
	virtual void			HandleResponse(const QVariantMap& Response)
	{
		if(m_pView)
		{
			if(m_Full) // schedule next full update
			{
				uint64 FullUpdateDelay = Min(Max(GetDuration() * 10, 2000), 10000);
				m_pView->m_NextFullUpdate = GetCurTick() + FullUpdateDelay;
			}
			if(m_Mode == m_pView->m_Mode && m_ID == m_pView->m_ID)
				m_pView->SyncTransfers(Response, m_Full);
		}
	}
	virtual void			Finish(bool bOK)
	{
		if(m_pView)
			m_pView->m_pTransferSyncJob = NULL;
		CInterfaceJob::Finish(bOK);
	}

protected:
	UINT					m_Mode;
	QString					m_Command;
	uint64					m_ID;
	bool					m_Full;
	QPointer<CTransfersView>m_pView; // Note: this can be deleted at any time
};

void CTransfersView::timerEvent(QTimerEvent *e)
{
    if (e && e->timerId() != m_TimerId) 
	{
        QObject::timerEvent(e);
		return;
    }

	if(m_ID == 0)
		return;

	if(m_pTransferSyncJob == NULL)
	{
		m_pTransferSyncJob = m_NextFullUpdate < GetCurTick() ? new CTransferSyncJob(this) : new CTransferSyncJob(this, m_VisibleTransfers);
		theGUI->ScheduleJob(m_pTransferSyncJob);
	}
}

void CTransfersView::SyncTransfers(const QVariantMap& Response, bool bFull)
{
	if(Response.contains("Token"))
	{
		if(m_SyncToken != Response["Token"].toULongLong())
		{
			m_pTransferModel->Clear();
			m_SyncToken = Response["Token"].toULongLong();
		}
		m_pTransferModel->IncrSync(Response["Transfers"].toList(), m_Mode);
	}
	else
		m_pTransferModel->Sync(Response["Transfers"].toList());

	if(bFull)
		OnScroll();
}

void CTransfersView::OnScroll()
{
	QSet<uint64> VisibleTransfers;
	QSet<uint64> NewVisibleTransfers;
	for(QModelIndex Index = m_pTransferTree->indexAt(QPoint(0,0)); ; Index = m_pTransferTree->indexBelow(Index))
	{
		if(!m_pTransferTree->viewport()->rect().intersects(m_pTransferTree->visualRect(Index)))
			break;
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		uint64 ID = m_pTransferModel->data(ModelIndex, Qt::UserRole, CTransfersModel::eUrl).toULongLong();
		if(ID == 0)
			continue; // just a directory not a real file

		VisibleTransfers.insert(ID);
		if(!m_VisibleTransfers.contains(ID))
			NewVisibleTransfers.insert(ID);
	}
	m_VisibleTransfers = VisibleTransfers;

	if(m_pTransferSyncJob == NULL)
	{
		m_pTransferSyncJob = new CTransferSyncJob(this, NewVisibleTransfers);
		theGUI->ScheduleJob(m_pTransferSyncJob);
	}

	uint64 uCurTick = GetCurTick();
	if(!m_pTransferTree->isColumnHidden(CTransfersModel::eProgress))
	{
		QMap<uint64, QPair<QPointer<QWidget>, QPersistentModelIndex> > OldProgressMap;
		m_pTransferTree->StartUpdatingWidgets(OldProgressMap, m_ProgressMap);
		for(QModelIndex Index = m_pTransferTree->indexAt(QPoint(0,0)); ;Index = m_pTransferTree->indexBelow(Index))
		{
			Index = Index.sibling(Index.row(), CTransfersModel::eProgress);
			if(!m_pTransferTree->viewport()->rect().intersects(m_pTransferTree->visualRect(Index)))
				break;
			QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
			uint64 ID = m_pTransferModel->data(ModelIndex, Qt::UserRole, CTransfersModel::eFileName).toULongLong();
			uint64 SubID = m_pTransferModel->data(ModelIndex, Qt::UserRole, CTransfersModel::eUrl).toULongLong();
			QWidget* pWidget = OldProgressMap.take(SubID).first;
			//if(!theGUI->Cfg()->GetBool("Gui/SimpleBars"))
			//{
			CProgressBar* pProgress = qobject_cast<CProgressBar*>(pWidget);
			if(!pProgress)
			{
				pProgress = new CProgressBar(ID, SubID);
				m_ProgressMap.insert(SubID, qMakePair((QPointer<QWidget>)pProgress, QPersistentModelIndex(Index)));
				m_pTransferTree->setIndexWidget(Index, pProgress);
			}
			pProgress->SetProgress(m_pTransferModel->data(ModelIndex, Qt::EditRole, CTransfersModel::eProgress).toInt());
			if(uCurTick > pProgress->GetNextUpdate())
				pProgress->Update();
			//}
			//else
			//{
			//	QProgressBar* pProgress = qobject_cast<QProgressBar*>(pWidget);
			//	if(!pProgress)
			//	{
			//		pProgress = new QProgressBar();
			//		pProgress->setMaximumHeight(16);
			//		m_ProgressMap.insert(SubID, qMakePair((QPointer<QWidget>)pProgress, QPersistentModelIndex(Index)));
			//		m_pTransferTree->setIndexWidget(Index, pProgress);
			//		pProgress->setMaximum(100);
			//	}
			//	pProgress->setValue(m_pTransferModel->data(ModelIndex, Qt::EditRole, CTransfersModel::eProgress).toInt());
			//}
		}
		m_pTransferTree->EndUpdatingWidgets(OldProgressMap, m_ProgressMap);
	}
}

void CTransfersView::OnMenuRequested(const QPoint &point)
{
	m_pMenu->popup(QCursor::pos());	
}

void CTransfersView::OnDoubleClicked(const QModelIndex& Index)
{
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	uint64 ID = m_pTransferModel->data(ModelIndex, Qt::UserRole, CTransfersModel::eFileName).toULongLong();
	uint64 SubID = m_pTransferModel->data(ModelIndex, Qt::UserRole, CTransfersModel::eUrl).toULongLong();

	emit OpenTransfer(ID, SubID);
}

class CTransferActionJob: public CInterfaceJob
{
public:
	CTransferActionJob(const QString& Action, uint64 ID, uint64 SubID)
	{
		m_Request["ID"] = ID;
		m_Request["SubID"] = SubID;
		m_Request["Action"] = Action;
		m_Request["Log"] = true;
	}

	virtual QString			GetCommand()	{return "TransferAction";}
	virtual void			HandleResponse(const QVariantMap& Response) {}
};

void CTransfersView::OnAction(const QString& Action)
{
	foreach(const QModelIndex& Index, m_pTransferTree->selectedRows())
	{
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		uint64 ID = m_pTransferModel->data(ModelIndex, Qt::UserRole, CTransfersModel::eFileName).toULongLong();
		uint64 SubID = m_pTransferModel->data(ModelIndex, Qt::UserRole, CTransfersModel::eUrl).toULongLong();
		OnAction(Action, ID, SubID);
	}
}

void CTransfersView::OnAction(const QString& Action, uint64 ID, uint64 SubID)
{
	CTransferActionJob* pTransferActionJob = new CTransferActionJob(Action, ID, SubID);
	theGUI->ScheduleJob(pTransferActionJob);
}

void CTransfersView::OnCopyUrl()
{
	QStringList Urls;
	foreach(const QModelIndex& Index, m_pTransferTree->selectedRows())
	{
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
#ifndef _DEBUG
		if(m_pTransferModel->data(ModelIndex, Qt::DisplayRole, CTransfersModel::eType).toString() == "Archive Link")
#endif
			Urls.append(m_pTransferModel->data(ModelIndex, Qt::DisplayRole, CTransfersModel::eUrl).toString());
	}
	QApplication::clipboard()->setText(Urls.join("\r\n"));
}
