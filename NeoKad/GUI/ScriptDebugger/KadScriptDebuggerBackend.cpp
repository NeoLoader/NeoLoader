#include "GlobalHeader.h"
#include "KadScriptDebuggerBackend.h"
#include <QEventLoop>
#include "../../Common/v8Engine/JSEngine.h"
#include "../../Common/v8Engine/JSTerminator.h"
#include "../../Common/v8Engine/JSDebug.h"
#include "../../../v8/include/v8-debug.h"

#include "../../../Framework/Settings.h"
#include "../../Common/FileIO.h"
#include "../../NeoKad.h"
#include "../../Kad/KadHeader.h"
#include "../../Kad/Kademlia.h"
#include "../../Kad/KadConfig.h"
#include "../../Kad/KadID.h"
#include "../../Kad/LookupManager.h"
#include "../../Kad/KadLookup.h"
#include "../../Kad/KadTask.h"
#include "../../Kad/KadOperation.h"
#include "../../Kad/KadEngine/KadEngine.h"
#include "../../Kad/KadEngine/KadDebugging.h"
#include "../../Kad/KadEngine/KadScript.h"
#include "../../Kad/KadEngine/JSKadRoute.h"
#include "../../Kad/KadEngine/KadOperator.h"

bool LogFilter(UINT Flags, const wstring& Trace);

CKadScriptDebuggerThread::CKadScriptDebuggerThread(const QString& Pipe, QObject *parent)
	: QThread(parent), m_Pipe(Pipe)
{
	m_pBackend = NULL;

	// dissable Terminator
	g_JSTerminator->Reset(true);
	delete g_JSTerminator;
	g_JSTerminator = NULL;

	g_LogFilter = LogFilter;

	//if(Pipe.left(1) == ":")
	//{
	//	v8::Debug::EnableAgent("NeoKad", Pipe.mid(1).toUInt(), false);
	//}
	//else
		start();

	m_TimerId = startTimer(100);
}

CKadScriptDebuggerThread::~CKadScriptDebuggerThread()
{
	killTimer(m_TimerId);

	quit();
	wait(3000);

	// re enable Terminator
	//g_JSTerminator = new CJSTerminatorThread();

	g_LogFilter = NULL;
}

void CKadScriptDebuggerThread::run()
{
	m_pBackend = new CKadScriptDebuggerBackend();
	m_pBackend->listen(m_Pipe);

	exec();

	delete m_pBackend;
	m_pBackend = NULL;
}

void CKadScriptDebuggerThread::timerEvent(QTimerEvent *e)
{
    if (e->timerId() != m_TimerId) 
	{
        QThread::timerEvent(e);
		return;
    }

	QMutexLocker Lock(&m_QueueMutex);
	while(!m_CommandQueue.isEmpty())
	{
		SCommand Cmd = m_CommandQueue.takeFirst();
		m_QueueMutex.unlock();
		QVariantMap Response = ProcessRequest(Cmd.Command, Cmd.Request);
		QMetaObject::invokeMethod(m_pBackend, "PushReply", Qt::QueuedConnection, Q_ARG(QString, Cmd.Command), Q_ARG(QVariant, Response), Q_ARG(int, Cmd.UID));
		m_QueueMutex.lock();
	}
}

pair<CKadScript*, CObject*> CKadScriptDebuggerThread::ResolveScope(const QVariantMap& Scope)
{
	CKadEngine* pEngine = theKad->Kad()->GetChild<CKadEngine>();

	CVariant CodeID;
	CodeID.FromQVariant(Scope["CodeID"]);

	const ScriptMap& ScriptMap = pEngine->GetScripts();
	ScriptMap::const_iterator I = ScriptMap.find(CodeID);
	if(I == ScriptMap.end())
		return make_pair((CKadScript*)NULL, (CKadScript*)NULL);
	
	CKadScript* pScript = I->second;
	if(Scope.contains("LookupID"))
	{
		CLookupManager* pLookupManager = theKad->Kad()->GetChild<CLookupManager>();

		CVariant LookupID;
		LookupID.FromQVariant(Scope["LookupID"]);

		CKadLookup* pLookup = pLookupManager->GetLookup(LookupID)->Cast<CKadLookup>();
		if(pLookup == NULL)
			return make_pair((CKadScript*)NULL, (CKadScript*)NULL);
		return make_pair(pScript, pLookup);
	}
	else if(Scope.contains("EntityID"))
	{
		CVariant EntityID;
		EntityID.FromQVariant(Scope["EntityID"]);
		//CVariant TargetID;
		//TargetID.FromQVariant(Scope["TargetID"]);
		
		CKadRoute* pRoute = pScript->GetRoute();
		if(!pRoute || pRoute->GetEntityID() != EntityID)
			return make_pair((CKadScript*)NULL, (CKadScript*)NULL);

		if(Scope.contains("SessionID"))
		{
			CVariant SessionID;
			SessionID.FromQVariant(Scope["SessionID"]);

			CKadRoute::SessionMap& SessionMap = pRoute->GetSessions();
			CKadRoute::SessionMap::iterator J = SessionMap.find(SessionID);
			if(J == SessionMap.end())
				return make_pair((CKadScript*)NULL, (CKadScript*)NULL);
			return make_pair(pScript, J->second);
		}
		else
			return make_pair(pScript, pRoute);
	}
	else
		return make_pair(pScript, pScript);

	return make_pair((CKadScript*)NULL, (CKadScript*)NULL);
}

QVariantMap	CKadScriptDebuggerThread::ProcessRequest(const QString& Command, const QVariantMap& Request)
{
	QVariantMap Response;

	if(Request.contains("UID"))
		Response["UID"] = Request["UID"];

	if(theKad->Kad()->IsDisconnected())
		return Response;

	if(Command == "ListScripts")
	{
		CKadEngine* pEngine = theKad->Kad()->GetChild<CKadEngine>();
		CLookupManager* pLookupManager = theKad->Kad()->GetChild<CLookupManager>();
		
		QMultiMap<QByteArray, CKadOperation*> TaskMap;
		const LookupMap& Lookups = pLookupManager->GetLookups();
		for(LookupMap::const_iterator I = Lookups.begin(); I != Lookups.end();I++)
		{
			CKadOperation* pLookup = I->second->Cast<CKadOperation>();
			if(!pLookup || !pLookup->GetOperator())
				continue;
			if(CKadScript* pScript = pLookup->GetOperator()->GetScript())
				TaskMap.insert(pScript->GetCodeID().ToQVariant().toByteArray(), pLookup);
		}

		QVariantList KadScripts;
		const ScriptMap& ScriptMap = pEngine->GetScripts();
		for(ScriptMap::const_iterator I = ScriptMap.begin(); I != ScriptMap.end(); I++)
		{
			QByteArray CID = I->first.ToQVariant().toByteArray();
			CKadScript* pScript = I->second;

			pScript->KeepAlive();

			QVariantMap Script;
			Script["CodeID"] = CID;
			Script["Name"] = QString::fromStdWString(pScript->GetName());
			Script["Version"] = QString::fromStdWString(CKadScript::GetVersion(pScript->GetVersion()));
			Script["Authenticated"] = pScript->IsAuthenticated();
			
			QVariantList Tasks;
			foreach(CKadOperation* pLookup, TaskMap.values(CID))
			{
				QVariantMap Task;
				Task["LookupID"] = pLookup->GetLookupID().ToQVariant();
				Task["TargetID"] = CVariant(pLookup->GetID()).ToQVariant();

				QString Status;
				if(pLookup->IsStopped())			
					Status = "Finished";
				else if(pLookup->GetStartTime())
				{
					Status = "Running";
					Status += QString(" (%1)").arg(pLookup->GetTimeRemaining()/1000);
				}
				else							
					Status = "Pending";
				Task["Status"] = Status;

				Tasks.append(Task);
			}
			Script["Tasks"] = Tasks;

			QVariantList Routes;
			if(CKadRoute* pRoute = pScript->GetRoute())
			{
				QVariantMap Route;
				Route["TargetID"] = CVariant(pRoute->GetID()).ToQVariant();
				Route["EntityID"] = pRoute->GetEntityID().ToQVariant();

				QVariantList Sessions;
				CKadRoute::SessionMap& SessionMap = pRoute->GetSessions();
				for(CKadRoute::SessionMap::const_iterator I = SessionMap.begin(); I != SessionMap.end(); I++)
				{
					CRouteSession* pSession = I->second;

					QVariantMap Session;
					Session["EntityID"] = I->first.ToQVariant();
					Session["SessionID"] = pSession->GetSessionID().ToQVariant();

					Sessions.append(Session);
							
				}
				Route["Sessions"] = Sessions;

				Routes.append(Route);
			}
			Script["Routes"] = Routes;

			KadScripts.append(Script);
		}
		Response["KadScripts"] = KadScripts;
	}
	else if(Command == "SelectScope")
	{
		pair<CKadScript*, CObject*> ScopePair = ResolveScope(Request);
		m_pScope = CPointer<CKadScript>(ScopePair.first, true);
		m_pAuxScope = CPointer<CObject>(ScopePair.first == ScopePair.second ? NULL : ScopePair.second, true);
	}
	else if(Command == "AddScope")
	{
		pair<CKadScript*, CObject*> ScopePair = ResolveScope(Request);
		if(ScopePair.first)
		{
			QMutexLocker Locker(&m_ScopesMutex);
			if(!m_Scopes.contains(ScopePair.first, ScopePair.second))
				m_Scopes.insert(ScopePair.first, ScopePair.second);
		}
	}
	else if(Command == "RemoveScope")
	{
		pair<CKadScript*, CObject*> ScopePair = ResolveScope(Request);
		if(ScopePair.first)
		{
			QMutexLocker Locker(&m_ScopesMutex);
			m_Scopes.remove(ScopePair.first, ScopePair.second);
		}
	}
	else if(Command == "InstallScript")
	{
		CKadEngine* pEngine = theKad->Kad()->GetChild<CKadEngine>();

		CVariant CodeID;
		CodeID.FromQVariant(Request["CodeID"]);

		string Source = Request["Source"].toString().toStdString();

		CVariant Authentication;
		if(Request.contains("DeveloperKey"))
		{
			CBuffer DevKey(Request["DeveloperKey"].toByteArray());

			CScoped<CPrivateKey> pPrivKey = new CPrivateKey();
			pPrivKey->SetKey(DevKey.GetBuffer(), DevKey.GetSize());

			CScoped<CPublicKey> pPubKey = pPrivKey->PublicKey();

#ifdef _DEBUG
			CVariant TestID((byte*)NULL, KEY_128BIT);
			CKadID::MakeID(pPubKey, TestID.GetData(), TestID.GetSize());
			ASSERT(TestID == CodeID);
#endif

			wstring ScriptPath = pEngine->GetParent<CKademlia>()->Cfg()->GetString("ScriptCachePath");
			if(!ScriptPath.empty())
			{
				wstring FileName = ToHex(CodeID.GetData(), CodeID.GetSize());
				WriteFile(ScriptPath + L"/" + FileName + L".der", 0, DevKey);
			}

			Authentication["PK"] = CVariant(pPubKey->GetKey(), pPubKey->GetSize());
			CBuffer SrcBuff((byte*)Source.data(), Source.size(), true);
			CBuffer Signature;
			pPrivKey->Sign(&SrcBuff,&Signature);
			Authentication["SIG"] = Signature;
			Authentication.Freeze();
		}
		else
			Authentication.FromQVariant(Request["Authentication"]);

		pEngine->Install(CodeID, Source, Authentication);
	}
	else if(Command == "UpdateScript")
	{
		CKadEngine* pEngine = theKad->Kad()->GetChild<CKadEngine>();

		CVariant CodeID;
		CodeID.FromQVariant(Request["CodeID"]);

		string Source = Request["Source"].toString().toStdString();

		CVariant Authentication;
		if(Request.contains("Authentication"))
			Authentication.FromQVariant(Request["Authentication"]);
		else
		{
			wstring ScriptPath = pEngine->GetParent<CKademlia>()->Cfg()->GetString("ScriptCachePath");
			if(!ScriptPath.empty())
			{
				wstring FileName = ToHex(CodeID.GetData(), CodeID.GetSize());

				CBuffer DevKey;
				ReadFile(ScriptPath + L"/" + FileName + L".der", 0, DevKey);
				
				CScoped<CPrivateKey> pPrivKey = new CPrivateKey();
				pPrivKey->SetKey(DevKey.GetBuffer(), DevKey.GetSize());

				CScoped<CPublicKey> pPubKey = pPrivKey->PublicKey();

#ifdef _DEBUG
				CVariant TestID((byte*)NULL, KEY_128BIT);
				CKadID::MakeID(pPubKey, TestID.GetData(), TestID.GetSize());
				ASSERT(TestID == CodeID);
#endif

				Authentication["PK"] = CVariant(pPubKey->GetKey(), pPubKey->GetSize());
				CBuffer SrcBuff((byte*)Source.data(), Source.size(), true);
				CBuffer Signature;
				pPrivKey->Sign(&SrcBuff,&Signature);
				Authentication["SIG"] = Signature;
				Authentication.Freeze();
			}
		}

		pEngine->Install(CodeID, Source, Authentication);
	}
	else if(Command == "GetDeveloperKey")
	{
		CKadEngine* pEngine = theKad->Kad()->GetChild<CKadEngine>();

		CVariant CodeID;
		CodeID.FromQVariant(Request["CodeID"]);

		wstring ScriptPath = pEngine->GetParent<CKademlia>()->Cfg()->GetString("ScriptCachePath");
		if(!ScriptPath.empty())
		{
			wstring FileName = ToHex(CodeID.GetData(), CodeID.GetSize());

			CBuffer DevKey;
			ReadFile(ScriptPath + L"/" + FileName + L".der", 0, DevKey);

			Response["DeveloperKey"] = DevKey.ToByteArray();
		}
	}
	else if(Command == "KillScript")
	{
		pair<CKadScript*, CObject*> ScopePair = ResolveScope(Request);
		if(ScopePair.first)
			ScopePair.first->Terminate();
	}
	else if(Command == "DeleteScript")
	{
		CKadEngine* pEngine = theKad->Kad()->GetChild<CKadEngine>();

		CVariant CodeID;
		CodeID.FromQVariant(Request["CodeID"]);
		pEngine->Remove(CodeID);
	}
	else if(Command == "AddTask")
	{
		CLookupManager* pLookupManager = theKad->Kad()->GetChild<CLookupManager>();

		CVariant TargetID;
		TargetID.FromQVariant(Request["TargetID"]);

		CKadTask* pKadTask = new CKadTask(TargetID, pLookupManager);
		pKadTask->SetName(L"*Kad Debugger*");
		pKadTask->SetTimeOut(SEC2MS(pKadTask->GetParent<CKademlia>()->Cfg()->GetInt64("MaxLookupTimeout")));

		CVariant CodeID;
		CodeID.FromQVariant(Request["CodeID"]);
		pKadTask->SetupScript(CodeID);
		
		CPointer<CKadLookup> pLookup(pKadTask);
		pLookup->EnableTrace(); // we want always to trace debug lookups
		CVariant LookupID = pLookupManager->StartLookup(pLookup);

		Response["LookupID"] = LookupID.ToQVariant();
	}
	else if(Command == "RemoveTask")
	{
		CLookupManager* pLookupManager = theKad->Kad()->GetChild<CLookupManager>();

		CVariant LookupID;
		LookupID.FromQVariant(Request["LookupID"]);

		CKadLookup* pLookup = pLookupManager->GetLookup(LookupID)->Cast<CKadLookup>();
		if(pLookup)
			pLookupManager->StopLookup(pLookup);
	}
	else {
		ASSERT(0); // unknown command
	}

	return Response;
}

void CKadScriptDebuggerThread::Evaluate(QString Program)
{
	QString Text;
	if(m_pScope)
	{
		m_pScope->KeepAlive();
		try
		{
			CJSScript* pScript = m_pScope->GetJSScript(true);
			
			string sPrgram = "(function() {" 
							+ Program.toStdString() 
							+ "\r\n})";

			v8::Isolate::Scope IsolateScope(g_Isolate);

			v8::Locker Lock(v8::Isolate::GetCurrent());
			v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
			v8::Local<v8::Context> Context = v8::Local<v8::Context>::New(v8::Isolate::GetCurrent(), pScript->GetContext());
			v8::Context::Scope SontextScope(Context);

			v8::Local<v8::String> ScriptSource = v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)sPrgram.c_str());

			v8::Local<v8::Script> Script = v8::Script::Compile(ScriptSource, v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Debugger Evaluate Code"));
			if(!Script.IsEmpty())
			{
				v8::Local<v8::Function> Function = v8::Local<v8::Function>::Cast(Script->Run());
				ASSERT(!Function.IsEmpty());
			
				v8::Local<v8::Object> Object;
				if(m_pAuxScope)
				{
					Object = pScript->GetObject(m_pAuxScope)->ToObject();
					ASSERT(!Object.IsEmpty());
				}
				else
					Object = Context->Global();

				CDebugScope Debug(m_pScope, m_pAuxScope);

				v8::Local<v8::Value> Result = Function->Call(Object, 0, NULL);

				if(!Result.IsEmpty())
					Text = QString::fromStdString(CJSEngine::GetStr(Result));				
				else
					Text = "Exception occured, no valid result!";
			}
			else
				Text = "Compile Failed";
		}
		catch (const CJSException& Exception)
		{
			Text = "Exception occured: " + QString::fromStdWString(Exception.GetLine());
		}
	}
	else
		Text = "No Scope Available for Eval";
	QMetaObject::invokeMethod(m_pBackend, "OnEvaluate", Qt::QueuedConnection, Q_ARG(QString, Text));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

CKadScriptDebuggerBackend* g_backend = NULL;

bool LogFilter(UINT Flags, const wstring& Trace)
{
	ASSERT(g_backend);
	QMetaObject::invokeMethod(g_backend, "onAsyncTrace", Qt::QueuedConnection, Q_ARG(QString, QString::fromStdWString(Trace)));
	return false; // for now we log all
}

void OnDebugMessage(const v8::Debug::Message& message)
{
	ASSERT(g_backend);
	wstring json = CJSEngine::GetWStr(message.GetJSON());
	QByteArray arrJson = QString::fromStdWString(json).toUtf8();
	QMetaObject::invokeMethod(g_backend, "onAsyncMessage", Qt::QueuedConnection, Q_ARG(QByteArray, arrJson));
}

CKadScriptDebuggerBackend::CKadScriptDebuggerBackend(QObject *parent)
: CIPCServer(parent)
{
	ASSERT(g_backend == NULL);
	g_backend = this;

	v8::Isolate::Scope IsolateScope(g_Isolate);

	v8::Locker Lock(v8::Isolate::GetCurrent());

	v8::Debug::SetMessageHandler(OnDebugMessage);

	m_Counter = 0;
	m_TimerId = startTimer(100);
}

CKadScriptDebuggerBackend::~CKadScriptDebuggerBackend()
{
	killTimer(m_TimerId);

	g_backend = NULL;
}

void CKadScriptDebuggerBackend::listen(const QString& Pipe)
{
	LocalListen(Pipe);
}

void CKadScriptDebuggerBackend::Evaluate(const QString& Program, int Frame)
{
	if(CDebugScope::IsEmpty())
	{
		CKadScriptDebuggerThread* pDebugger = qobject_cast<CKadScriptDebuggerThread*>(thread());
		ASSERT(pDebugger);
		QMetaObject::invokeMethod(pDebugger, "Evaluate", Qt::QueuedConnection, Q_ARG(QString, Program));
	}
	else
		CV8DebugAdapter::Evaluate(Program, Frame);
}

void CKadScriptDebuggerBackend::OnEvaluate(QString Text)
{
	CV8DebugAdapter::onEvaluate(false, Text);
}

bool CKadScriptDebuggerBackend::filterEvent(bool exception)
{
	CKadScriptDebuggerThread* pDebugger = qobject_cast<CKadScriptDebuggerThread*>(thread());
	ASSERT(pDebugger);

	CDebugScope::Type Scope = CDebugScope::Scope();
	if(Scope.first == NULL)
		return true;
	QMutexLocker Locker(&pDebugger->m_ScopesMutex);
	if(pDebugger->m_Scopes.contains(Scope.first, Scope.first) || pDebugger->m_Scopes.contains(Scope.first, Scope.second))
		return false;
	return true;
}

void CKadScriptDebuggerBackend::onAsyncTrace(const QString& Trace) 
{
	if(filterEvent(false))
		return;

	onTrace(Trace);
}

void CKadScriptDebuggerBackend::OnRequest(const QString& Command, const QVariant& Parameters, qint64 Number)
{
	CIPCSocket* pSocket = (CIPCSocket*)sender();
	QVariantMap Response;
	QVariantMap Request = Parameters.toMap();
	if(Command == "AttachDebugger")
		CV8DebugAdapter::attach();
	else if(Command == "DebuggerCommand")
	{
		int id = Request["ID"].toInt();
		ASSERT(id > 0);
		m_CommandMap.insert(id, QPair<QPointer<CIPCSocket>, qint64>(pSocket, Number)); 
		CV8DebugAdapter::onCommand(id, Request["Command"].toMap());
		return;
	}
	else if(Command == "DetachDebugger")
		CV8DebugAdapter::detach();
	else if(Command == "TerminateExecution")
		v8::V8::TerminateExecution(v8::Isolate::GetCurrent());
	else if(Command == "GetEngineState")
	{
		Response["Executing"] = !CDebugScope::IsEmpty();
		Response["Debugging"] = !isRunning();
	}
	else // custom kad staff, not V8 related
	{
		CKadScriptDebuggerThread* pDebugger = qobject_cast<CKadScriptDebuggerThread*>(thread());
		ASSERT(pDebugger);
		CKadScriptDebuggerThread::SCommand Cmd = {Command, Request, ++m_Counter};
		m_CommandMap.insert(-Cmd.UID, QPair<QPointer<CIPCSocket>, qint64>(pSocket, Number)); 
		QMutexLocker Lock(&pDebugger->m_QueueMutex);
		pDebugger->m_CommandQueue.append(Cmd);
		return;
	}
	pSocket->SendResponse(Command, Response, Number);
}

void CKadScriptDebuggerBackend::timerEvent(QTimerEvent *e)
{
    if (e->timerId() != m_TimerId) 
	{
        QObject::timerEvent(e);
		return;
    }

	// Note: if the engine is not running, it means the kad thread is halted,
	//			so we have to execute the commands ourselvs, and since the kad thread is locked,
	//			we can do this safely without any additional synchronisation
	if(!isRunning())
	{
		CKadScriptDebuggerThread* pDebugger = qobject_cast<CKadScriptDebuggerThread*>(thread());
		ASSERT(pDebugger);

		for(int i=0; i < pDebugger->m_CommandQueue.count(); i++)
		{
			CKadScriptDebuggerThread::SCommand Cmd = pDebugger->m_CommandQueue.at(i);

			// Note: only selected commands can be executed while engine is running
			if(!(Cmd.Command == "ListScripts" || Cmd.Command == "SelectScope" || Cmd.Command == "AddScope" || Cmd.Command == "RemoveScope"))
				continue;

			pDebugger->m_CommandQueue.removeAt(i);
			QVariantMap Response = pDebugger->ProcessRequest(Cmd.Command, Cmd.Request);
			PushReply(Cmd.Command, Response, Cmd.UID);
		}
	}
}

void CKadScriptDebuggerBackend::onAsyncMessage(QByteArray arrJson)
{
	onMessage(arrJson);
}

void CKadScriptDebuggerBackend::PushReply(QString Command, QVariant Parameters, int UID)
{
	QPair<QPointer<CIPCSocket>, qint64> Ret = m_CommandMap.take(-UID);
	if(Ret.first)
		Ret.first->SendResponse(Command, Parameters, Ret.second);
}

void CKadScriptDebuggerBackend::onRequest(const QByteArray& arrJson)
{
	v8::Isolate::Scope IsolateScope(g_Isolate);

	QString strJson = QString::fromLatin1(arrJson);
	v8::Debug::SendCommand(v8::Isolate::GetCurrent(), strJson.utf16(), strJson.size());
}

void CKadScriptDebuggerBackend::onResult(int id, const QVariantMap& Result)
{
	QVariantMap Response;
	Response["ID"] = id;
	Response["Result"] = Result;

	QPair<QPointer<CIPCSocket>, qint64> Ret = m_CommandMap.take(id);
	if(Ret.first)
		Ret.first->SendResponse("DebuggerCommand", Response, Ret.second);
}

void CKadScriptDebuggerBackend::onEvent(const QVariantMap& Event)
{
	QVariantMap Notification;
	Notification["Event"] = Event;
	PushNotification("DebuggerEvent", Notification);
}
