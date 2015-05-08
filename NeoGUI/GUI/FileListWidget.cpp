#include "GlobalHeader.h"
#include "FileListWidget.h"
#include "FileListView.h"
#include "FileSummary.h"
#include "DetailsView.h"
#include "TransfersView.h"
#include "SettingsView.h"
#include "TrackerView.h"
#include "HostingView.h"
#include "RatingView.h"
#include "PropertiesView.h"
#include "LogView.h"
#include "Finder.h"
#include "../NeoGUI.h"

/*class CFileSettingsView: public CSettingsView
{
public:
	CFileSettingsView(QMultiMap<int, SField> Layout, QWidget *parent = 0)
	 : CSettingsView(Layout, "", parent)
	{
		foreach(QWidget* pWidget, m_Settings)
		{
			if(QLineEdit* pEdit = qobject_cast<QLineEdit*>(pWidget))
				connect(pEdit, SIGNAL(editingFinished()), this, SLOT(ApplySettings()));
			else if(QSpinBox* pEdit = qobject_cast<QSpinBox*>(pWidget))
				connect(pEdit, SIGNAL(valueChanged(int)), this, SLOT(ApplySettings()));
			else if(QComboBox* pCombo = qobject_cast<QComboBox*>(pWidget))
				connect(pCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(ApplySettings()));
			else if(QCheckBox* pCheck = qobject_cast<QCheckBox*>(pWidget))
				connect(pCheck, SIGNAL(stateChanged(int)), this, SLOT(ApplySettings()));
			else if(QPlainTextEdit* pEdit = qobject_cast<QPlainTextEdit*>(pWidget))
				connect(pEdit, SIGNAL(textChanged()), this, SLOT(ApplySettings()));
			else {
				ASSERT(0);}
		}
	}

	virtual void ApplySettings()
	{
		if(!isEnabled())
			return;
		// we do that in the reverse order in order to get queue setings be in the right order
		for(int i = m_IDs.size()-1; i >= 0; i--)
			ApplySetting(m_Settings.keys(), m_IDs[i]);
	}
};
*/


CFileListWidget::CFileListWidget(UINT Mode, QWidget *parent)
:QWidget(parent)
{
	m_Mode = Mode;
	m_Ops = Mode2Str(Mode);
	m_CurID = 0;

	m_pGrabbingsSyncJob = NULL;
	m_OldPending = 0;

	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(1);

	m_pSplitter = new QSplitter();
	m_pSplitter->setOrientation(Qt::Vertical);

	m_pFileList = new CFileListView(Mode);
	connect(m_pFileList, SIGNAL(FileItemClicked(uint64, bool)), this, SLOT(OnFileItemClicked(uint64, bool)));
	connect(m_pFileList, SIGNAL(StreamFile(uint64, const QString&, bool)), this, SIGNAL(StreamFile(uint64, const QString&, bool)));
	connect(m_pFileList, SIGNAL(TogleDetails()), this, SIGNAL(TogleDetails()));

	m_pSubWidget = new QWidget();
	m_pSubLayout = new QVBoxLayout();
	m_pSubLayout->setMargin(0);

	m_pSubLayout->addWidget(m_pFileList);

	//
	m_pFinder = new CFinder();
	m_pSubLayout->addWidget(m_pFinder);
	QObject::connect(m_pFinder, SIGNAL(SetFilter(const QRegExp&)), m_pFileList, SLOT(SetFilter(const QRegExp&)));

    QAction* pFinder = new QAction(tr("&Find ..."), this);
	pFinder->setShortcut(QKeySequence::Find);
	pFinder->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	this->addAction(pFinder);
    QObject::connect(pFinder, SIGNAL(triggered()), m_pFinder, SLOT(Open()));
	//

	m_pSubWidget->setLayout(m_pSubLayout);

	m_pSplitter->addWidget(m_pSubWidget);

	m_pFileTabs = new QTabWidget();

	m_pSummary = new CFileSummary(Mode);
	m_pFileTabs->addTab(m_pSummary, tr("Summary"));

	m_pDetails = new CDetailsView(Mode);
	m_pFileTabs->addTab(m_pDetails, tr("Details"));

	//QMultiMap<int, SField> Settings;
	//SetupFile(Settings);
	//m_pSettings = new CFileSettingsView(Settings);
	//m_pFileTabs->addTab(m_pSettings, tr("Settings"));

	if(theGUI->Cfg()->GetInt("Gui/AdvancedControls"))
	{
		m_pTransfers = new CTransfersView(CTransfersView::eTransfers);
		m_pFileTabs->addTab(m_pTransfers, tr("Transfers"));
		connect(m_pTransfers, SIGNAL(OpenTransfer(uint64, uint64)), this, SLOT(OpenLog(uint64, uint64)));
	}
	else
		m_pTransfers = NULL;

	m_pSubFiles = new CFileListView(CFileListView::eSubFiles);
	m_pFileTabs->addTab(m_pSubFiles, tr("Sub Files"));

	m_pHosting = new CHostingView();
	m_pFileTabs->addTab(m_pHosting, tr("Hosting"));

	if(theGUI->Cfg()->GetInt("Gui/AdvancedControls"))
	{
		m_pTracker = new CTrackerView();
		m_pFileTabs->addTab(m_pTracker, tr("Tracker"));
	}
	else
		m_pTracker = NULL;
	
	m_pRating = new CRatingView();
	connect(m_pRating, SIGNAL(FindRating()), m_pFileList, SLOT(OnFindRating()));
	connect(m_pRating, SIGNAL(ClearRating()), m_pFileList, SLOT(OnClearRating()));
	m_pFileTabs->addTab(m_pRating, tr("Rating"));

	if(theGUI->Cfg()->GetInt("Gui/AdvancedControls") == 1)
	{
		m_pProperties = new CPropertiesView(true);
		m_pFileTabs->addTab(m_pProperties, tr("Properties"));
	}
	else
		m_pProperties = NULL;

	if(theGUI->Cfg()->GetBool("Gui/ShowLog"))
	{
		m_pLogView = new CLogView();
		m_pFileTabs->addTab(m_pLogView, tr("Log"));
	}
	else
		m_pLogView = NULL;

	// Make all additionaly added tabs closable - client log tab
	m_pFileTabs->setTabsClosable(true);
	QTabBar* pTabBar = m_pFileTabs->tabBar();
	m_TabOffset = m_pFileTabs->count();
	for(int i=0; i < m_TabOffset; i++)
		pTabBar->setTabButton(i,static_cast<QTabBar::ButtonPosition>(pTabBar->style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition,  0, pTabBar)),0);

	connect(pTabBar, SIGNAL(tabCloseRequested(int)), this, SLOT(CloseLog(int)));

	m_pSplitter->addWidget(m_pFileTabs);

	m_pMainLayout->addWidget(m_pSplitter);
	m_pProgress = new QProgressBar();
	m_pProgress->setVisible(false);
	m_pProgress->setMinimum(0);
	m_pProgress->setMaximum(100);
	m_pMainLayout->addWidget(m_pProgress);

	setLayout(m_pMainLayout);

	connect(m_pFileTabs, SIGNAL(currentChanged(int)), this, SLOT(OnTab(int)));

	m_pSplitter->restoreState(theGUI->Cfg()->GetBlob("Gui/Widget_" + m_Ops + "_Spliter"));
	m_pFileTabs->setCurrentIndex(theGUI->Cfg()->GetSetting("Gui/Widget_" + m_Ops + "_Detail").toInt());

	m_TimerId = startTimer(1000);
}

CFileListWidget::~CFileListWidget()
{
	theGUI->Cfg()->SetBlob("Gui/Widget_" + m_Ops + "_Spliter",m_pSplitter->saveState());
	theGUI->Cfg()->SetSetting("Gui/Widget_" + m_Ops + "_Detail",m_pFileTabs->currentIndex());

	if(m_TimerId != 0)
		killTimer(m_TimerId);
}

void CFileListWidget::SetID(uint64 ID)
{
	m_pFileList->SetID(ID);
}

uint64 CFileListWidget::GetID()
{
	return m_pFileList->GetID();
}

void CFileListWidget::ChangeMode(UINT Mode, UINT SubMode)
{
	m_Mode = Mode;
	m_pFileList->ChangeMode(Mode, SubMode);
	m_pSummary->ChangeMode(Mode);
	m_pDetails->ChangeMode(Mode);
	m_pHosting->ChangeMode(Mode);
	OnFileItemClicked(0, false);
}

void CFileListWidget::ShowDetails(bool bShow)
{
	m_pFileTabs->setVisible(bShow);
}

class CGrabbingsProgresJob: public CInterfaceJob
{
public:
	CGrabbingsProgresJob(CFileListWidget* pView)
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
			m_pView->m_pGrabbingsSyncJob = NULL;
		CInterfaceJob::Finish(bOK);
	}

protected:
	QPointer<CFileListWidget>	m_pView; // Note: this can be deleted at any time
};

void CFileListWidget::timerEvent(QTimerEvent *e)
{
    if (e && e->timerId() != m_TimerId) 
	{
        QObject::timerEvent(e);
		return;
    }

	if(m_Mode == CFileListView::eFilesGrabber && GetID() == 0)
	{
		if(m_pGrabbingsSyncJob == NULL)
		{
			m_pGrabbingsSyncJob = new CGrabbingsProgresJob(this);
			theGUI->ScheduleJob(m_pGrabbingsSyncJob);
		}
	}
	else if(m_pProgress->isVisible())
		m_pProgress->setVisible(false);
}

void CFileListWidget::SyncTasks(const QVariantMap& Response)
{
	int Pending = 0;

	foreach (const QVariant vTask, Response["Tasks"].toList())
	{
		QVariantMap Task = vTask.toMap();
		uint64 GrabberID = Task["ID"].toULongLong();

		int Tasks = Task["TasksPending"].toInt() - Task["TasksFailed"].toInt();
		if(Tasks > 0)
			Pending += Tasks;
	}

	if(!m_OldPending && Pending)
		m_OldPending = Pending;

	int iProgress = m_OldPending ? 100 * (m_OldPending - Pending) / m_OldPending : 0;

	if(Pending == 0)
		m_OldPending = 0;

	if(m_OldPending != 0)
	{
		m_pProgress->setVisible(true);
		m_pProgress->setValue(iProgress);
	}
	else
		m_pProgress->setVisible(false);
}

void CFileListWidget::OnFileItemClicked(uint64 ID, bool bOnly)
{
	for(int i = m_TabOffset; i < m_pFileTabs->count(); i++)
		m_pFileTabs->removeTab(i);

	if(bOnly)
	{
		m_CurID = ID;

		QVariantMap File = m_pFileList->GetFile(ID);
		m_pSummary->setEnabled(true);
		m_pDetails->setEnabled(true);
		//m_pSettings->setEnabled(false);
		if(m_pTransfers)
			m_pTransfers->setEnabled(true);
		m_pSubFiles->setEnabled(File["FileType"] == "MultiFile");
		m_pHosting->setEnabled(true);
		if(m_pTracker)
			m_pTracker->setEnabled(true);
		m_pRating->setEnabled(true);
		if(m_pProperties)
			m_pProperties->setEnabled(true);
		if(m_pLogView)
			m_pLogView->setEnabled(true);
	}
	else
	{
		m_CurID = 0;

		m_pSummary->setEnabled(false);
		m_pDetails->setEnabled(false);
		//m_pSettings->setEnabled(false);
		if(m_pTransfers)
			m_pTransfers->setEnabled(false);
		m_pSubFiles->setEnabled(false);
		m_pHosting->setEnabled(false);
		if(m_pTracker)
			m_pTracker->setEnabled(false);
		m_pRating->setEnabled(false);
		if(m_pProperties)
			m_pProperties->setEnabled(false);
		if(m_pLogView)
			m_pLogView->setEnabled(false);
	}

	OnTab(m_pFileTabs->currentIndex());
}

void CFileListWidget::OnTab(int Index)
{
	QWidget* pWidget = m_pFileTabs->widget(Index);
	if(m_CurID)
	{
		if(m_pSummary == pWidget)
			m_pSummary->ShowSummary(m_CurID);
		else
			m_pSummary->ShowSummary(0);

		m_pDetails->ShowDetails(m_CurID);

		QList<uint64> FileIDs;
		FileIDs.append(m_CurID);
		//m_pSettings->ShowSettings(FileIDs);

		if(m_pTransfers)
		{
			if(m_pTransfers == pWidget)
				m_pTransfers->ShowTransfers(m_CurID);
			else
				m_pTransfers->ShowTransfers(0);
		}

		m_pHosting->ShowHosting(m_CurID);

		if(m_pTracker)
			m_pTracker->ShowTracker(m_CurID);

		if(m_pSubFiles == pWidget && m_pSubFiles->isEnabled())
			m_pSubFiles->SetID(m_CurID);
		else
			m_pSubFiles->SetID(0);

		if(m_pRating == pWidget)
			m_pRating->ShowRating(m_CurID);
		else
			m_pRating->ShowRating(0);

		if(m_pProperties)
			m_pProperties->ShowFile(m_CurID);

		if(m_pLogView)
		{
			if(m_pLogView == pWidget)
				m_pLogView->ShowFile(m_CurID);
			else
				m_pLogView->ShowFile(0);
		}
	}
	else
	{
		m_pSummary->ShowSummary(0);

		m_pDetails->ShowDetails(0);

		QList<uint64> FileIDs = m_pFileList->GetSelectedFilesIDs();
		//m_pSettings->ShowSettings(FileIDs);

		if(m_pTransfers)
			m_pTransfers->ShowTransfers(0);

		m_pHosting->ShowHosting(0);

		if(m_pTracker)
			m_pTracker->ShowTracker(0);

		m_pSubFiles->SetID(0);

		m_pRating->ShowRating(0);

		if(m_pProperties)
			m_pProperties->ShowFile(0);

		if(m_pLogView)
			m_pLogView->ShowFile(0);

	}
}

void CFileListWidget::Suspend(bool bSet)
{
	m_pFileList->Suspend(bSet);

	QWidget* pWidget = m_pFileTabs->widget(m_pFileTabs->currentIndex());
	if(m_CurID)
	{
		if(m_pTransfers)
		{
			if(bSet)
				m_pTransfers->ShowTransfers(0);
			else if(m_pTransfers == pWidget)
				m_pTransfers->ShowTransfers(m_CurID);
		}

		if(m_pLogView)
		{
			if(bSet)
				m_pLogView->ShowFile(0);
			else if(m_pLogView == pWidget)
				m_pLogView->ShowFile(m_CurID);
		}

		if(bSet)
			m_pSubFiles->SetID(0);
		else if(m_pSubFiles == pWidget)
			m_pSubFiles->SetID(m_CurID);
	}
}

void CFileListWidget::OpenLog(uint64 ID, uint64 SubID)
{
	CLogView* pLogView = new CLogView();
	pLogView->ShowTransfer(ID, SubID);
	m_pFileTabs->addTab(pLogView, tr("Transfer Log"));
}

void CFileListWidget::CloseLog(int Index)
{
	m_pFileTabs->removeTab(Index);
}

bool StreamEnabled(const QVariantMap& File)
{
	return File["Streamable"].toBool();
}

/*void CFileListWidget::SetupFile(QMultiMap<int, SField>& Settings)
{
	int Counter = 0;

	Settings.insert(Counter, SField(new QLabel(tr("Bandwidth:")), QFormLayout::SpanningRole));
	Counter++;
	CKbpsEdit* pDownRate = new CKbpsEdit();
	pDownRate->setMaximumWidth(100);
	Settings.insert(Counter, SField(new QLabel(tr("Download (KB/s)")), QFormLayout::LabelRole));
	Settings.insert(Counter, SField(pDownRate, QFormLayout::FieldRole, "Download"));
	Counter++;
	CKbpsEdit* pUpRate = new CKbpsEdit();
	pUpRate->setMaximumWidth(100);
	Settings.insert(Counter, SField(new QLabel(tr("Upload (KB/s)")), QFormLayout::LabelRole));
	Settings.insert(Counter, SField(pUpRate, QFormLayout::FieldRole, "Upload"));
	Counter++;
	QComboBox* pPrio = new QComboBox();
	pPrio->setMaximumWidth(100);
	pPrio->addItem(tr("Default"), "0"); // Default
										// Extreme
	pPrio->addItem(tr("Highest"), "9"); // Highest
										// Higher
	pPrio->addItem(tr("High"), "7");	// High
										// Above
	pPrio->addItem(tr("Medium"), "5");	// Medium
										// Below
	pPrio->addItem(tr("Low"), "3");		// Low
										// Lower
	pPrio->addItem(tr("Lowest"), "1");	// Lowest
	pPrio->insertSeparator(2);
	Settings.insert(Counter, SField(new QLabel(tr("Priority")), QFormLayout::LabelRole));
	Settings.insert(Counter, SField(pPrio, QFormLayout::FieldRole, "Priority"));
	Counter++;
	QSpinBox* pQueue = new QSpinBox();
	pQueue->setMaximumWidth(100);
	pQueue->setMinimum(0);
	pQueue->setMaximum(INT_MAX);
	Settings.insert(Counter, SField(new QLabel(tr("Queue Position")), QFormLayout::LabelRole));
	Settings.insert(Counter, SField(pQueue, QFormLayout::FieldRole, "QueuePos"));

	Counter++;
	CFactorEdit* pShareRatio = new CFactorEdit(100, 2, true);
	pShareRatio->setMaximumWidth(100);
	Settings.insert(Counter, SField(new QLabel(tr("Sharing Ratio")), QFormLayout::LabelRole));
	Settings.insert(Counter, SField(pShareRatio, QFormLayout::FieldRole, "ShareRatio"));

	Counter = 100;
	Settings.insert(Counter, SField(new QLabel(tr("Hoster:")), QFormLayout::SpanningRole));
	Counter++;
	CMultiLineEdit* pPasswords = new CMultiLineEdit();
	pPasswords->setMaximumWidth(200);
	pPasswords->setMaximumHeight(50);
	Settings.insert(Counter, SField(new QLabel(tr("Password(s)")), QFormLayout::LabelRole));
	Settings.insert(Counter, SField(pPasswords, QFormLayout::FieldRole, "Passwords"));
	Counter++;
	Settings.insert(Counter, SField(new QCheckBoxEx(tr("Auto ReUpload")), QFormLayout::FieldRole, "ReUpload"));

	Counter++;
	Settings.insert(Counter, SField(new QLabel(tr("Download Management:")), QFormLayout::SpanningRole));
	Counter++;
	Settings.insert(Counter, SField(new QCheckBox(tr("Stream File")), QFormLayout::FieldRole, "Stream", StreamEnabled, QStringList("Streamable")));
}*/
