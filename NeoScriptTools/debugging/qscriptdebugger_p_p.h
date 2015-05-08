/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the QtSCriptTools module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QSCRIPTDEBUGGER_P_P_H
#define QSCRIPTDEBUGGER_P_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "private/qobject_p.h"

#include "qscriptdebugger_p.h"
#include "qscriptdebuggerconsolehistorianinterface_p.h"
#ifndef NO_QT_SCRIPT
#include "qscriptdebuggerconsole_p.h"
#include "qscriptdebuggerconsolecommandmanager_p.h"
#include "qscriptdebuggerconsolecommandjob_p.h"
#endif
#include "qscriptstdmessagehandler_p.h"
#include "qscriptdebuggerfrontend_p.h"
#include "qscriptdebuggereventhandlerinterface_p.h"
#include "qscriptdebuggerresponsehandlerinterface_p.h"
#include "qscriptdebuggerjobschedulerinterface_p.h"
#include "qscriptdebuggerconsolewidgetinterface_p.h"
#include "qscriptcompletionproviderinterface_p.h"
#include "qscriptcompletiontask_p.h"
#include "qscripttooltipproviderinterface_p.h"
#include "qscriptdebuggerstackwidgetinterface_p.h"
#include "qscriptdebuggerstackmodel_p.h"
#include "qscriptdebuggerscriptswidgetinterface_p.h"
#include "qscriptdebuggerscriptsmodel_p.h"
#include "qscriptdebuggerlocalswidgetinterface_p.h"
#include "qscriptdebuggerlocalsmodel_p.h"
#include "qscriptdebuggercodewidgetinterface_p.h"
#include "qscriptdebuggercodeviewinterface_p.h"
#include "qscriptdebuggercodefinderwidgetinterface_p.h"
#include "qscriptbreakpointswidgetinterface_p.h"
#include "qscriptbreakpointsmodel_p.h"
#include "qscriptdebugoutputwidgetinterface_p.h"
#include "qscripterrorlogwidgetinterface_p.h"
#include "qscriptdebuggerwidgetfactoryinterface_p.h"
#include "qscriptdebuggerevent_p.h"
#include "qscriptdebuggervalue_p.h"
#include "qscriptdebuggerresponse_p.h"
#include "qscriptdebuggercommand_p.h"
#include "qscriptdebuggercommandschedulerfrontend_p.h"
#include "qscriptdebuggercommandschedulerjob_p.h"
#include "qscriptdebuggerjob_p_p.h"
#include "qscriptxmlparser_p.h"
#include "qscriptcompletionproviderinterface_p.h"

QT_BEGIN_NAMESPACE

class QScriptDebuggerPrivate
    : public QObjectPrivate,
      public QScriptDebuggerCommandSchedulerInterface,
      public QScriptDebuggerJobSchedulerInterface,
      public QScriptDebuggerEventHandlerInterface,
      public QScriptDebuggerResponseHandlerInterface,
      public QScriptCompletionProviderInterface,
      public QScriptToolTipProviderInterface
{
    Q_DECLARE_PUBLIC(QScriptDebugger)
public:
    QScriptDebuggerPrivate();
    ~QScriptDebuggerPrivate();

    int scheduleJob(QScriptDebuggerJob *job);
    void finishJob(QScriptDebuggerJob *job);
    void hibernateUntilEvaluateFinished(QScriptDebuggerJob *job);

    void maybeStartNewJob();

    int scheduleCommand(const QScriptDebuggerCommand &command,
                        QScriptDebuggerResponseHandlerInterface *responseHandler);

    void handleResponse(
        const QScriptDebuggerResponse &response, int commandId);
    bool debuggerEvent(const QScriptDebuggerEvent &event);

    QScriptCompletionTaskInterface *createCompletionTask(
        const QString &contents, int cursorPosition, int frameIndex, int options);

    void showToolTip(const QPoint &pos, int frameIndex,
                     int lineNumber, const QStringList &path);

    static QPixmap pixmap(const QString &path);

    void startInteraction(QScriptDebuggerEvent::Type type,
                          qint64 scriptId, int lineNumber);
    void sync();
    void loadLocals(int frameIndex);
    QScriptDebuggerLocalsModel *createLocalsModel();
    void selectScriptForFrame(int frameIndex);
    void emitStoppedSignal();

    void maybeDelete(QWidget *widget);

    // private slots
    void _q_onLineEntered(const QString &contents);
    void _q_onCurrentFrameChanged(int frameIndex);
    void _q_onCurrentScriptChanged(qint64 scriptId);
    void _q_onScriptLocationSelected(int lineNumber);
    void _q_interrupt();
    void _q_continue();
    void _q_stepInto();
    void _q_stepOver();
    void _q_stepOut();
    void _q_runToCursor();
    void _q_runToNewScript();
    void _q_toggleBreakpoint();
    void _q_clearDebugOutput();
    void _q_clearErrorLog();
    void _q_clearConsole();
    void _q_findInScript();
    void _q_findNextInScript();
    void _q_findPreviousInScript();
    void _q_onFindCodeRequest(const QString &, int);
    void _q_goToLine();

    void executeConsoleCommand(const QString &command);
    void findCode(const QString &exp, int options);

    QScriptDebuggerFrontend *frontend;

    bool interactive;
#ifndef NO_QT_SCRIPT
    QScriptDebuggerConsole *console;
#else
	QScriptDebuggerConsoleHistorian *historian;
#endif

    int nextJobId;
    QList<QScriptDebuggerJob*> pendingJobs;
    QList<int> pendingJobIds;
    QScriptDebuggerJob *activeJob;
    bool activeJobHibernating;
    QHash<int, QScriptDebuggerCommand> watchedCommands;
    QHash<int, QScriptDebuggerResponseHandlerInterface*> responseHandlers;

    QScriptDebuggerConsoleWidgetInterface *consoleWidget;
    QScriptDebuggerStackWidgetInterface *stackWidget;
    QScriptDebuggerStackModel *stackModel;
    QScriptDebuggerScriptsWidgetInterface *scriptsWidget;
    QScriptDebuggerScriptsModel *scriptsModel;
    QScriptDebuggerLocalsWidgetInterface *localsWidget;
    QHash<int, QScriptDebuggerLocalsModel*> localsModels;
    QScriptDebuggerCodeWidgetInterface *codeWidget;
    QScriptDebuggerCodeFinderWidgetInterface *codeFinderWidget;
    QScriptBreakpointsWidgetInterface *breakpointsWidget;
    QScriptBreakpointsModel *breakpointsModel;
    QScriptDebugOutputWidgetInterface *debugOutputWidget;
    QScriptErrorLogWidgetInterface *errorLogWidget;
    QScriptDebuggerWidgetFactoryInterface *widgetFactory;

    QAction *interruptAction;
    QAction *continueAction;
    QAction *stepIntoAction;
    QAction *stepOverAction;
    QAction *stepOutAction;
    QAction *runToCursorAction;
    //QAction *runToNewScriptAction;

    QAction *toggleBreakpointAction;

    QAction *clearDebugOutputAction;
    QAction *clearErrorLogAction;
    QAction *clearConsoleAction;

    QAction *findInScriptAction;
    QAction *findNextInScriptAction;
    QAction *findPreviousInScriptAction;
    QAction *goToLineAction;

    int updatesEnabledTimerId;
};

QT_END_NAMESPACE

#endif
