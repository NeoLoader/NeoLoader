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

#include "qscriptdebuggercommand_p.h"
#include "qscriptbreakpointdata_p.h"
#include "qscriptdebuggervalue_p.h"

#include <QtCore/qhash.h>
#include <QtCore/qdatastream.h>
#include <QtCore/qstringlist.h>

Q_DECLARE_METATYPE(QScriptBreakpointData)
Q_DECLARE_METATYPE(QScriptDebuggerValue)

QT_BEGIN_NAMESPACE

/*!
  \since 4.5
  \class QScriptDebuggerCommand
  \internal

  \brief The QScriptDebuggerCommand class represents a command issued to a QScriptDebuggerFrontend.

  A debugger command is described by a command type and zero or more
  attributes.  Such commands are generated internally by the
  QScriptDebuggerFrontend class (through the scheduleXXX commands). A
  command is typically passed on to a QScriptDebuggerCommandExecutor
  that applies the command to a QScriptDebuggerBackend.
*/

class QScriptDebuggerCommandPrivate
{
public:
    QScriptDebuggerCommandPrivate();
    ~QScriptDebuggerCommandPrivate();

    QScriptDebuggerCommand::Type type;
    QHash<QScriptDebuggerCommand::Attribute, QVariant> attributes;
};

QScriptDebuggerCommandPrivate::QScriptDebuggerCommandPrivate()
    : type(QScriptDebuggerCommand::None)
{
}

QScriptDebuggerCommandPrivate::~QScriptDebuggerCommandPrivate()
{
}

/*!
  Constructs a QScriptDebuggerCommand of type None.
*/
QScriptDebuggerCommand::QScriptDebuggerCommand()
    : d_ptr(new QScriptDebuggerCommandPrivate)
{
    d_ptr->type = None;
}

/*!
  Constructs a QScriptDebuggerCommand of the given \a type, with no
  attributes defined.
*/
QScriptDebuggerCommand::QScriptDebuggerCommand(Type type)
    : d_ptr(new QScriptDebuggerCommandPrivate)
{
    d_ptr->type = type;
}

/*!
  Constructs a QScriptDebuggerCommand that is a copy of the \a other
  command.
*/
QScriptDebuggerCommand::QScriptDebuggerCommand(const QScriptDebuggerCommand &other)
    : d_ptr(new QScriptDebuggerCommandPrivate)
{
    *d_ptr = *other.d_ptr;
}

/*!
  Destroys this QScriptDebuggerCommand.
*/
QScriptDebuggerCommand::~QScriptDebuggerCommand()
{
}

/*!
  Assigns the \a other value to this QScriptDebuggerCommand.
*/
QScriptDebuggerCommand &QScriptDebuggerCommand::operator=(const QScriptDebuggerCommand &other)
{
    *d_ptr = *other.d_ptr;
    return *this;
}

/*!
  Returns the type of this command.
*/
QScriptDebuggerCommand::Type QScriptDebuggerCommand::type() const
{
    Q_D(const QScriptDebuggerCommand);
    return d->type;
}

/*!
  Returns the value of the given \a attribute, or \a defaultValue
  if the attribute is not defined.
*/
QVariant QScriptDebuggerCommand::attribute(Attribute attribute,
                                           const QVariant &defaultValue) const
{
    Q_D(const QScriptDebuggerCommand);
    return d->attributes.value(attribute, defaultValue);
}

/*!
  Sets the \a value of the given \a attribute.
*/
void QScriptDebuggerCommand::setAttribute(Attribute attribute,
                                          const QVariant &value)
{
    Q_D(QScriptDebuggerCommand);
    if (!value.isValid())
        d->attributes.remove(attribute);
    else
        d->attributes[attribute] = value;
}

QHash<QScriptDebuggerCommand::Attribute, QVariant> QScriptDebuggerCommand::attributes() const
{
    Q_D(const QScriptDebuggerCommand);
    return d->attributes;
}

/*!
  Returns the FileName attribute of this command converted to a string.
  This function is provided for convenience.

  \sa attribute()
*/
QString QScriptDebuggerCommand::fileName() const
{
    Q_D(const QScriptDebuggerCommand);
    return d->attributes.value(FileName).toString();
}

void QScriptDebuggerCommand::setFileName(const QString &fileName)
{
    Q_D(QScriptDebuggerCommand);
    d->attributes[FileName] = fileName;
}

/*!
  Returns the LineNumber attribute of this command converted to an int.
  This function is provided for convenience.

  \sa attribute()
*/
int QScriptDebuggerCommand::lineNumber() const
{ 
    Q_D(const QScriptDebuggerCommand);
    return d->attributes.value(LineNumber, -1).toInt();
}

void QScriptDebuggerCommand::setLineNumber(int lineNumber)
{
    Q_D(QScriptDebuggerCommand);
    d->attributes[LineNumber] = lineNumber;
}

/*!
  Returns the ScriptID attribute of this command converted to a qint64.
  This function is provided for convenience.

  \sa attribute()
*/
qint64 QScriptDebuggerCommand::scriptId() const
{
    Q_D(const QScriptDebuggerCommand);
    return d->attributes.value(ScriptID, -1).toLongLong();
}

void QScriptDebuggerCommand::setScriptId(qint64 id)
{
    Q_D(QScriptDebuggerCommand);
    d->attributes[ScriptID] = id;
}

QString QScriptDebuggerCommand::program() const
{
    Q_D(const QScriptDebuggerCommand);
    return d->attributes.value(Program).toString();
}

void QScriptDebuggerCommand::setProgram(const QString &program)
{
    Q_D(QScriptDebuggerCommand);
    d->attributes[Program] = program;
}

int QScriptDebuggerCommand::breakpointId() const
{
    Q_D(const QScriptDebuggerCommand);
    return d->attributes.value(BreakpointID, -1).toInt();
}

void QScriptDebuggerCommand::setBreakpointId(int id)
{
    Q_D(QScriptDebuggerCommand);
    d->attributes[BreakpointID] = id;
}

QScriptBreakpointData QScriptDebuggerCommand::breakpointData() const
{
    Q_D(const QScriptDebuggerCommand);
    //return qvariant_cast<QScriptBreakpointData>(d->attributes.value(BreakpointData));
	QScriptBreakpointData data;
	data.fromVariant(d->attributes.value(BreakpointData).toMap());
	return data;
}

void QScriptDebuggerCommand::setBreakpointData(const QScriptBreakpointData &data)
{
    Q_D(QScriptDebuggerCommand);
    //d->attributes[BreakpointData] = QVariant::fromValue(data);
	d->attributes[BreakpointData] = data.toVariant();
}

QScriptDebuggerValue QScriptDebuggerCommand::scriptValue() const
{
    Q_D(const QScriptDebuggerCommand);
    //return qvariant_cast<QScriptDebuggerValue>(d->attributes.value(ScriptValue));
	QScriptDebuggerValue value;
	value.fromVariant(d->attributes.value(ScriptValue).toMap());
	return value;
}

void QScriptDebuggerCommand::setScriptValue(const QScriptDebuggerValue &value)
{
    Q_D(QScriptDebuggerCommand);
    //d->attributes[ScriptValue] = QVariant::fromValue(value);
	d->attributes[ScriptValue] = value.toVariant();
}

int QScriptDebuggerCommand::contextIndex() const
{
    Q_D(const QScriptDebuggerCommand);
    return d->attributes.value(ContextIndex, -1).toInt();
}

void QScriptDebuggerCommand::setContextIndex(int index)
{
    Q_D(QScriptDebuggerCommand);
    d->attributes[ContextIndex] = index;
}

int QScriptDebuggerCommand::iteratorId() const
{
    Q_D(const QScriptDebuggerCommand);
    return d->attributes.value(IteratorID, -1).toInt();
}

void QScriptDebuggerCommand::setIteratorId(int id)
{
    Q_D(QScriptDebuggerCommand);
    d->attributes[IteratorID] = id;
}

QString QScriptDebuggerCommand::name() const
{
    Q_D(const QScriptDebuggerCommand);
    return d->attributes.value(Name).toString();
}

void QScriptDebuggerCommand::setName(const QString &name)
{
    Q_D(QScriptDebuggerCommand);
    d->attributes[Name] = name;
}

QScriptDebuggerValue QScriptDebuggerCommand::subordinateScriptValue() const
{
    Q_D(const QScriptDebuggerCommand);
    //return qvariant_cast<QScriptDebuggerValue>(d->attributes.value(SubordinateScriptValue));
	QScriptDebuggerValue value;
	value.fromVariant(d->attributes.value(SubordinateScriptValue).toMap());
	return value;
}

void QScriptDebuggerCommand::setSubordinateScriptValue(const QScriptDebuggerValue &value)
{
    Q_D(QScriptDebuggerCommand);
    //d->attributes[SubordinateScriptValue] = QVariant::fromValue(value);
	d->attributes[SubordinateScriptValue] = value.toVariant();
}

int QScriptDebuggerCommand::snapshotId() const
{
    Q_D(const QScriptDebuggerCommand);
    return d->attributes.value(SnapshotID, -1).toInt();
}

void QScriptDebuggerCommand::setSnapshotId(int id)
{
    Q_D(QScriptDebuggerCommand);
    d->attributes[SnapshotID] = id;
}

/*!
  Returns true if this QScriptDebuggerCommand is equal to the \a other
  command, otherwise returns false.
*/
bool QScriptDebuggerCommand::operator==(const QScriptDebuggerCommand &other) const
{
    Q_D(const QScriptDebuggerCommand);
    const QScriptDebuggerCommandPrivate *od = other.d_func();
    if (d == od)
        return true;
    if (!d || !od)
        return false;
    return ((d->type == od->type)
            && (d->attributes == od->attributes));
}

/*!
  Returns true if this QScriptDebuggerCommand is not equal to the \a
  other command, otherwise returns false.
*/
bool QScriptDebuggerCommand::operator!=(const QScriptDebuggerCommand &other) const
{
    return !(*this == other);
}

QScriptDebuggerCommand QScriptDebuggerCommand::interruptCommand()
{
    QScriptDebuggerCommand cmd(Interrupt);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::continueCommand()
{
    QScriptDebuggerCommand cmd(Continue);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::stepIntoCommand(int count)
{
    QScriptDebuggerCommand cmd(StepInto);
    cmd.setAttribute(StepCount, count);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::stepOverCommand(int count)
{
    QScriptDebuggerCommand cmd(StepOver);
    cmd.setAttribute(StepCount, count);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::stepOutCommand()
{
    QScriptDebuggerCommand cmd(StepOut);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::runToLocationCommand(const QString &fileName, int lineNumber)
{
    QScriptDebuggerCommand cmd(RunToLocation);
    cmd.setFileName(fileName);
    cmd.setLineNumber(lineNumber);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::runToLocationCommand(qint64 scriptId, int lineNumber)
{
    QScriptDebuggerCommand cmd(RunToLocationByID);
    cmd.setScriptId(scriptId);
    cmd.setLineNumber(lineNumber);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::forceReturnCommand(int contextIndex, const QScriptDebuggerValue &value)
{
    QScriptDebuggerCommand cmd(ForceReturn);
    cmd.setContextIndex(contextIndex);
    cmd.setScriptValue(value);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::resumeCommand()
{
    QScriptDebuggerCommand cmd(Resume);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::setBreakpointCommand(const QString &fileName, int lineNumber)
{
    QScriptDebuggerCommand cmd(SetBreakpoint);
    cmd.setBreakpointData(QScriptBreakpointData(fileName, lineNumber));
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::setBreakpointCommand(const QScriptBreakpointData &data)
{
    QScriptDebuggerCommand cmd(SetBreakpoint);
    cmd.setBreakpointData(data);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::deleteBreakpointCommand(int id)
{
    QScriptDebuggerCommand cmd(DeleteBreakpoint);
    cmd.setBreakpointId(id);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::deleteAllBreakpointsCommand()
{
    QScriptDebuggerCommand cmd(DeleteAllBreakpoints);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::getBreakpointsCommand()
{
    QScriptDebuggerCommand cmd(GetBreakpoints);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::getBreakpointDataCommand(int id)
{
    QScriptDebuggerCommand cmd(GetBreakpointData);
    cmd.setBreakpointId(id);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::setBreakpointDataCommand(int id, const QScriptBreakpointData &data)
{
    QScriptDebuggerCommand cmd(SetBreakpointData);
    cmd.setBreakpointId(id);
    cmd.setBreakpointData(data);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::getScriptsCommand()
{
    QScriptDebuggerCommand cmd(GetScripts);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::getScriptDataCommand(qint64 id)
{
    QScriptDebuggerCommand cmd(GetScriptData);
    cmd.setScriptId(id);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::scriptsCheckpointCommand()
{
    QScriptDebuggerCommand cmd(ScriptsCheckpoint);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::getScriptsDeltaCommand()
{
    QScriptDebuggerCommand cmd(GetScriptsDelta);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::resolveScriptCommand(const QString &fileName)
{
    QScriptDebuggerCommand cmd(ResolveScript);
    cmd.setFileName(fileName);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::getBacktraceCommand()
{
    QScriptDebuggerCommand cmd(GetBacktrace);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::getContextCountCommand()
{
    QScriptDebuggerCommand cmd(GetContextCount);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::getContextStateCommand(int contextIndex)
{
    QScriptDebuggerCommand cmd(GetContextState);
    cmd.setContextIndex(contextIndex);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::getContextInfoCommand(int contextIndex)
{
    QScriptDebuggerCommand cmd(GetContextInfo);
    cmd.setContextIndex(contextIndex);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::getContextIdCommand(int contextIndex)
{
    QScriptDebuggerCommand cmd(GetContextID);
    cmd.setContextIndex(contextIndex);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::getThisObjectCommand(int contextIndex)
{
    QScriptDebuggerCommand cmd(GetThisObject);
    cmd.setContextIndex(contextIndex);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::getActivationObjectCommand(int contextIndex)
{
    QScriptDebuggerCommand cmd(GetActivationObject);
    cmd.setContextIndex(contextIndex);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::getScopeChainCommand(int contextIndex)
{
    QScriptDebuggerCommand cmd(GetScopeChain);
    cmd.setContextIndex(contextIndex);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::contextsCheckpoint()
{
    QScriptDebuggerCommand cmd(ContextsCheckpoint);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::getPropertyExpressionValue(
    int contextIndex, int lineNumber, const QStringList &path)
{
    QScriptDebuggerCommand cmd(GetPropertyExpressionValue);
    cmd.setContextIndex(contextIndex);
    cmd.setLineNumber(lineNumber);
    cmd.setAttribute(UserAttribute, path);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::getCompletions(
    int contextIndex, const QStringList &path)
{
    QScriptDebuggerCommand cmd(GetCompletions);
    cmd.setContextIndex(contextIndex);
    cmd.setAttribute(UserAttribute, path);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::newScriptObjectSnapshotCommand()
{
    QScriptDebuggerCommand cmd(NewScriptObjectSnapshot);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::scriptObjectSnapshotCaptureCommand(int id, const QScriptDebuggerValue &object)
{
    Q_ASSERT(object.type() == QScriptDebuggerValue::ObjectValue);
    QScriptDebuggerCommand cmd(ScriptObjectSnapshotCapture);
    cmd.setSnapshotId(id);
    cmd.setScriptValue(object);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::deleteScriptObjectSnapshotCommand(int id)
{
    QScriptDebuggerCommand cmd(DeleteScriptObjectSnapshot);
    cmd.setSnapshotId(id);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::newScriptValueIteratorCommand(const QScriptDebuggerValue &object)
{
    QScriptDebuggerCommand cmd(NewScriptValueIterator);
    Q_ASSERT(object.type() == QScriptDebuggerValue::ObjectValue);
    cmd.setScriptValue(object);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::getPropertiesByIteratorCommand(int id, int count)
{
    Q_UNUSED(count);
    QScriptDebuggerCommand cmd(GetPropertiesByIterator);
    cmd.setIteratorId(id);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::deleteScriptValueIteratorCommand(int id)
{
    QScriptDebuggerCommand cmd(DeleteScriptValueIterator);
    cmd.setIteratorId(id);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::evaluateCommand(
    int contextIndex, const QString &program, const QString &fileName, int lineNumber)
{
    QScriptDebuggerCommand cmd(Evaluate);
    cmd.setContextIndex(contextIndex);
    cmd.setProgram(program);
    cmd.setFileName(fileName);
    cmd.setLineNumber(lineNumber);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::scriptValueToStringCommand(const QScriptDebuggerValue &value)
{
    QScriptDebuggerCommand cmd(ScriptValueToString);
    cmd.setScriptValue(value);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::setScriptValuePropertyCommand(
    const QScriptDebuggerValue &object, const QString &name,
    const QScriptDebuggerValue &value)
{
    QScriptDebuggerCommand cmd(SetScriptValueProperty);
    cmd.setScriptValue(object);
    cmd.setName(name);
    cmd.setSubordinateScriptValue(value);
    return cmd;
}

QScriptDebuggerCommand QScriptDebuggerCommand::clearExceptionsCommand()
{
    QScriptDebuggerCommand cmd(ClearExceptions);
    return cmd;
}

//> NeoScriptTools
void QScriptDebuggerCommand::fromVariant(const QVariant& var)
{
	QVariantMap in = var.toMap();
    QScriptDebuggerCommandPrivate *d = d_ptr.data();

	QString typeStr = in["type"].toString();
	Type type = MaxUserCommand;
	if(typeStr == "None") type = None;

	else if(typeStr == "Interrupt") type = Interrupt;
	else if(typeStr == "Continue") type = Continue;
	else if(typeStr == "StepInto") type = StepInto;
	else if(typeStr == "StepOver") type = StepOver;
	else if(typeStr == "StepOut") type = StepOut;
	else if(typeStr == "RunToLocation") type = RunToLocation;
	else if(typeStr == "RunToLocationByID") type = RunToLocationByID;
	else if(typeStr == "ForceReturn") type = ForceReturn;
	else if(typeStr == "Resume") type = Resume;

	else if(typeStr == "SetBreakpoint") type = SetBreakpoint;
	else if(typeStr == "DeleteBreakpoint") type = DeleteBreakpoint;
	else if(typeStr == "DeleteAllBreakpoints") type = DeleteAllBreakpoints;
	else if(typeStr == "GetBreakpoints") type = GetBreakpoints;
	else if(typeStr == "GetBreakpointData") type = GetBreakpointData;
	else if(typeStr == "SetBreakpointData") type = SetBreakpointData;

	else if(typeStr == "GetScripts") type = GetScripts;
	else if(typeStr == "GetScriptData") type = GetScriptData;
	else if(typeStr == "ScriptsCheckpoint") type = ScriptsCheckpoint;
	else if(typeStr == "GetScriptsDelta") type = GetScriptsDelta;
	else if(typeStr == "ResolveScript") type = ResolveScript;

	else if(typeStr == "GetBacktrace") type = GetBacktrace;
	else if(typeStr == "GetContextCount") type = GetContextCount;
	else if(typeStr == "GetContextInfo") type = GetContextInfo;
	else if(typeStr == "GetContextState") type = GetContextState;
	else if(typeStr == "GetContextID") type = GetContextID;
	else if(typeStr == "GetThisObject") type = GetThisObject;
	else if(typeStr == "GetActivationObject") type = GetActivationObject;
	else if(typeStr == "GetScopeChain") type = GetScopeChain;
	else if(typeStr == "ContextsCheckpoint") type = ContextsCheckpoint;
	else if(typeStr == "GetPropertyExpressionValue") type = GetPropertyExpressionValue;
	else if(typeStr == "GetCompletions") type = GetCompletions;

	else if(typeStr == "NewScriptObjectSnapshot") type = NewScriptObjectSnapshot;
	else if(typeStr == "ScriptObjectSnapshotCapture") type = ScriptObjectSnapshotCapture;
	else if(typeStr == "DeleteScriptObjectSnapshot") type = DeleteScriptObjectSnapshot;

	else if(typeStr == "NewScriptValueIterator") type = NewScriptValueIterator;
	else if(typeStr == "GetPropertiesByIterator") type = GetPropertiesByIterator;
	else if(typeStr == "DeleteScriptValueIterator") type = DeleteScriptValueIterator;

	else if(typeStr == "Evaluate") type = Evaluate;

	else if(typeStr == "SetScriptValueProperty") type = SetScriptValueProperty;
	else if(typeStr == "ScriptValueToString") type = ScriptValueToString;

	else if(typeStr == "ClearExceptions") type = ClearExceptions;

	else if(typeStr == "UserCommand") type = UserCommand;
	d->type = type;

    QHash<QScriptDebuggerCommand::Attribute, QVariant> attribs;
	QVariantMap attribsMap = in["attributes"].toMap();
	foreach(const QString& keyStr, attribsMap.keys()) {
		Attribute key = MaxUserAttribute;
		if(keyStr == "scriptId") key = ScriptID;
		else if(keyStr == "fileName") key = FileName;
		else if(keyStr == "lineNumber") key = LineNumber;
		else if(keyStr == "program") key = Program;
		else if(keyStr == "breakpointId") key = BreakpointID;
		else if(keyStr == "breakpointData") key = BreakpointData;
		else if(keyStr == "contextIndex") key = ContextIndex;
		else if(keyStr == "scriptValue") key = ScriptValue;
		else if(keyStr == "stepCount") key = StepCount;
		else if(keyStr == "iteratorId") key = IteratorID;
		else if(keyStr == "name") key = Name;
		else if(keyStr == "subordinateScriptValue") key = SubordinateScriptValue;
		else if(keyStr == "snapshotId") key = SnapshotID;
		else if(keyStr == "userAttribute") key = UserAttribute;
        attribs[key] =  attribsMap[keyStr];
    }
    d->attributes = attribs;
}

QVariant QScriptDebuggerCommand::toVariant() const
{
    const QScriptDebuggerCommandPrivate *d = d_ptr.data();
    QVariantMap out;

	QString typeStr;
	switch(d->type){
	case None: typeStr = "None"; break;

	case Interrupt: typeStr = "Interrupt"; break;
	case Continue: typeStr = "Continue"; break;
	case StepInto: typeStr = "StepInto"; break;
	case StepOver: typeStr = "StepOver"; break;
	case StepOut: typeStr = "StepOut"; break;
	case RunToLocation: typeStr = "RunToLocation"; break;
	case RunToLocationByID: typeStr = "RunToLocationByID"; break;
	case ForceReturn: typeStr = "ForceReturn"; break;
	case Resume: typeStr = "Resume"; break;

	case SetBreakpoint: typeStr = "SetBreakpoint"; break;
	case DeleteBreakpoint: typeStr = "DeleteBreakpoint"; break;
	case DeleteAllBreakpoints: typeStr = "DeleteAllBreakpoints"; break;
	case GetBreakpoints: typeStr = "GetBreakpoints"; break;
	case GetBreakpointData: typeStr = "GetBreakpointData"; break;
	case SetBreakpointData: typeStr = "SetBreakpointData"; break;

	case GetScripts: typeStr = "GetScripts"; break;
	case GetScriptData: typeStr = "GetScriptData"; break;
	case ScriptsCheckpoint: typeStr = "ScriptsCheckpoint"; break;
	case GetScriptsDelta: typeStr = "GetScriptsDelta"; break;
	case ResolveScript: typeStr = "ResolveScript"; break;

	case GetBacktrace: typeStr = "GetBacktrace"; break;
	case GetContextCount: typeStr = "GetContextCount"; break;
	case GetContextInfo: typeStr = "GetContextInfo"; break;
	case GetContextState: typeStr = "GetContextState"; break;
	case GetContextID: typeStr = "GetContextID"; break;
	case GetThisObject: typeStr = "GetThisObject"; break;
	case GetActivationObject: typeStr = "GetActivationObject"; break;
	case GetScopeChain: typeStr = "GetScopeChain"; break;
	case ContextsCheckpoint: typeStr = "ContextsCheckpoint"; break;
	case GetPropertyExpressionValue: typeStr = "GetPropertyExpressionValue"; break;
	case GetCompletions: typeStr = "GetCompletions"; break;

	case NewScriptObjectSnapshot: typeStr = "NewScriptObjectSnapshot"; break;
	case ScriptObjectSnapshotCapture: typeStr = "ScriptObjectSnapshotCapture"; break;
	case DeleteScriptObjectSnapshot: typeStr = "DeleteScriptObjectSnapshot"; break;

	case NewScriptValueIterator: typeStr = "NewScriptValueIterator"; break;
	case GetPropertiesByIterator: typeStr = "GetPropertiesByIterator"; break;
	case DeleteScriptValueIterator: typeStr = "DeleteScriptValueIterator"; break;

	case Evaluate: typeStr = "Evaluate"; break;

	case SetScriptValueProperty: typeStr = "SetScriptValueProperty"; break;
	case ScriptValueToString: typeStr = "ScriptValueToString"; break;

	case ClearExceptions: typeStr = "ClearExceptions"; break;

	case UserCommand: typeStr = "UserCommand"; break;
	default: Q_ASSERT(0);
	}
	out["type"] = typeStr;

    QHash<QScriptDebuggerCommand::Attribute, QVariant>::const_iterator it;
	QVariantMap attribsMap;
    for (it = d->attributes.constBegin(); it != d->attributes.constEnd(); ++it) {
		QString keyStr;
		switch(it.key()){
		case ScriptID: keyStr = "scriptId"; break;
		case FileName: keyStr = "fileName"; break;
		case LineNumber: keyStr = "lineNumber"; break;
		case Program: keyStr = "program"; break;
		case BreakpointID: keyStr = "breakpointId"; break;
		case BreakpointData: keyStr = "breakpointData"; break;
		case ContextIndex: keyStr = "contextIndex"; break;
		case ScriptValue: keyStr = "scriptValue"; break;
		case StepCount: keyStr = "stepCount"; break;
		case IteratorID: keyStr = "iteratorId"; break;
		case Name: keyStr = "name"; break;
		case SubordinateScriptValue: keyStr = "subordinateScriptValue"; break;
		case SnapshotID: keyStr = "snapshotId"; break;
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
  \fn QDataStream &operator<<(QDataStream &stream, const QScriptDebuggerCommand &command)
  \relates QScriptDebuggerCommand

  Writes the given \a command to the specified \a stream.
*/
QDataStream &operator<<(QDataStream &out, const QScriptDebuggerCommand &command)
{
    const QScriptDebuggerCommandPrivate *d = command.d_ptr.data();
    out << (quint32)d->type;
    out << (qint32)d->attributes.size();
    QHash<QScriptDebuggerCommand::Attribute, QVariant>::const_iterator it;
    for (it = d->attributes.constBegin(); it != d->attributes.constEnd(); ++it) {
        out << (quint32)it.key();
        out << it.value();
    }
    return out;
}

/*!
  \fn QDataStream &operator>>(QDataStream &stream, QScriptDebuggerCommand &command)
  \relates QScriptDebuggerCommand

  Reads a QScriptDebuggerCommand from the specified \a stream into the
  given \a command.
*/
QDataStream &operator>>(QDataStream &in, QScriptDebuggerCommand &command)
{
    QScriptDebuggerCommandPrivate *d = command.d_ptr.data();

    quint32 type;
    in >> type;
    d->type = QScriptDebuggerCommand::Type(type);

    qint32 attribCount;
    in >> attribCount;
    QHash<QScriptDebuggerCommand::Attribute, QVariant> attribs;
    for (qint32 i = 0; i < attribCount; ++i) {
        quint32 key;
        in >> key;
        QVariant value;
        in >> value;
        attribs[QScriptDebuggerCommand::Attribute(key)] = value;
    }
    d->attributes = attribs;

    return in;
}

QT_END_NAMESPACE
