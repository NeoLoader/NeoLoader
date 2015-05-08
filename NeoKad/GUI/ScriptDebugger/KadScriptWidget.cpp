#include "GlobalHeader.h"
#include "KadScriptDebugger.h"
#include "KadScriptWidget.h"

#include "../../Kad/KadHeader.h"
#include "../../Kad/KadID.h"

#define eKey 3
#define COLUMNS(x,x1,x2,x3) namespace E##x { enum _##x {x1, x2, x3, }; };
COLUMNS(Script, eName, eVersion, eCodeID)
COLUMNS(TaskList, eName, e1, e2)
COLUMNS(Task, eLookupID, eTargetID, eStatus)
COLUMNS(Route, eEntityID, eTargetID, eStatus)
COLUMNS(RouteSession, eEntityID, eSessionID, eStatus)

CKadScriptWidget::CKadScriptWidget(CKadScriptDebugger* pDebugger, QWidget *parent)
  : QWidget(parent), m_pDebugger(pDebugger)
  , m_pKadScriptSyncJob(0)
{
	m_Headers[eScript] = QString("Name|Version|ID").split("|");
	m_Headers[eTaskList] = QString("||").split("|");
	m_Headers[eTask] = QString("LookupID|TargtID|Status").split("|");
	m_Headers[eRoute] = QString("EntityID|TargtID|Status").split("|");
	m_Headers[eRouteSession] = QString("EntityID|SessionID|Status").split("|");

	m_pMainLayout = new QVBoxLayout(this);
	m_pMainLayout->setMargin(0);

	m_pToolBar = new QToolBar(this);

	/*QAction* pAddScript = new QAction(tr("Add Script"), this);
	connect(pAddScript, SIGNAL(triggered()), this, SLOT(OnAddScript()));
	QIcon Add;
	Add.addPixmap(QPixmap::fromImage(QImage(":/Add")), QIcon::Normal);
	pAddScript->setIcon(Add);
	m_pToolBar->addAction(pAddScript);*/


	m_pKillScript = new QAction(tr("Kill Script"), this);
	connect(m_pKillScript, SIGNAL(triggered()), this, SLOT(OnKillScript()));
	QIcon Kill;
	Kill.addPixmap(QPixmap::fromImage(QImage(":/Terminate")), QIcon::Normal);
	m_pKillScript->setIcon(Kill);
	m_pToolBar->addAction(m_pKillScript);
	m_pKillScript->setEnabled(false);

	m_pToolBar->addSeparator();

	m_pAddTask = new QAction(tr("Add Task"), this);
	connect(m_pAddTask, SIGNAL(triggered()), this, SLOT(OnAddTask()));
	QIcon Plus;
	Plus.addPixmap(QPixmap::fromImage(QImage(":/Plus")), QIcon::Normal);
	m_pAddTask->setIcon(Plus);
	m_pToolBar->addAction(m_pAddTask);
	m_pAddTask->setEnabled(false);

	m_pRemTask = new QAction(tr("Remove Task"), this);
	connect(m_pRemTask, SIGNAL(triggered()), this, SLOT(OnRemTask()));
	QIcon Minus;
	Minus.addPixmap(QPixmap::fromImage(QImage(":/Minus")), QIcon::Normal);
	m_pRemTask->setIcon(Minus);
	m_pToolBar->addAction(m_pRemTask);
	m_pRemTask->setEnabled(false);

	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pToolBar->addWidget(pSpacer);

	m_pDelScript = new QAction(tr("Remove Script"), this);
	connect(m_pDelScript, SIGNAL(triggered()), this, SLOT(OnDelScript()));
	QIcon Del;
	Del.addPixmap(QPixmap::fromImage(QImage(":/Del")), QIcon::Normal);
	m_pDelScript->setIcon(Del);
	m_pToolBar->addAction(m_pDelScript);
	m_pDelScript->setEnabled(false);

	m_pMainLayout->addWidget(m_pToolBar);

	m_pScriptTree = new QTreeWidget(this);
	m_pScriptTree->setColumnCount(3);
	m_pScriptTree->setHeaderLabels(m_Headers[eScript]);
	m_pScriptTree->setExpandsOnDoubleClick(false);
	connect(m_pScriptTree, SIGNAL(currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)), this, SLOT(OnItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)));
	connect(m_pScriptTree, SIGNAL(itemChanged(QTreeWidgetItem*, int)), this, SLOT(OnItemChanged(QTreeWidgetItem*, int)));
	//connect(m_pScriptTree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(OnItemDoubleClicked(QTreeWidgetItem*, int)));
	m_pMainLayout->addWidget(m_pScriptTree);

	m_TimerId = startTimer(500);
}

CKadScriptWidget::~CKadScriptWidget()
{
	killTimer(m_TimerId);
}

void CKadScriptWidget::DissableToolbar(bool bSet)
{
	m_pToolBar->setDisabled(bSet);
}

class CKadScriptSyncJob: public CInterfaceJob
{
public:
	CKadScriptSyncJob(CKadScriptWidget* pView)
	{
		m_pView = pView;
	}

	virtual QString			GetCommand()	{return "ListScripts";}
	virtual void			HandleResponse(const QVariantMap& Response)
	{
		if(m_pView)
			m_pView->SyncKadScripts(Response);
	}
	virtual void			Finish(bool bOK)
	{
		if(m_pView)
			m_pView->m_pKadScriptSyncJob = NULL;
		CInterfaceJob::Finish(bOK);
	}

protected:
	QPointer<CKadScriptWidget>	m_pView; // Note: this can be deleted at any time
};

void CKadScriptWidget::SyncKadScripts()
{
	if(m_pKadScriptSyncJob == NULL)
	{
		m_pKadScriptSyncJob = new CKadScriptSyncJob(this);
		m_pDebugger->ScheduleJob(m_pKadScriptSyncJob);
	}
}

void CKadScriptWidget::timerEvent(QTimerEvent *e)
{
    if (e->timerId() != m_TimerId) 
	{
        QObject::timerEvent(e);
		return;
    }

	SyncKadScripts();
}

void CKadScriptWidget::SyncKadScripts(const QVariantMap& Response)
{
	QMap<QByteArray, QTreeWidgetItem*> OldScripts;
	for(int i = 0; i < m_pScriptTree->topLevelItemCount(); ++i) 
	{
		QTreeWidgetItem* pItem = m_pScriptTree->topLevelItem(i);
		QByteArray CodeID = pItem->data(EScript::eCodeID, Qt::UserRole).toByteArray();
		Q_ASSERT(!OldScripts.contains(CodeID));
		OldScripts.insert(CodeID,pItem);
	}

	foreach(const QVariant& vScript, Response["KadScripts"].toList())
	{
		QVariantMap Script = vScript.toMap();
		QByteArray CodeID = Script["CodeID"].toByteArray();
		QTreeWidgetItem* pItem = OldScripts.take(CodeID);
		if(!pItem)
		{
			pItem = new QTreeWidgetItem();
			pItem->setCheckState(0, Qt::Unchecked);
			pItem->setData(eKey, Qt::UserRole, eScript);
			pItem->setData(EScript::eCodeID, Qt::UserRole, CodeID);
			m_pScriptTree->addTopLevelItem(pItem);

			QTreeWidgetItem* pSubItem = new QTreeWidgetItem();
			pSubItem->setText(ETaskList::eName,tr("Tasks"));
			pSubItem->setData(eKey, Qt::UserRole, eTaskList);
			pItem->addChild(pSubItem);
		}

		pItem->setText(EScript::eName, Script["Name"].toString());
		pItem->setText(EScript::eVersion, Script["Version"].toString() + (Script["Authenticated"].toBool() ? "(Auth)" : ""));
		pItem->setText(EScript::eCodeID, CodeID.toHex());

		QMap<QByteArray, QTreeWidgetItem*> OldTasks;
		for(int i = 0; i < pItem->child(0)->childCount(); ++i)
		{
			QTreeWidgetItem* pSubItem = pItem->child(0)->child(i);
			QByteArray LookupID = pSubItem->data(ETask::eLookupID, Qt::UserRole).toByteArray();
			Q_ASSERT(!OldTasks.contains(LookupID));
			OldTasks.insert(LookupID,pSubItem);
		}	

		foreach(const QVariant& vTask, Script["Tasks"].toList())
		{
			QVariantMap Task = vTask.toMap();

			QByteArray LookupID = Task["LookupID"].toByteArray();
			QTreeWidgetItem* pSubItem = OldTasks.take(LookupID);
			if(!pSubItem)
			{
				pSubItem = new QTreeWidgetItem();
				pSubItem->setCheckState(0, Qt::Unchecked);
				pSubItem->setData(eKey, Qt::UserRole, eTask);
				pSubItem->setData(ETask::eLookupID, Qt::UserRole, LookupID);
				pItem->child(0)->addChild(pSubItem);
			}
			
			pSubItem->setText(ETask::eLookupID, LookupID.toHex());
			pSubItem->setText(ETask::eTargetID, QString::fromStdWString(CUInt128(CVariant(CBuffer(Task["TargetID"].toByteArray()))).ToHex()));
			pSubItem->setText(ETask::eStatus, Task["Status"].toString());
		}

		foreach(QTreeWidgetItem* pSubItem, OldTasks)
			delete pSubItem;


		QMap<QByteArray, QTreeWidgetItem*> OldRoutes;
		for(int i = 1; i < pItem->childCount(); ++i)
		{
			QTreeWidgetItem* pSubItem = pItem->child(i);
			QByteArray EntityID = pSubItem->data(ERoute::eEntityID, Qt::UserRole).toByteArray();
			Q_ASSERT(!OldRoutes.contains(EntityID));
			OldRoutes.insert(EntityID,pSubItem);
		}	

		foreach(const QVariant& vRoute, Script["Routes"].toList())
		{
			QVariantMap Route = vRoute.toMap();

			QByteArray EntityID = Route["EntityID"].toByteArray();
			QTreeWidgetItem* pSubItem = OldRoutes.take(EntityID);
			if(!pSubItem)
			{
				pSubItem = new QTreeWidgetItem();
				pSubItem->setCheckState(0, Qt::Unchecked);
				pSubItem->setData(eKey, Qt::UserRole, eRoute);
				pSubItem->setData(ERoute::eEntityID, Qt::UserRole, EntityID);
				pItem->addChild(pSubItem);
			}
			
			pSubItem->setText(ERoute::eEntityID, EntityID.toHex());
			pSubItem->setText(ERoute::eTargetID, QString::fromStdWString(CUInt128(CVariant(CBuffer(Route["TargetID"].toByteArray()))).ToHex()));
			pSubItem->setText(ERoute::eStatus, Route["Status"].toString());


			QMap<QByteArray, QTreeWidgetItem*> OldSessions;
			for(int i = 0; i < pSubItem->childCount(); ++i)
			{
				QTreeWidgetItem* pItem = pSubItem->child(i);
				QByteArray SessionID = pItem->data(ERouteSession::eSessionID, Qt::UserRole).toByteArray();
				Q_ASSERT(!OldTasks.contains(SessionID));
				OldSessions.insert(SessionID,pItem);
			}	

			foreach(const QVariant& vSession, Route["Sessions"].toList())
			{
				QVariantMap Session = vSession.toMap();

				QByteArray SessionID = Session["SessionID"].toByteArray();
				QTreeWidgetItem* pItem = OldSessions.take(SessionID);
				if(!pItem)
				{
					pItem = new QTreeWidgetItem();
					pItem->setCheckState(0, Qt::Unchecked);
					pItem->setData(eKey, Qt::UserRole, eRouteSession);
					pItem->setData(ERouteSession::eSessionID, Qt::UserRole, SessionID);
					pSubItem->addChild(pItem);
				}
			
				pItem->setText(ERouteSession::eEntityID, Session["EntityID"].toByteArray().toHex());
				pItem->setText(ERouteSession::eSessionID, SessionID.toHex());
				pItem->setText(ERouteSession::eStatus, Session["Status"].toString());
			}

			foreach(QTreeWidgetItem* pItem, OldSessions)
				delete pItem;
		}

		foreach(QTreeWidgetItem* pItem, OldRoutes)
			delete pItem;
	}

	foreach(QTreeWidgetItem* pItem, OldScripts)
		delete pItem;
}

QVariantMap	CKadScriptWidget::ResolveScope(QTreeWidgetItem* pItem)
{
	QVariantMap Scope;
	switch(pItem->data(eKey, Qt::UserRole).toInt())
	{
		case eScript:
			Scope["CodeID"] = pItem->data(EScript::eCodeID, Qt::UserRole);
			break;
		case eTaskList:
			Scope = ResolveScope(pItem->parent());
			break;
		case eTask:
			Scope = ResolveScope(pItem->parent());
			Scope["LookupID"] = pItem->data(ETask::eLookupID, Qt::UserRole);
			break;
		case eRoute:
			Scope = ResolveScope(pItem->parent());
			Scope["EntityID"] = pItem->data(ERoute::eEntityID, Qt::UserRole);
			break;
		case eRouteSession:
			Scope = ResolveScope(pItem->parent());
			Scope["SessionID"] = pItem->data(ERouteSession::eSessionID, Qt::UserRole);
			break;
	}
	return Scope;
}

void CKadScriptWidget::OnItemChanged(QTreeWidgetItem* pItem, QTreeWidgetItem*)
{
	if(!pItem)
		return;

	ELevels Level = (ELevels)pItem->data(eKey, Qt::UserRole).toInt();
	m_pScriptTree->setHeaderLabels(m_Headers[Level]);

	m_pKillScript->setEnabled(Level == eScript);
	m_pAddTask->setEnabled(Level == eScript || Level == eTaskList);
	m_pRemTask->setEnabled(Level == eTask);
	m_pDelScript->setEnabled(Level == eScript);

	if(pItem->data(eKey, Qt::UserRole) == eScript)
		m_pDebugger->openScript(pItem->text(EScript::eCodeID).toUpper());

	CKadActionJob* pKadActionJob = new CKadActionJob("SelectScope", ResolveScope(pItem));
	m_pDebugger->ScheduleJob(pKadActionJob);
}

void CKadScriptWidget::OnItemChanged(QTreeWidgetItem* pItem, int Column)
{
	if(Column != 0)
		return;

	Qt::CheckState State = pItem->checkState(0);

	CKadActionJob* pKadActionJob = new CKadActionJob(State == Qt::Unchecked ? "RemoveScope" : "AddScope", ResolveScope(pItem));
	m_pDebugger->ScheduleJob(pKadActionJob);
}

//void CKadScriptWidget::OnItemDoubleClicked(QTreeWidgetItem* pItem, int Column)
//{
//	QVariant CodeID = pItem->data(eKey, Qt::UserRole);
//	if(!CodeID.isValid())
//		return;
//
//	m_pDebugger->openScript(pItem->text(0).toUpper());
//}

void CKadScriptWidget::OnKillScript()
{
	QTreeWidgetItem* pItem = m_pScriptTree->currentItem();
	if(!pItem || pItem->data(eKey, Qt::UserRole).toInt() != eScript)
		return;

	CKadActionJob* pKadActionJob = new CKadActionJob("KillScript", ResolveScope(pItem));
	m_pDebugger->ScheduleJob(pKadActionJob);
}

void CKadScriptWidget::OnDelScript()
{
	QTreeWidgetItem* pItem = m_pScriptTree->currentItem();
	if(!pItem || pItem->data(eKey, Qt::UserRole).toInt() != eScript)
		return;
	
	if(QMessageBox("Remove Script", tr("Remove script %1 ?").arg(pItem->text(1)), QMessageBox::Question, QMessageBox::Yes, QMessageBox::No | QMessageBox::Default | QMessageBox::Escape, QMessageBox::NoButton).exec() != QMessageBox::Yes)
		return;

	CKadActionJob* pKadActionJob = new CKadActionJob("DeleteScript", ResolveScope(pItem));
	m_pDebugger->ScheduleJob(pKadActionJob);
}

void CKadScriptWidget::OnAddTask()
{
	QTreeWidgetItem* pItem = m_pScriptTree->currentItem();
	if(!pItem || (pItem->data(eKey, Qt::UserRole).toInt() != eScript && pItem->data(eKey, Qt::UserRole).toInt() != eTaskList))
		return;

	QString TargetID = QInputDialog::getText(this, tr("Target ID"),tr("Enter Target ID in big endian hex notation"));
	if(TargetID.isEmpty())
		return;

	CUInt128 ID;
	if(!ID.FromHex(TargetID.leftJustified(32,'0').toStdWString()))
		return;

	CKadActionJob* pKadActionJob = new CKadActionJob("AddTask", ResolveScope(pItem));
	pKadActionJob->Set("TargetID", CVariant(ID).ToQVariant());
	m_pDebugger->ScheduleJob(pKadActionJob);
}

void CKadScriptWidget::OnRemTask()
{
	QTreeWidgetItem* pItem = m_pScriptTree->currentItem();
	if(!pItem || pItem->data(eKey, Qt::UserRole).toInt() != eTask)
		return;

	CKadActionJob* pKadActionJob = new CKadActionJob("RemoveTask", ResolveScope(pItem));
	m_pDebugger->ScheduleJob(pKadActionJob);
}
