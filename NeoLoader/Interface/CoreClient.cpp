#include "GlobalHeader.h"
#include "CoreClient.h"
#include "CoreServer.h"
#include "CoreBus.h"
#include "../NeoCore.h"
#include "../GUI/NeoLoader.h"

//#define _NO_CORE	// Use this to test client core communication using only one instance

CCoreClient::CCoreClient(QObject* qObject)
: CIPCClient(qObject)
{
	//m_Bus = NULL;
	m_uTimeOut = 0;
}

bool CCoreClient::HasCore()
{
#ifndef _NO_CORE
	if(theCore)
		return true;
#endif
	return false;
}

bool CCoreClient::CanConnect()
{
#ifndef _NO_CORE
	if(theCore)
	{
		LogLine(LOG_ERROR, tr("this instance has a core and therefor can not be connected to a core"));
		return false;
	}
#endif
	return CIPCClient::CanConnect();
}

bool CCoreClient::IsConnected()
{
#ifndef _NO_CORE
	if(theCore)
		return theCore->m_Server;
#endif
	if(HasTimedOut())
		return false;
	return CIPCClient::IsConnected();
}

bool CCoreClient::IsDisconnected()
{
#ifndef _NO_CORE
	if(theCore)
		return false;
#endif
	return CIPCClient::IsDisconnected();
}

void CCoreClient::Disconnect()
{
	m_uTimeOut = 0;
	CIPCClient::Disconnect();
}

QString CCoreClient::GetLoginToken()
{
#ifndef _NO_CORE
	if(theCore)
		return theCore->GetLoginToken();
#endif
	return CIPCClient::GetLoginToken();
}

/*int CCoreClient::ConnectBus(QString BusName, quint16 BusPort)
{
	if(m_Bus)
	{
		if(BusName == m_Bus->GetBusName() && BusPort == m_Bus->GetBusPort())
			return 1;
		delete m_Bus;
		m_Bus = NULL;
	}

	if(!BusName.isEmpty() || BusPort != 0)
	{
		m_Bus = new CCoreBus(BusName, BusPort, true);
		m_Bus->setParent(this);
	}
	return m_Bus ? -1 : 0;
}

bool CCoreClient::ListNodes(bool inLAN)	
{
	if(m_Bus) 
		return m_Bus->ListNodes(inLAN); 
	return false; 
}

QList<CCoreBus::SCore> CCoreClient::GetNodeList()
{
	if(m_Bus) 
		return m_Bus->GetNodeList();
	return QList<CCoreBus::SCore>();
}*/

bool CCoreClient::SendRequest(const QString& Command, const QVariant& Parameters)
{
#ifndef _NO_CORE
	if(theCore && theCore->m_Server)
		emit Request(Command, Parameters);
	else
#endif
	if(!CIPCClient::SendRequest(Command, Parameters))
		return false;

	SetTimeOut();
	return false;
}

void CCoreClient::DispatchResponse(const QString& Command, const QVariant& Result)
{
	StopTimeOut();

	theLoader->Dispatch(Command, Result.toMap());
}

void CCoreClient::OnResponse(const QString& Command, const QVariant& Parameters, sint64 Number)
{
	DispatchResponse(Command, Parameters);
}

void CCoreClient::SetTimeOut()
{
	if(!m_uTimeOut)
		m_uTimeOut = GetCurTick() + SEC2MS(theLoader->Cfg()->GetInt("Core/TimeoutSecs"));
}