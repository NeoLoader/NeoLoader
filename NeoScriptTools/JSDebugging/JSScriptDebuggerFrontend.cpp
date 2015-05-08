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

#include "JSScriptDebuggerFrontend.h"
#include <private/qobject_p.h>

class CJSScriptDebuggerFrontendPrivate: public QObjectPrivate
{
	Q_DECLARE_PUBLIC(CJSScriptDebuggerFrontend)
public:
	
	int eventTimerId;
};

CJSScriptDebuggerFrontend::CJSScriptDebuggerFrontend(QObject *parent)
	: QObject(*new CJSScriptDebuggerFrontendPrivate, parent)
{
	Q_D(CJSScriptDebuggerFrontend);
	d->eventTimerId = startTimer(75); // pull events 
}

CJSScriptDebuggerFrontend::~CJSScriptDebuggerFrontend()
{
	Q_D(CJSScriptDebuggerFrontend);
	killTimer(d->eventTimerId);
}

void CJSScriptDebuggerFrontend::processResponse(const QVariant& var)
{
	Q_D(CJSScriptDebuggerFrontend);

	QVariantMap in = var.toMap();
	if (in.contains("Event")) 
		notifyEvent(in["Event"].toMap());
	else if (in.contains("Result")) 
		notifyCommandFinished((int)in["ID"].toInt(), in["Result"].toMap());
	else if (in.contains("Response")) 
		emit processCustom(in["Response"]);
}

void CJSScriptDebuggerFrontend::timerEvent(QTimerEvent *e)
{
	Q_D(CJSScriptDebuggerFrontend);
    if (e->timerId() != d->eventTimerId) {
        QObject::timerEvent(e);
		return;
    }

	// Pull Events
	emit sendRequest(QVariant());
}

void CJSScriptDebuggerFrontend::processCommand(int id, const QVariantMap &command)
{
	Q_D(CJSScriptDebuggerFrontend);

	QVariantMap out;
	out["ID"] = id;
	out["Command"] = command;
    emit sendRequest(out);
}

void CJSScriptDebuggerFrontend::sendCustom(const QVariant& var)
{
	QVariantMap out;
	out["Request"] = var;
    emit sendRequest(out);
}