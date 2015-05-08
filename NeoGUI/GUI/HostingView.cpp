#include "GlobalHeader.h"
#include "HostingView.h"
#include "../NeoGUI.h"
#include "../Common/TreeWidgetEx.h"
#include "FileListView.h"
#include "TransfersView.h"
#include "ProgressBar.h"
#include "HosterUploader.h"
#include "../Common/Common.h"

CHostingView::CHostingView(QWidget *parent)
:QWidget(parent)
{
	m_ID = 0;
	m_Mode = 0;
	m_pHostingSyncJob = NULL;

	m_pMainLayout = new QVBoxLayout();

	m_pHosting = new QTreeWidgetEx();
	m_pHosting->setHeaderLabels(tr("Hoster|Status|Progress|Sharing").split("|"));
	m_pHosting->setContextMenuPolicy(Qt::CustomContextMenu);
	m_pHosting->setSelectionMode(QAbstractItemView::ExtendedSelection);

	connect(m_pHosting, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenuRequested(const QPoint &)));
	m_pMainLayout->addWidget(m_pHosting);

	m_pHostingWidget = new QWidget();
	m_pHostingLayout = new QFormLayout();
	m_pHostingLayout->setMargin(0);
	

	m_pMenu = new QMenu();
	
	m_pUpload = new QAction(tr("Upload to new Hoster(s)"), m_pMenu);
	connect(m_pUpload, SIGNAL(triggered()), this, SLOT(OnUpload()));
	m_pMenu->addAction(m_pUpload);

	m_pCheck = new QAction(tr("Check Link(s)"), m_pMenu);
	connect(m_pCheck, SIGNAL(triggered()), this, SLOT(OnCheck()));
	m_pMenu->addAction(m_pCheck);

	m_pReUpload = new QAction(tr("ReUpload dead Link(s)"), m_pMenu);
	connect(m_pReUpload, SIGNAL(triggered()), this, SLOT(OnReUpload()));
	m_pMenu->addAction(m_pReUpload);

	m_pMenu->addSeparator();

	m_pAddArch = new QAction(tr("Add split Archive"), m_pMenu);
	connect(m_pAddArch, SIGNAL(triggered()), this, SLOT(OnAddArch()));
	m_pMenu->addAction(m_pAddArch);

	m_pDelArch = new QAction(tr("Remove split Archive"), m_pMenu);
	connect(m_pDelArch, SIGNAL(triggered()), this, SLOT(OnDelArch()));
	m_pMenu->addAction(m_pDelArch);

	m_pMenu->addSeparator();

	m_pAddLink = new QAction(tr("Add Link(s)"), m_pMenu);
	connect(m_pAddLink, SIGNAL(triggered()), this, SLOT(OnAddLink()));
	m_pMenu->addAction(m_pAddLink);

	m_pDelLink = new QAction(tr("Remove Link(s)"), m_pMenu);
	connect(m_pDelLink, SIGNAL(triggered()), this, SLOT(OnDelLink()));
	m_pMenu->addAction(m_pDelLink);

	m_pMenu->addSeparator();

	m_pCopyUrl = new QAction(tr("Copy URL(s)"), m_pMenu);
	connect(m_pCopyUrl, SIGNAL(triggered()), this, SLOT(OnCopyUrl()));
	m_pMenu->addAction(m_pCopyUrl);

	m_pHostingWidget->setLayout(m_pHostingLayout);

	m_pMainLayout->addWidget(m_pHostingWidget);

	setLayout(m_pMainLayout);

	m_pHosting->header()->restoreState(theGUI->Cfg()->GetBlob("Gui/Widget_Hosting_Columns"));

	m_TimerId = startTimer(500);
}

CHostingView::~CHostingView()
{
	theGUI->Cfg()->SetBlob("Gui/Widget_Hosting_Columns",m_pHosting->header()->saveState());

	killTimer(m_TimerId);
}

void CHostingView::ShowHosting(uint64 ID)
{
	if(m_ID == ID)
		return;

	m_pHosting->clear();
	m_ProgressMap.clear();

	m_ID = ID;
	if(m_ID != 0)
		SyncHosting();
}

void CHostingView::timerEvent(QTimerEvent *e)
{
    if (e->timerId() != m_TimerId) 
	{
        QObject::timerEvent(e);
		return;
    }

	if(m_ID == 0)
		return;

	SyncHosting();

	uint64 uCurTick = GetCurTick();
	QMap<QTreeWidgetItem*, CProgressBar*> OldProgressMap = m_ProgressMap;
	for(QTreeWidgetItem* pItem = m_pHosting->itemAt(0,0); m_pHosting->viewport()->rect().intersects(m_pHosting->visualItemRect(pItem)); pItem = m_pHosting->itemBelow(pItem))
	{
		CProgressBar* pProgress = OldProgressMap.take(pItem);
		if(!pProgress)
		{
			switch(pItem->data(eStatus,Qt::UserRole).toInt())
			{
				case eGroupe:
					pProgress = new CProgressBar(m_ID, pItem->data(eName,Qt::UserRole).toString(), "", "");
					break;
				case eHoster:
					ASSERT(pItem->parent());
					if(pItem->parent())
						pProgress = new CProgressBar(m_ID, pItem->parent()->data(eName,Qt::UserRole).toString(), pItem->data(eName,Qt::UserRole).toString(), "");
					break;
				case eUser:
					ASSERT(pItem->parent());
					ASSERT(pItem->parent()->parent());
					if(pItem->parent() && pItem->parent()->parent())
						pProgress = new CProgressBar(m_ID, pItem->parent()->parent()->data(eName,Qt::UserRole).toString()
							, pItem->parent()->data(eName,Qt::UserRole).toString(), pItem->data(eName,Qt::UserRole).toString().isEmpty() ? "Pub": pItem->data(eName,Qt::UserRole).toString());
					break;
				case eLink:
					pProgress = new CProgressBar(m_ID, pItem->data(eName,Qt::UserRole).toULongLong());
					break;
				default: ASSERT(0);
			}
			ASSERT(pProgress);
			m_ProgressMap.insert(pItem, pProgress);
			m_pHosting->setItemWidget(pItem, eProgress, pProgress);
		}
		if(uCurTick > pProgress->GetNextUpdate())
			pProgress->Update();
	}
	foreach(QTreeWidgetItem* pItem, OldProgressMap.keys())
	{
		m_ProgressMap.remove(pItem);
		m_pHosting->setItemWidget(pItem, eProgress, NULL);
	}
}

class CHostingSyncJob: public CInterfaceJob
{
public:
	CHostingSyncJob(CHostingView* pView, uint64 ID)
	{
		m_pView = pView;
		m_Request["ID"] = ID;
	}

	virtual QString			GetCommand()	{return "GetHosting";}
	virtual void			HandleResponse(const QVariantMap& Response) 
	{
		if(m_pView)
			m_pView->SyncHosting(Response);
	}
	virtual void			Finish(bool bOK)
	{
		if(m_pView)
			m_pView->m_pHostingSyncJob = NULL;
		CInterfaceJob::Finish(bOK);
	}

protected:
	QPointer<CHostingView>	m_pView; // Note: this can be deleted at any time
};

void CHostingView::SyncHosting()
{
	if(m_pHostingSyncJob == NULL)
	{
		m_pHostingSyncJob = new CHostingSyncJob(this, m_ID);
		theGUI->ScheduleJob(m_pHostingSyncJob);
	}
}

void CHostingView::DeleteRecursivly(QTreeWidgetItem* pCurItem)
{
	m_ProgressMap.remove(pCurItem);
	while(pCurItem->childCount() > 0)
		DeleteRecursivly(pCurItem->child(0));
	delete pCurItem;
}

void CHostingView::SyncHosting(const QVariantMap& Response)
{
	QMap<QString, QTreeWidgetItem*> OldGroups;
	for(int i = 0; i < m_pHosting->topLevelItemCount(); ++i) 
	{
		QTreeWidgetItem* pItem = m_pHosting->topLevelItem(i);
		QString ID = pItem->data(0, Qt::UserRole).toString();
		ASSERT(!OldGroups.contains(ID));
		OldGroups.insert(ID,pItem);
	}

	foreach (const QVariant vGroupe, Response["Groups"].toList())
	{
		QVariantMap Groupe = vGroupe.toMap();
		QString ID = Groupe["ID"].toString();

		QTreeWidgetItem* pItem = OldGroups.take(ID);
		if(!pItem)
		{
			pItem = new QTreeWidgetItem();
			pItem->setText(eName, ID.isEmpty() ? tr("P2P compatible Neo Parts") : (tr("Archive Set: ") + ID));
			pItem->setData(eName, Qt::UserRole, ID);
			pItem->setData(eStatus, Qt::UserRole, eGroupe);
			m_pHosting->addTopLevelItem(pItem);
			pItem->setExpanded(true);
		}

		pItem->setText(eStatus, Groupe["Status"].toString());
		pItem->setText(eSharing, QString("%1/%2").arg(QString::number(Groupe["MyShare"].toDouble(), 'g', 2)).arg(QString::number(Groupe["Share"].toDouble(), 'g', 2)));

		SyncHosters(Groupe["Hosters"].toList(), pItem);
	}

	foreach(QTreeWidgetItem* pCurItem, OldGroups)
		DeleteRecursivly(pCurItem);
}

void CHostingView::SyncHosters(const QVariantList& Hosters, QTreeWidgetItem* pRoot)
{
	QMap<QString, QTreeWidgetItem*> OldHosters;
	for(int i = 0; i < pRoot->childCount(); ++i)
	{
		QTreeWidgetItem* pItem = pRoot->child(i);
		QString HostName = pItem->data(0, Qt::UserRole).toString();
		ASSERT(!OldHosters.contains(HostName));
		OldHosters.insert(HostName,pItem);
	}

	foreach (const QVariant vHoster, Hosters)
	{
		QVariantMap Hoster = vHoster.toMap();
		QString HostName = Hoster["HostName"].toString();

		QTreeWidgetItem* pItem = OldHosters.take(HostName);
		if(!pItem)
		{
			pItem = new QTreeWidgetItem();
			pItem->setText(eName, HostName);
			pItem->setData(eName, Qt::UserRole, HostName);
			pItem->setData(eStatus, Qt::UserRole, eHoster);
			pRoot->addChild(pItem);
		}
		if(pItem->icon(eName).isNull())
			pItem->setIcon(eName, theGUI->GetHosterIcon(HostName, false));

		pItem->setText(eStatus, Hoster["Status"].toString());
		pItem->setText(eSharing, QString("%1/%2").arg(QString::number(Hoster["MyShare"].toDouble(), 'g', 2)).arg(QString::number(Hoster["Share"].toDouble(), 'g', 2)));


		SyncUsers(Hoster["Users"].toList(), pItem);
	}

	foreach(QTreeWidgetItem* pCurItem, OldHosters)
		DeleteRecursivly(pCurItem);
}

void CHostingView::SyncUsers(const QVariantList& Users, QTreeWidgetItem* pItem)
{
	QMap<QString, QTreeWidgetItem*> OldUsers;
	for(int i = 0; i < pItem->childCount(); ++i)
	{
		QTreeWidgetItem* pSubItem = pItem->child(i);
		QString UserName = pSubItem->data(eName, Qt::UserRole).toString();
		Q_ASSERT(!OldUsers.contains(UserName));
		OldUsers.insert(UserName,pSubItem);
	}	

	foreach(const QVariant& vUser, Users)
	{
		QVariantMap User = vUser.toMap();
		QString UserName = User["UserName"].toString();

		QTreeWidgetItem* pSubItem = OldUsers.take(UserName);
		if(!pSubItem)
		{
			pSubItem = new QTreeWidgetItem();
			pSubItem->setText(eName, UserName.isEmpty() ? tr("Public") : UserName);
			pSubItem->setData(eName, Qt::UserRole, UserName);
			pSubItem->setData(eStatus, Qt::UserRole, eUser);
			pItem->addChild(pSubItem);
			pItem->setExpanded(true);
		}
			
		pSubItem->setText(eStatus, User["Status"].toString());
		pSubItem->setText(eSharing, QString("%1").arg(QString::number(User["Share"].toDouble(), 'g', 2)));


		SyncLinks(User["Links"].toList(), pSubItem);
	}

	foreach(QTreeWidgetItem* pCurItem, OldUsers)
		DeleteRecursivly(pCurItem);
}

void CHostingView::SyncLinks(const QVariantList& Links, QTreeWidgetItem* pItem)
{
	QMap<uint64, QTreeWidgetItem*> OldLinks;
	for(int i = 0; i < pItem->childCount(); ++i)
	{
		QTreeWidgetItem* pSubItem = pItem->child(i);
		uint64 ID = pSubItem->data(eName, Qt::UserRole).toULongLong();
		Q_ASSERT(!OldLinks.contains(ID));
		OldLinks.insert(ID,pSubItem);
	}	

	foreach(const QVariant& vLink, Links)
	{
		QVariantMap Link = vLink.toMap();
		uint64 ID = Link["ID"].toULongLong();

		QTreeWidgetItem* pSubItem = OldLinks.take(ID);
		if(!pSubItem)
		{
			pSubItem = new QTreeWidgetItem();
			pSubItem->setData(eName, Qt::UserRole, ID);
			pSubItem->setData(eStatus, Qt::UserRole, eLink);
			pItem->addChild(pSubItem);
		}
			
		pSubItem->setText(eName, tr("%1 (%2)").arg(Link["FileName"].toString()).arg(FormatSize(Link["FileSize"].toULongLong())));
		pSubItem->setToolTip(eName, Link["Url"].toString());
		
		QString Status = Link["TransferStatus"].toString();
		QString Info = Link["TransferInfo"].toString();
		if(!Info.isEmpty())
			Status = Info;
		Status += " " + Link["UploadState"].toString() + "/" + Link["DownloadState"].toString();
		pSubItem->setText(eStatus, Status);

		pSubItem->setText(eSharing, Link["Share"].toString());
	}

	foreach(QTreeWidgetItem* pCurItem, OldLinks)
		DeleteRecursivly(pCurItem);
}



void CHostingView::OnMenuRequested(const QPoint &point)
{
	if(!(m_Mode == CFileListView::eDownload || CFileListView::eSharedFiles))
		return;

	QTreeWidgetItem* pItem = m_pHosting->currentItem();
	int Type = pItem ? pItem->data(eStatus, Qt::UserRole).toInt() : -1;

	m_pUpload->setEnabled(Type == eGroupe);
	//m_pCheck->setEnabled
	//m_pReUpload->setEnabled
	m_pAddArch->setEnabled(m_Mode == CFileListView::eSharedFiles);
	m_pDelArch->setEnabled(Type == eGroupe && !pItem->data(eName, Qt::UserRole).toString().isEmpty());
	//m_pAddLink->setEnabled
	m_pDelLink->setEnabled(Type != -1);
	m_pCopyUrl->setEnabled(Type != -1);

	m_pMenu->popup(QCursor::pos());	
}

QSet<QTreeWidgetItem*> CHostingView::GetLinks()
{
	QSet<QTreeWidgetItem*> List;
	foreach(QTreeWidgetItem* pItem, m_pHosting->selectedItems())
		List.unite(GetLinks(pItem));
	return List;
}

QSet<QTreeWidgetItem*> CHostingView::GetLinks(QTreeWidgetItem* pItem)
{
	QSet<QTreeWidgetItem*> List;
	if(pItem->data(eStatus, Qt::UserRole) == eLink)
		List.insert(pItem);
	else for(int i=0; i < pItem->childCount(); i++)
		List.unite(GetLinks(pItem->child(i)));
	return List;
}

void CHostingView::OnUpload()
{
	CHosterUploader HosterUploader(false);
	if(!HosterUploader.exec())
		return;

	QStringList Hosters = HosterUploader.GetHosters();
	if(Hosters.isEmpty())
	{
		QMessageBox::warning(NULL, tr("Hoster Upload"), tr("No Hosters Selected"));
		return;
	}

	foreach(QTreeWidgetItem* pItem, m_pHosting->selectedItems())
	{
		if(pItem->data(eStatus, Qt::UserRole).toInt() != eGroupe)
			continue;

		QString Groupe = pItem->data(eName, Qt::UserRole).toString();
		if(Groupe.isEmpty())
		{
			QVariantMap Options;
			Options["Hosters"] = Hosters;
			CFileListView::DoAction("UploadParts", m_ID, Options); 
		}
		else
		{
			QVariantMap Options;
			Options["Hosters"] = Hosters;
			Options["ArchiveID"] = Groupe;
			CFileListView::DoAction("UploadArchive", m_ID, Options); 
		}
	}
}

void CHostingView::OnCheck()
{
	if(m_pHosting->selectedItems().isEmpty())
		CFileListView::DoAction("CheckLinks", m_ID); 
	else
	{
		foreach(QTreeWidgetItem* pItem, GetLinks())
		{
			uint64 ID = pItem->data(eName, Qt::UserRole).toULongLong();
			CTransfersView::OnAction("CheckLink", m_ID, ID);
		}
	}
}

void CHostingView::OnReUpload()
{
	if(m_pHosting->selectedItems().isEmpty())
		CFileListView::DoAction("ReUpload", m_ID); 
	else
	{
		foreach(QTreeWidgetItem* pItem, m_pHosting->selectedItems())
		{
			switch(pItem->data(eStatus, Qt::UserRole).toInt())
			{
				case eGroupe:
				{
					QString Groupe = pItem->data(eName, Qt::UserRole).toString();
					if(Groupe.isEmpty())
						continue;
		
					QVariantMap Options;
					Options["ArchiveID"] = Groupe;
					CFileListView::DoAction("ReUpload", m_ID, Options); 
					break;
				}
				case eLink:
				{
					uint64 ID = pItem->data(eName, Qt::UserRole).toULongLong();
					CTransfersView::OnAction("ReUpload", m_ID, ID);
					break;
				}
			}
		}
	}
}

void CHostingView::OnAddArch()
{
	QVariantMap Options;
	Options["PartSize"] = 0;
	Options["Format"] = "";
	CFileListView::DoAction("CreateArchive", m_ID, Options); 
}

void CHostingView::OnDelArch()
{
	foreach(QTreeWidgetItem* pItem, m_pHosting->selectedItems())
	{
		if(pItem->data(eStatus, Qt::UserRole).toInt() != eGroupe)
			continue;

		QString Groupe = pItem->data(eName, Qt::UserRole).toString();
		if(Groupe.isEmpty())
			continue;
		
		QVariantMap Options;
		Options["ArchiveID"] = Groupe;
		CFileListView::DoAction("RemoveArchive", m_ID, Options); 
	}
}

void CHostingView::OnAddLink()
{
	CFileListView::AddSource(m_ID);
}

void CHostingView::OnDelLink()
{
	foreach(QTreeWidgetItem* pItem, GetLinks())
	{
		uint64 ID = pItem->data(eName, Qt::UserRole).toULongLong();
		CTransfersView::OnAction("RemoveTransfer", m_ID, ID);
	}
}

void CHostingView::OnCopyUrl() 
{
	QString URLs;
	foreach(QTreeWidgetItem* pItem, GetLinks())
	{
		if(!URLs.isEmpty())
			URLs += "\r\n";
		URLs += pItem->toolTip(eName);
	}
	QApplication::clipboard()->setText(URLs);
}