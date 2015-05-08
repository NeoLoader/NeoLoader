#pragma once

#include <QObject>
#include <QVariant>

#include "../../../NeoScriptTools/JSDebugging/JSScriptDebuggerFrontendInterface.h"
#include "../../../Framework/IPC/IPCClient.h"

class QScriptDebugger;
class CKadScriptDebuggerFrontend : public CIPCClient, public CJSScriptDebuggerFrontendInterface
{
    Q_OBJECT
public:
    CKadScriptDebuggerFrontend(QObject *parent = 0);

	void connectTo(const QString& pipe);

	void terminate();
	QVariantMap executeCommand(const QString& Command, const QVariantMap& Request);

signals:
	void				DispatchResponse(const QString& Command, const QVariantMap& Response);

private slots:
	void				SendRequest(const QString& Command, const QVariantMap& Request);

	virtual void		OnResponse(const QString& Command, const QVariant& Parameters, qint64 Number);
	virtual void		OnEvent(const QString& Command, const QVariant& Parameters, qint64 Number);
	//void onConnected();

protected:
	virtual void processCommand(int id, const QVariantMap &command);
	virtual void attach();
	virtual void detach();
};

