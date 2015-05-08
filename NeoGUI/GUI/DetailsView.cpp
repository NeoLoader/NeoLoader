#include "GlobalHeader.h"
#include "DetailsView.h"
#include "FileListView.h"
#include "CoverView.h"
#include "../NeoGUI.h"
#include "FileListView.h"

CDetailsView::CDetailsView(UINT Mode, QWidget *parent)
:QWidget(parent)
{
	m_ID = 0;
	m_Mode = Mode;

	m_IsComplete = false;

	m_bLockDown = false;

	m_pMainLayout = new QVBoxLayout();

	m_pWidget = new QWidget();
	m_pLayout = new QHBoxLayout(m_pWidget);
	m_pLayout->setMargin(0);

	m_pDetailWidget = new QWidget();
	m_pDetailLayout = new QFormLayout();

	m_pFileName = new QLineEdit();
	m_pFileName->setMaximumWidth(450);
	connect(m_pFileName, SIGNAL(editingFinished()), this, SLOT(OnSetFileName()));
	m_pDetailLayout->setWidget(0, QFormLayout::LabelRole, new QLabel(tr("FileName:")));
	m_pDetailLayout->setWidget(0, QFormLayout::FieldRole, m_pFileName);

	m_pHashes = new QTableWidget();
	m_pHashes->setMaximumWidth(450);
	m_pHashes->horizontalHeader()->hide();
	m_pHashes->setSelectionMode(QAbstractItemView::NoSelection);
	m_pHashes->setEditTriggers(QTableWidget::NoEditTriggers);
	m_pDetailLayout->setWidget(1, QFormLayout::LabelRole, new QLabel(tr("Hashes:")));
	m_pDetailLayout->setWidget(1, QFormLayout::FieldRole, m_pHashes);

	m_pHashes->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pHashes, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenuRequested(const QPoint &)));

	if(theGUI->Cfg()->GetInt("Gui/AdvancedControls"))
	{
		m_pMenu = new QMenu(this);

		m_pCopyHash = new QAction(tr("Copy Hash"), m_pMenu);
		connect(m_pCopyHash, SIGNAL(triggered()), this, SLOT(OnCopyHash()));
		m_pCopyHash->setShortcut(QKeySequence::Copy);
		m_pCopyHash->setShortcutContext(Qt::WidgetShortcut);
		m_pHashes->addAction(m_pCopyHash);
		m_pMenu->addAction(m_pCopyHash);

		m_pSetMaster = new QAction(tr("Set as Master Hash"), m_pMenu);
		connect(m_pSetMaster, SIGNAL(triggered()), this, SLOT(OnSetMaster()));
		m_pMenu->addAction(m_pSetMaster);

		m_pAddHash = new QAction(tr("Add Hash"), m_pMenu);
		connect(m_pAddHash, SIGNAL(triggered()), this, SLOT(OnAddHash()));
		m_pMenu->addAction(m_pAddHash);

		m_pRemoveHash = new QAction(tr("Remove Hash"), m_pMenu);
		connect(m_pRemoveHash, SIGNAL(triggered()), this, SLOT(OnRemoveHash()));
		m_pMenu->addAction(m_pRemoveHash);

		m_pSelectHash = new QAction(tr("Use Hash"), m_pMenu);
		m_pSelectHash->setCheckable(true);
		connect(m_pSelectHash, SIGNAL(triggered()), this, SLOT(OnSelectHash()));
		m_pMenu->addAction(m_pSelectHash);

		m_pBanHash = new QAction(tr("Blacklist Hash"), m_pMenu);
		m_pBanHash->setCheckable(true);
		connect(m_pBanHash, SIGNAL(triggered()), this, SLOT(OnBanHash()));
		m_pMenu->addAction(m_pBanHash);
	}
	else
	{
		m_pMenu = NULL;
		m_pSetMaster = NULL;
		m_pAddHash = NULL;
		m_pRemoveHash = NULL;
		m_pSelectHash = NULL;
		m_pMenu = NULL;
	}

	m_pDescription = new QTextEdit();
	m_pDescription->setMaximumWidth(450);
	connect(m_pDescription, SIGNAL(textChanged()), this, SLOT(OnEnableSubmit()));
	m_pDetailLayout->setWidget(2, QFormLayout::LabelRole, new QLabel(tr("Description:")));
	m_pDetailLayout->setWidget(2, QFormLayout::FieldRole, m_pDescription);

	m_pRating = new QComboBox();
	m_pRating->setMaximumWidth(100);
	m_pRating->addItem(tr("Not Rated"));
	m_pRating->addItem(tr("Fake"));
	m_pRating->addItem(tr("Poor"));
	m_pRating->addItem(tr("Fair"));
	m_pRating->addItem(tr("Good"));
	m_pRating->addItem(tr("Excellent"));
	connect(m_pRating, SIGNAL(currentIndexChanged(int)), this, SLOT(OnEnableSubmit()));
	m_pDetailLayout->setWidget(3, QFormLayout::LabelRole, new QLabel(tr("Rating:")));
	QWidget* pRatingSubmit = new QWidget();
	pRatingSubmit->setMaximumWidth(450);
	QHBoxLayout* pSubmitLayout = new QHBoxLayout();
	pSubmitLayout->setMargin(0);
	pSubmitLayout->addWidget(m_pRating);
	//QWidget* pSpacer = new QWidget();
	//pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	//pSubmitLayout->addWidget(pSpacer);
	m_pInfo = new QLabel();
	m_pInfo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pInfo->setAlignment(Qt::AlignCenter);
	pSubmitLayout->addWidget(m_pInfo);
	m_pSubmit = new QPushButton(tr("Submit"));
	connect(m_pSubmit, SIGNAL(pressed()), this, SLOT(OnSetRating()));
	pSubmitLayout->addWidget(m_pSubmit);
	pRatingSubmit->setLayout(pSubmitLayout);
	m_pDetailLayout->setWidget(3, QFormLayout::FieldRole, pRatingSubmit);

	m_pDetailWidget->setLayout(m_pDetailLayout);

	m_pLayout->addWidget(m_pDetailWidget);

	m_pCoverView = new CCoverView();
	m_pCoverView->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));

	m_pLayout->addWidget(m_pCoverView);

	if(theGUI->Cfg()->GetInt("Gui/AdvancedControls") == 1)
		m_pProgress = new CProgressBar(-1, 0, "-Plan");
	else
		m_pProgress = new CProgressBar(-1, 0, "-");
	m_pProgress->setMinimumHeight(24);
	m_pProgress->setMaximumHeight(24);
	m_pProgress->setMinimumWidth(450);

	m_pMainLayout->addWidget(m_pProgress);
	m_pMainLayout->addWidget(m_pWidget);

	setLayout(m_pMainLayout);

	m_TimerId = startTimer(500);
}

CDetailsView::~CDetailsView()
{
	killTimer(m_TimerId);
}

void CDetailsView::ShowDetails(uint64 ID)
{
	m_ID = ID;
	m_pProgress->SetID(ID);
	UpdateDetails();
}

void CDetailsView::timerEvent(QTimerEvent *e)
{
    if (e->timerId() != m_TimerId) 
	{
        QObject::timerEvent(e);
		return;
    }

	m_pProgress->Update();
}

class CSetRatingJob: public CInterfaceJob
{
public:
	CSetRatingJob(uint64 ID)
	{
		m_Request["ID"] = ID;
		m_Request["Action"] = "FindRating";
		m_Request["Log"] = false;
	}

	virtual QString			GetCommand()	{return "FileAction";}
	virtual void			HandleResponse(const QVariantMap& Response) {}
};

class CDetailsUpdateJob: public CInterfaceJob
{
public:
	CDetailsUpdateJob(CDetailsView* pView, uint64 ID)
	{
		m_pView = pView;
		m_Request["ID"] = ID;
		//m_Request["Section"] = "Details";
	}

	virtual QString			GetCommand()	{return "GetFile";}
	virtual void			HandleResponse(const QVariantMap& Response) 
	{
		if(m_pView)
		{
			m_pView->m_bLockDown = true;

			
			QStringList Info;
			if(Response.contains("Error"))
				Info.append(tr("Error: %2").arg(Response["Error"].toString()));
			if(theGUI->Cfg()->GetInt("Gui/AdvancedControls") == 1)
				Info.append(tr("FileID: %1").arg(Response["ID"].toULongLong()));
			m_pView->m_pInfo->setText(Info.join(", "));

			m_pView->m_pFileName->setText(Response["FileName"].toString());

			QVariantList HashMap = Response["HashMap"].toList();
			m_pView->m_pHashes->setColumnCount(1);
#if QT_VERSION < 0x050000
			m_pView->m_pHashes->horizontalHeader()->setResizeMode(0, QHeaderView::Stretch);
#else
			m_pView->m_pHashes->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
#endif
			m_pView->m_pHashes->setRowCount(HashMap.count());
			QStringList Hashes;
			StrPair MasterHash = Split2(Response["MasterHash"].toString(), ":");
			foreach(const QVariant& vHash, HashMap)
			{
				QVariantMap Hash = vHash.toMap();

				QTableWidgetItem* pItem = new QTableWidgetItem(Hash["Value"].toString());

				QFont Font = pItem->font();

				if(Hash["Value"].toString() == MasterHash.second)
				{
					Font.setBold(true);
					pItem->setData(Qt::UserRole, "Master");
				}
				else
					pItem->setData(Qt::UserRole, Hash["State"]);

				if(!MasterHash.first.isEmpty()) // file complate all hashes just black
				{
					if(Hash["State"] == "Parent")
						Font.setItalic(true);
					else if(Hash["State"] == "Empty")
						pItem->setForeground(Qt::gray);
					else if(Hash["State"] == "Aux")
						pItem->setForeground(Qt::darkYellow);
					else if(Hash["State"] == "Bad")
						pItem->setForeground(Qt::darkRed);
					else if(Hash["Value"].toString() != MasterHash.second)
						pItem->setForeground(Qt::darkGreen);
				}

				pItem->setFont(Font);

				m_pView->m_pHashes->setItem(Hashes.count(), 0, pItem);
				m_pView->m_pHashes->resizeRowToContents(Hashes.count());
				Hashes.append(Hash["Type"].toString());
			}

			m_pView->m_pHashes->setVerticalHeaderLabels(Hashes);
			
			if(int Count = m_pView->m_pHashes->rowCount())
				m_pView->m_pHashes->setMaximumHeight(((m_pView->m_pHashes->rowHeight(0)) * Count) + 2);
			else
				m_pView->m_pHashes->setMaximumHeight(30);

			m_pView->m_IsComplete = Response["FileStatus"] == "Complete";

			QVariantMap Properties = Response["Properties"].toMap();
			m_pView->m_pRating->setCurrentIndex(Properties["Rating"].toInt());
			m_pView->m_pDescription->setPlainText(Properties["Description"].toString());
			m_pView->m_pCoverView->ShowCover(m_pView->m_ID, Properties["CoverUrl"].toString());
			m_pView->m_pSubmit->setEnabled(false);

			bool Searching = Response["FileJobs"].toStringList().contains("Searching");
			if(m_pView->m_Mode == CFileListView::eFilesSearch || m_pView->m_Mode == CFileListView::eFilesGrabber)
			{
				if(!Searching && Properties["Description"].toString().isEmpty())
				{
					Searching = true;
					CSetRatingJob* pSetRatingJob = new CSetRatingJob(m_pView->m_ID);
					theGUI->ScheduleJob(pSetRatingJob);
				}

				if(Searching)
					QTimer::singleShot(1000, m_pView, SLOT(UpdateDetails()));
			}

			m_pView->m_bLockDown = false;
		}
	}
protected:
	QPointer<CDetailsView>	m_pView; // Note: this can be deleted at any time
};

void CDetailsView::UpdateDetails()
{
	CDetailsUpdateJob* pDetailsUpdateJob = new CDetailsUpdateJob(this, m_ID);
	theGUI->ScheduleJob(pDetailsUpdateJob);
}

class CSetFileJob: public CInterfaceJob
{
public:
	CSetFileJob(CDetailsView* pView, uint64 ID)
	{
		m_Request["ID"] = ID;
		//m_Request["Section"] = "Details";
	}

	virtual void			SetValue(const QString& Key, const QVariant& Value)		{m_Request[Key] = Value;}
	virtual QString			GetCommand()	{return "SetFile";}
	virtual void			HandleResponse(const QVariantMap& Response) {}
};

void CDetailsView::OnSetFileName()
{
	CSetFileJob* pSetFileJob = new CSetFileJob(this, m_ID);
	pSetFileJob->SetValue("FileName", m_pFileName->text());
	theGUI->ScheduleJob(pSetFileJob);
}

void CDetailsView::OnSetRating()
{
	QVariantMap Properties;
	Properties["Rating"] = m_pRating->currentIndex();
	Properties["Description"] = m_pDescription->toPlainText();

	CSetFileJob* pSetFileJob = new CSetFileJob(this, m_ID);
	pSetFileJob->SetValue("Properties", Properties);
	theGUI->ScheduleJob(pSetFileJob);
}

void CDetailsView::OnEnableSubmit()
{
	if(m_bLockDown)
		return;

	m_pSubmit->setEnabled(true);
}

void CDetailsView::OnMenuRequested(const QPoint &point)
{
	if(!m_pMenu || !(m_Mode == CFileListView::eDownload || m_Mode == CFileListView::eFilesSearch || m_Mode == CFileListView::eFilesGrabber))
		return;

	QString Type;
	QString Value;
	QString State;
	int Index = m_pHashes->currentRow();
	if(Index != -1)
	{
		Type = m_pHashes->verticalHeaderItem(Index)->text();
		Value = m_pHashes->item(Index, 0)->text();
		State = m_pHashes->item(Index, 0)->data(Qt::UserRole).toString();
	}

	m_pSetMaster->setEnabled(!m_IsComplete && !Type.isEmpty());
	
	m_pAddHash->setEnabled(true);
	m_pRemoveHash->setEnabled(!Type.isEmpty());

	m_pSelectHash->setEnabled(!m_IsComplete && !Type.isEmpty() && State != "Bad" && State != "Master" && State != "Parent");
	m_pSelectHash->setChecked(State != "Aux" && m_pSelectHash->isEnabled());

	m_pBanHash->setEnabled(!m_IsComplete && !Type.isEmpty() && State != "Master" && State != "Parent");
	m_pBanHash->setChecked(State == "Bad");

	m_pMenu->popup(QCursor::pos());	
}

void CDetailsView::OnCopyHash()
{
	int Index = m_pHashes->currentRow();
	if(Index == -1)
		return;

	QApplication::clipboard()->setText(m_pHashes->verticalHeaderItem(Index)->text() + ":" + m_pHashes->item(Index, 0)->text());
}

void CDetailsView::OnSetMaster()
{
	int Index = m_pHashes->currentRow();
	if(Index == -1)
		return;

	CSetFileJob* pSetFileJob = new CSetFileJob(this, m_ID);
	pSetFileJob->SetValue("MasterHash", m_pHashes->verticalHeaderItem(Index)->text() + ":" + m_pHashes->item(Index, 0)->text());
	theGUI->ScheduleJob(pSetFileJob);

	UpdateDetails();
}

void CDetailsView::OnAddHash()
{
	QString Hash = QInputDialog::getText(this, tr("Add Hash to File"),tr("type:hash") + QString(100,' '));
	if(Hash.isEmpty())
		return;

	QVariantMap Options;
	Options["Hash"] = Hash;
	CFileListView::DoAction("AddHash", m_ID, Options);

	UpdateDetails();
}

void CDetailsView::OnRemoveHash()
{
	int Index = m_pHashes->currentRow();
	if(Index == -1)
		return;

	QVariantMap Options;
	Options["Hash"] = m_pHashes->verticalHeaderItem(Index)->text() + ":" + m_pHashes->item(Index, 0)->text();
	CFileListView::DoAction("RemoveHash", m_ID, Options);

	UpdateDetails();
}

void CDetailsView::OnSelectHash()
{
	int Index = m_pHashes->currentRow();
	if(Index == -1)
		return;

	QVariantMap Options;
	Options["Hash"] = m_pHashes->verticalHeaderItem(Index)->text() + ":" + m_pHashes->item(Index, 0)->text();
	CFileListView::DoAction(m_pSelectHash->isChecked() ? "SelectHash" : "UnSelectHash", m_ID, Options);

	UpdateDetails();
}

void CDetailsView::OnBanHash()
{
	int Index = m_pHashes->currentRow();
	if(Index == -1)
		return;

	QVariantMap Options;
	Options["Hash"] = m_pHashes->verticalHeaderItem(Index)->text() + ":" + m_pHashes->item(Index, 0)->text();
	CFileListView::DoAction(m_pBanHash->isChecked() ? "BanHash" : "UnBanHash", m_ID, Options);

	UpdateDetails();
}