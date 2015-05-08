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

#ifndef CJSSCRIPTDEBUGGERBACKEND_H
#define CJSSCRIPTDEBUGGERBACKEND_H

#include <QObject>
#include <QVariant>

#include "../neoscripttools_global.h"

class QScriptEngine;
class CJSScriptDebuggerBackendPrivate;
class NEOSCRIPTTOOLS_EXPORT CJSScriptDebuggerBackend : public QObject
{
    Q_OBJECT
public:
    CJSScriptDebuggerBackend(QObject *parent = 0);
    ~CJSScriptDebuggerBackend();

	QVariant handleRequest(const QVariant& var);

	void attachTo(QScriptEngine* engine);
    void detach();
	bool isEvaluating();
	void resume();

signals:
	void sendResponse(const QVariant& var);

public slots:
	void processRequest(const QVariant& var);

protected:
	virtual QVariant handleCustom(const QVariant& var) {return QVariant();}
	virtual void requestStart() {}

private slots:
	void onPendingEvaluate();

private:

	Q_DECLARE_PRIVATE(CJSScriptDebuggerBackend)
	Q_DISABLE_COPY(CJSScriptDebuggerBackend)
};

#endif
