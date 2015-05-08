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

#ifndef QSCRIPTDEBUGGERCONSOLEWIDGET_P_P_H
#define QSCRIPTDEBUGGERCONSOLEWIDGET_P_P_H

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
#include <QtGui/qplaintextedit.h>
#include <QtGui/qlabel.h>
#include <QtGui/qlineedit.h>
#include <QtGui/qlistview.h>
#include <QtGui/qscrollbar.h>
#include <QtGui/qboxlayout.h>
#include <QtGui/qcompleter.h>
#else
#include <QtWidgets/qplaintextedit.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qlistview.h>
#include <QtWidgets/qscrollbar.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qcompleter.h>
#endif

#include "qscriptdebuggerconsolewidgetinterface_p_p.h"
#include "qscriptdebuggerconsolewidget_p.h"


class PromptLabel : public QLabel
{
public:
    PromptLabel(QWidget *parent = 0)
        : QLabel(parent)
    {
        setFrameShape(QFrame::NoFrame);
        setIndent(2);
        setMargin(2);
        setSizePolicy(QSizePolicy::Minimum, sizePolicy().verticalPolicy());
        setAlignment(Qt::AlignHCenter);
#ifndef QT_NO_STYLE_STYLESHEET
        setStyleSheet(QLatin1String("background: white;"));
#endif
    }

    QSize sizeHint() const {
        QFontMetrics fm(font());
        return fm.size(0, text()) + QSize(8, 0);
    }
};

class InputEdit : public QLineEdit
{
public:
    InputEdit(QWidget *parent = 0)
        : QLineEdit(parent)
    {
        setFrame(false);
        setSizePolicy(QSizePolicy::MinimumExpanding, sizePolicy().verticalPolicy());
    }
};

class CommandLine : public QWidget
{
    Q_OBJECT

public:
    CommandLine(QWidget *parent = 0)
        : QWidget(parent)
    {
        promptLabel = new PromptLabel();
        inputEdit = new InputEdit();
        QHBoxLayout *hbox = new QHBoxLayout(this);
        hbox->setSpacing(0);
        hbox->setMargin(0);
        hbox->addWidget(promptLabel);
        hbox->addWidget(inputEdit);

        QObject::connect(inputEdit, SIGNAL(returnPressed()),
                         this, SLOT(onReturnPressed()));
        QObject::connect(inputEdit, SIGNAL(textEdited(QString)),
                         this, SIGNAL(lineEdited(QString)));

        setFocusProxy(inputEdit);
    }

    QString prompt() const
    {
        return promptLabel->text();
    }
    void setPrompt(const QString &prompt)
    {
        promptLabel->setText(prompt);
    }

    QString input() const
    {
        return inputEdit->text();
    }
    void setInput(const QString &input)
    {
        inputEdit->setText(input);
    }

    int cursorPosition() const
    {
        return inputEdit->cursorPosition();
    }
    void setCursorPosition(int position)
    {
        inputEdit->setCursorPosition(position);
    }

    QWidget *editor() const
    {
        return inputEdit;
    }

Q_SIGNALS:
    void lineEntered(const QString &contents);
    void lineEdited(const QString &contents);

private Q_SLOTS:
    void onReturnPressed()
    {
        QString text = inputEdit->text();
        inputEdit->clear();
        emit lineEntered(text);
    }

private:
    PromptLabel *promptLabel;
    InputEdit *inputEdit;
};

class QScriptDebuggerConsoleWidgetOutputEdit : public QPlainTextEdit
{
public:
    QScriptDebuggerConsoleWidgetOutputEdit(QWidget *parent = 0)
        : QPlainTextEdit(parent)
    {
        setFrameShape(QFrame::NoFrame);
        setReadOnly(true);
// ### there's no context menu when the edit can't have focus,
//     even though you can select text in it.
//        setFocusPolicy(Qt::NoFocus);
        setMaximumBlockCount(2500);
    }

    void scrollToBottom()
    {
        QScrollBar *bar = verticalScrollBar();
        bar->setValue(bar->maximum());
    }

    int charactersPerLine() const
    {
        QFontMetrics fm(font());
        return width() / fm.maxWidth();
    }
};

QT_BEGIN_NAMESPACE


class QScriptDebuggerConsoleWidgetPrivate
    : public QScriptDebuggerConsoleWidgetInterfacePrivate
{
    Q_DECLARE_PUBLIC(QScriptDebuggerConsoleWidget)
public:
    QScriptDebuggerConsoleWidgetPrivate();
    ~QScriptDebuggerConsoleWidgetPrivate();

    // private slots
    void _q_onLineEntered(const QString &contents);
    void _q_onLineEdited(const QString &contents);
    void _q_onCompletionTaskFinished();

    CommandLine *commandLine;
    QScriptDebuggerConsoleWidgetOutputEdit *outputEdit;
    int historyIndex;
    QString newInput;
};

QT_END_NAMESPACE

#endif
