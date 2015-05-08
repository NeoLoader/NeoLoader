#include "GlobalHeader.h"
#include "TrackerView.h"
#include "../NeoGUI.h"
#include "../Common/TreeWidgetEx.h"
#include "FileListView.h"
#include "../Common/Common.h"
#include "../Common/Dialog.h"

CTrackerView::CTrackerView(QWidget *parent)
:QWidget(parent)
{
	m_pMainLayout = new QVBoxLayout();

	m_pTrackerWidget = new QWidget();
	m_pTrackerLayout = new QFormLayout();
	m_pTrackerLayout->setMargin(0);

	m_pTrackers = new QTreeWidgetEx();
	m_pTrackers->setHeaderLabels(tr("URL|Type|Status|Next Request").split("|"));
	m_pTrackers->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pTrackers, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenuRequested(const QPoint &)));
	m_pTrackerLayout->setWidget(0, QFormLayout::SpanningRole, m_pTrackers);
	
	m_pMenu = new QMenu();

	m_pAdd = new QAction(tr("Add"), m_pMenu);
	connect(m_pAdd, SIGNAL(triggered()), this, SLOT(OnAdd()));
	m_pMenu->addAction(m_pAdd);

	m_pCopyUrl = new QAction(tr("Copy Url"), m_pMenu);
	connect(m_pCopyUrl, SIGNAL(triggered()), this, SLOT(OnCopyUrl()));
	m_pMenu->addAction(m_pCopyUrl);

	m_pRemove = new QAction(tr("Remove"), m_pMenu);
	connect(m_pRemove, SIGNAL(triggered()), this, SLOT(OnRemove()));
	m_pMenu->addAction(m_pRemove);

	m_pAnnounce = new QToolButton();
	m_pAnnounce->setText(tr("Announce"));
	connect(m_pAnnounce, SIGNAL(pressed()), this, SLOT(OnAnnounce()));
	m_pTrackerLayout->setWidget(1, QFormLayout::LabelRole, m_pAnnounce);

	QMenu* pMenu = new QMenu(m_pAnnounce);
	pMenu->addAction(tr("Announce"), this, SLOT(OnAnnounce()));
	pMenu->addSeparator();
	pMenu->addAction(tr("Republish"), this, SLOT(OnRepublish()));

	m_pAnnounce->setPopupMode(QToolButton::MenuButtonPopup);
	m_pAnnounce->setMenu(pMenu);

	/*QWidget* pButtons = new QWidget();
	QHBoxLayout* pButtonsLayout = new QHBoxLayout(pButtons);
	pButtonsLayout->setMargin(0);

	m_pAnnounce = new QPushButton(tr("Announce"));
	connect(m_pAnnounce, SIGNAL(pressed()), this, SLOT(OnAnnounce()));
	pButtonsLayout->addWidget(m_pAnnounce);

	m_pRepublish = new QPushButton(tr("Announce"));
	connect(m_pRepublish, SIGNAL(pressed()), this, SLOT(OnRepublish()));
	pButtonsLayout->addWidget(m_pRepublish);

	m_pTrackerLayout->setWidget(1, QFormLayout::LabelRole, pButtons);*/

	m_pTrackerWidget->setLayout(m_pTrackerLayout);

	m_pMainLayout->addWidget(m_pTrackerWidget);

	setLayout(m_pMainLayout);

	m_pTrackers->header()->restoreState(theGUI->Cfg()->GetBlob("Gui/Widget_Trackers_Columns"));

	m_TimerId = startTimer(500);
}

CTrackerView::~CTrackerView()
{
	theGUI->Cfg()->SetBlob("Gui/Widget_Trackers_Columns",m_pTrackers->header()->saveState());

	killTimer(m_TimerId);
}

void CTrackerView::ShowTracker(uint64 ID)
{
	m_ID = ID;
	m_pTrackers->clear();
	GetTrackers();
}

class CGetTrackersJob: public CInterfaceJob
{
public:
	CGetTrackersJob(CTrackerView* pView, uint64 ID)
	{
		m_pView = pView;
		m_Request["ID"] = ID;
		//m_Request["Section"] = "Tracker";
	}

	virtual QString			GetCommand()	{return "GetFile";}
	virtual void			HandleResponse(const QVariantMap& Response) 
	{
		if(m_pView)
		{
			QMap<QString, QTreeWidgetItem*> OldTrackers;
			for(int i = 0; i < m_pView->m_pTrackers->topLevelItemCount(); ++i) 
			{
				QTreeWidgetItem* pItem = m_pView->m_pTrackers->topLevelItem(i);
				QString Url = pItem->text(0);
				ASSERT(!OldTrackers.contains(Url));
				OldTrackers.insert(Url,pItem);
			}

			QVariantList Trackers = Response["Trackers"].toList();
			foreach(const QVariant& vTracker, Trackers)
			{
				QVariantMap Tracker = vTracker.toMap();
				QString Url = Tracker["Url"].toString();

				QTreeWidgetItem* pItem = OldTrackers.take(Url);
				if(!pItem)
				{
					pItem = new QTreeWidgetItem();
					pItem->setText(CTrackerView::eUrl, Url);
				}
				
				QString Type = Tracker["Type"].toString();
				if(Type == "Tracker")
				{
					int Tier = Tracker["Tier"].toInt();
					pItem->setText(CTrackerView::eType, tr("%1, Tier %2").arg(Type).arg(Tier));
					pItem->setData(CTrackerView::eType, Qt::UserRole, Tier);
				}
				else if(Type == "Server")
				{
					int Tier = Tracker["Tier"].toInt();
					pItem->setText(CTrackerView::eType, Type);
					pItem->setData(CTrackerView::eType, Qt::UserRole, Tier);
				}
				else
				{
					pItem->setText(CTrackerView::eType, Type);
					pItem->setData(CTrackerView::eType, Qt::UserRole, -1);
				}
				pItem->setText(CTrackerView::eStatus, Tracker["Status"].toString());
				pItem->setText(CTrackerView::eNext, FormatTime(Tracker["Next"].toULongLong()/1000));
				m_pView->m_pTrackers->addTopLevelItem(pItem);
			}

			foreach(QTreeWidgetItem* pItem, OldTrackers)
			{
				if(pItem->text(CTrackerView::eStatus) == "Added")
					continue;
				delete pItem;
			}
		}
	}
protected:
	QPointer<CTrackerView>	m_pView; // Note: this can be deleted at any time
};

void CTrackerView::GetTrackers()
{
	CGetTrackersJob* pGetTrackersJob = new CGetTrackersJob(this, m_ID);
	theGUI->ScheduleJob(pGetTrackersJob);
}

void CTrackerView::timerEvent(QTimerEvent *e)
{
    if (e->timerId() != m_TimerId) 
	{
        QObject::timerEvent(e);
		return;
    }

	if(m_ID != 0)
		GetTrackers();
}

class CSetTrackersJob: public CInterfaceJob
{
public:
	CSetTrackersJob(CTrackerView* pView, uint64 ID)
	{
		m_Request["ID"] = ID;
		//m_Request["Section"] = "Tracker";

		QVariantList Trackers;
		for(int i=0; i < pView->m_pTrackers->topLevelItemCount(); i++)
		{
			QTreeWidgetItem* pItem = pView->m_pTrackers->topLevelItem(i);
			int Tier = pItem->data(CTrackerView::eType, Qt::UserRole).toInt();
			if(Tier == -1)
				continue;

			QVariantMap Tracker;
			Tracker["Url"] = pItem->text(CTrackerView::eUrl);
			Tracker["Tier"] = Tier;
			Trackers.append(Tracker);
		}
		m_Request["Trackers"] = Trackers;
	}

	virtual QString			GetCommand()	{return "SetFile";}
	virtual void			HandleResponse(const QVariantMap& Response) {}
};

void CTrackerView::SetTrackers()
{
	CSetTrackersJob* pSetTrackersJob = new CSetTrackersJob(this, m_ID);
	theGUI->ScheduleJob(pSetTrackersJob);

	GetTrackers();
}

void CTrackerView::OnMenuRequested(const QPoint &point)
{
	QTreeWidgetItem* pItem = m_pTrackers->currentItem();
	int Tier = pItem ? pItem->data(CTrackerView::eType, Qt::UserRole).toInt() : -1;

	m_pRemove->setEnabled(Tier != -1);

	m_pMenu->popup(QCursor::pos());	
}

class CTrackerEdit : public QDialogEx
{
	//Q_OBJECT

public:
	CTrackerEdit(const QString& Url, int Tier, QWidget *pMainWindow = NULL)
		: QDialogEx(pMainWindow)
	{
		setWindowTitle(CTrackerView::tr("Tracker"));

		m_pMainLayout = new QFormLayout(this);

		m_pUrl = new QLineEdit();
		m_pUrl->setText(Url);
		m_pUrl->setMaximumWidth(300);
		m_pMainLayout->setWidget(0, QFormLayout::LabelRole, new QLabel(CTrackerView::tr("URL:")));
		m_pMainLayout->setWidget(0, QFormLayout::FieldRole, m_pUrl);

		m_pTier = new QLineEdit();
		m_pTier->setText(QString::number(Tier));
		m_pTier->setMaximumWidth(50);
		m_pMainLayout->setWidget(1, QFormLayout::LabelRole, new QLabel(CTrackerView::tr("Tier:")));
		m_pMainLayout->setWidget(1, QFormLayout::FieldRole, m_pTier);

		m_pButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
		QObject::connect(m_pButtonBox, SIGNAL(accepted()), this, SLOT(accept()));
		QObject::connect(m_pButtonBox, SIGNAL(rejected()), this, SLOT(reject()));
		m_pMainLayout->setWidget(2, QFormLayout::FieldRole, m_pButtonBox);
	}

	QString				GetUrl()	{return m_pUrl->text();}
	int					GetTier()	{return m_pTier->text().toInt();}

protected:
	QLineEdit*			m_pUrl;
	QLineEdit*			m_pTier;
	QDialogButtonBox*	m_pButtonBox;
	QFormLayout*		m_pMainLayout;
};

void CTrackerView::OnAdd()
{
	CTrackerEdit TrackerEdit("", 0);
	if(!TrackerEdit.exec())
		return;

	QTreeWidgetItem* pItem = new QTreeWidgetItem();
	pItem->setText(CTrackerView::eUrl, TrackerEdit.GetUrl());
	int Tier = TrackerEdit.GetTier();
	pItem->setText(CTrackerView::eType, tr("Tracker, Tier %1").arg(Tier));
	pItem->setData(CTrackerView::eType, Qt::UserRole, Tier);
	pItem->setText(CTrackerView::eStatus, "");
	m_pTrackers->addTopLevelItem(pItem);

	SetTrackers();
}

void CTrackerView::OnCopyUrl()
{
	QTreeWidgetItem* pItem = m_pTrackers->currentItem();
	if(!pItem)
		return;

	QApplication::clipboard()->setText(pItem->text(CTrackerView::eUrl));
}

void CTrackerView::OnRemove()
{
	QTreeWidgetItem* pItem = m_pTrackers->currentItem();
	delete pItem;

	SetTrackers();
}

void CTrackerView::OnAnnounce()
{
	CFileListView::DoAction("Announce", m_ID);
}

void CTrackerView::OnRepublish()
{
	CFileListView::DoAction("Republish", m_ID);
}