#pragma once

class CSettingsView;
class CPropertiesView;
class CLogView;

struct SField;

class CCrawlerSite: public QWidget
{
	Q_OBJECT

#ifdef CRAWLER
public:
	CCrawlerSite(QWidget *parent = 0);

	void				ShowSite(const QString& SiteName);

private slots:
	void				OnClicked(QAbstractButton* pButton);

protected:
	void				LoadAll();
	void				SaveAll();

	void				SetupSite(QMultiMap<int, SField>&);

	QString				m_SiteName;

	QVBoxLayout*		m_pMainLayout;

	QTabWidget*			m_pCrawlerTabs;

	CSettingsView*		m_pSettings;

	CPropertiesView*	m_pProperties;
	CLogView*			m_pLogView;

	QDialogButtonBox*	m_pButtons;
#endif
};