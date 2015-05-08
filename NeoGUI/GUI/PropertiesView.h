#pragma once

class CPropertiesView: public QWidget
{
	Q_OBJECT

public:
	CPropertiesView(bool bWithButtons = false, QWidget *parent = 0);
	~CPropertiesView();

	void				ShowFile(uint64 ID);
	void				ShowHoster(const QString& Hoster, const QString& Account = "");
#ifdef CRAWLER
	void				ShowSite(const QString& SiteName);
#endif
	void				ShowReadOnly(const QVariantMap& Properties);

public slots:
	void				UpdateSettings();
	void				ApplySettings();

private slots:
	void				OnMenuRequested(const QPoint &point);

	void				OnAdd();
	void				OnAddRoot();
	void				OnRemove();

	void				OnClicked(QAbstractButton* pButton);

protected:
	friend class CPropertiesGetJob;

	void				WriteProperties(const QVariantMap& Root);
	void				WriteProperties(QTreeWidgetItem* pItem, const QVariant& Value, QMap<QTreeWidgetItem*,QWidget*>& Widgets);
	QVariantMap			ReadProperties();
	QVariant			ReadProperties(QTreeWidgetItem* pItem);

	enum EColumns
	{
		eKey = 0,
		eValue,
		eType,
	};

	QVariantMap			m_Request;
	bool				m_ReadOnly;

	QVBoxLayout*		m_pMainLayout;

	QTreeWidget*		m_pPropertiesTree;

	QDialogButtonBox*	m_pButtons;

	QMenu*				m_pMenu;
	QAction*			m_pAdd;
	QAction*			m_pAddRoot;
	QAction*			m_pRemove;
};