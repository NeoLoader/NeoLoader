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

#include "V8DebugAdapter.h"
#include <QDebug>
#include <QDateTime>
#include <QStringList>

#include "../../qjson/src/parser.h"
#include "../../qjson/src/serializer.h"

struct ValueProperty
{
	ValueProperty() {}
	ValueProperty(const QString& name, const QVariantMap& value)
		: Name(name), Value(value) {}

	const QString& name() const			{return Name;}
	const QVariantMap& value() const	{return Value;}

	static QVariantMap toVariant(const QVariantMap& Value)
	{
		QVariantMap value;
		QString type = Value["type"].toString();
		if(type == "undefined")
			value["type"] = "UndefinedValue";
		else if(type == "null")
			value["type"] = "NullValue";
		//else if(type == "function") ;
		//else if(type == "frame") ;
		//else if(type == "script") ;
		else 
		{
			if(type == "boolean")
				value["type"] = "BooleanValue";
			else if(type == "number")
				value["type"] = "NumberValue";
			else if(type == "string")
				value["type"] = "StringValue";
			else if(type == "object")
				value["type"] = "ObjectValue";
			else 
				value["type"] = "NoValue";
			value["value"] = Value["handle"];
		}
		return value;
	}

	QVariantMap toVariant() const
	{
		QVariantMap out;
		out["name"] = name();
		out["value"] = toVariant(Value);
		out["valueAsString"] = Value["text"];
		out["flags"] = 0x00000001; //QScriptValue::ReadOnly; // see "SetScriptValueProperty" comment
		return out;
	}

	QString Name;
	QVariantMap Value;
};

struct ObjectSnapshot
{
    struct Delta {
        QStringList removedProperties;
        QList<ValueProperty> changedProperties;
        QList<ValueProperty> addedProperties;
    };

	ObjectSnapshot() : pending(0), handle(0) {}
	QList<ValueProperty>	properties;
	int						pending;
	int						handle;
	int						index;
};

struct ValueIterator: ObjectSnapshot
{
	ValueIterator() : index(0) {}
	int						index;
};

CV8DebugAdapter::CV8DebugAdapter()
{
	attached = false;

	step = eNone;

	exception = false;
	running = true;
	//ContextID = 0;

	nextScriptObjectSnapshotId = 0;
	nextScriptValueIteratorId = 0;
}

CV8DebugAdapter::~CV8DebugAdapter()
{
	clear();
}

void CV8DebugAdapter::onCommand(int id, const QVariantMap& Command)
{
	Q_ASSERT(attached || id == 0);

	QString typeStr = Command["type"].toString();
	QVariantMap Attributes = Command["attributes"].toMap();
	//QByteArray Temp = QJson::Serializer().serialize(Attributes);
	//qDebug() << "cmd: " << typeStr;

	pending.insert(id, typeStr);

	QJson::Serializer json;
	QVariantMap Response;
	if(typeStr == "Interrupt")
	{
		QVariantMap Variant;
		Variant["seq"] = id;
		Variant["type"] = "request";
		Variant["command"] = "suspend";

		QByteArray arrJson = json.serialize(Variant);
		onRequest(arrJson);
	}
	else if(typeStr == "Continue" || typeStr == "StepInto" || typeStr == "StepOver" || typeStr == "StepOut" || typeStr == "Resume")
	{
		// Note: Resume is an internal command mostly equivalent to Continue, but not exposed in the gui, 
		//	it is used to resume after breaks that are not intended to start interactive debugging.
		//	The debugger assumes that every event breaks the exectiotion, which is here not the case here,
		//	custome events dont stop the engine, those we ignore Resume commands if running is true
		if(!(typeStr == "Resume" && running))
		{
			// we have to preemptivly set the flag 
			//	or else we would have a short window in which we could mistake the engine for being halted while it actually runs.
			running = true; 

			QVariantMap Variant;
			Variant["seq"] = id;
			Variant["type"] = "request";
			Variant["command"] = "continue";

			Q_ASSERT(step == eNone || typeStr == "Resume");
			if(typeStr == "StepInto" || typeStr == "StepOver" || typeStr == "StepOut")
			{
				QVariantMap Arguments;
				if(typeStr == "StepInto")
				{
					step = eIn;
					Arguments["stepaction"] = "in";
				}
				else if(typeStr == "StepOver")
				{
					step = eNext;
					Arguments["stepaction"] = "next";
				}
				else if(typeStr == "StepOut")
				{
					step = eOut;
					Arguments["stepaction"] = "out";
				}
				//Arguments["stepcount"] = 1; // default is 1
				Variant["arguments"] = Arguments;
			}

			QByteArray arrJson = json.serialize(Variant);
			onRequest(arrJson);
		}

		Response["async"] = true;
	}
	else if(typeStr == "RunToLocation" || typeStr == "RunToLocationByID")
	{
		step = eLocation;

		{
		QVariantMap AuxCommand;
		AuxCommand["type"] = "SetBreakpoint";
		QVariantMap out;
		if(typeStr == "RunToLocationByID")
			out["scriptId"] = Attributes["scriptId"];
		else
			out["fileName"] = Attributes["fileName"];
		out["lineNumber"] = Attributes["lineNumber"];
		out["enabled"] = true;
		out["singleShot"] = true;
		QVariantMap AuxAttributes;
		AuxAttributes["breakpointData"] = out;
		AuxCommand["attributes"] = AuxAttributes;
		onCommand(0, AuxCommand);
		}

		{
		QVariantMap AuxCommand;
		AuxCommand["type"] = "Resume";
		onCommand(0, AuxCommand);
		}

		Response["async"] = true;
	}
	// {"seq":157,"type":"request","command":"restartframe","arguments":{"frame":0}} resets a frame
	else if(typeStr == "ForceReturn") // Used only in console commands
	{
		// Note: V8 currently does not support terminating frame execution early
		qDebug() << "Forced Return is Not Supported by V8";
	}

	else if(typeStr == "SetBreakpoint")
	{
		QVariantMap in = Attributes["breakpointData"].toMap();

		if(id != 0) // Note: breakpoint sets issued internaly are not listed
			breakpoints.insert(-id, in);

		QVariantMap Variant;
		Variant["seq"] = id;
		Variant["type"] = "request";
		Variant["command"] = "setbreakpoint";
		QVariantMap Arguments;
		if(in["scriptId"].toInt() != 0)
		{
			Arguments["type"] = "scriptId";
			Arguments["target"] = in["scriptId"];
		}
		else
		{
			Arguments["type"] = "script";
			Arguments["target"] = in["fileName"];
		}
		Arguments["line"] = in["lineNumber"];
		//Arguments["column"]
		Arguments["enabled"] = in["enabled"];
		Arguments["ignoreCount"] = in["ignoreCount"];
		Arguments["condition"] = in["condition"];
		Variant["arguments"] = Arguments;

		QByteArray arrJson = json.serialize(Variant);
		onRequest(arrJson);
		return;
	}
	else if(typeStr == "DeleteBreakpoint")
	{
		breakpoints.remove(Attributes["breakpointId"].toInt());

		QVariantMap Variant;
		Variant["seq"] = id;
		Variant["type"] = "request";
		Variant["command"] = "clearbreakpoint";
		QVariantMap Arguments;
		Arguments["breakpoint"] = Attributes["breakpointId"].toInt();
		Variant["arguments"] = Arguments;

		QByteArray arrJson = json.serialize(Variant);
		onRequest(arrJson);
	}
	else if(typeStr == "DeleteAllBreakpoints")
	{
		foreach(int break_id, breakpoints.keys())
		{
			QVariantMap AuxCommand;
			AuxCommand["type"] = "DeleteBreakpoint";
			QVariantMap AuxAttributes;
			AuxAttributes["breakpointId"] = break_id;
			AuxCommand["attributes"] = AuxAttributes;
			onCommand(0, AuxCommand);
		}
	}
	else if(typeStr == "GetBreakpoints")
	{
		// Note: the response to listbreakpoints does not contain the break condition to have full info we cache the list.
		//			Also we are using unlisted breaks to emulate RunToLocation

		/*
		QVariantMap Variant;
		Variant["seq"] = id;
		Variant["type"] = "request";
		Variant["command"] = "listbreakpoints";
		
		QByteArray arrJson = json.serialize(Variant);
		onRequest(arrJson);
		*/

		QVariantList result;
		for(QMap<int, QVariantMap>::iterator I = breakpoints.begin(); I != breakpoints.end(); ++I)
		{
			if(I.key() < 0)
				continue;

			QVariantMap out = I.value();
			out["id"] = I.key();
			result.append(out);
		}
		Response["result"] = result;
	}
	else if(typeStr == "GetBreakpointData")
	{
		QMap<int, QVariantMap>::iterator I = breakpoints.find(Attributes["breakpointId"].toInt());
		if(I != breakpoints.end())
			Response["result"] = I.value();
		else
			Response["error"] = "InvalidBreakpointID";
	}
	else if(typeStr == "SetBreakpointData")
	{
		QMap<int, QVariantMap>::iterator I = breakpoints.find(Attributes["breakpointId"].toInt());
		if(I != breakpoints.end())
		{
			QVariantMap in = Attributes["breakpointData"].toMap();
			Q_ASSERT(id > 0);
			breakpoints.insert(-id, in);

			QVariantMap Variant;
			Variant["seq"] = id;
			Variant["type"] = "request";
			Variant["command"] = "changebreakpoint";
			QVariantMap Arguments;
			Arguments["breakpoint"] = Attributes["breakpointId"].toInt();
			Arguments["enabled"] = in["enabled"];
			Arguments["ignoreCount"] = in["ignoreCount"];
			Arguments["condition"] = in["condition"];
			Variant["arguments"] = Arguments;

			QByteArray arrJson = json.serialize(Variant);
			onRequest(arrJson);
		}
		else
			Response["error"] = "InvalidBreakpointID";
	}

	else if(typeStr == "GetScriptData" || typeStr == "GetScripts" || typeStr == "ResolveScript" || typeStr == "ScriptsCheckpoint") // GetScripts and ResolveScript are used only in console commands
	{
		QVariantMap Variant;
		Variant["seq"] = id;
		Variant["type"] = "request";
		Variant["command"] = "scripts";

		QVariantMap Arguments;
		if(typeStr == "GetScriptData") // get only one script
		{
			QVariantList Scripts;
			Scripts.append(Attributes["scriptId"]);
			Arguments["ids"] = Scripts;
			Arguments["includeSource"] = true;
		}
		else if(typeStr == "ResolveScript") // get script ID of the script with this file name
		{
			Arguments["filter"] = Attributes["fileName"];
		}
		// else // get all scripts
		Variant["arguments"] = Arguments;

		QByteArray arrJson = json.serialize(Variant);
		onRequest(arrJson);
		return;
	}
	else if(typeStr == "GetScriptsDelta")
	{	
		Response["result"] = scriptDelta();
	}

	else if(typeStr == "GetBacktrace" || typeStr == "GetContextCount") // used only in console commands
	{
		QVariantMap Variant;
		Variant["seq"] = id;
		Variant["type"] = "request";
		Variant["command"] = "backtrace";

		QVariantMap Arguments;
		Arguments["fromFrame"] = 0;
		Arguments["toFrame"] = 127; // lets get as much as we can 
		Variant["arguments"] = Arguments;

		QByteArray arrJson = json.serialize(Variant);
		onRequest(arrJson);
		return;
	}

	else if(typeStr == "GetContextInfo")
	{
		QVariantMap Variant;
		Variant["seq"] = id;
		Variant["type"] = "request";
		Variant["command"] = "frame";

		QVariantMap Arguments;
		Arguments["number"] = Attributes["contextIndex"];
		Variant["arguments"] = Arguments;

		QByteArray arrJson = json.serialize(Variant);
		onRequest(arrJson);
		return;
	}
	else if(typeStr == "GetContextState")
	{
		Response["result"] = exception ? 1 : 0;
	}
	else if(typeStr == "GetContextID") 
	{
		//Response["result"] = ContextID;
		Response["result"] = 0;
	}
	else if(typeStr == "ContextsCheckpoint")
	{
		QVariantList removed;
		//for (int i = 0; i < RemovedContextID.size(); ++i)
		//	removed.append(RemovedContextID.at(i));
		//RemovedContextID.clear();

		QVariantMap Result;
		Result["added"] = QVariantList(); // not used by the debugger GUI
		Result["removed"] = removed;
        Response["result"] = Result;
	}
	else if(typeStr == "GetThisObject")
	{
		QVariantMap Variant;
		Variant["seq"] = id;
		Variant["type"] = "request";
		Variant["command"] = "frame";

		QVariantMap Arguments;
		Arguments["number"] = Attributes["contextIndex"];
		Variant["arguments"] = Arguments;

		QByteArray arrJson = json.serialize(Variant);
		onRequest(arrJson);
		return;
	}
	else if(typeStr == "GetScopeChain" || typeStr == "GetActivationObject") // GetActivationObject is used only in console commands
	{
		QVariantMap Variant;
		Variant["seq"] = id;
		Variant["type"] = "request";

		QVariantMap Arguments;
		if(typeStr == "GetScopeChain")
			Variant["command"] = "scopes";
		else if(typeStr == "GetActivationObject")
		{
			Variant["command"] = "scope";
			Arguments["number"] = 0;
		}
		Arguments["frameNumber"] = Attributes["contextIndex"];
		Variant["arguments"] = Arguments;

		QByteArray arrJson = json.serialize(Variant);
		onRequest(arrJson);
		return;
	}

	else if(typeStr == "GetPropertyExpressionValue")
		; // irrelevant used only for tooltips
	else if(typeStr == "GetCompletions")
		; // irrelevant used only for autocomplete

	else if(typeStr == "NewScriptObjectSnapshot")
	{
		int snap_id = nextScriptObjectSnapshotId;
		++nextScriptObjectSnapshotId;
		scriptObjectSnapshots.insert(snap_id, new ObjectSnapshot());
		Response["result"] = snap_id;
	}
	else if(typeStr == "ScriptObjectSnapshotCapture" || typeStr == "ScriptValueToString" || typeStr == "NewScriptValueIterator") // ScriptValueToString and NewScriptValueIterator are used only in console commands
	{		
		QVariantMap value = Attributes["scriptValue"].toMap();
		Q_ASSERT(value["type"] == "ObjectValue");
		int Handle = value["value"].toInt();

		if(typeStr == "ScriptObjectSnapshotCapture")
		{
			int snap_id = Attributes["snapshotId"].toInt();
			ObjectSnapshot *snap = scriptObjectSnapshots.value(snap_id);
			Q_ASSERT(snap != 0);

			snap->pending = id;
			snap->handle = Handle;
		}
		else if(typeStr == "NewScriptValueIterator")
		{
			int iter_id = nextScriptValueIteratorId;
			++nextScriptValueIteratorId;
			ValueIterator *iter = new ValueIterator();
			scriptValueIterators.insert(id, iter);

			iter->pending = id;
			iter->handle = Handle;
		}

		QVariantMap Variant;
		Variant["seq"] = id;
		Variant["type"] = "request";
		Variant["command"] = "lookup";

		QVariantMap Arguments;
		QVariantList Handles;
		Handles.append(Handle);
		Arguments["handles"] = Handles;
		Variant["arguments"] = Arguments;

		QByteArray arrJson = json.serialize(Variant);
		onRequest(arrJson);
		return;
	}
	else if(typeStr == "DeleteScriptObjectSnapshot")
	{
		int snap_id = Attributes["snapshotId"].toInt();
		delete scriptObjectSnapshots.take(snap_id);
	}

	else if(typeStr == "GetPropertiesByIterator") // used only in console commands
	{
		int iter_id = Attributes["iteratorId"].toInt();
		ValueIterator *iter = scriptValueIterators.value(iter_id);
		Q_ASSERT(iter != 0);

		QVariantList Result;
		for(;iter->index < iter->properties.size(); iter->index++)
			Result.append(iter->properties.at(iter->index).toVariant());
		Response["result"] = Result;
	}
	else if(typeStr == "DeleteScriptValueIterator") // used only in console commands
	{
		int iter_id = Attributes["iteratorId"].toInt();
		delete scriptValueIterators.take(iter_id);
	}

	else if(typeStr == "Evaluate")
	{
		Evaluate(Attributes["program"].toString(), Attributes["contextIndex"].toInt());

		Response["async"] = true;
	}

	else if(typeStr == "SetScriptValueProperty")
	{
		// Note: V8 does not seam to support value modification, eval must be used
		//Attributes["name"].toString();
		//Attributes["scriptValue"].toMap(); // id of value to be modifyed
		//Attributes["subordinateScriptValue"].toMap(); // value to be set
		Q_ASSERT(0); // we should have end up here, as we declade all values as read only
	}

	else if(typeStr == "ClearExceptions")
	{
		// Note: V8 currently does not have/needs such a feature
		//	also the debugger GUI is not using it
	}

	else // unknown commands
	{
		//Q_ASSERT(0);
	}

	//Response["type"] = typeStr;
	if(id)
		onReply(id, Response);
}

QVariantMap CV8DebugAdapter::scriptDelta()
{
	QSet<qint64> prevSet = previousCheckpointScripts;
	QSet<qint64> currSet = checkpointScripts;
	QList<qint64> addedScriptIds = (currSet - prevSet).toList();
	QList<qint64> removedScriptIds = (prevSet - currSet).toList();

	QVariantList added;
	for (int i = 0; i < addedScriptIds.size(); ++i)
		added.append(addedScriptIds.at(i));
	QVariantList removed;
	for (int i = 0; i < removedScriptIds.size(); ++i)
		removed.append(removedScriptIds.at(i));
	QVariantMap result;
	result["added"] = added;
	result["removed"] = removed;

	return result;
}

void CV8DebugAdapter::Evaluate(const QString& Program, int Frame)
{
	// we have to preemptivly set the flag 
	running = true; 

	QVariantMap Variant;
	Variant["seq"] = 0;
	Variant["type"] = "request";
	Variant["command"] = "evaluate";

	QVariantMap Arguments;
	Arguments["expression"] = Program;
	if(Frame == -1)
		Arguments["global"] = Frame;
	else
		Arguments["frame"] = Frame;
	//Arguments["disable_break"] = true;
	Variant["arguments"] = Arguments;

	QJson::Serializer json;
	QByteArray arrJson = json.serialize(Variant);
	onRequest(arrJson);
}

void CV8DebugAdapter::onEvaluate(bool nested, const QString& Text, const QVariantMap& Value)
{
	QVariantMap Response;
	Response["type"] = "InlineEvalFinished";

	QVariantMap Attributes;
	Attributes["value"] = Value;
	Attributes["isNestedEvaluate"] = nested;
	Attributes["message"] = Text;
	Response["attributes"] = Attributes;

	onEvent(Response);
}

void CV8DebugAdapter::onMessage(const QByteArray& arrJson)
{
	QJson::Parser json;
	bool ok;
	QVariantMap Variant = json.parse(arrJson, &ok).toMap();

	if(Variant["type"] == "event")
	{
		if(Variant.contains("message"))
			qDebug() << QString("V8 Debug Message: %1").arg(Variant["message"].toString());
		
		if(Variant["event"] == "exception")
			exception = true;
		else if(Variant["event"] == "break")
			exception = false;
		else // event is irrelevant
			return;

		running = false; // every break or exception holds the execution

		// If we are not attached, or we dont want to see the event a.k.a. we filter it,
		//	resumt automatically the script execution
		if(!attached || filterEvent(exception))
		{
			QVariantMap AuxCommand;
			AuxCommand["type"] = "Resume";
			onCommand(0, AuxCommand);
			return;
		}

		QVariantMap Body = Variant["body"].toMap();
		QVariantMap Script = Body["script"].toMap();

		//onScript(Script);

		// Note: V8 does not support poeristent contexts, all handles are only valid for one debugger event
		//			So on each event we must drop the entire old locals
		//RemovedContextID.append(ContextID);
		//ContextID++;
		//foreach(ObjectSnapshot* snap, scriptObjectSnapshots)
		//	delete snap;
		//scriptObjectSnapshots.clear();

		QVariantMap Response;
		if(exception)
		{
			step = eNone;
			QVariantMap Exception = Body["exception"].toMap();

			Response["type"] = "Exception";
			QVariantMap Attributes;
			Attributes["message"] = Exception["text"];
			Attributes["value"] = Exception["value"];
			Attributes["scriptId"] = Script["id"];
			Attributes["fileName"] = Script["name"];
			Attributes["lineNumber"] = Body["sourceLine"];
			Attributes["columnNumber"] = Body["sourceColumn"];
			Attributes["hasExceptionHandler"] = !Body["uncaught"].toBool();
			Response["attributes"] = Attributes;
		}
		else
		{
			QVariantList Breaks = Body["breakpoints"].toList();

			int break_id = -1;
			foreach(const QVariant& var, Breaks)
			{
				int cur_id = var.toInt();
				if(!breakpoints.contains(cur_id)) // auto clear unlisted breakpoints
				{
					QVariantMap AuxCommand;
					AuxCommand["type"] = "DeleteBreakpoint";
					QVariantMap AuxAttributes;
					AuxAttributes["breakpointId"] = cur_id;
					AuxCommand["attributes"] = AuxAttributes;
					onCommand(0, AuxCommand);
				}
				else if (break_id == -1)
					break_id = cur_id;
			}

			//bool bDebugger = (Body["sourceLineText"].toString().mid(Body["sourceColumn"].toInt(), 9) == "debugger;");

			QVariantMap Attributes;
			if(break_id != -1)
			{
				Response["type"] = "Breakpoint";
				Attributes["breakpointId"] = break_id;
			}
			else if(step != eNone)
			{
				if(step == eLocation)
				{
					if(Breaks.isEmpty()) // if we ware going to a location and broken rearly, it must have been an "debugger;"
						Response["type"] = "DebuggerInvocationRequest";
					else
						Response["type"] = "LocationReached";
				}
				else
					Response["type"] = "SteppingFinished";
				step = eNone;
			}
			else //if(bDebugger)
				Response["type"] = "DebuggerInvocationRequest";
			Attributes["scriptId"] = Script["id"];
			Attributes["fileName"] = Script["name"];
			Attributes["lineNumber"] = Body["sourceLine"];
			Attributes["columnNumber"] = Body["sourceColumn"];
			Response["attributes"] = Attributes;
		}
		onEvent(Response);
	}
	else if(Variant["type"] == "response")
	{
		if(Variant.contains("message"))
			qDebug() << QString("V8 Debug Message: %1").arg(Variant["message"].toString());

		if(Variant.contains("running"))
			running = Variant["running"].toBool();

		QString typeStr = pending.value(Variant["request_seq"].toInt());
		QString cmdStr = Variant["command"].toString();
		if(cmdStr.isEmpty())
		{
			if(Variant["request_seq"].toInt() != 0)
			{
				QVariantMap Response;
				Response["error"] = "UserError";
				onReply(Variant["request_seq"].toInt(), Response);
			}
		}
		else if(cmdStr == "suspend")
		{
			exception = false;

			QVariantMap Response;
			Response["type"] = "Interrupted";

			QVariantMap Attributes;
			Attributes["scriptId"] = 0;
			Attributes["fileName"] = "";
			Attributes["lineNumber"] = 0;
			Attributes["columnNumber"] = 0;
			Response["attributes"] = Attributes;

			onEvent(Response);
		}
		else if(cmdStr == "setbreakpoint")
		{
			QVariantMap Body = Variant["body"].toMap();
			
			if(breakpoints.contains(-Variant["request_seq"].toInt()))
			{
				QVariantMap data = breakpoints.take(-Variant["request_seq"].toInt());
				breakpoints.insert(Body["breakpoint"].toInt(), data);

				QVariantMap Response;
				Response["result"] = Body["breakpoint"];

				onReply(Variant["request_seq"].toInt(), Response);
			}
		}
		else if(cmdStr == "scripts")
		{
			QVariantList Bodys = Variant["body"].toList();

			QVariantMap Response;

			if(typeStr == "GetScriptData")
			{
				if(Bodys.size() > 0)
				{
					Q_ASSERT(Bodys.size() == 1);

					QVariantMap Script = Bodys.first().toMap();

					QVariantMap Result;
					Result["contents"] = Script["source"];
					Result["fileName"] = Script["name"];
					Result["baseLineNumber"] = Script["lineOffset"];
					//Result["timeStamp"] = (quint64)scripts.value(Script["id"].toLongLong());

					Response["result"] = Result;
				}
			}
			else if(typeStr == "GetScripts")
			{
				QVariantList Scripts;
				foreach(const QVariant& var, Bodys)
				{
					QVariantMap Script = var.toMap();

					QVariantMap Result;
					Result["id"] = Script["id"];
					Result["contents"] = Script["source"];
					Result["fileName"] = Script["name"];
					Result["baseLineNumber"] = Script["lineOffset"];

					Scripts.append(Result);
				}
				Response["result"] = Scripts;
			}
			else if(typeStr == "ResolveScript")
			{
				if(Bodys.size() > 0)
				{
					//Q_ASSERT(Bodys.size() == 1);
					// Note: V8 is not strict here "... scripts whose names contain the filter string will be retrieved."

					QVariantMap Script = Bodys.first().toMap();
					//onScript(Script);

					Response["result"] = Script["id"];
				}
				else
					Response["result"] = -1; // not found, invalid name
			}
			else if(typeStr == "ScriptsCheckpoint")
			{
				previousCheckpointScripts = checkpointScripts;
				//checkpointScripts = scripts.keys().toSet();
				checkpointScripts.clear();
				foreach(const QVariant& var, Bodys)
				{
					QVariantMap Script = var.toMap();
					checkpointScripts.insert(Script["id"].toLongLong());
				}

				Response["result"] = scriptDelta();
			}

			onReply(Variant["request_seq"].toInt(), Response);
		}

		else if(cmdStr == "backtrace")
		{
			QVariantMap Body = Variant["body"].toMap();

			QVariantMap Response;

			if(typeStr == "GetBacktrace")
			{
				QVariantList Frames = Body["frames"].toList();
				QMap<int, QVariantMap> Refs;
				foreach(const QVariant& vRef, Variant["refs"].toList())
				{
					QVariantMap Ref = vRef.toMap();
					Refs.insert(Ref["handle"].toInt(), Ref);
				}

				QStringList Backtrace;
				foreach(const QVariant& var, Frames)
				{
					QVariantMap Frame = var.toMap();
					QVariantMap Script = Refs[Frame["script"].toMap()["ref"].toInt()];
					QVariantMap Function = Refs[Frame["func"].toMap()["ref"].toInt()];
					QString Name = Function["name"].toString();

					Backtrace.append(QString("%1() at %2:%3").arg(Name.isEmpty() ? "<anonymous>" : Name).arg(Script["name"].toString()).arg(Frame["line"].toInt()));
				}
				Response["result"] = Backtrace;
			}
			else if(typeStr == "GetContextCount")
				Response["result"] = Body["totalFrames"];

			onReply(Variant["request_seq"].toInt(), Response);
		}

		else if(cmdStr == "frame")
		{
			QVariantMap Response;

			if(Variant["success"] == false)
			{
				Response["error"] = "InvalidContextIndex";
			}
			else if(typeStr == "GetContextInfo")
			{
				QVariantMap Body = Variant["body"].toMap();
				QMap<int, QVariantMap> Refs;
				foreach(const QVariant& vRef, Variant["refs"].toList())
				{
					QVariantMap Ref = vRef.toMap();
					Refs.insert(Ref["handle"].toInt(), Ref);
				}
				QVariantMap Script = Refs[Body["script"].toMap()["ref"].toInt()];
				QVariantMap Function = Refs[Body["func"].toMap()["ref"].toInt()];

				//onScript(Script);

				QVariantMap Result;
				Result["scriptId"] = Script["id"];
				Result["lineNumber"] = Body["line"];
				Result["columnNumber"] = Body["column"];

				Result["fileName"] = Script["name"];
				Result["functionName"] = Function["name"];
				Result["functionParameterNames"] = QStringList();
				
				Response["result"] = Result;
			}
			else if(typeStr == "GetThisObject")
			{
				QVariantMap Body = Variant["body"].toMap();
				QVariantMap Receiver = Body["receiver"].toMap();

				QVariantMap Value;
				Value["type"] = "ObjectValue";
				Value["value"] = Receiver["ref"].toInt();;
				Response["result"] = Value;
			
				onReply(Variant["request_seq"].toInt(), Response);
			}

			onReply(Variant["request_seq"].toInt(), Response);
		}
		else if(cmdStr == "evaluate")
		{
			QVariantMap Body = Variant["body"].toMap();

			onEvaluate(true, Body["text"].toString(), ValueProperty::toVariant(Body));
		}
		else if(cmdStr == "scopes")
		{
			QVariantMap Body = Variant["body"].toMap();
			QVariantList Scopes = Body["scopes"].toList();

			QVariantMap Response;

			QVariantList Result;
			foreach(const QVariant& vScope, Scopes)
			{
				QVariantMap Object = vScope.toMap()["object"].toMap();

				QVariantMap Value;
				Value["type"] = "ObjectValue";
				Value["value"] = Object["ref"].toInt();

				Result.append(Value);
			}
			Response["result"] = Result;
			
			onReply(Variant["request_seq"].toInt(), Response);			
		}
		else if(cmdStr == "scope")
		{
			QVariantMap Body = Variant["body"].toMap();
			QVariantMap Object = Body["object"].toMap();

			QVariantMap Value;
			Value["type"] = "ObjectValue";
			Value["value"] = Object["ref"].toInt();

			QVariantMap Response;
			Response["result"] = Value;
			onReply(Variant["request_seq"].toInt(), Response);			
		}
		else if(cmdStr == "lookup")
		{
			QVariantMap Response;

			if(typeStr == "ScriptObjectSnapshotCapture")
			{
				ObjectSnapshot *snap = NULL;
				foreach(ObjectSnapshot* cur_snap, scriptObjectSnapshots)
				{
					if(cur_snap->pending == Variant["request_seq"].toInt())
					{
						cur_snap->pending = 0;
						snap = cur_snap;
						break;
					}
				}
				Q_ASSERT(snap != 0);

				QVariantMap Bodys = Variant["body"].toMap();
				QVariantMap Body = Bodys[QString::number(snap->handle)].toMap();
				QVariantList Properties = Body["properties"].toList();
				QMap<int, QVariantMap> Refs;
				foreach(const QVariant& vRef, Variant["refs"].toList())
				{
					QVariantMap Ref = vRef.toMap();
					Refs.insert(Ref["handle"].toInt(), Ref);
				}

				ObjectSnapshot::Delta delta;

				{
					QMap<QString, ValueProperty> currProps;
					QHash<QString, int> propertyNameToIndex;
					int i = 0;
					foreach(const QVariant& vProperty, Properties)
					{
						QVariantMap Property = vProperty.toMap();
						QString name = Property["name"].toString();
						currProps.insert(name, ValueProperty(name, Refs[Property["ref"].toInt()]));
						propertyNameToIndex.insert(name, i);
						i++;
					}

					QSet<QString> prevSet;
					for (int i = 0; i < snap->properties.size(); ++i)
						prevSet.insert(snap->properties.at(i).name());
					QSet<QString> currSet = currProps.keys().toSet();
					QSet<QString> removedProperties = prevSet - currSet;
					QSet<QString> addedProperties = currSet - prevSet;
					QSet<QString> maybeChangedProperties = currSet & prevSet;

					{
						QMap<int, ValueProperty> am;
						QSet<QString>::const_iterator it;
						for (it = addedProperties.constBegin(); it != addedProperties.constEnd(); ++it) {
							int idx = propertyNameToIndex[*it];
							am[idx] = currProps[*it];
						}
						delta.addedProperties = am.values();
					}

					{
						QSet<QString>::const_iterator it;
						for (it = maybeChangedProperties.constBegin(); it != maybeChangedProperties.constEnd(); ++it) {
							const ValueProperty &p1 = currProps[*it];
							ValueProperty p2 = ValueProperty();
							for (int i = 0; i < snap->properties.size(); ++i) {
								if (snap->properties.at(i).name() == *it) {
									p2 = snap->properties.at(i);
									break;
								}
							}
    
							if (p1.value() != p2.value()) {
								delta.changedProperties.append(p1);
							}
						}
					}

					delta.removedProperties = removedProperties.toList();

					snap->properties = currProps.values();			
				}

				QVariantList changedProperties;
				for (int i = 0; i < delta.changedProperties.size(); ++i)
					changedProperties.append(delta.changedProperties.at(i).toVariant());
				QVariantList addedProperties;
				for (int j = 0; j < delta.addedProperties.size(); ++j)
					addedProperties.append(delta.addedProperties.at(j).toVariant());
				QVariantMap result;
				result["removedProperties"] = delta.removedProperties;
				result["changedProperties"] = changedProperties;
				result["addedProperties"] = addedProperties;

				Response["result"] = result;
			}
			else if(typeStr == "ScriptValueToString")
			{
				QVariantMap Bodys = Variant["body"].toMap();
				if(!Bodys.isEmpty())
				{
					Q_ASSERT(Bodys.size() == 1);

					QVariantMap Body = Bodys.begin().value().toMap();
					Response["result"] = Body["text"].toString();
				}
			}
			else if(typeStr == "NewScriptValueIterator")
			{
				ValueIterator *iter = NULL;
				foreach(ValueIterator* cur_iter, scriptValueIterators)
				{
					if(cur_iter->pending == Variant["request_seq"].toInt())
					{
						cur_iter->pending = 0;
						iter = cur_iter;
						break;
					}
				}
				Q_ASSERT(iter != 0);

				QVariantMap Bodys = Variant["body"].toMap();
				QVariantMap Body = Bodys[QString::number(iter->handle)].toMap();
				QVariantList Properties = Body["properties"].toList();
				QMap<int, QVariantMap> Refs;
				foreach(const QVariant& vRef, Variant["refs"].toList())
				{
					QVariantMap Ref = vRef.toMap();
					Refs.insert(Ref["handle"].toInt(), Ref);
				}

				foreach(const QVariant& vProperty, Properties)
				{
					QVariantMap Property = vProperty.toMap();
					QString name = Property["name"].toString();
					iter->properties.append(ValueProperty(name, Refs[Property["ref"].toInt()]));
				}

				Response["result"] = scriptValueIterators.key(iter);
			}

			onReply(Variant["request_seq"].toInt(), Response);	
		}
		// else // we are not interested in the response
	}
}

//void CV8DebugAdapter::onScript(const QVariantMap& Script)
//{
//	if(Script["debuggerFrame"].toBool()) // do not list internal frames
//		return;
//	int id = Script["id"].toLongLong();
//	if(!scripts.contains(id))
//		scripts.insert(id, QDateTime::currentDateTime().toTime_t());
//}

void CV8DebugAdapter::onReply(int id, const QVariantMap& Result)
{
	pending.remove(id);
	onResult(id, Result);
}

void CV8DebugAdapter::onTrace(const QString& Message)
{
	// Note: onTrace can not be used wihth the remote debugger, only with an properly extended inprocess debugger
	QVariantMap Response;
	Response["type"] = "Trace";

	QVariantMap Attributes;
	Attributes["message"] = Message;
	Response["attributes"] = Attributes;

	onEvent(Response);
}

void CV8DebugAdapter::attach()
{
	QVariantMap Variant;
	Variant["seq"] = 0;
	Variant["type"] = "request";
	Variant["command"] = "setexceptionbreak";
	QVariantMap Arguments;
	Arguments["type"] = "all"; // "uncaught";
	Arguments["enabled"] = true;
	Variant["arguments"] = Arguments;

	QJson::Serializer json;
	QByteArray arrJson = json.serialize(Variant);
	onRequest(arrJson);

	attached = true;
}

void CV8DebugAdapter::detach()
{
	QVariantMap Variant;
	Variant["seq"] = 0;
	Variant["type"] = "request";
	Variant["command"] = "disconnect";

	QJson::Serializer json;
	QByteArray arrJson = json.serialize(Variant);
	onRequest(arrJson);

	attached = false;
	clear();
}

void CV8DebugAdapter::clear()
{
	//scripts.clear();
	checkpointScripts.clear();
	previousCheckpointScripts.clear();

	step = eNone;
	breakpoints.clear();
	pending.clear();

	foreach(ObjectSnapshot* snap, scriptObjectSnapshots)
		delete snap;

	foreach(ValueIterator* iter, scriptValueIterators)
		delete iter;
}
