#pragma once

#include <QObject>
#include <QVariant>

#include "../../../Framework/IPC/IPCServer.h"
#include "../../../NeoScriptTools/V8Debugging/V8DebugAdapter.h"
#include "../../Common/Pointer.h"
#include "../../Common/Variant.h"
class CKadScript;
class CKadTask;
class CObject;

class CKadScriptDebuggerBackend;
class CKadScriptDebuggerThread: public QThread
{
    Q_OBJECT
public:
    CKadScriptDebuggerThread(const QString& Pipe, QObject *parent = 0);
	~CKadScriptDebuggerThread();

	const QString& Pipe() {return m_Pipe;}

	QVariantMap	ProcessRequest(const QString& Command, const QVariantMap& Request);

private slots:
	void	Evaluate(QString Program);

protected:
	friend class CKadScriptDebuggerBackend;
	void run();

	pair<CKadScript*, CObject*> ResolveScope(const QVariantMap& Scope);

	void timerEvent(QTimerEvent *e);

	int	m_TimerId;

	const QString m_Pipe;

	CPointer<CKadScript> m_pScope;
	CPointer<CObject> m_pAuxScope;
	QMutex m_ScopesMutex;
	QMultiMap<void*, void*> m_Scopes;
	struct SCommand
	{
		QString Command;
		QVariantMap Request;
		int UID;
	};
	QMutex m_QueueMutex;
	QList<SCommand> m_CommandQueue;
	CKadScriptDebuggerBackend* m_pBackend;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

class CKadScriptDebuggerBackend: public CIPCServer, protected CV8DebugAdapter
{
    Q_OBJECT
public:
    CKadScriptDebuggerBackend(QObject *parent = 0);
	~CKadScriptDebuggerBackend();

	void listen(const QString& Pipe);

private slots:
	virtual void		OnRequest(const QString& Command, const QVariant& Parameters, qint64 Number);

	void onAsyncMessage(QByteArray arrJson);

	void onAsyncTrace(const QString& Trace);

	void PushReply(QString Command, QVariant Parameters, int UID);
	void OnEvaluate(QString Text);

protected:
	virtual void Evaluate(const QString& Program, int Frame = -1);

	virtual bool filterEvent(bool exception);

	virtual void onRequest(const QByteArray& arrJson);

	virtual void onEvent(const QVariantMap& Event);
	virtual void onResult(int id, const QVariantMap& Result);

	void timerEvent(QTimerEvent *e);

	QMap<int, QPair<QPointer<CIPCSocket>, qint64> > m_CommandMap;
	int m_Counter;
	int	m_TimerId;
};

