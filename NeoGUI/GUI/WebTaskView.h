#pragma once

class CWebTaskSyncJob;

class CWebTaskView: public QWidget
{
	Q_OBJECT

public:
	CWebTaskView(QWidget *parent = 0);
	~CWebTaskView();

	void				SetID(uint64 ID) {m_ID = ID;}

	void				Suspend(bool bSet);

protected:
	friend class CWebTaskSyncJob;
	void				SyncWebTasks();
	void				SyncWebTasks(const QVariantMap& Response);

	virtual void		timerEvent(QTimerEvent *e);

	int					m_TimerId;
	uint64				m_ID;

	CWebTaskSyncJob*	m_pWebTaskSyncJob;


	enum EColumns
	{
		eUrl = 0,
		eEntry,
		eStatus,
	};

	QVBoxLayout*		m_pMainLayout;

	QTreeWidget*		m_pWebTaskTree;
};
