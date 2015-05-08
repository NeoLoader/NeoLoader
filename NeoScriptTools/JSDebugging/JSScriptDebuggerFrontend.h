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

#ifndef JSSCRIPTDEBUGGERFRONTEND_H
#define JSSCRIPTDEBUGGERFRONTEND_H

#include <QObject>
#include <QVariant>

#include "JSScriptDebuggerFrontendInterface.h"

#include "../neoscripttools_global.h"

class QScriptDebugger;
class CJSScriptDebuggerFrontendPrivate;
class NEOSCRIPTTOOLS_EXPORT CJSScriptDebuggerFrontend : public QObject, public CJSScriptDebuggerFrontendInterface
{
    Q_OBJECT
public:
    CJSScriptDebuggerFrontend(QObject *parent = 0);
    ~CJSScriptDebuggerFrontend();

signals:
    void sendRequest(const QVariant& var);
	void processCustom(const QVariant& var);

public slots:
    void processResponse(const QVariant& var);
	void sendCustom(const QVariant& var);

protected:
	void processCommand(int id, const QVariantMap &command);

	void timerEvent(QTimerEvent *e);

private:
	Q_DECLARE_PRIVATE(CJSScriptDebuggerFrontend)
    Q_DISABLE_COPY(CJSScriptDebuggerFrontend)
};

#endif
