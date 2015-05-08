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

#include "JSScriptDebugger.h"

#if QT_VERSION < 0x050000
#include <QtGui/qapplication.h>
#include <QtGui/qdockwidget.h>
#include <QtGui/qmainwindow.h>
#include <QtGui/qmenubar.h>
#include <QtGui/qstatusbar.h>
#include <QtGui/qboxlayout.h>
#else
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qdockwidget.h>
#include <QtWidgets/qmainwindow.h>
#include <QtWidgets/qmenubar.h>
#include <QtWidgets/qstatusbar.h>
#include <QtWidgets/qboxlayout.h>
#endif

#include "../debugging/qscriptdebugger_p.h"
#include "../debugging/qscriptdebuggercommand_p.h"
#include "../debugging/qscriptdebuggerevent_p.h"
#include "../debugging/qscriptdebuggerresponse_p.h"
#include "../debugging/qscriptdebuggerstandardwidgetfactory_p.h"

#if QT_VERSION < 0x050000
CJSScriptDebugger::CJSScriptDebugger(QWidget *parent, Qt::WFlags flags)
#else
CJSScriptDebugger::CJSScriptDebugger(QWidget *parent, Qt::WindowFlags flags)
#endif
  : QMainWindow(parent, flags)
{
	m_frontend = NULL;

	m_debugger = new QScriptDebugger();
    m_debugger->setWidgetFactory(new QScriptDebuggerStandardWidgetFactory(this));
}

CJSScriptDebugger::~CJSScriptDebugger()
{
    delete m_frontend;
    delete m_debugger;
}

void CJSScriptDebugger::closeEvent(QCloseEvent *e)
{
	Q_ASSERT(m_frontend);
	m_frontend->detach();
	emit detach();
	QMainWindow::closeEvent(e);
}

void CJSScriptDebugger::attachTo(CJSScriptDebuggerFrontendInterface *frontend)
{
	Q_ASSERT(m_frontend == 0);

	setup();

    m_frontend = frontend;
	m_frontend->attachTo(m_debugger);

	show();
	statusBar()->showMessage(tr("Debugger ready..."));
}

void CJSScriptDebugger::setup()
{
	// Setup Dock
    QDockWidget *scriptsDock = new QDockWidget(this);
	scriptsDock->setObjectName(QLatin1String("qtscriptdebugger_scriptsDockWidget"));
    scriptsDock->setWindowTitle(tr("Loaded Scripts"));
    scriptsDock->setWidget(widget(ScriptsWidget));
    addDockWidget(Qt::LeftDockWidgetArea, scriptsDock);

    QDockWidget *breakpointsDock = new QDockWidget(this);
	breakpointsDock->setObjectName(QLatin1String("qtscriptdebugger_breakpointsDockWidget"));
    breakpointsDock->setWindowTitle(tr("Breakpoints"));
    breakpointsDock->setWidget(widget(BreakpointsWidget));
    addDockWidget(Qt::LeftDockWidgetArea, breakpointsDock);

    QDockWidget *stackDock = new QDockWidget(this);
	stackDock->setObjectName(QLatin1String("qtscriptdebugger_stackDockWidget"));
    stackDock->setWindowTitle(tr("Stack"));
    stackDock->setWidget(widget(StackWidget));
    addDockWidget(Qt::RightDockWidgetArea, stackDock);

    QDockWidget *localsDock = new QDockWidget(this);
	localsDock->setObjectName(QLatin1String("qtscriptdebugger_localsDockWidget"));
    localsDock->setWindowTitle(tr("Locals"));
    localsDock->setWidget(widget(LocalsWidget));
    addDockWidget(Qt::RightDockWidgetArea, localsDock);

    QDockWidget *consoleDock = new QDockWidget(this);
	consoleDock->setObjectName(QLatin1String("qtscriptdebugger_consoleDockWidget"));
    consoleDock->setWindowTitle(tr("Console"));
    consoleDock->setWidget(widget(ConsoleWidget));
    addDockWidget(Qt::BottomDockWidgetArea, consoleDock);

    QDockWidget *debugOutputDock = new QDockWidget(this);
	debugOutputDock->setObjectName(QLatin1String("qtscriptdebugger_debugOutputDockWidget"));
    debugOutputDock->setWindowTitle(tr("Debug Output"));
    debugOutputDock->setWidget(widget(DebugOutputWidget));
    addDockWidget(Qt::BottomDockWidgetArea, debugOutputDock);

    QDockWidget *errorLogDock = new QDockWidget(this);
	errorLogDock->setObjectName(QLatin1String("qtscriptdebugger_errorLogDockWidget"));
    errorLogDock->setWindowTitle(tr("Error Log"));
    errorLogDock->setWidget(widget(ErrorLogWidget));
    addDockWidget(Qt::BottomDockWidgetArea, errorLogDock);

    tabifyDockWidget(errorLogDock, debugOutputDock);
    tabifyDockWidget(debugOutputDock, consoleDock);

	// Setup MenuBar
    menuBar()->addMenu(createStandardMenu(this));

    QMenu *editMenu = menuBar()->addMenu(tr("Search"));
    editMenu->addAction(action(FindInScriptAction));
    editMenu->addAction(action(FindNextInScriptAction));
    editMenu->addAction(action(FindPreviousInScriptAction));
    editMenu->addSeparator();
    editMenu->addAction(action(GoToLineAction));

    QMenu *viewMenu = menuBar()->addMenu(tr("View"));
    viewMenu->addAction(scriptsDock->toggleViewAction());
    viewMenu->addAction(breakpointsDock->toggleViewAction());
    viewMenu->addAction(stackDock->toggleViewAction());
    viewMenu->addAction(localsDock->toggleViewAction());
    viewMenu->addAction(consoleDock->toggleViewAction());
    viewMenu->addAction(debugOutputDock->toggleViewAction());
    viewMenu->addAction(errorLogDock->toggleViewAction());

	// Setup ToolBar
    addToolBar(Qt::TopToolBarArea, createStandardToolBar());

	// Setup Window
    QWidget* pCentral = new QWidget();
    QVBoxLayout* pVbox = new QVBoxLayout(pCentral);
    pVbox->addWidget(widget(CodeWidget));
    pVbox->addWidget(widget(CodeFinderWidget));
    widget(CodeFinderWidget)->hide();
    setCentralWidget(pCentral);

    setWindowTitle(tr("Script Debugger"));
}

void CJSScriptDebugger::openScript(const QString& fileName)
{
	m_debugger->codeWidget()->setCurrentScript(fileName);
}

void CJSScriptDebugger::setCurrentCustom(qint64 customID, const QString& name, QScriptDebuggerCustomViewInterface* (*factory)(qint64, void*), void* param)
{
	m_debugger->codeWidget()->setCurrentCustom(customID, name, factory, param);
}

QScriptDebuggerCustomViewInterface* CJSScriptDebugger::currentCustom()
{
	return m_debugger->codeWidget()->currentCustom();
}

bool CJSScriptDebugger::isInteractive()
{
	return m_debugger->isInteractive();
}

QString CJSScriptDebugger::currentName() const
{
	return m_debugger->codeWidget()->currentName();
}

QString CJSScriptDebugger::currentText(bool bReset)
{
	if(QScriptDebuggerCodeViewInterface* view = m_debugger->codeWidget()->currentView())
	{
		if(bReset)
			view->setModified(false);
		return view->text();
	}
	return QString();
}

void CJSScriptDebugger::renameCurrent(const QString& FileName)
{
	m_debugger->codeWidget()->renameCurrent(NULL, FileName);
}

QToolBar *CJSScriptDebugger::createStandardToolBar(QWidget *parent)
{
    return m_debugger->createStandardToolBar(parent, this);
}

QMenu *CJSScriptDebugger::createStandardMenu(QWidget *parent)
{
    return m_debugger->createStandardMenu(parent, this);
}

QAction *CJSScriptDebugger::action(int action) const
{
	CJSScriptDebugger *that = const_cast<CJSScriptDebugger*>(this);
	return m_debugger->action(static_cast<QScriptDebugger::DebuggerAction>(action), that);
}

QWidget *CJSScriptDebugger::widget(int widget) const
{
	return m_debugger->widget(static_cast<QScriptDebugger::DebuggerWidget>(widget));
}

namespace
{
void grayScale (QImage& Image)
{
	if (Image.depth () == 32)
	{
		uchar* r = (Image.bits ());
		uchar* g = (Image.bits () + 1);
		uchar* b = (Image.bits () + 2);

#if QT_VERSION < 0x050000
		uchar* end = (Image.bits() + Image.numBytes ());
#else
		uchar* end = (Image.bits() + Image.byteCount ());
#endif

		while (r != end)
		{
			*r = *g = *b = (((*r + *g) >> 1) + *b) >> 1; // (r + b + g) / 3

			r += 4;
			g += 4;
			b += 4;
		}
	}
	else
	{
#if QT_VERSION < 0x050000
		for (int i = 0; i < Image.numColors (); i++)
#else
		for (int i = 0; i < Image.colorCount (); i++)
#endif
		{
			uint r = qRed (Image.color (i));
			uint g = qGreen (Image.color (i));
			uint b = qBlue (Image.color (i));

			uint gray = (((r + g) >> 1) + b) >> 1;

			Image.setColor (i, qRgba (gray, gray, gray, qAlpha (Image.color (i))));
		}
	}
}
}

QPixmap CJSScriptDebugger::pixmap(const QString &path, bool bGray, bool bOS)
{
    QString prefix;
	if(path.left(2) != ":/")
	{
		prefix = QString::fromLatin1(":/qt/scripttools/debugging/images/");
		QString system = QLatin1String("win");
#ifdef Q_OS_MAC
		system = QLatin1String("mac");
#endif
		prefix += (bOS ? system + "/" : "");
	}
	
	QImage Image = QImage(prefix + path);
	if(bGray)
		grayScale(Image);
	return QPixmap::fromImage(Image);
}