#include "GlobalHeader.h"
#include "../NeoCore.h"
#include "InterfaceManager.h"
#include "../FileTransfer/ed2kMule/MuleManager.h"

CInterfaceManager::CInterfaceManager(QObject* qObject)
 : QObjectEx(qObject)
{
}

CInterfaceManager::~CInterfaceManager()
{
	// terminate started modules
	foreach(SInterface* pInterface, m_Interfaces)
	{
		if(pInterface->pProcess)
			RemoteProcedureCall(pInterface->Name, "Shutdown", QVariant());
	}

	// give 10 sec for termination
	for(int i=0; i < 100; i++)
	{
		int Running = 0;
		foreach(SInterface* pInterface, m_Interfaces)
		{
			if(pInterface->pProcess && pInterface->pProcess->state() != QProcess::NotRunning)
				Running ++;
		}
		if(Running == 0)
			break;
		QThreadEx::msleep(100);
	}

	// delete interfaces and Kill processes if not terminated
	foreach(SInterface* pInterface, m_Interfaces)
	{
		if(pInterface->pClient)
		{
			delete pInterface->pClient;
			pInterface->pClient = NULL;
		}
		delete pInterface;
	}
}

void CInterfaceManager::EstablishInterface(QString Interface, QObject* pTarget)
{
	Interface = theCore->Cfg()->GetString("Modules/" + Interface);
	if(Interface.isEmpty())
		return;

	ASSERT(!m_Interfaces.contains(Interface));
	SInterface* pInterface = new SInterface(Interface);
#ifdef WIN32
	pInterface->FilePath = CSettings::GetAppDir() + "/" + Interface + ".exe";
#else
	pInterface->FilePath = CSettings::GetAppDir() + "/" + Interface;
#endif
	pInterface->pTarget = pTarget;
	m_Interfaces.insert(Interface, pInterface); 
}

bool CInterfaceManager::IsInterfaceEstablished(const QString& Interface)
{
	if(!m_Interfaces.contains(Interface))
		return false;

	CIPCClient*	pClient = m_Interfaces.value(Interface)->pClient;
	if(!pClient)
		return false;
	return pClient->IsConnected();
}

void CInterfaceManager::Process()
{
	int AutoStart = theCore->Cfg()->GetInt("Modules/AutoStart");
	if(AutoStart == -1)
		return;

	foreach(SInterface* pInterface, m_Interfaces)
	{
		if(pInterface->pClient && !pInterface->pClient->IsConnected() && pInterface->ConnectTime + SEC2MS(theCore->Cfg(false)->GetInt("Core/TimeoutSecs")/3) < GetCurTick())
		{
			delete pInterface->pClient;
			pInterface->pClient = NULL;

			if(!pInterface->pProcess || pInterface->StartTime + SEC2MS(theCore->Cfg(false)->GetInt("Core/TimeoutSecs")) < GetCurTick())
			{
				if(AutoStart != 0)
				{
					pInterface->StartTime = GetCurTick();
					if(pInterface->pProcess)
					{
						pInterface->pProcess->terminate();
						delete pInterface->pProcess;
					}
					pInterface->pProcess = new QProcess(this);
					QStringList Arguments;
					if(AutoStart == 1)
						Arguments.append("-silent");
					Arguments.append("-name");
					pInterface->Aux = GetRand64();
					QString Name = QString::number(pInterface->Aux);
					Arguments.append(Name);
					LogLine(LOG_DEBUG, tr("Starting Module %1").arg(pInterface->Name));
					//LogLine(LOG_DEBUG, tr("Starting Module %1 Pipe Name %2").arg(pInterface->Name).arg(Name));
					pInterface->pProcess->setWorkingDirectory(CSettings::GetAppDir());
					pInterface->pProcess->start(pInterface->FilePath, Arguments);
				}
			}
		}

		if(!pInterface->pClient && (!pInterface->StartTime || GetCurTick() - pInterface->StartTime > SEC2MS(5)))
		{
			pInterface->ConnectTime = GetCurTick();
			pInterface->pClient = new CIPCClient(this);
			if(pInterface->pTarget)
				connect(pInterface->pClient, SIGNAL(NotificationRecived(const QString&, const QVariant&)), pInterface->pTarget, SLOT(OnNotificationRecived(const QString&, const QVariant&)));

			QString Name = pInterface->Aux ? QString::number(pInterface->Aux) : pInterface->Name;
			//LogLine(LOG_DEBUG, tr("Connecting Interface %1 name %2").arg(pInterface->Name).arg(Name));
			pInterface->pClient->ConnectLocal(Name);
		}
	}
}

void CInterfaceManager::TerminateInterface(QString Interface)
{
	if(!m_Interfaces.contains(Interface))
		return;

	SInterface*	pInterface = m_Interfaces.value(Interface);
	if(pInterface->pProcess)
	{
		RemoteProcedureCall(pInterface->Name, "Shutdown", QVariant());

		if(pInterface->pClient)
		{
			pInterface->pClient->Disconnect();
			delete pInterface->pClient;
			pInterface->pClient = NULL;
		}

		for(int i=0; i < 100; i++)
		{
			if(pInterface->pProcess->state() == QProcess::NotRunning)
				break;
			QThreadEx::msleep(100);
		}

		delete pInterface->pProcess;
	}
	m_Interfaces.remove(Interface);
	delete pInterface;
}

bool CInterfaceManager::SendNotification(const QString& Interface, const QString& Command, const QVariant& Parameters)
{
	if(!m_Interfaces.contains(Interface))
		return false;

	CIPCClient*	pClient = m_Interfaces.value(Interface)->pClient;
	if(!pClient || !pClient->SendRequest(Command, Parameters))
		return false;
	return true;
}

QVariant CInterfaceManager::RemoteProcedureCall(const QString& Interface, const QString& Command, const QVariant& Parameters)
{
	if(!m_Interfaces.contains(Interface))
	{
		QVariantMap Response;
		Response["Error"] = "Interface not established";
		Response["Result"] = "fail";
		return Response;
	}

	QVariant Result;
	CIPCClient*	pClient = m_Interfaces.value(Interface)->pClient;
	if(!pClient || !pClient->SendRequest(Command, Parameters, Result))
	{
		QVariantMap Response;
		Response["Error"] = "Interface not connected or timed out";
		Response["Result"] = "fail";
		return Response;
	}
	return Result;
}
