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

#ifndef V8SCRIPTDEBUGGERFRONTEND_H
#define V8SCRIPTDEBUGGERFRONTEND_H

#include <QObject>
#include <QVariant>

#include "../JSDebugging/JSScriptDebuggerFrontendInterface.h"
#include "V8DebugAdapter.h"

#include "../neoscripttools_global.h"

class QScriptDebugger;
class CV8ScriptDebuggerFrontendPrivate;
class NEOSCRIPTTOOLS_EXPORT CV8ScriptDebuggerFrontend : public QObject, public CJSScriptDebuggerFrontendInterface, protected CV8DebugAdapter
{
    Q_OBJECT
public:
    CV8ScriptDebuggerFrontend(QObject *parent = 0);
    ~CV8ScriptDebuggerFrontend();

	virtual void connectTo(quint16 port);
	virtual void detach();

protected:
	virtual void processCommand(int id, const QVariantMap &command);

	virtual void onRequest(const QByteArray& arrJson);

	virtual void onEvent(const QVariantMap& Event);
	virtual void onResult(int id, const QVariantMap& Result);

private slots:
	void onReadyRead();

private:
	Q_DECLARE_PRIVATE(CV8ScriptDebuggerFrontend)
    Q_DISABLE_COPY(CV8ScriptDebuggerFrontend)
};

#endif
