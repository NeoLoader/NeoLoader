#pragma once

class CTrackerSyncJob;

class CTrackerView: public QWidget
{
	Q_OBJECT

public:
	CTrackerView(QWidget *parent = 0);
	~CTrackerView();

	void				ShowTracker(uint64 ID);

private slots:
	void				OnAnnounce();
	void				OnRepublish();

public slots:
	void				OnMenuRequested(const QPoint &point);

	void				OnAdd();
	void				OnCopyUrl();
	void				OnRemove();

	void				SetTrackers();
	void				GetTrackers();

protected:
	virtual void		timerEvent(QTimerEvent *e);

	int					m_TimerId;

	uint64				m_ID;

	friend class CGetTrackersJob;
	friend class CSetTrackersJob;

	enum EColumns
	{
		eUrl,
		eType,
		eStatus,
		eNext
	};

	QVBoxLayout*		m_pMainLayout;

	QWidget*			m_pTrackerWidget;
	QFormLayout*		m_pTrackerLayout;

	QToolButton*		m_pAnnounce;

	QTreeWidget*		m_pTrackers;

	QMenu*				m_pMenu;
	QAction*			m_pAdd;
	QAction*			m_pCopyUrl;
	QAction*			m_pRemove;
};