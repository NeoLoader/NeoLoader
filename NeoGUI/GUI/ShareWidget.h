#pragma once

#include "SettingsView.h"

class CFileShareModel;

class CShareWidget: public CSettingsView
{
	Q_OBJECT

public:
	CShareWidget(QMultiMap<int, SField> Layout, const QString& Title = "", QWidget *parent = 0);
	~CShareWidget();

public slots:
	void				UpdateSettings();
	void				ApplySettings();

private slots:
	void				OnItemClicked(const QModelIndex& index);
	void				OnItemDoubleClicked(QListWidgetItem* item);

protected:
	friend class CShareUpdateJob;
	friend class CShareApplyJob;

	//QVBoxLayout*		m_pMainLayout;
	QSplitter*			m_pSplitter;

	QTreeView*			m_pFsTree;
	CFileShareModel*	m_pFsModel;

	QListWidget*		m_pSharedList;
};