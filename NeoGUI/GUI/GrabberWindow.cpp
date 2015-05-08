#include "GlobalHeader.h"
#include "GrabberWindow.h"
#include "FileListWidget.h"
#include "FileListView.h"
#include "WebTaskView.h"
#include "../NeoGUI.h"
#include "../Common/TreeWidgetEx.h"
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif

CGrabberWindow::CGrabberWindow(QWidget *parent)
:QWidget(parent)
{
	m_pGrabberSyncJob = NULL;

	m_pMainLayout = new QVBoxLayout(this);
	m_pMainLayout->setMargin(1);

	m_pGrabberWidget = new QWidget();
	m_pGrabberLayout = new QGridLayout();
	m_pGrabberLayout->setMargin(3);

	m_pLinks = new QPlainTextEdit();
	//m_pLinks->setMaximumHeight(200);
	m_AddFile = new QPushButton(tr("Add File"));
	m_AddFile->setMaximumWidth(150);
	connect(m_AddFile, SIGNAL(pressed()), this, SLOT(OnAddFile()));
	m_AddLinks = new QPushButton(tr("Add Links"));
	connect(m_AddLinks, SIGNAL(pressed()), this, SLOT(OnAddLinks()));

	QLabel* pLabel = new QLabel(tr("Paste Links:"));
	pLabel->setMaximumHeight(10);
	m_pGrabberLayout->addWidget(pLabel, 0, 0);
	m_pGrabberLayout->addWidget(m_pLinks, 1, 0, 3, 4);
	m_pGrabberLayout->addWidget(m_AddFile, 1, 5);
	m_pGrabberLayout->addWidget(m_AddLinks, 3, 5);
	//m_pGrabberLayout->addWidget(, 3, 5);

	m_pGrabberWidget->setLayout(m_pGrabberLayout);

	m_pMainLayout->addWidget(m_pGrabberWidget);

	m_pGrabberTree = new QTreeWidgetEx();
	m_pGrabberTree->setHeaderLabels(tr("Uris|Status|Found Files|Tasks").split("|"));
	//m_pGrabberTree->setContextMenuPolicy(Qt::CustomContextMenu);
	//connect(m_pGrabberTree, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenuRequested(const QPoint &)));
	//connect(m_pGrabberTree, SIGNAL(itemClicked(QTreeWidgetItem*, int)), this, SLOT(OnItemClicked(QTreeWidgetItem*, int)));
	connect(m_pGrabberTree, SIGNAL(itemSelectionChanged()), this, SLOT(OnSelectionChanged()));
	//connect(m_pGrabberTree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(OnItemDoubleClicked(QTreeWidgetItem*, int)));
	m_pMainLayout->addWidget(m_pGrabberTree);

	m_pGrabberTree->header()->restoreState(theGUI->Cfg()->GetBlob("Gui/Widget_Grabber_Columns"));

	setLayout(m_pMainLayout);

	m_TimerId = startTimer(500);
}

CGrabberWindow::~CGrabberWindow()
{
	theGUI->Cfg()->SetBlob("Gui/Widget_Grabber_Columns",m_pGrabberTree->header()->saveState());

	killTimer(m_TimerId);
}

class CGrabberSyncJob: public CInterfaceJob
{
public:
	CGrabberSyncJob(CGrabberWindow* pView)
	{
		m_pView = pView;
	}

	virtual QString			GetCommand()	{return "GrabberList";}
	virtual void			HandleResponse(const QVariantMap& Response)
	{
		if(m_pView)
			m_pView->SyncTasks(Response);
	}
	virtual void			Finish(bool bOK)
	{
		if(m_pView)
			m_pView->m_pGrabberSyncJob = NULL;
		CInterfaceJob::Finish(bOK);
	}

protected:
	QPointer<CGrabberWindow>m_pView; // Note: this can be deleted at any time
};

void CGrabberWindow::timerEvent(QTimerEvent *e)
{
    if (e->timerId() != m_TimerId) 
	{
        QObject::timerEvent(e);
		return;
    }

	if(m_pGrabberSyncJob == NULL)
	{
		m_pGrabberSyncJob = new CGrabberSyncJob(this);
		theGUI->ScheduleJob(m_pGrabberSyncJob);
	}
}

QString CGrabberWindow::GetDisplayName(const QStringList& Uris)
{
	QString DisplayName;
	if(Uris.isEmpty())
		DisplayName = "Empty";
	else
	{
		DisplayName = Uris.first();
		if(DisplayName.contains("magnet:"))
		{
			QUrl Url(DisplayName);
#if QT_VERSION < 0x050000
			DisplayName = Url.queryItemValue("dn");
#else
			QUrlQuery UrlQuery(Url);
			DisplayName = UrlQuery.queryItemValue("dn");
#endif
		}
		else
		{
			while(DisplayName.right(1) == "/")
				DisplayName.truncate(DisplayName.length()-1);
			DisplayName = Split2(DisplayName, "/", true).second;
		}
	}
	return DisplayName;
}

class CGrabberActionJob: public CInterfaceJob
{
public:
	CGrabberActionJob(uint64 ID, const QString& Action)
	{
		m_Request["ID"] = ID;
		m_Request["Action"] = Action;
		m_Request["Log"] = true;
	}

	virtual QString			GetCommand()	{return "GrabberAction";}
	virtual void			HandleResponse(const QVariantMap& Response) {}
};

void CGrabberWindow::GrabberAction(uint64 ID, const QString& Action)
{
	CGrabberActionJob* pGrabberActionJob = new CGrabberActionJob(ID, Action);
	theGUI->ScheduleJob(pGrabberActionJob);
}

class CGrabberAddJob: public CInterfaceJob
{
public:
	CGrabberAddJob(const QString& Links, bool bUseGrabber)
	{
		m_Command = "GrabLinks";
		m_Request["Links"] = Links;
		if(!bUseGrabber)
			m_Request["Direct"] = true;
	}
	CGrabberAddJob(const QString& FilePath, const char* Import)
	{
		m_Command = "AddFile";
		m_Request["FilePath"] = FilePath;
		m_Request["Import"] = QString::fromLatin1(Import);
	}
	CGrabberAddJob(const QString& FileName, const QByteArray& FileData, bool bUseGrabber)
	{
		m_Command = bUseGrabber ? "GrabLinks" : "AddFile";
		m_Request["FileName"] = FileName;
		m_Request["FileData"] = FileData;
	}

	virtual QString			GetCommand()	{return m_Command;}
	virtual void			HandleResponse(const QVariantMap& Response) {}

protected:
	QString					m_Command;
};

void CGrabberWindow::AddLinks(const QString& Links, bool bUseGrabber)
{
	CGrabberAddJob* pGrabberAddJob = new CGrabberAddJob(Links, bUseGrabber);
	theGUI->ScheduleJob(pGrabberAddJob);
}

void CGrabberWindow::AddFile(const QString& FilePath, bool bUseGrabber)
{
	if(FilePath.right(8).compare("part.met", Qt::CaseInsensitive) != 0 == 0)
	{
		// if this is a file import it must not go through the grabber
		CGrabberAddJob* pGrabberAddJob = new CGrabberAddJob(FilePath, "Ed2kMule");
		theGUI->ScheduleJob(pGrabberAddJob);
		return;
	}

	QFile File(FilePath);
	File.open(QFile::ReadOnly);

	if(!(FilePath.right(7).compare("torrent", Qt::CaseInsensitive) == 0 || FilePath.right(14).compare("mulecollection", Qt::CaseInsensitive) == 0))
		bUseGrabber = true; // if this is not a torrent or a collection it must go through the grabber

	CGrabberAddJob* pGrabberAddJob = new CGrabberAddJob(FilePath, File.readAll(), bUseGrabber);
	theGUI->ScheduleJob(pGrabberAddJob);
}

QString CGrabberWindow::GetOpenFilter(bool bImport)
{
	QStringList Files;
	Files << "All Supported Files (*.torrent *.emulecollection *.dlc)";
	Files << "Torrent Files (*.torrent)";
	Files << "eMule Collections (*.emulecollection)";
	Files << "Download Containers (*.dlc)";
	//Files << "Download Containers (*.dlc *.ccf *.rsdf)";
	if(bImport)
		Files << "Incomplete eMule Downloads (*.met)";
	//Files << "Any File (*.*)";
	return Files.join(";;");
}

void CGrabberWindow::OnAddFile()
{
	QStringList FilePaths = QFileDialog::getOpenFileNames(0, tr("Add File"), "", GetOpenFilter());
	foreach(const QString& FilePath, FilePaths)
		AddFile(FilePath, true);
}

void CGrabberWindow::OnAddLinks()
{
	AddLinks(m_pLinks->toPlainText(), true);
	m_pLinks->clear();
}

void CGrabberWindow::SyncTasks(const QVariantMap& Response)
{
	QMap<uint64, STask*> OldTasks = m_Tasks;

	QList<QTreeWidgetItem*> NewItems;
	foreach (const QVariant vTask, Response["Tasks"].toList())
	{
		QVariantMap Task = vTask.toMap();
		uint64 GrabberID = Task["ID"].toULongLong();

		STask* pTask = OldTasks.take(GrabberID);
		if(!pTask)
		{
			pTask = new STask();
			pTask->Urls = Task["Uris"].toStringList();
			m_Tasks.insert(GrabberID, pTask);

			pTask->pItem = new QTreeWidgetItem();
			pTask->pItem->setData(eUris, Qt::UserRole, GrabberID);
			NewItems.append(pTask->pItem);

			pTask->pItem->setText(eUris, GetDisplayName(pTask->Urls));
		}

		QString Status;
		if(Task["TasksPending"].toInt() > Task["TasksFailed"].toInt())
			Status = "Running";
		else if(Task["TasksFailed"].toInt() == 0)
			Status = "Finished";
		else
			Status = "Errors";
		pTask->pItem->setText(eStatus, Status);

		pTask->pItem->setText(eFoundFiles, Task["FileCount"].toString());
		pTask->pItem->setText(ePendingTasks, tr("%1 (%2)").arg(Task["TasksPending"].toInt()).arg(Task["TasksFailed"].toInt()));

		SyncTask(GrabberID);
	}
	m_pGrabberTree->addTopLevelItems(NewItems);

	foreach(STask* pTask, OldTasks)
	{
		m_Tasks.remove(OldTasks.key(pTask));
		delete pTask->pItem;
		delete pTask;
	}
}

void CGrabberWindow::SyncTask(uint64 ID, const QVariantMap& Response)
{
	STask* pTask = m_Tasks.value(ID);
	if(!pTask)
		return;

	QMap<uint64, QTreeWidgetItem*> OldWebTasks;
	for(int i = 0; i < pTask->pItem->childCount(); ++i) 
	{
		QTreeWidgetItem* pItem = pTask->pItem->child(i);
		uint64 ID = pItem->data(0, Qt::UserRole).toULongLong();
		Q_ASSERT(!OldWebTasks.contains(ID));
		OldWebTasks.insert(ID,pItem);
	}

	QList<QTreeWidgetItem*> NewItems;
	foreach (const QVariant vWebTask, Response["Tasks"].toList())
	{
		QVariantMap WebTask = vWebTask.toMap();
		uint64 SubID = WebTask["ID"].toULongLong();

		QTreeWidgetItem* pItem = OldWebTasks.take(SubID);
		if(!pItem)
		{
			pItem = new QTreeWidgetItem();
			pItem->setData(eUris, Qt::UserRole, SubID);
			NewItems.append(pItem);
		}

		pItem->setText(eUris, WebTask["Url"].toString());

		pItem->setText(eStatus, WebTask["Status"].toString());
	}
	pTask->pItem->addChildren(NewItems);
	pTask->pItem->setExpanded(true);

	foreach(QTreeWidgetItem* pItem, OldWebTasks)
		delete pItem;
}

class CTaskSyncJob: public CInterfaceJob
{
public:
	CTaskSyncJob(CGrabberWindow* pView, uint64 ID)
	{
		m_ID = ID;
		m_pView = pView;
		m_Request["GrabberID"] = ID;
	}

	virtual QString			GetCommand()	{return "WebTaskList";}
	virtual void			HandleResponse(const QVariantMap& Response)
	{
		if(m_pView)
			m_pView->SyncTask(m_ID, Response);
	}
	virtual void			Finish(bool bOK)
	{
		if(m_pView)
		{
			if(CGrabberWindow::STask* pTask = m_pView->m_Tasks.value(m_ID))
				pTask->pTaskSyncJob = NULL;
		}
		CInterfaceJob::Finish(bOK);
	}

protected:
	QPointer<CGrabberWindow>m_pView; // Note: this can be deleted at any time
	uint64					m_ID;
};

void CGrabberWindow::SyncTask(uint64 ID)
{
	STask* pTask = m_Tasks.value(ID);
	if(!pTask || pTask->pTaskSyncJob)
		return;

	pTask->pTaskSyncJob = new CTaskSyncJob(this, ID);
	theGUI->ScheduleJob(pTask->pTaskSyncJob);
}

void CGrabberWindow::OnItemClicked(QTreeWidgetItem* pItem, int Column)
{
	uint64 GrabberID = pItem->data(eUris, Qt::UserRole).toULongLong();
	if(STask* pTask = m_Tasks.value(GrabberID))
		m_pLinks->setPlainText(pTask->Urls.join("\r\n"));
}

void CGrabberWindow::OnSelectionChanged()
{
	if(QTreeWidgetItem* pItem = m_pGrabberTree->currentItem())
		OnItemClicked(pItem, 0);
}

//void CGrabberWindow::OnItemDoubleClicked(QTreeWidgetItem* pItem, int Column)
//{
//
//}

//void CGrabberWindow::OnMenuRequested(const QPoint &point)
//{
//	m_pMenu->popup(QCursor::pos());	
//}