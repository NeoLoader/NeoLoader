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

#ifndef QSCRIPTDEBUGGERCODEWIDGETINTERFACE_P_H
#define QSCRIPTDEBUGGERCODEWIDGETINTERFACE_P_H

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

#include <QtCore/qglobal.h>
#if QT_VERSION < 0x050000
#include <QtGui/qwidget.h>
#else
#include <QtWidgets/qwidget.h>
#endif

QT_BEGIN_NAMESPACE

class QScriptDebuggerScriptsModel;
class QScriptBreakpointsModel;
class QScriptToolTipProviderInterface;
class QScriptDebuggerJobSchedulerInterface;
class QScriptDebuggerCommandSchedulerInterface;

class QScriptDebuggerCodeViewInterface;

//> NeoScriptTools
class QScriptDebuggerCustomViewInterface;
typedef QScriptDebuggerCustomViewInterface* (*QScriptDebuggerCustomViewInterfaceFactory)(qint64, void*);
//< NeoScriptTools

class QScriptDebuggerCodeWidgetInterfacePrivate;
class Q_AUTOTEST_EXPORT QScriptDebuggerCodeWidgetInterface:
    public QWidget
{
    Q_OBJECT
public:
    ~QScriptDebuggerCodeWidgetInterface();

    virtual QScriptDebuggerScriptsModel *scriptsModel() const = 0;
    virtual void setScriptsModel(QScriptDebuggerScriptsModel *model, 
								QScriptDebuggerJobSchedulerInterface *jobScheduler = NULL,
                                QScriptDebuggerCommandSchedulerInterface *commandScheduler = NULL) = 0;

    virtual QScriptBreakpointsModel *breakpointsModel() const = 0;
    virtual void setBreakpointsModel(QScriptBreakpointsModel *model) = 0;

    virtual void setToolTipProvider(QScriptToolTipProviderInterface *toolTipProvider) = 0;

    virtual qint64 currentScriptId() const = 0;
    virtual bool setCurrentScript(qint64 scriptId) = 0;
	virtual void setCurrentScript(const QString& fileName) = 0;

    virtual void invalidateExecutionLineNumbers() = 0;

    virtual QScriptDebuggerCodeViewInterface *currentView() const = 0;

	//> NeoScriptTools
	virtual int find(const QString &exp, int options = 0) = 0;

	virtual void setReadOnly(bool set) = 0;

    virtual void setCurrentCustom(qint64 customID, const QString& name, QScriptDebuggerCustomViewInterfaceFactory factory, void* param = 0) = 0;

	virtual QScriptDebuggerCustomViewInterface *currentCustom() const = 0;

	virtual QString currentName() const = 0;

	virtual void renameCurrent(QScriptDebuggerCodeViewInterface* view, const QString& name) = 0;
	//< NeoScriptTools

protected:
    QScriptDebuggerCodeWidgetInterface(
        QScriptDebuggerCodeWidgetInterfacePrivate &dd,
        QWidget *parent, Qt::WindowFlags flags);

private:
    Q_DECLARE_PRIVATE(QScriptDebuggerCodeWidgetInterface)
    Q_DISABLE_COPY(QScriptDebuggerCodeWidgetInterface)
};

QT_END_NAMESPACE

#endif
