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

#ifndef QSCRIPTDEBUGGERCUSTOMVIEWINTERFACE_H
#define QSCRIPTDEBUGGERCUSTOMVIEWINTERFACE_H

#include <QtCore/qglobal.h>
#if QT_VERSION < 0x050000
#include <QtGui/qwidget.h>
#else
#include <QtWidgets/qwidget.h>
#endif

#include <QtCore/qvariant.h>

#include "../neoscripttools_global.h"

QT_BEGIN_NAMESPACE

class NEOSCRIPTTOOLS_EXPORT QScriptDebuggerCustomViewInterface:
    public QWidget
{
    Q_OBJECT
public:
	QScriptDebuggerCustomViewInterface(QWidget *parent, Qt::WindowFlags flags)
		: QWidget(parent, flags) {}

	virtual qint64 customID() = 0;
	virtual QString text() = 0;

	virtual void setData(const QVariant& var) = 0;

	virtual int find(const QString &exp, int options = 0) = 0;

private:
    Q_DISABLE_COPY(QScriptDebuggerCustomViewInterface)
};

QT_END_NAMESPACE

#endif
