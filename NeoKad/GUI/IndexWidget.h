#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QToolBar>
#include <QSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QSplitter>
#include <QTreeWidget>
#include <QTextEdit>
#include <QHeaderView>

class CIndexWidget: public QWidget
{
	Q_OBJECT
public:
	CIndexWidget(QWidget *parent = 0);
	~CIndexWidget();

	void				DumpLocalIndex();

private slots:
	void				OnValueChanged(int i)				{DumpLocalIndex();}
	void				OnReturnPressed()					{DumpLocalIndex();}

	void				OnPayload(QTreeWidgetItem* pItem, int column);

protected:
	QVBoxLayout			*m_pMainLayout;

	QToolBar*			m_pToolBar;
	QSpinBox*			m_pLimit;
	QSpinBox*			m_pPage;
	QLabel*				m_pPages;
	QLineEdit*			m_pFilter;

	QSplitter			*m_pIndexSplitter;
	QTreeWidget 		*m_pIndexTree;
	QTextEdit			*m_pIndexEdit;
};