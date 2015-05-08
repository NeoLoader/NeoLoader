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

#ifndef QSCRIPTDEBUGGERLOCALSWIDGET_P_P_H
#define QSCRIPTDEBUGGERLOCALSWIDGET_P_P_H

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
#include <QtGui/qheaderview.h>
#include <QtGui/qcompleter.h>
#include <QtGui/qstringlistmodel.h>
#include <QtGui/qtreeview.h>
#include <QtGui/qboxlayout.h>
#include <QtGui/qsortfilterproxymodel.h>
#include <QtGui/qlineedit.h>
#include <QtGui/qstyleditemdelegate.h>
#include <QtGui/qmessagebox.h>
#else
#include <QtWidgets/qheaderview.h>
#include <QtWidgets/qcompleter.h>
#include <QtCore/qstringlistmodel.h>
#include <QtWidgets/qtreeview.h>
#include <QtWidgets/qboxlayout.h>
#include <QtCore/qsortfilterproxymodel.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qstyleditemdelegate.h>
#include <QtWidgets/qmessagebox.h>
#endif
#include <QtGui/qevent.h>
#ifndef NO_QT_SCRIPT
#include <QtScript/qscriptengine.h>
#endif

#include "qscriptdebuggerlocalswidgetinterface_p_p.h"
#include "qscriptdebuggerlocalswidget_p.h"

QT_BEGIN_NAMESPACE

class QScriptDebuggerLocalsWidgetPrivate
    : public QScriptDebuggerLocalsWidgetInterfacePrivate
{
    Q_DECLARE_PUBLIC(QScriptDebuggerLocalsWidget)
public:
    QScriptDebuggerLocalsWidgetPrivate();
    ~QScriptDebuggerLocalsWidgetPrivate();

    void complete(QLineEdit *le);

    // private slots
    void _q_onCompletionTaskFinished();
    void _q_insertCompletion(const QString &text);
    void _q_expandIndex(const QModelIndex &index);

    QTreeView *view;
    QPointer<QLineEdit> completingEditor;
    QCompleter *completer;
    CustomProxyModel *proxy;
};

class QScriptDebuggerLocalsItemDelegate
    : public QStyledItemDelegate
{
    Q_OBJECT
public:
    QScriptDebuggerLocalsItemDelegate(QObject *parent = 0);

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;

    bool eventFilter(QObject *watched, QEvent *event);

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

private:
    static const QWidget *widget(const QStyleOptionViewItem &option)
    {
        if (const QStyleOptionViewItemV3 *v3 = qstyleoption_cast<const QStyleOptionViewItemV3 *>(&option))
            return v3->widget;
        return 0;
    }
};

QT_END_NAMESPACE

#endif
