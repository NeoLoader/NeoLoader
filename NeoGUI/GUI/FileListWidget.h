#pragma once

class CFileListView;
class CFileSummary;
class CDetailsView;
class CTransfersView;
class CFileSettingsView;
class CTrackerView;
class CHostingView;
class CRatingView;
class CPropertiesView;
class CLogView;
class CGrabbingsProgresJob;
class CFinder;

struct SField;

class CFileListWidget: public QWidget
{
	Q_OBJECT

public:
	CFileListWidget(UINT Mode = 0, QWidget *parent = 0);
	~CFileListWidget();

	virtual void		SetID(uint64 ID);
	uint64				GetID();

	void				ChangeMode(UINT Mode, UINT SubMode = 0);
	UINT				GetMode() {return m_Mode;}

	void				ShowDetails(bool bShow);

	virtual void		Suspend(bool bSet);

	void				Insert(QWidget* pWidget)	{m_pSplitter->insertWidget(0, pWidget);}

	CFileListView*		GetFileList()				{return m_pFileList;}

private slots:
	void				OnFileItemClicked(uint64 ID, bool bOnly);
	void				OnTab(int Index);

	void				OpenLog(uint64 ID, uint64 SubID);
	void				CloseLog(int Index);

signals:
	void				StreamFile(uint64 ID, const QString& FileName, bool bComplete);
	void				TogleDetails();

protected:

	//void				SetupFile(QMultiMap<int, SField>&);

	virtual void		timerEvent(QTimerEvent *e);

	int					m_TimerId;

	friend class CGrabbingsProgresJob;
	CGrabbingsProgresJob*	m_pGrabbingsSyncJob;
	void				SyncTasks(const QVariantMap& Response);

	QString				m_Ops;
	uint64				m_CurID;
	UINT				m_Mode;
	uint64				m_ID;
	int					m_OldPending;

	int					m_TabOffset;

	QVBoxLayout*		m_pMainLayout;

	QSplitter*			m_pSplitter;
	QProgressBar*		m_pProgress;

	QWidget*			m_pSubWidget;
	QVBoxLayout*		m_pSubLayout;

	CFinder*			m_pFinder;

	CFileListView*		m_pFileList;
	QTabWidget*			m_pFileTabs;

	CFileSummary*		m_pSummary;
	CDetailsView*		m_pDetails;
	CTransfersView*		m_pTransfers;
	CFileListView*		m_pSubFiles;
	//CFileSettingsView*	m_pSettings;
	CTrackerView*		m_pTracker;
	CHostingView*		m_pHosting;
	CRatingView*		m_pRating;
	CPropertiesView*	m_pProperties;
	CLogView*			m_pLogView;
};