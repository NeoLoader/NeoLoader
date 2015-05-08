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

#ifndef V8SCRIPTDEBUGGERFRONTENDINTERFACE_H
#define V8SCRIPTDEBUGGERFRONTENDINTERFACE_H

#include <QVariant>

#include "../neoscripttools_global.h"

class QScriptDebugger;
class QScriptDebuggerFrontendImpl;
class NEOSCRIPTTOOLS_EXPORT CJSScriptDebuggerFrontendInterface
{
public:
	CJSScriptDebuggerFrontendInterface();
	virtual ~CJSScriptDebuggerFrontendInterface();

	virtual void attachTo(QScriptDebugger* debugger);
	virtual void detach() {}

protected:
	friend class QScriptDebuggerFrontendImpl;

	virtual void processCommand(int id, const QVariantMap &command) = 0;

	virtual void notifyCommandFinished(int id, const QVariantMap &result);
	virtual void notifyEvent(const QVariantMap &event);

private:
	QScriptDebuggerFrontendImpl*	m_impl;
};

#endif
