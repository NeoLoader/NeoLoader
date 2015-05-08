#pragma once

#include <QVBoxLayout>
#include <QTreeWidget>
#include <QMenu>
#include <QAction>
#include <QStyleFactory>
#include <QHeaderView>
#include <QApplication>
#include <QClipboard>

class CRoutingZone;

class CRoutingWidget: public QWidget
{
	Q_OBJECT
public:
	CRoutingWidget(QWidget *parent = 0);
	~CRoutingWidget();

	void				ClearRoutignTable() {m_pRoutingTable->clear();}
	void				DumpRoutignTable(QTreeWidgetItem* pSessionItem, CRoutingZone* pRoot);
	void				DumpRoutignTable();

private slots:
	void				OnItemDoubleClicked(QTreeWidgetItem* pItem, int iColumn);
	void				OnMenuRequested(const QPoint &);
	void				OnCopyCell();
	void				OnCopyRow();
	void				OnCopyColumn();
	void				OnCopyPanel();

protected:
	enum EColumns
	{
		eID = 0,
		eType,
		eDistance,
		eAddress,
		eCrypto,
		eUpload,
		eDownload,
		eSoftware,
		eExpire
	};

	QVBoxLayout			*m_pMainLayout;
	QTreeWidget 		*m_pRoutingTable;

	QMenu				*m_pCopyMenu;
	QAction				*m_pCopyCell;
	QAction				*m_pCopyRow;
	QAction				*m_pCopyColumn;
	QAction				*m_pCopyPanel;
};