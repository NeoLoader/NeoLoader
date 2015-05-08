#include "GlobalHeader.h"
#include "NeoRoute.h"
#include "NeoSession.h"
#include "NeoManager.h"
#include "../../NeoCore.h"
#include "../../Interface/InterfaceManager.h"

//int _CNeoSession_pType = qRegisterMetaType<CNeoSession*>("CNeoSession*");
int _CNeoSession_pType = qRegisterMetaType<CNeoSession*>("CNeoSession*", (CNeoSession**)-1);

CNeoRoute::CNeoRoute(const QByteArray& EntityID, const QByteArray& TargetID, CPrivateKey* pEntityKey, bool bStatic, QObject* pObject) 
: QObjectEx(pObject)
{
	m_EntityID = EntityID;
	m_TargetID = TargetID;
	m_pEntityKey = pEntityKey;
	m_TimeOut = bStatic ? -1 : (GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("NeoShare/RouteTimeout"))); // TimeOut == -1 -> static route
	m_Duration = 0; 
} 

CNeoRoute::~CNeoRoute()
{
	foreach(CNeoSession* pSession, m_Sessions)
	{
		if(pSession != INVALID_SESSION)
			pSession->Dispose();
	}
	delete m_pEntityKey;
}

CNeoSession* CNeoRoute::NewSession(const QByteArray& EntityID, const QByteArray& TargetID, const QByteArray& SessionID)
{
	CNeoSession* pSession = new CNeoSession(EntityID, TargetID, SessionID, this);
	pSession->AddUpLimit(theCore->m_NeoManager->GetUpLimit());
	pSession->AddDownLimit(theCore->m_NeoManager->GetDownLimit());
	m_Sessions[SessionID] = pSession;
	return pSession;
}

void CNeoRoute::DelSession(CNeoSession* pSession)
{
	QVariantMap Request;
	Request["MyEntityID"] = m_EntityID;
	Request["EntityID"] = pSession->GetEntityID();
	Request["SessionID"] = pSession->GetSessionID();

	theCore->m_Interfaces->RemoteProcedureCall("NeoKad", "CloseSession", Request).toMap();

	pSession->Dispose();
}

bool CNeoRoute::Process()
{
	QVariantMap Request;
	Request["MyEntityID"] = m_EntityID;
	QVariantMap Response = theCore->m_Interfaces->RemoteProcedureCall("NeoKad", "QuerySessions", Request).toMap();
	if(Response["Result"] != "ok")
		return false; // route broken, manager will handle it later

	QMap<QByteArray, CNeoSession*> Sessions = m_Sessions;

	QVariantList SessionList = Response["Sessions"].toList();
	foreach(const QVariant& vSession, SessionList)
	{
		QVariantMap Session = vSession.toMap();

		bool bConnected = Session["Connected"].toBool();

		CNeoSession* pSession = Sessions.take(Session["SessionID"].toByteArray());
		if(pSession == NULL)
		{
			if(!bConnected)
				continue; // dont bother with broken sessions

			pSession = NewSession(Session["EntityID"].toByteArray(), Session["TargetID"].toByteArray(), Session["SessionID"].toByteArray());
			emit Connection(pSession);
		}
		
		if(pSession == INVALID_SESSION) // if we have dropped this sesison we dont want to be bothered by it anymore
			continue;
		if(bConnected || pSession->IsConnected()) // session must be connected or at elast have been connected
			pSession->Process(bConnected, Session);
	}

	for(QMap<QByteArray, CNeoSession*>::iterator I = Sessions.begin(); I != Sessions.end(); I++)
	{
		// Note: we remember once added sessions as long as the NeoKad Module Handles them.
		CNeoSession* pSession = I.value();
		if(pSession == INVALID_SESSION)
			m_Sessions.remove(I.key());
		else if(pSession->IsConnected()) // if the session dont know its disconnected, notify it
			pSession->Process(false); // this wil emit a disconnect
	}

	if(m_TimeOut != -1 && !SessionList.isEmpty())
		m_TimeOut = GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("NeoShare/RouteTimeout"));

	return true;
}