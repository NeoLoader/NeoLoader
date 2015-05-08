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

#include "qscriptdebuggercodewidget_p.h"
#include "qscriptdebuggercodewidgetinterface_p_p.h"
#include "qscriptdebuggercodeview_p.h"
#include "qscriptdebuggerscriptsmodel_p.h"
#include "qscriptbreakpointsmodel_p.h"
#include "qscripttooltipproviderinterface_p.h"
#include "qscriptdebuggercommandschedulerjob_p.h"
#include "qscriptdebuggercommandschedulerfrontend_p.h"
#include "qscriptdebuggerjobschedulerinterface_p.h"
#include "qscriptdebuggerresponse_p.h"
//> NeoScriptTools
#include "qscriptdebuggercustomviewinterface.h"
//< NeoScriptTools
#if QT_VERSION < 0x050000
#include <QtGui/qmessagebox.h>
#else
#include <QtWidgets/qmessagebox.h>
#endif
#include <QtCore/qdebug.h>
#include <QtCore/qfileinfo.h>

QT_BEGIN_NAMESPACE

QScriptDebuggerCodeWidgetPrivate::QScriptDebuggerCodeWidgetPrivate()
{
	jobScheduler = 0;
    commandScheduler = 0;
	//> NeoScriptTools
	readOnly = false;
	//< NeoScriptTools
    scriptsModel = 0;
    breakpointsModel = 0;
    toolTipProvider = 0;
}

QScriptDebuggerCodeWidgetPrivate::~QScriptDebuggerCodeWidgetPrivate()
{
}

qint64 QScriptDebuggerCodeWidgetPrivate::scriptId(QScriptDebuggerCodeViewInterface *view) const
{
    if (!view)
        return -1;
	//> NeoScriptTools
	return viewHash.key(view, -1);
	//< NeoScriptTools
    //return viewHash.key(view);
}

void QScriptDebuggerCodeWidgetPrivate::_q_onBreakpointToggleRequest(int lineNumber, bool on)
{
    QScriptDebuggerCodeViewInterface *view = qobject_cast<QScriptDebuggerCodeViewInterface*>(q_func()->sender());
    qint64 sid = scriptId(view);
    Q_ASSERT(sid != -1);
    if (on) {
        QScriptBreakpointData data(sid, lineNumber);
        data.setFileName(scriptsModel->scriptData(sid).fileName());
        breakpointsModel->setBreakpoint(data);
    } else {
        int bpid = breakpointsModel->resolveBreakpoint(sid, lineNumber);
        if (bpid == -1)
            bpid = breakpointsModel->resolveBreakpoint(scriptsModel->scriptData(sid).fileName(), lineNumber);
        Q_ASSERT(bpid != -1);
        breakpointsModel->deleteBreakpoint(bpid);
    }
}

void QScriptDebuggerCodeWidgetPrivate::_q_onBreakpointEnableRequest(int lineNumber, bool enable)
{
    QScriptDebuggerCodeViewInterface *view = qobject_cast<QScriptDebuggerCodeViewInterface*>(q_func()->sender());
    qint64 sid = scriptId(view);
    int bpid = breakpointsModel->resolveBreakpoint(sid, lineNumber);
    if (bpid == -1)
        bpid = breakpointsModel->resolveBreakpoint(scriptsModel->scriptData(sid).fileName(), lineNumber);
    Q_ASSERT(bpid != -1);
    QScriptBreakpointData data = breakpointsModel->breakpointData(bpid);
    data.setEnabled(enable);
    breakpointsModel->setBreakpointData(bpid, data);
}

void QScriptDebuggerCodeWidgetPrivate::_q_onBreakpointsAboutToBeRemoved(
    const QModelIndex &, int first, int last)
{
    for (int i = first; i <= last; ++i) {
        QScriptBreakpointData data = breakpointsModel->breakpointDataAt(i);
        qint64 scriptId = data.scriptId();
        if (scriptId == -1) {
            scriptId = scriptsModel->resolveScript(data.fileName());
            if (scriptId == -1)
                continue;
        }
        QScriptDebuggerCodeViewInterface *view = viewHash.value(scriptId);
        if (!view)
            continue;
        view->deleteBreakpoint(data.lineNumber());
    }
}

void QScriptDebuggerCodeWidgetPrivate::_q_onBreakpointsInserted(
    const QModelIndex &, int first, int last)
{
    for (int i = first; i <= last; ++i) {
        QScriptBreakpointData data = breakpointsModel->breakpointDataAt(i);
        qint64 scriptId = data.scriptId();
        if (scriptId == -1) {
            scriptId = scriptsModel->resolveScript(data.fileName());
            if (scriptId == -1)
                continue;
        }
        QScriptDebuggerCodeViewInterface *view = viewHash.value(scriptId);
        if (!view)
            continue;
        view->setBreakpoint(data.lineNumber());
    }
}

void QScriptDebuggerCodeWidgetPrivate::_q_onBreakpointsDataChanged(
    const QModelIndex &tl, const QModelIndex &br)
{
    for (int i = tl.row(); i <= br.row(); ++i) {
        QScriptBreakpointData data = breakpointsModel->breakpointDataAt(i);
        qint64 scriptId = data.scriptId();
        if (scriptId == -1) {
            scriptId = scriptsModel->resolveScript(data.fileName());
            if (scriptId == -1)
                continue;
        }
        QScriptDebuggerCodeViewInterface *view = viewHash.value(scriptId);
        if (!view)
            continue;
        view->setBreakpointEnabled(data.lineNumber(), data.isEnabled());
    }
}

void QScriptDebuggerCodeWidgetPrivate::_q_onScriptsChanged()
{
    // kill editors for scripts that have been removed
    QHash<qint64, QScriptDebuggerCodeViewInterface*>::iterator it;
    for (it = viewHash.begin(); it != viewHash.end(); ) {
        if (!scriptsModel->scriptData(it.key()).isValid()) {
            it = viewHash.erase(it);
        } else
            ++it;
    }
}

void QScriptDebuggerCodeWidgetPrivate::_q_onToolTipRequest(
    const QPoint &pos, int lineNumber, const QStringList &path)
{
    toolTipProvider->showToolTip(pos, /*frameIndex=*/-1, lineNumber, path);
}

//> NeoScriptTools
void QScriptDebuggerCodeWidgetPrivate::_q_onModificationChanged(bool changed)
{
	int index = viewStack->currentIndex();
	QString Name = viewStack->tabText(index);
	if((Name.right(1) == "*") != changed) {
		if(changed)
			viewStack->setTabText(index, Name + "*");
		else
			viewStack->setTabText(index, Name.left(Name.length()-1));
	}
}
//< NeoScriptTools

QScriptDebuggerCodeWidget::QScriptDebuggerCodeWidget(QWidget *parent)
    : QScriptDebuggerCodeWidgetInterface(*new QScriptDebuggerCodeWidgetPrivate, parent, 0)
{
    Q_D(QScriptDebuggerCodeWidget);
    QVBoxLayout *vbox = new QVBoxLayout(this);
    vbox->setMargin(0);
	//> NeoScriptTools
	d->viewStack = new QTabWidget();
	d->viewStack->setTabsClosable(true);
	connect(d->viewStack, SIGNAL(tabCloseRequested(int)), this, SLOT(tabCloseRequested(int)));
	//< NeoScriptTools
    //d->viewStack = new QStackedWidget();
    vbox->addWidget(d->viewStack);
}

QScriptDebuggerCodeWidget::~QScriptDebuggerCodeWidget()
{
}

QScriptDebuggerScriptsModel *QScriptDebuggerCodeWidget::scriptsModel() const
{
    Q_D(const QScriptDebuggerCodeWidget);
    return d->scriptsModel;
}

void QScriptDebuggerCodeWidget::setScriptsModel(QScriptDebuggerScriptsModel *model, 
								QScriptDebuggerJobSchedulerInterface *jobScheduler,
                                QScriptDebuggerCommandSchedulerInterface *commandScheduler)
{
    Q_D(QScriptDebuggerCodeWidget);
    d->scriptsModel = model;
	d->jobScheduler = jobScheduler;
	d->commandScheduler = commandScheduler;
    QObject::connect(model, SIGNAL(layoutChanged()),
                     this, SLOT(_q_onScriptsChanged()));
}

qint64 QScriptDebuggerCodeWidget::currentScriptId() const
{
    Q_D(const QScriptDebuggerCodeWidget);
    return d->scriptId(currentView());
}

bool QScriptDebuggerCodeWidget::setCurrentScript(qint64 scriptId)
{
    Q_D(QScriptDebuggerCodeWidget);
    if (scriptId == -1) {
        // ### show "native script"
        return false;
    }
    QScriptDebuggerCodeViewInterface *view = d->viewHash.value(scriptId);
    if (!view) {
		Q_ASSERT(d->scriptsModel != 0);
		QScriptScriptData data = d->scriptsModel->scriptData(scriptId);
		if (!data.isValid())
			return false;
		view = setCurrentScript(scriptId, data);
		//view->setExecutableLineNumbers(d->scriptsModel->executableLineNumbers(scriptId));
    } else {
		d->viewStack->setCurrentWidget(view);
	}
	return true;
}

QScriptDebuggerCodeViewInterface* QScriptDebuggerCodeWidget::setCurrentScript(qint64 scriptId, const QScriptScriptData& data)
{
	Q_D(QScriptDebuggerCodeWidget);

    QScriptDebuggerCodeViewInterface* view = new QScriptDebuggerCodeView(); // ### use factory, so user can provide his own view
	view->setName(data.fileName());
    view->setBaseLineNumber(data.baseLineNumber());
    view->setText(data.contents());
    QObject::connect(view, SIGNAL(breakpointToggleRequest(int,bool)),
                        this, SLOT(_q_onBreakpointToggleRequest(int,bool)));
    QObject::connect(view, SIGNAL(breakpointEnableRequest(int,bool)),
                        this, SLOT(_q_onBreakpointEnableRequest(int,bool)));
    QObject::connect(view, SIGNAL(toolTipRequest(QPoint,int,QStringList)),
                        this, SLOT(_q_onToolTipRequest(QPoint,int,QStringList)));
	//> NeoScriptTools
    QObject::connect(view, SIGNAL(modificationChanged(bool)),
                        this, SLOT(_q_onModificationChanged(bool)));
	d->viewStack->addTab(view, QFileInfo(data.fileName()).fileName());
	view->setReadOnly(d->readOnly);
	//< NeoScriptTools
    //d->viewStack->addWidget(view);
    d->viewHash.insert(scriptId, view);

	d->viewStack->setCurrentWidget(view);
	return view;
}

class ResolveScriptJob : public QScriptDebuggerCommandSchedulerJob
{
public:
    ResolveScriptJob(QScriptDebuggerCommandSchedulerInterface *commandScheduler, QScriptDebuggerCodeWidget *widget, const QString& fileName)
        : QScriptDebuggerCommandSchedulerJob(commandScheduler),
          m_widget(widget), m_fileName(fileName), m_state(0), m_scriptId(0) {}

    void start()
    {
        QScriptDebuggerCommandSchedulerFrontend frontend(commandScheduler(), this);
		frontend.scheduleResolveScript(m_fileName);
    }
    void handleResponse(const QScriptDebuggerResponse &response,
                        int)
    {   
        switch (m_state) {
        case 0: {
				m_scriptId = response.result().toLongLong();
				if(m_scriptId == -1 || m_widget->setCurrentScript(m_scriptId)) {
					finish();
					break;
				}
				++m_state;
				QScriptDebuggerCommandSchedulerFrontend frontend(commandScheduler(), this);
				frontend.scheduleGetScriptData(m_scriptId);
			}	break;
        case 1: {
				QScriptScriptData data = response.resultAsScriptData();
				m_widget->setCurrentScript(m_scriptId, data);
				finish();
			}	break;
        }
    }

private:
	QString m_fileName;
	qint64 m_scriptId;
	int m_state;
	QScriptDebuggerCodeWidget *m_widget;
};

void QScriptDebuggerCodeWidget::setCurrentScript(const QString& fileName)
{
    Q_D(QScriptDebuggerCodeWidget);
	for(int i=0; i < d->viewStack->count(); i++) {
		if(QScriptDebuggerCodeViewInterface *view = qobject_cast<QScriptDebuggerCodeViewInterface*>(d->viewStack->widget(i))) {
			if(view->name() == fileName) {
				d->viewStack->setCurrentIndex(i);
				return;
			}
		}
	}

	ResolveScriptJob *job = new ResolveScriptJob(d->commandScheduler, this, fileName);
	d->jobScheduler->scheduleJob(job);
}

//> NeoScriptTools
void QScriptDebuggerCodeWidget::tabCloseRequested(int index)
{
    Q_D(QScriptDebuggerCodeWidget);
	QWidget* widget = d->viewStack->widget(index);
	if(QScriptDebuggerCodeViewInterface *view = qobject_cast<QScriptDebuggerCodeViewInterface*>(widget)) {
		QString Name = d->viewStack->tabText(index);
		if(Name.right(1) == "*"){
			if(QMessageBox(tr("Close script..."), tr("Do you want to close the script without Saving"), 
							QMessageBox::Question, QMessageBox::Yes, QMessageBox::No | QMessageBox::Default, QMessageBox::NoButton).exec() == QMessageBox::No)
				return;
		}
		qint64 scriptId = d->scriptId(view);
		if(scriptId != -1)
			d->viewHash.remove(scriptId);
	} else if (QScriptDebuggerCustomViewInterface *view = qobject_cast<QScriptDebuggerCustomViewInterface*>(widget)) {
		foreach(QScriptDebuggerCustomViewInterfaceFactory factory, d->resourceHash.keys()) {
			QHash<qint64, QScriptDebuggerCustomViewInterface*>& resourceHash = d->resourceHash[factory];
			qint64 resourceID = resourceHash.key(view, -1);
			if(resourceID != -1)
				resourceHash.remove(resourceID);
		}
	}
	widget->deleteLater();
	d->viewStack->removeTab(index);
}

void QScriptDebuggerCodeWidget::setReadOnly(bool set)
{
    Q_D(QScriptDebuggerCodeWidget);
	d->readOnly = set;
	foreach(QScriptDebuggerCodeViewInterface *view, d->viewHash)
		view->setReadOnly(set);
}

int QScriptDebuggerCodeWidget::find(const QString &exp, int options)
{
    Q_D(QScriptDebuggerCodeWidget);
	if(QScriptDebuggerCodeViewInterface *view = currentView()) {
		return view->find(exp, options);
	} else if (QScriptDebuggerCustomViewInterface *view = currentCustom()) {
		return view->find(exp, options);
	}
	return 0;
}

void QScriptDebuggerCodeWidget::setCurrentCustom(qint64 customID, const QString& name, QScriptDebuggerCustomViewInterfaceFactory factory, void* param)
{
    Q_D(QScriptDebuggerCodeWidget);
    if (customID == -1) {
        return;
    }
	QHash<qint64, QScriptDebuggerCustomViewInterface*>& resourceHash = d->resourceHash[factory];
    QScriptDebuggerCustomViewInterface *view = resourceHash.value(customID);
    if (!view) {
        view = factory(customID, param); // ### use factory, so user can provide his own view
		d->viewStack->addTab(view, name);
		resourceHash.insert(customID, view);
    }
    d->viewStack->setCurrentWidget(view);
}

QScriptDebuggerCustomViewInterface *QScriptDebuggerCodeWidget::currentCustom() const
{
    Q_D(const QScriptDebuggerCodeWidget);
    return qobject_cast<QScriptDebuggerCustomViewInterface*>(d->viewStack->currentWidget());
}

QString QScriptDebuggerCodeWidget::currentName() const
{
    Q_D(const QScriptDebuggerCodeWidget);
	if(QScriptDebuggerCodeViewInterface *view = qobject_cast<QScriptDebuggerCodeViewInterface*>(d->viewStack->currentWidget())) {
		return view->name();
	}
	return QString();
}

void QScriptDebuggerCodeWidget::renameCurrent(QScriptDebuggerCodeViewInterface* view, const QString& name)
{
	if(!view)
		view = currentView();

    Q_D(const QScriptDebuggerCodeWidget);
	qint64 scriptId = d->scriptId(view);
	if(scriptId != -1) {
		int index = d->viewStack->indexOf(view);
		d->viewStack->setTabText(index, name);
	}
}
//< NeoScriptTools

void QScriptDebuggerCodeWidget::invalidateExecutionLineNumbers()
{
    Q_D(QScriptDebuggerCodeWidget);
    QHash<qint64, QScriptDebuggerCodeViewInterface*>::const_iterator it;
    for (it = d->viewHash.constBegin(); it != d->viewHash.constEnd(); ++it)
        it.value()->setExecutionLineNumber(-1, /*error=*/false);
}

QScriptBreakpointsModel *QScriptDebuggerCodeWidget::breakpointsModel() const
{
    Q_D(const QScriptDebuggerCodeWidget);
    return d->breakpointsModel;
}

void QScriptDebuggerCodeWidget::setBreakpointsModel(QScriptBreakpointsModel *model)
{
    Q_D(QScriptDebuggerCodeWidget);
    d->breakpointsModel = model;
    QObject::connect(model, SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)),
                     this, SLOT(_q_onBreakpointsAboutToBeRemoved(QModelIndex,int,int)));
    QObject::connect(model, SIGNAL(rowsInserted(QModelIndex,int,int)),
                     this, SLOT(_q_onBreakpointsInserted(QModelIndex,int,int)));
    QObject::connect(model, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
                     this, SLOT(_q_onBreakpointsDataChanged(QModelIndex,QModelIndex)));
}

void QScriptDebuggerCodeWidget::setToolTipProvider(QScriptToolTipProviderInterface *toolTipProvider)
{
    Q_D(QScriptDebuggerCodeWidget);
    d->toolTipProvider = toolTipProvider;
}

QScriptDebuggerCodeViewInterface *QScriptDebuggerCodeWidget::currentView() const
{
    Q_D(const QScriptDebuggerCodeWidget);
    return qobject_cast<QScriptDebuggerCodeViewInterface*>(d->viewStack->currentWidget());
}

QT_END_NAMESPACE

