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

#include "qscriptdebuggervalue_p.h"

#include <QtCore/qdatastream.h>
#include <QtCore/qdebug.h>
#include <QtCore/qshareddata.h>

QT_BEGIN_NAMESPACE

/*!
  \since 4.5
  \class QScriptDebuggerValue
  \internal

  \brief The QScriptDebuggerValue class represents a script value.
*/

class QScriptDebuggerValuePrivate : public QSharedData
{
public:
    QScriptDebuggerValuePrivate();
    ~QScriptDebuggerValuePrivate();

    QScriptDebuggerValue::ValueType type;
    union {
        bool booleanValue;
        QString *stringValue;
        double numberValue;
        qint64 objectId;
    };
};

QScriptDebuggerValuePrivate::QScriptDebuggerValuePrivate()
    : type(QScriptDebuggerValue::NoValue)
{
}

QScriptDebuggerValuePrivate::~QScriptDebuggerValuePrivate()
{
    if (type == QScriptDebuggerValue::StringValue)
        delete stringValue;
}

QScriptDebuggerValue::QScriptDebuggerValue()
    : d_ptr(0)
{
}

QScriptDebuggerValue::QScriptDebuggerValue(double value)
    : d_ptr(new QScriptDebuggerValuePrivate)
{
    d_ptr->type = NumberValue;
    d_ptr->numberValue = value;
    d_ptr->ref.ref();
}

QScriptDebuggerValue::QScriptDebuggerValue(bool value)
    : d_ptr(new QScriptDebuggerValuePrivate)
{
    d_ptr->type = BooleanValue;
    d_ptr->booleanValue = value;
    d_ptr->ref.ref();
}

QScriptDebuggerValue::QScriptDebuggerValue(const QString &value)
    : d_ptr(new QScriptDebuggerValuePrivate)
{
    d_ptr->type = StringValue;
    d_ptr->stringValue = new QString(value);
    d_ptr->ref.ref();
}

QScriptDebuggerValue::QScriptDebuggerValue(qint64 objectId)
    : d_ptr(new QScriptDebuggerValuePrivate)
{
    d_ptr->type = ObjectValue;
    d_ptr->objectId = objectId;
    d_ptr->ref.ref();
}

QScriptDebuggerValue::QScriptDebuggerValue(ValueType type)
    : d_ptr(new QScriptDebuggerValuePrivate)
{
    d_ptr->type = type;
    d_ptr->ref.ref();
}

QScriptDebuggerValue::QScriptDebuggerValue(const QScriptDebuggerValue &other)
    : d_ptr(other.d_ptr.data())
{
    if (d_ptr)
        d_ptr->ref.ref();
}

QScriptDebuggerValue::~QScriptDebuggerValue()
{
}

QScriptDebuggerValue &QScriptDebuggerValue::operator=(const QScriptDebuggerValue &other)
{
    d_ptr.assign(other.d_ptr.data());
    return *this;
}

/*!
  Returns the type of this value.
*/
QScriptDebuggerValue::ValueType QScriptDebuggerValue::type() const
{
    Q_D(const QScriptDebuggerValue);
    if (!d)
        return NoValue;
    return d->type;
}

/*!
  Returns this value as a number.
*/
double QScriptDebuggerValue::numberValue() const
{
    Q_D(const QScriptDebuggerValue);
    if (!d)
        return 0;
    Q_ASSERT(d->type == NumberValue);
    return d->numberValue;
}

/*!
  Returns this value as a boolean.
*/
bool QScriptDebuggerValue::booleanValue() const
{
    Q_D(const QScriptDebuggerValue);
    if (!d)
        return false;
    Q_ASSERT(d->type == BooleanValue);
    return d->booleanValue;
}

/*!
  Returns this value as a string.
*/
QString QScriptDebuggerValue::stringValue() const
{
    Q_D(const QScriptDebuggerValue);
    if (!d)
        return QString();
    Q_ASSERT(d->type == StringValue);
    return *d->stringValue;
}

/*!
  Returns this value as an object ID.
*/
qint64 QScriptDebuggerValue::objectId() const
{
    Q_D(const QScriptDebuggerValue);
    if (!d)
        return -1;
    Q_ASSERT(d->type == ObjectValue);
    return d->objectId;
}

/*!
  Returns a string representation of this value.
*/
QString QScriptDebuggerValue::toString() const
{
    Q_D(const QScriptDebuggerValue);
    if (!d)
        return QString();
    switch (d->type) {
    case NoValue:
        return QString();
    case UndefinedValue:
        return QString::fromLatin1("undefined");
    case NullValue:
        return QString::fromLatin1("null");
    case BooleanValue:
        if (d->booleanValue)
            return QString::fromLatin1("true");
        else
            return QString::fromLatin1("false");
    case StringValue:
        return *d->stringValue;
    case NumberValue:
        return QString::number(d->numberValue); // ### qScriptNumberToString()
    case ObjectValue:
        return QString::fromLatin1("[object Object]");
    }
    return QString();
}

/*!
  Returns true if this QScriptDebuggerValue is equal to the \a other
  value, otherwise returns false.
*/
bool QScriptDebuggerValue::operator==(const QScriptDebuggerValue &other) const
{
    Q_D(const QScriptDebuggerValue);
    const QScriptDebuggerValuePrivate *od = other.d_func();
    if (d == od)
        return true;
    if (!d || !od)
        return false;
    if (d->type != od->type)
        return false;
    switch (d->type) {
    case NoValue:
    case UndefinedValue:
    case NullValue:
        return true;
    case BooleanValue:
        return d->booleanValue == od->booleanValue;
    case StringValue:
        return *d->stringValue == *od->stringValue;
    case NumberValue:
        return d->numberValue == od->numberValue;
    case ObjectValue:
        return d->objectId == od->objectId;
    }
    return false;
}

/*!
  Returns true if this QScriptDebuggerValue is not equal to the \a
  other value, otherwise returns false.
*/
bool QScriptDebuggerValue::operator!=(const QScriptDebuggerValue &other) const
{
    return !(*this == other);
}

//> NeoScriptTools
void QScriptDebuggerValue::fromVariant(const QVariantMap& in)
{
	d_ptr.reset(new QScriptDebuggerValuePrivate);
	QString strType = in["type"].toString();
	if(strType == "UndefinedValue")
		d_ptr->type = QScriptDebuggerValue::UndefinedValue;
	else if(strType == "NullValue")
		d_ptr->type = QScriptDebuggerValue::NullValue;
	else if(strType == "BooleanValue")
	{
		d_ptr->type = QScriptDebuggerValue::BooleanValue;
		d_ptr->booleanValue = in["value"].toBool();
	}
	else if(strType == "StringValue")
	{
		d_ptr->type = QScriptDebuggerValue::StringValue;
		d_ptr->stringValue = new QString(in["value"].toString());
	}
	else if(strType == "NumberValue")
	{
		d_ptr->type = QScriptDebuggerValue::NumberValue;
		d_ptr->numberValue = in["value"].toBool();
	}
	else if(strType == "ObjectValue")
	{
		d_ptr->type = QScriptDebuggerValue::ObjectValue;
		d_ptr->objectId = in["value"].toLongLong();
	}
	else 
		d_ptr->type = QScriptDebuggerValue::NoValue;
	d_ptr->ref.ref();
}

QVariantMap QScriptDebuggerValue::toVariant() const
{
	QVariantMap out;
	out["type"] = type();
    switch (type()) {
    case QScriptDebuggerValue::NoValue:
		out["type"] = "NoValue";
		break;
    case QScriptDebuggerValue::UndefinedValue:
		out["type"] = "UndefinedValue";
		break;
    case QScriptDebuggerValue::NullValue:
		out["type"] = "NullValue";
		break;
    case QScriptDebuggerValue::BooleanValue:
		out["type"] = "BooleanValue";
		out["value"] = booleanValue();
        break;
    case QScriptDebuggerValue::StringValue:
		out["type"] = "StringValue";
        out["value"] = stringValue();
        break;
    case QScriptDebuggerValue::NumberValue:
		out["type"] = "NumberValue";
        out["value"] = numberValue();
        break;
    case QScriptDebuggerValue::ObjectValue:
		out["type"] = "ObjectValue";
        out["value"] = objectId();
        break;
    }
    return out;
}
//< NeoScriptTools

/*!
  \fn QDataStream &operator<<(QDataStream &stream, const QScriptDebuggerValue &value)
  \relates QScriptDebuggerValue

  Writes the given \a value to the specified \a stream.
*/
QDataStream &operator<<(QDataStream &out, const QScriptDebuggerValue &value)
{
    out << (quint32)value.type();
    switch (value.type()) {
    case QScriptDebuggerValue::NoValue:
    case QScriptDebuggerValue::UndefinedValue:
    case QScriptDebuggerValue::NullValue:
        break;
    case QScriptDebuggerValue::BooleanValue:
        out << value.booleanValue();
        break;
    case QScriptDebuggerValue::StringValue:
        out << value.stringValue();
        break;
    case QScriptDebuggerValue::NumberValue:
        out << value.numberValue();
        break;
    case QScriptDebuggerValue::ObjectValue:
        out << value.objectId();
        break;
    }
    return out;
}

/*!
  \fn QDataStream &operator>>(QDataStream &stream, QScriptDebuggerValue &value)
  \relates QScriptDebuggerValue

  Reads a QScriptDebuggerValue from the specified \a stream into the
  given \a value.
*/
QDataStream &operator>>(QDataStream &in, QScriptDebuggerValue &value)
{
    quint32 type;
    in >> type;
    switch (QScriptDebuggerValue::ValueType(type)) {
    case QScriptDebuggerValue::UndefinedValue:
    case QScriptDebuggerValue::NullValue:
        value = QScriptDebuggerValue(QScriptDebuggerValue::ValueType(type));
        break;
    case QScriptDebuggerValue::BooleanValue: {
        bool b;
        in >> b;
        value = QScriptDebuggerValue(b);
    }   break;
    case QScriptDebuggerValue::StringValue: {
        QString s;
        in >> s;
        value = QScriptDebuggerValue(s);
    }   break;
    case QScriptDebuggerValue::NumberValue: {
        double d;
        in >> d;
        value = QScriptDebuggerValue(d);
    }   break;
    case QScriptDebuggerValue::ObjectValue: {
        qint64 id;
        in >> id;
        value = QScriptDebuggerValue(id);
    }   break;
    case QScriptDebuggerValue::NoValue:
    default:
        value = QScriptDebuggerValue();
        break;
    }
    return in;
}

QT_END_NAMESPACE
