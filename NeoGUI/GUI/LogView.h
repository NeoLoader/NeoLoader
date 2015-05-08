#pragma once

class CLogSyncJob;
class QLineEditEx;

class CLogView: public QWidget
{
	Q_OBJECT

public:
	CLogView(QWidget *parent = 0);
	~CLogView();

	void				ShowFile(uint64 ID);
	void				ShowTransfer(uint64 ID, uint64 SubID);
	
#ifdef CRAWLER
	void				ShowSite(const QString& SiteName);
#endif

	void				Suspend(bool bSet);

	virtual void		AddLog(uint32 uFlag, const QString &Entry);
	virtual void		ClearLog();

protected:
	friend class CLogSyncJob;

	void				AppendLog(QTextEdit* pLogEdit, uint32 uFlag, QString Entry);

	CLogSyncJob*		m_pLogSyncJob;
	uint64				m_uLastLog;

	QVariantMap			m_Request;
	bool				m_bFilter;

	virtual void		timerEvent(QTimerEvent *e);

	int					m_TimerId;

	QVBoxLayout*		m_pMainLayout;

	QMap<UINT,QTextEdit*> m_LogEdits;
};

class CConsoleJob;

class CLogViewEx: public CLogView
{
	Q_OBJECT

public:
	CLogViewEx(QWidget *parent = 0);

//private slots:
//	void				OnCommand();

protected:
	friend class CConsoleJob;

	virtual void		AddLog(uint32 uFlag, const QString &Entry);

	QTabWidget*			m_pLogTabs;

	/*QWidget*			m_pConsoleWidget;
	QVBoxLayout*		m_pConsoleLayout;
	QTextEdit*			m_pConsoleText;
	QLineEditEx*		m_pConsoleLine;*/
};
