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

#ifndef QSCRIPTBREAKPOINTSWIDGET_P_P_H
#define QSCRIPTBREAKPOINTSWIDGET_P_P_H

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

#include "qscriptbreakpointswidgetinterface_p_p.h"

#include "qscriptbreakpointswidget_p.h"

QT_BEGIN_NAMESPACE

class QScriptNewBreakpointWidget : public QWidget
{
    Q_OBJECT
public:
    QScriptNewBreakpointWidget(QWidget *parent = 0)
        : QWidget(parent) {
	QString system = QLatin1String("win");
        QHBoxLayout *hboxLayout = new QHBoxLayout(this);
#ifdef Q_OS_MAC
        system = QLatin1String("mac");
#else
        hboxLayout->setSpacing(6);
        hboxLayout->setMargin(0);
#endif

        toolClose = new QToolButton(this);
        toolClose->setIcon(QIcon(QString::fromUtf8(":/qt/scripttools/debugging/images/%1/closetab.png").arg(system)));
        toolClose->setAutoRaise(true);
        toolClose->setText(tr("Close"));
        hboxLayout->addWidget(toolClose);

        fileNameEdit = new QLineEdit();
        setFocusProxy(fileNameEdit);
        QRegExp locationRegExp(QString::fromLatin1(".+:[0-9]+"));
        QRegExpValidator *validator = new QRegExpValidator(locationRegExp, fileNameEdit);
        fileNameEdit->setValidator(validator);
        hboxLayout->addWidget(fileNameEdit);

        toolOk = new QToolButton(this);
        toolOk->setIcon(QIcon(QString::fromUtf8(":/qt/scripttools/debugging/images/%1/plus.png").arg(system)));
        toolOk->setAutoRaise(true);
        toolOk->setEnabled(false);
        hboxLayout->addWidget(toolOk);

        QObject::connect(toolClose, SIGNAL(clicked()), this, SLOT(hide()));
        QObject::connect(toolOk, SIGNAL(clicked()), this, SLOT(onOkClicked()));
        QObject::connect(fileNameEdit, SIGNAL(textChanged(QString)),
                         this, SLOT(onTextChanged()));
        QObject::connect(fileNameEdit, SIGNAL(returnPressed()),
                         this, SLOT(onOkClicked()));
    }

    void setCompleter(QCompleter *comp)
        { fileNameEdit->setCompleter(comp); }

Q_SIGNALS:
    void newBreakpointRequest(const QString &fileName, int lineNumber);

protected:
    void keyPressEvent(QKeyEvent *e)
    {
        if (e->key() == Qt::Key_Escape)
            hide();
        else
            QWidget::keyPressEvent(e);
    }

private Q_SLOTS:
    void onOkClicked()
    {
        QString location = fileNameEdit->text();
        fileNameEdit->clear();
        QString fileName = location.left(location.lastIndexOf(QLatin1Char(':')));
        int lineNumber = location.mid(fileName.length()+1).toInt();
        emit newBreakpointRequest(fileName, lineNumber);
    }

    void onTextChanged()
    {
        toolOk->setEnabled(fileNameEdit->hasAcceptableInput());
    }

private:
    QLineEdit *fileNameEdit;
    QToolButton *toolClose;
    QToolButton *toolOk;
};

class QScriptBreakpointsWidgetPrivate
    : public QScriptBreakpointsWidgetInterfacePrivate
{
    Q_DECLARE_PUBLIC(QScriptBreakpointsWidget)
public:
    QScriptBreakpointsWidgetPrivate();
    ~QScriptBreakpointsWidgetPrivate();

    void _q_newBreakpoint();
    void _q_deleteBreakpoint();
    void _q_onCurrentChanged(const QModelIndex &index);
    void _q_onNewBreakpointRequest(const QString &fileName, int lineNumber);

    static QPixmap pixmap(const QString &path)
    {
        static QString prefix = QString::fromLatin1(":/qt/scripttools/debugging/images/");
        return QPixmap(prefix + path);
    }

    QTreeView *view;
    QScriptNewBreakpointWidget *newBreakpointWidget;
    QAction *deleteBreakpointAction;
    QScriptDebuggerScriptsModel *scriptsModel;
};

class QScriptBreakpointsItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    QScriptBreakpointsItemDelegate(QObject *parent = 0)
        : QStyledItemDelegate(parent) {}

    QWidget *createEditor(QWidget *parent,
                          const QStyleOptionViewItem &option,
                          const QModelIndex &index) const
    {
        QWidget *editor = QStyledItemDelegate::createEditor(parent, option, index);
        if (index.column() == 2) {
            // condition
            QLineEdit *le = qobject_cast<QLineEdit*>(editor);
            if (le) {
                QObject::connect(le, SIGNAL(textEdited(QString)),
                                 this, SLOT(validateInput(QString)));
            }
        }
        return editor;
    }

    bool eventFilter(QObject *editor, QEvent *event)
    {
        if (QLineEdit *le = qobject_cast<QLineEdit*>(editor)) {
            if (event->type() == QEvent::KeyPress) {
                int key = static_cast<QKeyEvent*>(event)->key();
                if ((key == Qt::Key_Enter) || (key == Qt::Key_Return)) {
#ifndef NO_QT_SCRIPT
                    if (QScriptEngine::checkSyntax(le->text()).state() != QScriptSyntaxCheckResult::Valid) {
                        // ignore when script contains syntax error
                        return true;
                    }
#endif
                }
            }
        }
        return QStyledItemDelegate::eventFilter(editor, event);
    }

    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const
    {
#ifndef NO_QT_SCRIPT
        if (index.column() == 2) {
            // check that the syntax is OK
            QString condition = qobject_cast<QLineEdit*>(editor)->text();
            if (QScriptEngine::checkSyntax(condition).state() != QScriptSyntaxCheckResult::Valid)
                return;
        }
#endif
        QStyledItemDelegate::setModelData(editor, model, index);
    }

private Q_SLOTS:
    void validateInput(const QString &text)
    {
#ifndef NO_QT_SCRIPT
        QWidget *editor = qobject_cast<QWidget*>(sender());
        QPalette pal = editor->palette();
        QColor col;
        bool ok = (QScriptEngine::checkSyntax(text).state() == QScriptSyntaxCheckResult::Valid);
        if (ok) {
            col = Qt::white;
        } else {
            QScriptSyntaxCheckResult result = QScriptEngine::checkSyntax(
                text + QLatin1Char('\n'));
            if (result.state() == QScriptSyntaxCheckResult::Intermediate)
                col = QColor(255, 240, 192);
            else
                col = QColor(255, 102, 102);
        }
        pal.setColor(QPalette::Active, QPalette::Base, col);
        editor->setPalette(pal);
#endif
    }
};

QT_END_NAMESPACE

#endif
