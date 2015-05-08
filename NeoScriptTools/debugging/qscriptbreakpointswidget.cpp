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

#include "qscriptbreakpointswidget_p.h"
#include "qscriptbreakpointswidgetinterface_p_p.h"

#include "qscriptbreakpointsmodel_p.h"
#include "qscriptdebuggerscriptsmodel_p.h"

#include <QtCore/qdebug.h>
#if QT_VERSION < 0x050000
#include <QtGui/qaction.h>
#include <QtGui/qcompleter.h>
#include <QtGui/qheaderview.h>
#include <QtGui/qlineedit.h>
#include <QtGui/qmessagebox.h>
#include <QtGui/qtoolbar.h>
#include <QtGui/qtoolbutton.h>
#include <QtGui/qtreeview.h>
#include <QtGui/qboxlayout.h>
#include <QtGui/qstyleditemdelegate.h>
#else
#include <QtWidgets/qaction.h>
#include <QtWidgets/qcompleter.h>
#include <QtWidgets/qheaderview.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qmessagebox.h>
#include <QtWidgets/qtoolbar.h>
#include <QtWidgets/qtoolbutton.h>
#include <QtWidgets/qtreeview.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qstyleditemdelegate.h>
#endif
#include <QtGui/qevent.h>
#ifndef NO_QT_SCRIPT
#include <QtScript/qscriptengine.h>
#endif

QT_BEGIN_NAMESPACE


QScriptBreakpointsWidgetPrivate::QScriptBreakpointsWidgetPrivate()
{
}

QScriptBreakpointsWidgetPrivate::~QScriptBreakpointsWidgetPrivate()
{
}

void QScriptBreakpointsWidgetPrivate::_q_newBreakpoint()
{
    newBreakpointWidget->show();
    newBreakpointWidget->setFocus(Qt::OtherFocusReason);
}

void QScriptBreakpointsWidgetPrivate::_q_deleteBreakpoint()
{
    Q_Q(QScriptBreakpointsWidget);
    QModelIndex index = view->currentIndex();
    if (index.isValid()) {
        int id = q->breakpointsModel()->breakpointIdAt(index.row());
        q->breakpointsModel()->deleteBreakpoint(id);
    }
}

void QScriptBreakpointsWidgetPrivate::_q_onCurrentChanged(const QModelIndex &index)
{
    deleteBreakpointAction->setEnabled(index.isValid());
}

void QScriptBreakpointsWidgetPrivate::_q_onNewBreakpointRequest(const QString &fileName, int lineNumber)
{
    QScriptBreakpointData data(fileName, lineNumber);
    q_func()->breakpointsModel()->setBreakpoint(data);
}

QScriptBreakpointsWidget::QScriptBreakpointsWidget(QWidget *parent)
    : QScriptBreakpointsWidgetInterface(*new QScriptBreakpointsWidgetPrivate, parent, 0)
{
    Q_D(QScriptBreakpointsWidget);
    d->view = new QTreeView();
//    d->view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    d->view->setEditTriggers(QAbstractItemView::AllEditTriggers);
//    d->view->setAlternatingRowColors(true);
    d->view->setRootIsDecorated(false);
    d->view->setSelectionBehavior(QAbstractItemView::SelectRows);
//    d->view->header()->hide();
//    d->view->header()->setDefaultAlignment(Qt::AlignLeft);
//    d->view->header()->setResizeMode(QHeaderView::ResizeToContents);
    d->view->setItemDelegate(new QScriptBreakpointsItemDelegate(this));

    d->newBreakpointWidget = new QScriptNewBreakpointWidget();
    d->newBreakpointWidget->hide();
    QObject::connect(d->newBreakpointWidget, SIGNAL(newBreakpointRequest(QString,int)),
                     this, SLOT(_q_onNewBreakpointRequest(QString,int)));

    QIcon newBreakpointIcon;
    newBreakpointIcon.addPixmap(d->pixmap(QString::fromLatin1("new.png")), QIcon::Normal);
    QAction *newBreakpointAction = new QAction(newBreakpointIcon, tr("New"), this);
    QObject::connect(newBreakpointAction, SIGNAL(triggered()),
                     this, SLOT(_q_newBreakpoint()));

    QIcon deleteBreakpointIcon;
    deleteBreakpointIcon.addPixmap(d->pixmap(QString::fromLatin1("delete.png")), QIcon::Normal);
    d->deleteBreakpointAction = new QAction(deleteBreakpointIcon, tr("Delete"), this);
    d->deleteBreakpointAction->setEnabled(false);
    QObject::connect(d->deleteBreakpointAction, SIGNAL(triggered()),
                     this, SLOT(_q_deleteBreakpoint()));

#ifndef QT_NO_TOOLBAR
    QToolBar *toolBar = new QToolBar();
    toolBar->addAction(newBreakpointAction);
    toolBar->addAction(d->deleteBreakpointAction);
#endif

    QVBoxLayout *vbox = new QVBoxLayout(this);
    vbox->setMargin(0);
#ifndef QT_NO_TOOLBAR
    vbox->addWidget(toolBar);
#endif
    vbox->addWidget(d->newBreakpointWidget);
    vbox->addWidget(d->view);
}

QScriptBreakpointsWidget::~QScriptBreakpointsWidget()
{
}

/*!
  \reimp
*/
QScriptBreakpointsModel *QScriptBreakpointsWidget::breakpointsModel() const
{
    Q_D(const QScriptBreakpointsWidget);
    return qobject_cast<QScriptBreakpointsModel*>(d->view->model());
}

/*!
  \reimp
*/
void QScriptBreakpointsWidget::setBreakpointsModel(QScriptBreakpointsModel *model)
{
    Q_D(QScriptBreakpointsWidget);
    d->view->setModel(model);
    d->view->header()->resizeSection(0, 50);
    QObject::connect(d->view->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)),
                     this, SLOT(_q_onCurrentChanged(QModelIndex)));
}

/*!
  \reimp
*/
QScriptDebuggerScriptsModel *QScriptBreakpointsWidget::scriptsModel() const
{
    Q_D(const QScriptBreakpointsWidget);
    return d->scriptsModel;
}

/*!
  \reimp
*/
void QScriptBreakpointsWidget::setScriptsModel(QScriptDebuggerScriptsModel *model)
{
    Q_D(QScriptBreakpointsWidget);
    d->scriptsModel = model;
    QCompleter *completer = new QCompleter(model, this);
    completer->setCompletionRole(Qt::DisplayRole);
    d->newBreakpointWidget->setCompleter(completer);
}

/*!
  \reimp
*/
void QScriptBreakpointsWidget::keyPressEvent(QKeyEvent *e)
{
    Q_D(QScriptBreakpointsWidget);
    if (e->key() == Qt::Key_Delete) {
        QModelIndex index = d->view->currentIndex();
        if (!index.isValid())
            return;
        int id = breakpointsModel()->breakpointIdAt(index.row());
        breakpointsModel()->deleteBreakpoint(id);
    }
}

QT_END_NAMESPACE
