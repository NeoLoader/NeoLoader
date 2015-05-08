#pragma once

class CFileListWidget;
class CSearchSyncJob;

class CSearchWindow: public QWidget
{
	Q_OBJECT

public:
	CSearchWindow(QWidget *parent = 0);
	~CSearchWindow();

	static void			StartSearch(const QString& SearchNet, const QString& Expression, const QVariantMap& Criteria = QVariantMap());
	static void			UpdateSearch(uint64 ID);
	static void			StopSearch(uint64 ID);

private slots:
	void				OnStart();
	void				OnNetwork(int Index);
	void				OnCriteria(int Index);
	void				OnCriteriaChanged(int row, int column);

	void				OnRemoveCriteria();

	void				OnGoToNew(bool bChecked);

	//void				OnMenuRequested(const QPoint &point);
	void				OnItemClicked(QTreeWidgetItem* pItem, int Column);
	//void				OnItemDoubleClicked(QTreeWidgetItem* pItem, int Column);
	void				OnSelectionChanged();

	void				LoadFinders();

signals:
	void				OnLine(const QString& Expression);

protected:
	friend class CSearchSyncJob;
	virtual void		SyncSearches(const QVariantMap& Response);

	virtual void		StartNewSearch(const QString& SearchNet, const QString& Expression, const QVariantMap& Criteria = QVariantMap());

	QStringList			GetOptions(const QString& Name);

	void				RestoreSearch(const QString& SearchNet, const QString& Expression, const QVariantMap& Criteria);


	virtual void		timerEvent(QTimerEvent *e);

	int					m_TimerId;

	enum EColumns
	{
		eExpression,
		eStatus,
		eFoundFiles,
	};

	struct SSearch
	{
		QTreeWidgetItem*	pItem;
		QString				Expression;
		QString				SearchNet;
		QVariantMap			Criteria;
	};
	QMap<uint64, SSearch*>	m_Searches;

	QTreeWidget*		m_pSearchTree;

	CSearchSyncJob*		m_pSearchSyncJob;

	QVBoxLayout*		m_pMainLayout;

	QWidget*			m_pSearchWidget;
	QGridLayout*		m_pSearchLayout;

	QLineEdit*			m_pExpression;
	QComboBox*			m_pNetwork;
	QStackedWidget*		m_pAux;
	QComboBox*			m_pType;
	QComboBox*			m_pSite;
	QPushButton*		m_pStart;
	QCheckBox*			m_pGoToNew;
	QTableWidget*		m_pCriteria;
	QComboBox*			m_pCombo;
};
