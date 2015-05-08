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

#include "JSScriptDebuggerFrontendInterface.h"

#include "../debugging/qscriptdebugger_p.h"
#include "../debugging/qscriptdebuggercommand_p.h"

class QScriptDebuggerFrontendImpl: public QScriptDebuggerFrontend
{
public:
	QScriptDebuggerFrontendImpl(CJSScriptDebuggerFrontendInterface*	itf)
	{
		m_itf = itf;
	}

	void processCommand(int id, const QScriptDebuggerCommand &command)
	{
		m_itf->processCommand(id, command.toVariant().toMap());
	}

    void notifyCommandFinished(int id, const QVariantMap &result)
	{
		QScriptDebuggerResponse response;
		response.fromVariant(result);
		QScriptDebuggerFrontend::notifyCommandFinished(id, response);
	}

    void notifyEvent(const QVariantMap &event)
	{
		QScriptDebuggerEvent e(QScriptDebuggerEvent::None);
		e.fromVariant(event);
		bool handled = QScriptDebuggerFrontend::notifyEvent(e);
		if (handled) 
			QScriptDebuggerFrontend::scheduleCommand(QScriptDebuggerCommand::resumeCommand(),0);
	}

private:
	CJSScriptDebuggerFrontendInterface*	m_itf;
};


CJSScriptDebuggerFrontendInterface::CJSScriptDebuggerFrontendInterface()
{
	m_impl = new QScriptDebuggerFrontendImpl(this);
}

CJSScriptDebuggerFrontendInterface::~CJSScriptDebuggerFrontendInterface()
{
	delete m_impl;
}

void CJSScriptDebuggerFrontendInterface::notifyCommandFinished(int id, const QVariantMap &result)
{
	m_impl->notifyCommandFinished(id, result);
}

void CJSScriptDebuggerFrontendInterface::notifyEvent(const QVariantMap &event)
{
	m_impl->notifyEvent(event);
}

void CJSScriptDebuggerFrontendInterface::attachTo(QScriptDebugger* debugger)
{
	debugger->setFrontend(m_impl);
}