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

#ifndef QSCRIPTENGINEDEBUGGER_P
#define QSCRIPTENGINEDEBUGGER_P

#include <QWidget>
#include <QEvent>

#include <private/qobject_p.h>

#include "qscriptenginedebugger.h"

class WidgetClosedNotifier : public QObject
{
    Q_OBJECT
public:
    WidgetClosedNotifier(QWidget *w, QObject *parent = 0)
        : QObject(parent), widget(w)
    {
        w->installEventFilter(this);
    }

    bool eventFilter(QObject *watched, QEvent *e)
    {
        if (watched != widget)
            return false;
        if (e->type() != QEvent::Close)
            return false;
        emit widgetClosed();
        return true;
    }

Q_SIGNALS:
    void widgetClosed();

private:
    QWidget *widget;
};

QT_BEGIN_NAMESPACE

class QScriptDebugger;
class QScriptEngineDebuggerFrontend;

extern void initScriptEngineDebuggerResources();

class QtScriptDebuggerResourceInitializer
{
public:
    QtScriptDebuggerResourceInitializer() {
        // call outside-the-namespace function
        initScriptEngineDebuggerResources();
    }
};

class QScriptEngineDebuggerPrivate
    : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QScriptEngineDebugger)
public:
    QScriptEngineDebuggerPrivate();
    ~QScriptEngineDebuggerPrivate();

    // private slots
    void _q_showStandardWindow();

    void createDebugger();

    QScriptDebugger *debugger;
    QScriptEngineDebuggerFrontend *frontend;
#ifndef QT_NO_MAINWINDOW
    QMainWindow *standardWindow;
#endif
    bool autoShow;

    static QtScriptDebuggerResourceInitializer resourceInitializer;
};

QT_END_NAMESPACE

#endif