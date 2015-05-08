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

#ifndef V8DEBUGADAPTER_H
#define V8DEBUGADAPTER_H

#include <QObject>
#include <QVariant>
#include <QSet>

#include "../neoscripttools_global.h"

struct ObjectSnapshot; 
struct ValueIterator;

class NEOSCRIPTTOOLS_EXPORT CV8DebugAdapter
{
public:
	CV8DebugAdapter();
	~CV8DebugAdapter();

	void onCommand(int id, const QVariantMap& Command);
	void attach();
	void detach();

	virtual void onEvent(const QVariantMap& Event) = 0;
	virtual void onResult(int id, const QVariantMap& Result) = 0;

	void onMessage(const QByteArray& arrJson);

	void onTrace(const QString& Message);

	bool isRunning() {return running;}

protected:
	virtual void onRequest(const QByteArray& arrJson) = 0;
	void onReply(int id, const QVariantMap& Result);
	//void onScript(const QVariantMap& Script);

	QVariantMap scriptDelta();

	virtual void Evaluate(const QString& Program, int Frame = -1);
	virtual void onEvaluate(bool nested, const QString& Text, const QVariantMap& Value = QVariantMap());

	virtual bool filterEvent(bool exception) {return false;}

	void clear();

	bool					attached;

	//QMap<qint64, time_t>	scripts;
    QSet<qint64>			checkpointScripts;
    QSet<qint64>			previousCheckpointScripts;

	enum EStep
	{
		eNone,
		eIn,
		eNext,
		eOut,
		eLocation,
	}						step;
	QMap<int, QVariantMap>	breakpoints;
	QMap<int, QString>		pending;
	bool					exception;
	bool					running;
	//int						ContextID;
	//QList<int>				RemovedContextID;

	int						nextScriptObjectSnapshotId;
    QMap<int, ObjectSnapshot*> scriptObjectSnapshots;

	int						nextScriptValueIteratorId;
    QMap<int, ValueIterator*> scriptValueIterators;
};

#endif
