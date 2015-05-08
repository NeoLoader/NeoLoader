#pragma once

class CPropertiesView;
class CCoverView;

class CCrawlerEntry: public QWidget
{
	Q_OBJECT

#ifdef CRAWLER
public:
	CCrawlerEntry(QWidget *parent = 0);

	void				ShowEntry(const QString& FileName, const QMap<QString, QString>& HasheMap, const QString& Description, const QString& Cover, const QStringList& Links, const QVariantMap& Details);

protected:

	QVBoxLayout*		m_pMainLayout;

	QTabWidget*			m_pCrawlerTabs;

	QWidget*			m_pSummary;
	QHBoxLayout*		m_pSummaryLayout;

	QWidget*			m_pDetailWidget;
	QFormLayout*		m_pDetailLayout;

	QLineEdit*			m_pFileName;
	QTableWidget*		m_pHashes;
	QTextEdit*			m_pDescription;

	QTreeWidget*		m_pTransferTree;

	CCoverView*			m_pCoverView;

	CPropertiesView*	m_pProperties;

#endif
};