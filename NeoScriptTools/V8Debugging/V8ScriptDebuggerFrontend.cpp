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

#include "V8ScriptDebuggerFrontend.h"

#include <QTcpSocket>
#include <QHostAddress>
#include <private/qobject_p.h>

class CV8ScriptDebuggerFrontendPrivate: public QObjectPrivate
{
	Q_DECLARE_PUBLIC(CV8ScriptDebuggerFrontend)
public:

	QTcpSocket	socket;
	QByteArray	buffer;
};

CV8ScriptDebuggerFrontend::CV8ScriptDebuggerFrontend(QObject *parent)
	: QObject(*new CV8ScriptDebuggerFrontendPrivate, parent)
{
	Q_D(CV8ScriptDebuggerFrontend);
}

CV8ScriptDebuggerFrontend::~CV8ScriptDebuggerFrontend()
{
	Q_D(CV8ScriptDebuggerFrontend);
}

void CV8ScriptDebuggerFrontend::onRequest(const QByteArray& arrJson)
{
	Q_D(CV8ScriptDebuggerFrontend);

	Q_ASSERT(!arrJson.isEmpty());

	QByteArray Data = "Content-Length: " + QByteArray::number(arrJson.length()) + "\r\n";
	Data += "\r\n" + arrJson;
	d->socket.write(Data);
}

void CV8ScriptDebuggerFrontend::onEvent(const QVariantMap& Event)
{
	notifyEvent(Event);
}

void CV8ScriptDebuggerFrontend::onResult(int id, const QVariantMap& Result)
{
	notifyCommandFinished(id, Result);
}

void CV8ScriptDebuggerFrontend::onReadyRead()
{
	Q_D(CV8ScriptDebuggerFrontend);

	d->buffer.append(d->socket.readAll());

	for(;;)
	{
		int End = d->buffer.indexOf("\r\n\r\n");
		if(End == -1) // hreader not yet complee
			break;

		int ContentLength = 0;
		foreach(const QByteArray &Line, d->buffer.left(End).split('\n'))
		{
			int Sep = Line.indexOf(":");
			QByteArray Key = Line.left(Sep);
			QByteArray Value = Line.mid(Sep+1).trimmed();
			if(Key == "Content-Length")
				ContentLength = Value.toUInt();
		}

		if(d->buffer.size() < End+4 + ContentLength) // data not yet complete
			break;

		if(ContentLength == 0) // thats a handshake
			attach();
		else
			onMessage(d->buffer.mid(End+4, ContentLength));

		d->buffer.remove(0, End+4 + ContentLength);
	}
}

void CV8ScriptDebuggerFrontend::processCommand(int id, const QVariantMap &command)
{
	Q_D(CV8ScriptDebuggerFrontend);

	onCommand(id, command);
}

void CV8ScriptDebuggerFrontend::connectTo(quint16 port)
{
	Q_D(CV8ScriptDebuggerFrontend);

	d->socket.connectToHost(QHostAddress("127.0.0.1"), 9222);
	connect(&d->socket, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
}

void CV8ScriptDebuggerFrontend::detach()
{
	Q_D(CV8ScriptDebuggerFrontend);

	CV8DebugAdapter::detach();
	d->socket.waitForBytesWritten();
}