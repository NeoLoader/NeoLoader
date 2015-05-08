#include "GlobalHeader.h"
#include "KadScriptDebuggerFrontend.h"

CKadScriptDebuggerFrontend::CKadScriptDebuggerFrontend(QObject *parent)
 : CIPCClient(parent)
{
}

void CKadScriptDebuggerFrontend::connectTo(const QString& Pipe)
{
	ConnectLocal(Pipe);

	attach();
}

void CKadScriptDebuggerFrontend::processCommand(int id, const QVariantMap &command)
{
	QVariantMap Parameters;
	Parameters["ID"] = id;
	Parameters["Command"] = command;

	CIPCClient::SendRequest("DebuggerCommand", Parameters);
}

void CKadScriptDebuggerFrontend::OnResponse(const QString& Command, const QVariant& Parameters, qint64 Number)
{
	QVariantMap Response = Parameters.toMap();
	if(Command == "DebuggerCommand")
		notifyCommandFinished(Response["ID"].toInt(), Response["Result"].toMap());
	else 
		emit DispatchResponse(Command, Response);
}

void CKadScriptDebuggerFrontend::OnEvent(const QString& Command, const QVariant& Parameters, qint64 Number)
{
	QVariantMap Response = Parameters.toMap();
	if(Command == "DebuggerEvent")
		notifyEvent(Response["Event"].toMap());
}

void CKadScriptDebuggerFrontend::SendRequest(const QString& Command, const QVariantMap& Request)
{
	CIPCClient::SendRequest(Command, Request);
}

void CKadScriptDebuggerFrontend::attach()
{
	QVariantMap Parameters;
	QVariant Result; // wait for result
	CIPCClient::SendRequest("AttachDebugger", Parameters, Result);
}

void CKadScriptDebuggerFrontend::detach()
{
	QVariantMap Parameters;
	QVariant Result; // wait for result
	CIPCClient::SendRequest("DetachDebugger", Parameters, Result);
}

void CKadScriptDebuggerFrontend::terminate()
{
	QVariantMap Parameters;
	CIPCClient::SendRequest("TerminateExecution", Parameters);
}

QVariantMap CKadScriptDebuggerFrontend::executeCommand(const QString& Command, const QVariantMap& Request)
{
	QVariant Result; // wait for result
	CIPCClient::SendRequest(Command, Request, Result);
	return Result.toMap();
}