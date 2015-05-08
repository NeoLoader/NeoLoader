#pragma once

#include <QVBoxLayout>
#include <QToolBar>
#include <QTreeWidget>
#include <QAction>
#include <QHeaderView>
#include <QMessageBox>
#include <QInputDialog>

class CKadScriptDebugger;
class CKadScriptSyncJob;

class CKadScriptWidget: public QWidget
{
	Q_OBJECT
public:
	CKadScriptWidget(CKadScriptDebugger* pDebugger, QWidget *parent = 0);
    ~CKadScriptWidget();

	void					DissableToolbar(bool bSet);

	QByteArray				saveState() const						{return m_pScriptTree->header()->saveState();}
	bool					restoreState(const QByteArray& state)	{return m_pScriptTree->header()->restoreState(state);}

private slots:
	void					OnItemChanged(QTreeWidgetItem* pItem, QTreeWidgetItem*);
	void					OnItemChanged(QTreeWidgetItem* pItem, int Column);
	//void					OnItemDoubleClicked(QTreeWidgetItem* pItem, int Column);

	void					OnDelScript();
	void					OnKillScript();
	void					OnAddTask();
	void					OnRemTask();

protected:
	friend class CKadScriptSyncJob;
	void					SyncKadScripts();
	void					SyncKadScripts(const QVariantMap& Response);

	QVariantMap				ResolveScope(QTreeWidgetItem* pItem);

	virtual void			timerEvent(QTimerEvent *e);

	int						m_TimerId;

	CKadScriptDebugger*		m_pDebugger;
	CKadScriptSyncJob*		m_pKadScriptSyncJob;

	QVBoxLayout*			m_pMainLayout;
	QToolBar*				m_pToolBar;
	QTreeWidget*			m_pScriptTree;

	QAction*				m_pKillScript;
	QAction*				m_pAddTask;
	QAction*				m_pRemTask;
	QAction*				m_pDelScript;

	enum ELevels
	{
		eScript,
		eTaskList,
		eTask,
		eRoute,
		eRouteSession,
	};
	QMap<ELevels, QStringList>	m_Headers;
};