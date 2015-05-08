#pragma once

class CCoverView;

#include "ProgressBar.h"

class CDetailsView: public QWidget
{
	Q_OBJECT

public:
	CDetailsView(UINT Mode, QWidget *parent = 0);
	~CDetailsView();

	void				ShowDetails(uint64 ID);

	void				ChangeMode(UINT Mode)		{m_Mode = Mode;}

public slots:
	void				UpdateDetails();

	void				OnMenuRequested(const QPoint &point);

	void				OnSetFileName();
	void				OnSetRating();
	void				OnEnableSubmit();

	void				OnCopyHash();
	void				OnSetMaster();
	void				OnAddHash();
	void				OnRemoveHash();
	void				OnSelectHash();
	void				OnBanHash();

protected:
	virtual void		timerEvent(QTimerEvent *e);

	int					m_TimerId;

	uint64				m_ID;
	UINT				m_Mode;

	friend class CDetailsUpdateJob;
	friend class CDetailsApplyJob;

	QVBoxLayout*		m_pMainLayout;

	CProgressBar*		m_pProgress;

	QWidget*			m_pWidget;
	QHBoxLayout*		m_pLayout;

	QWidget*			m_pDetailWidget;
	QFormLayout*		m_pDetailLayout;

	QLineEdit*			m_pFileName;
	QTextEdit*			m_pDescription;
	QComboBox*			m_pRating;
	QLabel*				m_pInfo;
	QPushButton*		m_pSubmit;
	QTableWidget*		m_pHashes;

	QMenu*				m_pMenu;
	QAction*			m_pCopyHash;
	QAction*			m_pSetMaster;
	QAction*			m_pAddHash;
	QAction*			m_pRemoveHash;
	QAction*			m_pSelectHash;
	QAction*			m_pBanHash;

	CCoverView*			m_pCoverView;

	bool				m_IsComplete;

	bool				m_bLockDown;
};