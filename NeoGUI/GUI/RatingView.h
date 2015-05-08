#pragma once

class CRatingSyncJob;

class CRatingView: public QWidget
{
	Q_OBJECT

public:
	CRatingView(QWidget *parent = 0);
	~CRatingView();

	void				ShowRating(uint64 ID);

signals:
	void				FindRating();
	void				ClearRating();

public slots:
	//void				OnMenuRequested(const QPoint &point);

protected:
	friend class CRatingSyncJob;

	void				ShowRating();

	void				SyncRatings(const QVariantMap& Response);

	virtual void		timerEvent(QTimerEvent *e);

	int					m_TimerId;

	uint64 m_ID;

	CRatingSyncJob*		m_pRatingSyncJob;

	/*enum EColumns
	{
		eRating,
		eDescription,
		eFileName
	};*/
	enum EColumns
	{
		eName,
		eValue,
	};

	QVBoxLayout*		m_pMainLayout;

	QWidget*			m_pRatingWidget;
	QFormLayout*		m_pRatingLayout;

	QPushButton*		m_pFindRating;
	QPushButton*		m_pClearRating;

	QTreeWidget*		m_pRatings;
};