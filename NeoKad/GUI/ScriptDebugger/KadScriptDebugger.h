#pragma once

#include <QAction>
#include <QToolBar>
#include <QWidget>
#include <QMenu>
#include <QStatusBar>
#include <QFileDialog>
#include <QMenuBar>
#include <QDockWidget>

#include "../../../NeoScriptTools/JSDebugging/JSScriptDebugger.h"
#include "../../../Framework/IPC/JobManager.h"

class CSettings;
class CKadScriptWidget;
class CKadScriptDebuggerFrontend;
class CKadScriptDebugger: public CJSScriptDebugger
{
	Q_OBJECT
public:
	CKadScriptDebugger(const QString& Pipe);
	~CKadScriptDebugger();

	virtual void		attachTo(CKadScriptDebuggerFrontend *frontend);

	CKadScriptDebuggerFrontend* frontend();

	CSettings*			Cfg()		{return m_Settings;}
	void				ScheduleJob(CInterfaceJob* pJob);
	void				UnScheduleJob(CInterfaceJob* pJob);

private slots:
	void OnNew();
	void OnLoad();
	void OnSave();
	void OnSaveAs();

	void OnTerminate();

protected:
	virtual void setup();

	virtual QToolBar *createStandardToolBar(QWidget *parent = 0);
    virtual QMenu *createStandardMenu(QWidget *parent = 0);
	virtual QMenu *createFileMenu(QWidget *parent = 0);

	virtual QWidget *widget(int widget) const;
    virtual QAction *action(int action) const;

	virtual void		timerEvent(QTimerEvent *e);

	int					m_TimerId;

	CSettings*			m_Settings;
	CJobManager*		m_Manager;

private:
	CKadScriptWidget*	m_KadScriptWidget;

	QAction*			m_NewAction;
	QAction*			m_LoadAction;
	QAction*			m_SaveAction;
	QAction*			m_SaveAsAction;

	QAction*			m_TerminateAction;
};

class CKadActionJob: public CInterfaceJob
{
public:
	CKadActionJob(const QString& Action, const QVariantMap& Scope = QVariantMap())
	{
		m_Command = Action;
		m_Request = Scope;
	}

	void					Set(const QString& Name, const QVariant& Value) {m_Request[Name] = Value;}
	
	virtual QString			GetCommand()	{return m_Command;}
	virtual void			HandleResponse(const QVariantMap& Response) {}

protected:
	QString	m_Command;
};