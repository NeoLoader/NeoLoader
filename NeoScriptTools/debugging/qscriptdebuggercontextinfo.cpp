/****************************************************************************
**
** Copyright (C) 2012 NeoLoader Team
** All rights reserved.
** Contact: NeoLoader.to
**
** This file is part of the V8ScriptTools module for NeoLoader
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

#include "qscriptdebuggercontextinfo_p.h"

#include <QtCore/qdatastream.h>
#include <QtCore/qstring.h>
#include <QtCore/qshareddata.h>

class QScriptDebuggerContextInfoPrivate : public QSharedData
{
public:
    QScriptDebuggerContextInfoPrivate();
    ~QScriptDebuggerContextInfoPrivate();

    qint64 scriptId;
    QString fileName;
    int lineNumber;
	int columnNumber;

    QString functionName;
    QScriptDebuggerContextInfo::FunctionType functionType;
};

QScriptDebuggerContextInfoPrivate::QScriptDebuggerContextInfoPrivate()
{
}

QScriptDebuggerContextInfoPrivate::~QScriptDebuggerContextInfoPrivate()
{
}

QScriptDebuggerContextInfo::QScriptDebuggerContextInfo()
    : d_ptr(0)
{
}

QScriptDebuggerContextInfo::QScriptDebuggerContextInfo(qint64 scriptId, const QString& fileName, int lineNumber, int columnNumber,
										const QString& functionName, FunctionType functionType)
    : d_ptr(new QScriptDebuggerContextInfoPrivate)
{
    d_ptr->scriptId = scriptId;
	d_ptr->fileName = fileName;
	d_ptr->lineNumber = lineNumber;
	d_ptr->columnNumber = columnNumber;

	d_ptr->functionName = functionName;
	d_ptr->functionType = functionType;
    d_ptr->ref.ref();
}

QScriptDebuggerContextInfo::QScriptDebuggerContextInfo(const QScriptDebuggerContextInfo &other)
    : d_ptr(other.d_ptr.data())
{
    if (d_ptr)
        d_ptr->ref.ref();
}

QScriptDebuggerContextInfo::~QScriptDebuggerContextInfo()
{
}

QScriptDebuggerContextInfo &QScriptDebuggerContextInfo::operator=(const QScriptDebuggerContextInfo &other)
{
    d_ptr.assign(other.d_ptr.data());
    return *this;
}

bool QScriptDebuggerContextInfo::isNull() const
{
    Q_D(const QScriptDebuggerContextInfo);
    return (d == 0);
}

qint64 QScriptDebuggerContextInfo::scriptId() const
{
    Q_D(const QScriptDebuggerContextInfo);
    if (!d)
        return -1;
    return d->scriptId;
}

QString QScriptDebuggerContextInfo::fileName() const
{
    Q_D(const QScriptDebuggerContextInfo);
    if (!d)
        return QString();
    return d->fileName;
}

int QScriptDebuggerContextInfo::lineNumber() const
{
    Q_D(const QScriptDebuggerContextInfo);
    if (!d)
        return -1;
    return d->lineNumber;
}

int QScriptDebuggerContextInfo::columnNumber() const
{
    Q_D(const QScriptDebuggerContextInfo);
    if (!d)
        return -1;
    return d->columnNumber;
}

QString QScriptDebuggerContextInfo::functionName() const
{
    Q_D(const QScriptDebuggerContextInfo);
    if (!d)
        return QString();
    return d->functionName;
}

QScriptDebuggerContextInfo::FunctionType QScriptDebuggerContextInfo::functionType() const
{
    Q_D(const QScriptDebuggerContextInfo);
    if (!d)
        return NativeFunction;
    return d->functionType;
}