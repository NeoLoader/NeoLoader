#pragma once

#include <QtNetwork>

class CCoverView;

class COnlineFileList: public QWidget
{
	Q_OBJECT

public:
	COnlineFileList(QWidget *parent = 0);

	void				SearchOnline(const QString& Expression, const QVariantMap& Criteria = QVariantMap());

private slots:
	void				OnFinished(QNetworkReply*);

	void				OnMenuRequested(const QPoint &point);
	void				OnItemClicked(QTreeWidgetItem* pItem, int Column);
	void				OnItemDoubleClicked(QTreeWidgetItem* pItem, int Column);
	void				OnSelectionChanged();

	void				OnGrab();
	void				OnWebPage();
	void				OnSrcPage();

protected:
	QNetworkAccessManager*	m_pNet;
	QMap<QString,QString>	m_Downloads;

	QVBoxLayout*		m_pMainLayout;

	QSplitter*			m_pSplitter;

	QTreeWidget*		m_pFileTree;

	QMenu*				m_pMenu;

	QAction*			m_pGrab;
	QAction*			m_pWebPage;
	QAction*			m_pSrcPage;

	QTabWidget*			m_pFileTabs;

	QWidget*			m_pSummary;
	QHBoxLayout*		m_pSummaryLayout;

	QWidget*			m_pDetailWidget;
	QFormLayout*		m_pDetailLayout;

	QLineEdit*			m_pFileName;
	QTextEdit*			m_pDescription;

	QTreeWidget*		m_pTransferTree;

	CCoverView*			m_pCoverView;
};