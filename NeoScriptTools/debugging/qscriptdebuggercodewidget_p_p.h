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

#ifndef QSCRIPTDEBUGGERCODEWIDGET_P_P_H
#define QSCRIPTDEBUGGERCODEWIDGET_P_P_H

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

#include <QtCore/qdebug.h>
#if QT_VERSION < 0x050000
#include <QtGui/qboxlayout.h>
#include <QtGui/qstackedwidget.h>
//> NeoScriptTools
#include <QtGui/qtabwidget.h>
//< NeoScriptTools
#else
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qstackedwidget.h>
//> NeoScriptTools
#include <QtWidgets/qtabwidget.h>
//< NeoScriptTools
#endif

#include "qscriptdebuggercodewidgetinterface_p_p.h"
#include "qscriptbreakpointsmodel_p.h"

QT_BEGIN_NAMESPACE

class QScriptDebuggerCodeViewInterface;
class QScriptDebuggerCodeWidgetPrivate
    : public QScriptDebuggerCodeWidgetInterfacePrivate
{
    Q_DECLARE_PUBLIC(QScriptDebuggerCodeWidget)
public:
    QScriptDebuggerCodeWidgetPrivate();
    ~QScriptDebuggerCodeWidgetPrivate();

    qint64 scriptId(QScriptDebuggerCodeViewInterface *view) const;

    // private slots
    void _q_onBreakpointToggleRequest(int lineNumber, bool on);
    void _q_onBreakpointEnableRequest(int lineNumber, bool enable);
    void _q_onBreakpointsAboutToBeRemoved(const QModelIndex&, int, int);
    void _q_onBreakpointsInserted(const QModelIndex&, int, int);
    void _q_onBreakpointsDataChanged(const QModelIndex &, const QModelIndex &);
    void _q_onScriptsChanged();
    void _q_onToolTipRequest(const QPoint &pos, int lineNumber, const QStringList &path);
	//> NeoScriptTools
	void _q_onModificationChanged(bool changed);
	//< NeoScriptTools

	QScriptDebuggerJobSchedulerInterface *jobScheduler;
    QScriptDebuggerCommandSchedulerInterface* commandScheduler;
    QScriptDebuggerScriptsModel *scriptsModel;
	//> NeoScriptTools
	QMap<QScriptDebuggerCustomViewInterfaceFactory, QHash<qint64, QScriptDebuggerCustomViewInterface*> > resourceHash;
	QTabWidget* viewStack;
	bool readOnly;
	//< NeoScriptTools
	//QStackedWidget *viewStack;
    QHash<qint64, QScriptDebuggerCodeViewInterface*> viewHash;
    QScriptBreakpointsModel *breakpointsModel;
    QScriptToolTipProviderInterface *toolTipProvider;
};

QT_END_NAMESPACE

#endif
