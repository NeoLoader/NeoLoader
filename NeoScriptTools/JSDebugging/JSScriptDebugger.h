/****************************************************************************
**
** Copyright (C) 2012 NeoLoader Team
** All rights reserved.
** Contact: NeoLoader.to
**
** This file is part of the NeoScriptTools module for NeoLoader
**
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
**
**
****************************************************************************/

#ifndef CJSSCRIPTDEBUGGER_H
#define CJSSCRIPTDEBUGGER_H


#include <QMainWindow>
#include "../neoscripttools_global.h"
#include "JSScriptDebuggerFrontendInterface.h"
class QScriptDebuggerCustomViewInterface;

class NEOSCRIPTTOOLS_EXPORT CJSScriptDebugger: public QMainWindow
{
    Q_OBJECT
public:
    // mirrors QScriptEngineDebugger::DebuggerWidget
    enum DebuggerWidget {
        ConsoleWidget,
        StackWidget,
        ScriptsWidget,
        LocalsWidget,
        CodeWidget,
        CodeFinderWidget,
        BreakpointsWidget,
        DebugOutputWidget,
        ErrorLogWidget
    };
    // mirrors QScriptEngineDebugger::DebuggerAction
    enum DebuggerAction {
        InterruptAction,
        ContinueAction,
        StepIntoAction,
        StepOverAction,
        StepOutAction,
        RunToCursorAction,
        //RunToNewScriptAction,
        ToggleBreakpointAction,
        ClearDebugOutputAction,
        ClearErrorLogAction,
        ClearConsoleAction,
        FindInScriptAction,
        FindNextInScriptAction,
        FindPreviousInScriptAction,
        GoToLineAction
    };

#if QT_VERSION < 0x050000
    CJSScriptDebugger(QWidget *parent = 0, Qt::WFlags flags = 0);
#else
    CJSScriptDebugger(QWidget *parent = 0, Qt::WindowFlags flags = 0);
#endif
    virtual ~CJSScriptDebugger();

    virtual void attachTo(CJSScriptDebuggerFrontendInterface *frontend);

	virtual void openScript(const QString& fileName);

	void setCurrentCustom(qint64 customID, const QString& name, QScriptDebuggerCustomViewInterface* (*factory)(qint64, void*), void* param = NULL);
	QScriptDebuggerCustomViewInterface* currentCustom();
	virtual bool isInteractive();
	virtual QString currentName() const;
	virtual QString currentText(bool bReset = false);
	virtual void renameCurrent(const QString& FileName);

	static QPixmap pixmap(const QString &path, bool bGray = false, bool bOS = true);

signals:
	void detach();

protected:
	virtual void closeEvent(QCloseEvent *e);

	virtual void setup();

    virtual QToolBar *createStandardToolBar(QWidget *parent = 0);
    virtual QMenu *createStandardMenu(QWidget *parent = 0);

    virtual QWidget *widget(int widget) const;
    virtual QAction *action(int action) const;

    CJSScriptDebuggerFrontendInterface *m_frontend;

private:
    QScriptDebugger *m_debugger;

    Q_DISABLE_COPY(CJSScriptDebugger)
};

#endif
