#include "GlobalHeader.h"
#include "WebTaskView.h"
#include "../NeoGUI.h"
#include "../Common/TreeWidgetEx.h"

CWebTaskView::CWebTaskView(QWidget *parent)
:QWidget(parent)
{
	m_pWebTaskSyncJob = NULL;
	m_ID = -1;

	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);

	m_pWebTaskTree = new QTreeWidgetEx();
	m_pWebTaskTree->setHeaderLabels(tr("URL|Entry|Status").split("|"));
	m_pWebTaskTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
#ifdef WIN32
	m_pWebTaskTree->setStyle(QStyleFactory::create("windowsxp"));
#endif
	m_pWebTaskTree->setSortingEnabled(true);

	m_pMainLayout->addWidget(m_pWebTaskTree);

	setLayout(m_pMainLayout);

	m_pWebTaskTree->header()->restoreState(theGUI->Cfg()->GetBlob("Gui/Widget_WebTasks_Columns"));

	m_TimerId = startTimer(500);
}

CWebTaskView::~CWebTaskView()
{
	theGUI->Cfg()->SetBlob("Gui/Widget_WebTasks_Columns",m_pWebTaskTree->header()->saveState());

	killTimer(m_TimerId);
}

class CWebTaskSyncJob: public CInterfaceJob
{
public:
	CWebTaskSyncJob(CWebTaskView* pView, uint64 ID)
	{
		m_pView = pView;
		if(ID != -1)
			m_Request["GrabberID"] = ID;
	}

	virtual QString			GetCommand()	{return "WebTaskList";}
	virtual void			HandleResponse(const QVariantMap& Response)
	{
		if(m_pView)
			m_pView->SyncWebTasks(Response);
	}
	virtual void			Finish(bool bOK)
	{
		if(m_pView)
			m_pView->m_pWebTaskSyncJob = NULL;
		CInterfaceJob::Finish(bOK);
	}

protected:
	QPointer<CWebTaskView>	m_pView; // Note: this can be deleted at any time
};

void CWebTaskView::SyncWebTasks()
{
	if(m_pWebTaskSyncJob == NULL)
	{
		m_pWebTaskSyncJob = new CWebTaskSyncJob(this, m_ID);
		theGUI->ScheduleJob(m_pWebTaskSyncJob);
	}
}

void CWebTaskView::timerEvent(QTimerEvent *e)
{
    if (e->timerId() != m_TimerId) 
	{
        QObject::timerEvent(e);
		return;
    }

	SyncWebTasks();
}

void CWebTaskView::SyncWebTasks(const QVariantMap& Response)
{
	QMap<uint64, QTreeWidgetItem*> OldWebTasks;
	for(int i = 0; i < m_pWebTaskTree->topLevelItemCount(); ++i) 
	{
		QTreeWidgetItem* pItem = m_pWebTaskTree->topLevelItem(i);
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
			pItem->setData(eUrl, Qt::UserRole, SubID);
			NewItems.append(pItem);
		}

		pItem->setText(eUrl, WebTask["Url"].toString());

		pItem->setText(eEntry, WebTask["Entry"].toString());

		pItem->setText(eStatus, WebTask["Status"].toString());
	}
	m_pWebTaskTree->addTopLevelItems(NewItems);

	foreach(QTreeWidgetItem* pItem, OldWebTasks)
		delete pItem;
}

void CWebTaskView::Suspend(bool bSet)
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
			m_TimerId = startTimer(500);
		SyncWebTasks();
	}
}