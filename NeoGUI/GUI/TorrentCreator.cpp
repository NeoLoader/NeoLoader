#include "GlobalHeader.h"
#include "TorrentCreator.h"
#include "../NeoGUI.h"

class CGetDescrJob: public CInterfaceJob
{
public:
	CGetDescrJob(uint64 ID, CTorrentCreator* pView)
	{
		m_pView = pView;
		m_Request["ID"] = ID;
	}
	virtual QString			GetCommand()	{return "GetFile";}
	virtual void			HandleResponse(const QVariantMap& Response)
	{
		if(m_pView)
			m_pView->SetComment(Response["Properties"].toMap()["Description"].toString());
	}

protected:
	QPointer<CTorrentCreator>m_pView; // Note: this can be deleted at any time
};

CTorrentCreator::CTorrentCreator(uint64 ID, QWidget *parent)
	: QDialogEx(parent)
{
	m_ID = ID;

	setWindowTitle(tr("Torrent Creator"));
	resize(400, 300);
	setMinimumSize(QSize(400, 300));
	setMaximumSize(QSize(400, 300));

	QGridLayout* pMainLayout = new QGridLayout(this);
	pMainLayout->setSpacing(0);
	pMainLayout->setMargin(0);

	m_pCreatorTabs= new QTabWidget(this);
    
	// Details

	m_pDetailsWidget = new QWidget();
	QGridLayout* pDetailsLayout = new QGridLayout(m_pDetailsWidget);
	pDetailsLayout->setSpacing(4);
	pDetailsLayout->setMargin(4);

	QGridLayout* pCreatorLayout = new QGridLayout();
	pCreatorLayout->addWidget(new QLabel(tr("Creator")), 0, 0, 1, 1);
	m_pCreator = new QLineEdit(m_pDetailsWidget);
	m_pCreator->setText(tr("NeoLoader - NeoLoader.com"));
	pCreatorLayout->addWidget(m_pCreator, 0, 1, 1, 1);
	pDetailsLayout->addLayout(pCreatorLayout, 0, 0, 1, 1);

	pDetailsLayout->addWidget(new QLabel(tr("Comment")), 1, 0, 1, 1);
	m_pComment = new QPlainTextEdit(m_pDetailsWidget);
	pDetailsLayout->addWidget(m_pComment, 2, 0, 1, 1);

	QGridLayout* pOptionsLayout = new QGridLayout();
	pOptionsLayout->addWidget(new QLabel(tr("Torrent Name:")), 0, 0, 1, 1);
	m_pName = new QLineEdit(m_pDetailsWidget);
	//m_pName->setMinimumWidth(250);
	pOptionsLayout->addWidget(m_pName, 0, 1, 1, 1);

	pOptionsLayout->addWidget(new QLabel(tr("Piece Size:")), 1, 0, 1, 1);
	m_pPieceSize = new QComboBox(m_pDetailsWidget);
	m_pPieceSize->setMaximumWidth(64);
	m_pPieceSize->addItem("Auto", 0);
	m_pPieceSize->addItem("64 MB", MB2B(64));
	m_pPieceSize->addItem("32 MB", MB2B(32));
	m_pPieceSize->addItem("16 MB", MB2B(16));
	m_pPieceSize->addItem("8 MB", MB2B(8));
	m_pPieceSize->addItem("4 MB", MB2B(4));
	m_pPieceSize->addItem("2 MB", MB2B(2));
	m_pPieceSize->addItem("1 MB", MB2B(1));
	m_pPieceSize->addItem("512 KB", KB2B(512));
	m_pPieceSize->addItem("256 KB", KB2B(256));
	m_pPieceSize->addItem("128 KB", KB2B(128));
	pOptionsLayout->addWidget(m_pPieceSize, 1, 1, 1, 1);
	pDetailsLayout->addLayout(pOptionsLayout, 3, 0, 1, 1);

	m_pMerkle = new QCheckBox(tr("Merkle Torrent"), m_pDetailsWidget);
	pDetailsLayout->addWidget(m_pMerkle, 4, 0, 1, 1);
	m_pPrivate = new QCheckBox(tr("Private Torrent"), m_pDetailsWidget);
	pDetailsLayout->addWidget(m_pPrivate, 5, 0, 1, 1);

	m_pCreatorTabs->addTab(m_pDetailsWidget, tr("Details"));

	//

	// Trackers

	m_pTrackersWidget = new QWidget();
	QGridLayout* pTrackersLayout = new QGridLayout(m_pTrackersWidget);
	pTrackersLayout->setSpacing(2);
	pTrackersLayout->setContentsMargins(4, 4, 4, 4);

	QGridLayout* pTrackerEntrylLayout = new QGridLayout();
	pTrackerEntrylLayout->addWidget(new QLabel(tr("Enter Tracker Url's for the Torrent")), 0, 0, 1, 3);
	pTrackerEntrylLayout->addWidget(new QLabel(tr("Url")), 1, 0, 1, 1);

	m_pTackerURL = new QLineEdit(m_pTrackersWidget);
	pTrackerEntrylLayout->addWidget(m_pTackerURL, 1, 1, 1, 1);
	m_pTackerTier = new QSpinBox(m_pTrackersWidget);
	m_pTackerTier->setPrefix(tr("Tier ", 0));
	QSizePolicy sizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
	sizePolicy.setHorizontalStretch(0);
	sizePolicy.setVerticalStretch(0);
	sizePolicy.setHeightForWidth(m_pTackerTier->sizePolicy().hasHeightForWidth());
	m_pTackerTier->setSizePolicy(sizePolicy);
	m_pTackerTier->setMinimumSize(QSize(60, 0));
	m_pTackerTier->setMaximum(5);
	m_pTackerTier->setValue(1);
	pTrackerEntrylLayout->addWidget(m_pTackerTier, 1, 2, 1, 1);
	pTrackersLayout->addLayout(pTrackerEntrylLayout, 0, 0, 1, 1);

	QGridLayout* pTrackerControllLayout = new QGridLayout();
	m_pAddTracker = new QPushButton(tr("Add"), m_pTrackersWidget);
	connect(m_pAddTracker, SIGNAL(pressed()), this, SLOT(OnAddTracker()));
	pTrackerControllLayout->addWidget(m_pAddTracker, 0, 0, 1, 1);
	m_pRemoveTracker = new QPushButton(tr("Remove"), m_pTrackersWidget);
	connect(m_pRemoveTracker, SIGNAL(pressed()), this, SLOT(OnRemoveTracker()));
	pTrackerControllLayout->addWidget(m_pRemoveTracker, 0, 1, 1, 1);
	pTrackerControllLayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum), 0, 2, 1, 1);
	pTrackersLayout->addLayout(pTrackerControllLayout, 1, 0, 1, 1);

	m_pTrackerList = new QTreeWidget(m_pTrackersWidget);
	m_pTrackerList->setRootIsDecorated(false);
	m_pTrackerList->setItemsExpandable(false);
	m_pTrackerList->setAllColumnsShowFocus(true);
	m_pTrackerList->setExpandsOnDoubleClick(false);
	m_pTrackerList->setHeaderLabels(QString("Tracker|Tier").split("|"));
	pTrackersLayout->addWidget(m_pTrackerList, 3, 0, 1, 1);

	m_pCreatorTabs->addTab(m_pTrackersWidget, tr("Trackers"));
        
	//
		
	// Bootstrap
	/*
	m_pBootstrapWidget = new QWidget();
    QGridLayout* pBootstrapLayout = new QGridLayout(m_pBootstrapWidget);
    pBootstrapLayout->setSpacing(2);
    pBootstrapLayout->setContentsMargins(4, 4, 4, 4);
	pBootstrapLayout->addWidget(new QLabel(tr("Enter WebSeed Url's and DHT Nodes")), 0, 0, 1, 2);

    QGridLayout* pEntryLayout = new QGridLayout();
    pEntryLayout->addWidget(new QLabel(tr("Url")), 0, 0, 1, 1);
    m_pBootstrapURL = new QLineEdit(m_pBootstrapWidget);
    pEntryLayout->addWidget(m_pBootstrapURL, 0, 1, 1, 1);
	m_pBootstrapType = new QComboBox(m_pBootstrapWidget);
	m_pBootstrapType->addItem(tr("DHT Node"), 0);
	m_pBootstrapType->addItem(tr("Web Seed"), 1);
	pEntryLayout->addWidget(m_pBootstrapType, 0, 3, 1, 1);
	pBootstrapLayout->addLayout(pEntryLayout, 1, 0, 1, 1);

	QGridLayout* pControllsLayout = new QGridLayout();
	m_pBootstrapAdd = new QPushButton(tr("Add"), m_pBootstrapWidget);
	connect(m_pBootstrapAdd, SIGNAL(pressed()), this, SLOT(OnBootstrapAdd()));
	pControllsLayout->addWidget(m_pBootstrapAdd, 0, 0, 1, 1);
	m_pBootstrapRemove = new QPushButton(tr("Remove"), m_pBootstrapWidget);
	connect(m_pBootstrapRemove, SIGNAL(pressed()), this, SLOT(OnBootstrapRemove()));
	pControllsLayout->addWidget(m_pBootstrapRemove, 0, 1, 1, 1);
	pControllsLayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum), 0, 2, 1, 1);
	pBootstrapLayout->addLayout(pControllsLayout, 2, 0, 1, 2);

	m_pBootstrapList = new QTreeWidget(m_pBootstrapWidget);
	m_pBootstrapList->setRootIsDecorated(false);
	m_pBootstrapList->setItemsExpandable(false);
	m_pBootstrapList->setAllColumnsShowFocus(true);
	m_pBootstrapList->setExpandsOnDoubleClick(false);
	m_pBootstrapList->setHeaderLabels(QString("Url|Type").split("|"));
	pBootstrapLayout->addWidget(m_pBootstrapList, 3, 0, 1, 2);

    m_pCreatorTabs->addTab(m_pBootstrapWidget, tr("Bootstrap"));
	*/
	//

	pMainLayout->addWidget(m_pCreatorTabs, 0, 0, 1, 1);

	m_pButtonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, Qt::Horizontal, this);
	QObject::connect(m_pButtonBox, SIGNAL(accepted()), this, SLOT(OnCreateTorrent()));
	QObject::connect(m_pButtonBox, SIGNAL(accepted()), this, SLOT(accept()));
	QObject::connect(m_pButtonBox, SIGNAL(rejected()), this, SLOT(reject()));
	pMainLayout->addWidget(m_pButtonBox, 1, 0, 1, 1);
}

void CTorrentCreator::OnAddTracker()
{
	QString Url = m_pTackerURL->text();
	int iTier = m_pTackerTier->value();
	QTreeWidgetItem* pItem = new QTreeWidgetItem();
	pItem->setText(0, Url);
	pItem->setText(1, QString::number(iTier));
	pItem->setData(1, Qt::UserRole, iTier);
	m_pTrackerList->addTopLevelItem(pItem);
}

void CTorrentCreator::OnRemoveTracker()
{
	delete m_pTrackerList->currentItem();
}

/*void CTorrentCreator::OnBootstrapAdd()
{
	QString Url = m_pBootstrapURL->text();
	int iType = m_pBootstrapType->itemData(m_pBootstrapType->currentIndex()).toInt();
	QTreeWidgetItem* pItem = new QTreeWidgetItem();
	pItem->setText(0, Url);
	pItem->setText(1, iType ? tr("Web Seed") : tr("DHT Node"));
	pItem->setData(1, Qt::UserRole, iType);
	m_pTrackerList->addTopLevelItem(pItem);
}

void CTorrentCreator::OnBootstrapRemove()
{
	delete m_pBootstrapList->currentItem();
}*/

class CMakeTorrentJob: public CInterfaceJob
{
public:
	CMakeTorrentJob(uint64 ID, int iPieceSize, bool bMerkle, const QString& Name, bool bPrivate)
	{
		m_Request["ID"] = ID;
		m_Request["Action"] = "MakeTorrent";
		m_Request["TorrentName"] = Name;
		m_Request["PieceLength"] = iPieceSize;
		m_Request["MerkleTorrent"] = bMerkle;
		m_Request["PrivateTorrent"] = bPrivate;
		m_Request["Log"] = true;
	}

	void					SetCreator(const QString& Creator)			{m_Request["Creator"] = Creator;}
	void					SetComment(const QString& Comment)			{m_Request["Description"] = Comment;}

	void					SetTrackers(const QVariantList& Trackers)	{m_Request["Trackers"] = Trackers;}
	/*void					SetDHTNodes(const QVariantList& DHTNodes)	{m_Request["DHTNodes"] = DHTNodes;}
	void					SetWebSeeds(const QVariantList& WebSeeds)	{m_Request["WebSeeds"] = WebSeeds;}*/

	virtual QString			GetCommand()	{return "FileAction";}
	virtual void			HandleResponse(const QVariantMap& Response) {}
};

void CTorrentCreator::OnCreateTorrent()
{
	CMakeTorrentJob* pMakeTorrentJob = new CMakeTorrentJob(m_ID, m_pPieceSize->itemData(m_pPieceSize->currentIndex()).toInt(), m_pMerkle->isChecked(), m_pName->text(), m_pPrivate->isChecked());

	pMakeTorrentJob->SetCreator(m_pCreator->text());
	pMakeTorrentJob->SetComment(m_pComment->toPlainText());
	
	QVariantList Trackers;
	for(int i=0; i < m_pTrackerList->topLevelItemCount(); i++)
	{
		QTreeWidgetItem* pItem = m_pTrackerList->topLevelItem(i);
		QVariantMap Tracker;
		Tracker["Url"] = pItem->text(0);
		Tracker["Tier"] = pItem->data(1, Qt::UserRole);
		Trackers.append(Tracker);
	}
	pMakeTorrentJob->SetTrackers(Trackers);

	/*QVariantList WebSeeds;
	QVariantList DHTNodes;
	for(int i=0; i < m_pBootstrapList->topLevelItemCount(); i++)
	{
		QTreeWidgetItem* pItem = m_pBootstrapList->topLevelItem(i);
		if(pItem->data(1, Qt::UserRole).toInt())
			WebSeeds.append(pItem->text(0));
		else
			DHTNodes.append(pItem->text(0));
	}
	pMakeTorrentJob->SetWebSeeds(WebSeeds);
	pMakeTorrentJob->SetDHTNodes(DHTNodes);*/

	theGUI->ScheduleJob(pMakeTorrentJob);
}