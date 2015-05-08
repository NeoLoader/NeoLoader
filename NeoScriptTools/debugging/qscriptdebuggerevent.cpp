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

#include "qscriptdebuggerevent_p.h"
#include "qscriptdebuggervalue_p.h"

#include <QtCore/qhash.h>
#include <QtCore/qdatastream.h>

Q_DECLARE_METATYPE(QScriptDebuggerValue)

QT_BEGIN_NAMESPACE

class QScriptDebuggerEventPrivate
{
public:
    QScriptDebuggerEventPrivate();
    ~QScriptDebuggerEventPrivate();

    QScriptDebuggerEvent::Type type;
    QHash<QScriptDebuggerEvent::Attribute, QVariant> attributes;
};

QScriptDebuggerEventPrivate::QScriptDebuggerEventPrivate()
    : type(QScriptDebuggerEvent::None)
{
}

QScriptDebuggerEventPrivate::~QScriptDebuggerEventPrivate()
{
}

QScriptDebuggerEvent::QScriptDebuggerEvent()
    : d_ptr(new QScriptDebuggerEventPrivate)
{
    d_ptr->type = None;
}

QScriptDebuggerEvent::QScriptDebuggerEvent(Type type)
    : d_ptr(new QScriptDebuggerEventPrivate)
{
    d_ptr->type = type;
}

QScriptDebuggerEvent::QScriptDebuggerEvent(Type type, qint64 scriptId,
                                           int lineNumber, int columnNumber)
    : d_ptr(new QScriptDebuggerEventPrivate)
{
    d_ptr->type = type;
    d_ptr->attributes[ScriptID] = scriptId;
    d_ptr->attributes[LineNumber] = lineNumber;
    d_ptr->attributes[ColumnNumber] = columnNumber;
}

QScriptDebuggerEvent::QScriptDebuggerEvent(const QScriptDebuggerEvent &other)
    : d_ptr(new QScriptDebuggerEventPrivate)
{
    *d_ptr = *other.d_ptr;
}

QScriptDebuggerEvent::~QScriptDebuggerEvent()
{
}

QScriptDebuggerEvent &QScriptDebuggerEvent::operator=(const QScriptDebuggerEvent &other)
{
    *d_ptr = *other.d_ptr;
    return *this;
}

QScriptDebuggerEvent::Type QScriptDebuggerEvent::type() const
{
    Q_D(const QScriptDebuggerEvent);
    return d->type;
}

QVariant QScriptDebuggerEvent::attribute(Attribute attribute,
                                         const QVariant &defaultValue) const
{
    Q_D(const QScriptDebuggerEvent);
    return d->attributes.value(attribute, defaultValue);
}

void QScriptDebuggerEvent::setAttribute(Attribute attribute,
                                        const QVariant &value)
{
    Q_D(QScriptDebuggerEvent);
    if (!value.isValid())
        d->attributes.remove(attribute);
    else
        d->attributes[attribute] = value;
}

QHash<QScriptDebuggerEvent::Attribute, QVariant> QScriptDebuggerEvent::attributes() const
{
    Q_D(const QScriptDebuggerEvent);
    return d->attributes;
}

qint64 QScriptDebuggerEvent::scriptId() const
{
    Q_D(const QScriptDebuggerEvent);
    return d->attributes.value(ScriptID, -1).toLongLong();
}

void QScriptDebuggerEvent::setScriptId(qint64 id)
{
    Q_D(QScriptDebuggerEvent);
    d->attributes[ScriptID] = id;
}

QString QScriptDebuggerEvent::fileName() const
{
    Q_D(const QScriptDebuggerEvent);
    return d->attributes.value(FileName).toString();
}

void QScriptDebuggerEvent::setFileName(const QString &fileName)
{
    Q_D(QScriptDebuggerEvent);
    d->attributes[FileName] = fileName;
}

int QScriptDebuggerEvent::lineNumber() const
{
    Q_D(const QScriptDebuggerEvent);
    return d->attributes.value(LineNumber, -1).toInt();
}

void QScriptDebuggerEvent::setLineNumber(int lineNumber)
{
    Q_D(QScriptDebuggerEvent);
    d->attributes[LineNumber] = lineNumber;
}

int QScriptDebuggerEvent::columnNumber() const
{
    Q_D(const QScriptDebuggerEvent);
    return d->attributes.value(ColumnNumber, -1).toInt();
}

void QScriptDebuggerEvent::setColumnNumber(int columnNumber)
{
    Q_D(QScriptDebuggerEvent);
    d->attributes[ColumnNumber] = columnNumber;
}

int QScriptDebuggerEvent::breakpointId() const
{
    Q_D(const QScriptDebuggerEvent);
    return d->attributes.value(BreakpointID, -1).toInt();
}

void QScriptDebuggerEvent::setBreakpointId(int id)
{
    Q_D(QScriptDebuggerEvent);
    d->attributes[BreakpointID] = id;
}

QString QScriptDebuggerEvent::message() const
{
    Q_D(const QScriptDebuggerEvent);
    return d->attributes.value(Message).toString();
}

void QScriptDebuggerEvent::setMessage(const QString &message)
{
    Q_D(QScriptDebuggerEvent);
    d->attributes[Message] = message;
}

QScriptDebuggerValue QScriptDebuggerEvent::scriptValue() const
{
    Q_D(const QScriptDebuggerEvent);
    //return qvariant_cast<QScriptDebuggerValue>(d->attributes[Value]);
	QScriptDebuggerValue value;
	value.fromVariant(d->attributes.value(Value).toMap());
	return value;
}

void QScriptDebuggerEvent::setScriptValue(const QScriptDebuggerValue &value)
{
    Q_D(QScriptDebuggerEvent);
    //d->attributes[Value] = QVariant::fromValue(value);
	d->attributes[Value] = value.toVariant();
}

void QScriptDebuggerEvent::setNestedEvaluate(bool nested)
{
    Q_D(QScriptDebuggerEvent);
    d->attributes[IsNestedEvaluate] = nested;
}

bool QScriptDebuggerEvent::isNestedEvaluate() const
{
    Q_D(const QScriptDebuggerEvent);
    return d->attributes.value(IsNestedEvaluate).toBool();
}

void QScriptDebuggerEvent::setHasExceptionHandler(bool hasHandler)
{
    Q_D(QScriptDebuggerEvent);
    d->attributes[HasExceptionHandler] = hasHandler;
}

bool QScriptDebuggerEvent::hasExceptionHandler() const
{
    Q_D(const QScriptDebuggerEvent);
    return d->attributes.value(HasExceptionHandler).toBool();
}

/*!
  Returns true if this QScriptDebuggerEvent is equal to the \a other
  event, otherwise returns false.
*/
bool QScriptDebuggerEvent::operator==(const QScriptDebuggerEvent &other) const
{
    Q_D(const QScriptDebuggerEvent);
    const QScriptDebuggerEventPrivate *od = other.d_func();
    if (d == od)
        return true;
    if (!d || !od)
        return false;
    return ((d->type == od->type)
            && (d->attributes == od->attributes));
}

/*!
  Returns true if this QScriptDebuggerEvent is not equal to the \a
  other event, otherwise returns false.
*/
bool QScriptDebuggerEvent::operator!=(const QScriptDebuggerEvent &other) const
{
    return !(*this == other);
}

//> NeoScriptTools
void QScriptDebuggerEvent::fromVariant(const QVariant& var)
{
	QVariantMap in = var.toMap();
    QScriptDebuggerEventPrivate *d = d_ptr.data();

	QString typeStr = in["type"].toString();
	Type type = MaxUserEvent;
	if(typeStr == "Interrupted") type = Interrupted;
	else if(typeStr == "SteppingFinished") type = SteppingFinished;
	else if(typeStr == "LocationReached") type = LocationReached;
	else if(typeStr == "Breakpoint") type = Breakpoint;
	else if(typeStr == "Exception") type = Exception;
	else if(typeStr == "Trace") type = Trace;
	else if(typeStr == "InlineEvalFinished") type = InlineEvalFinished;
	else if(typeStr == "DebuggerInvocationRequest") type = DebuggerInvocationRequest;
	else if(typeStr == "ForcedReturn") type = ForcedReturn;
	else if(typeStr == "UserEvent") type = UserEvent;
	else type = None;
    d->type = type;

    QHash<QScriptDebuggerEvent::Attribute, QVariant> attribs;
	QVariantMap attribsMap = in["attributes"].toMap();
	foreach(const QString& keyStr, attribsMap.keys()) {
		Attribute key = MaxUserAttribute;
		if(keyStr == "scriptId") key = ScriptID;
		else if(keyStr == "fileName") key = FileName;
		else if(keyStr == "breakpointId") key = BreakpointID;
		else if(keyStr == "lineNumber") key = LineNumber;
		else if(keyStr == "columnNumber") key = ColumnNumber;
		else if(keyStr == "value") key = Value;
		else if(keyStr == "message") key = Message;
		else if(keyStr == "isNestedEvaluate") key = IsNestedEvaluate;
		else if(keyStr == "hasExceptionHandler") key = HasExceptionHandler;
		else if(keyStr == "userAttribute") key = UserAttribute;
		attribs[key] = attribsMap[keyStr];
    }
    d->attributes = attribs;
}

QVariant QScriptDebuggerEvent::toVariant() const
{
	QVariantMap out;
    const QScriptDebuggerEventPrivate *d = d_ptr.data();

	QString typeStr;
	switch(d->type){
	case None: typeStr = "None"; break;
    case Interrupted: typeStr = "Interrupted"; break;
    case SteppingFinished: typeStr = "SteppingFinished"; break;
    case LocationReached: typeStr = "LocationReached"; break;
    case Breakpoint: typeStr = "Breakpoint"; break;
    case Exception: typeStr = "Exception"; break;
    case Trace: typeStr = "Trace"; break;
    case InlineEvalFinished: typeStr = "InlineEvalFinished"; break;
    case DebuggerInvocationRequest: typeStr = "DebuggerInvocationRequest"; break;
    case ForcedReturn: typeStr = "ForcedReturn"; break;
    case UserEvent: typeStr = "UserEvent"; break;
	default: Q_ASSERT(0);
	}
	out["type"] = typeStr;

    QHash<QScriptDebuggerEvent::Attribute, QVariant>::const_iterator it;
	QVariantMap attribsMap;
    for (it = d->attributes.constBegin(); it != d->attributes.constEnd(); ++it) {
		QString keyStr;
		switch(it.key()){
		case ScriptID: keyStr = "scriptId"; break;
        case FileName: keyStr = "fileName"; break;
        case BreakpointID: keyStr = "breakpointId"; break;
        case LineNumber: keyStr = "lineNumber"; break;
        case ColumnNumber: keyStr = "columnNumber"; break;
        case Value: keyStr = "value"; break;
        case Message: keyStr = "message"; break;
        case IsNestedEvaluate: keyStr = "isNestedEvaluate"; break;
        case HasExceptionHandler: keyStr = "hasExceptionHandler"; break;
        case UserAttribute: keyStr = "userAttribute"; break;
		default: Q_ASSERT(0);
		}
		attribsMap[keyStr] = it.value();
    }
	out["attributes"] = attribsMap;

    return out;
}
//< NeoScriptTools

/*!
  \fn QDataStream &operator<<(QDataStream &stream, const QScriptDebuggerEvent &event)
  \relates QScriptDebuggerEvent

  Writes the given \a event to the specified \a stream.
*/
QDataStream &operator<<(QDataStream &out, const QScriptDebuggerEvent &event)
{
    const QScriptDebuggerEventPrivate *d = event.d_ptr.data();
    out << (quint32)d->type;
    out << (qint32)d->attributes.size();
    QHash<QScriptDebuggerEvent::Attribute, QVariant>::const_iterator it;
    for (it = d->attributes.constBegin(); it != d->attributes.constEnd(); ++it) {
        out << (quint32)it.key();
        out << it.value();
    }
    return out;
}

/*!
  \fn QDataStream &operator>>(QDataStream &stream, QScriptDebuggerEvent &event)
  \relates QScriptDebuggerEvent

  Reads a QScriptDebuggerEvent from the specified \a stream into the
  given \a event.
*/
QDataStream &operator>>(QDataStream &in, QScriptDebuggerEvent &event)
{
    QScriptDebuggerEventPrivate *d = event.d_ptr.data();

    quint32 type;
    in >> type;
    d->type = QScriptDebuggerEvent::Type(type);

    qint32 attribCount;
    in >> attribCount;
    QHash<QScriptDebuggerEvent::Attribute, QVariant> attribs;
    for (qint32 i = 0; i < attribCount; ++i) {
        quint32 key;
        in >> key;
        QVariant value;
        in >> value;
        attribs[QScriptDebuggerEvent::Attribute(key)] = value;
    }
    d->attributes = attribs;

    return in;
}

QT_END_NAMESPACE
