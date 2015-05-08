#pragma once

class CGrabberSyncJob;
class CTaskSyncJob;
class CFileListWidget;
class CWebTaskView;

class CGrabberWindow: public QWidget
{
	Q_OBJECT

public:
	CGrabberWindow(QWidget *parent = 0);
	~CGrabberWindow();

	static void			AddLinks(const QString& Links, bool bUseGrabber = false);
	static void			AddFile(const QString& FilePath, bool bUseGrabber = false);

	static void			GrabberAction(uint64 ID, const QString& Action);
	static QString		GetDisplayName(const QStringList& Uris);
	static QString		GetOpenFilter(bool bImport = false);

private slots:
	void				OnAddFile();
	void				OnAddLinks();

	//void				OnMenuRequested(const QPoint &point);
	void				OnItemClicked(QTreeWidgetItem* pItem, int Column);
	//void				OnItemDoubleClicked(QTreeWidgetItem* pItem, int Column);
	void				OnSelectionChanged();

protected:
	friend class CGrabberSyncJob;
	friend class CTaskSyncJob;

	virtual void		SyncTasks(const QVariantMap& Response);
	void				SyncTask(uint64 ID, const QVariantMap& Response);
	void				SyncTask(uint64 ID);

	virtual void		timerEvent(QTimerEvent *e);

	int					m_TimerId;
	
	enum EColumns
	{
		eUris,
		eStatus,
		eFoundFiles,
		ePendingTasks,
	};

	struct STask
	{
		STask() {pTaskSyncJob = NULL;}

		QTreeWidgetItem*	pItem;
		QStringList			Urls;
		CTaskSyncJob*		pTaskSyncJob;
	};
	QMap<uint64, STask*>	m_Tasks;

	QTreeWidget*		m_pGrabberTree;

	CGrabberSyncJob*	m_pGrabberSyncJob;

	QVBoxLayout*		m_pMainLayout;

	QWidget*			m_pGrabberWidget;
	QGridLayout*		m_pGrabberLayout;

	QPlainTextEdit*		m_pLinks;
	QPushButton*		m_AddFile;
	QPushButton*		m_AddLinks;
};
