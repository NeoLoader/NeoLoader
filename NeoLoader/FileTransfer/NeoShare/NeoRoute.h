#pragma once

#include "../../../Framework/ObjectEx.h"
#include "../../../Framework/Cryptography/AsymmetricKey.h"

class CNeoSession;
class CBandwidthLimit;

#define INVALID_SESSION ((CNeoSession*)-1)

class CNeoRoute: public QObjectEx
{
	Q_OBJECT
public:
	CNeoRoute(const QByteArray& EntityID, const QByteArray& TargetID, CPrivateKey* pEntityKey, bool bStatic = false, QObject* pObject = NULL);
	~CNeoRoute();

	bool				Process();

	uint64				GetTimeOut() const				{return m_TimeOut;}
	bool				IsStatic() const				{return m_TimeOut == -1;}
	const QByteArray&	GetEntityID() const				{return m_EntityID;}
	const QByteArray&	GetTargetID() const				{return m_TargetID;}
	CPrivateKey*		GetEntityKey() const			{return m_pEntityKey;}
	void				SetDuration(uint64 Duration)	{m_Duration = Duration;}

	CNeoSession*		NewSession(const QByteArray& EntityID, const QByteArray& TargetID, const QByteArray& SessionID);
	void				DelSession(CNeoSession* pSession);

	void				RemoveSession(const QByteArray& SessionID)	{m_Sessions[SessionID] = INVALID_SESSION;}

signals:
	void				RouteBroken();
	void				Connection(CNeoSession* pSession);

protected:
	QMap<QByteArray, CNeoSession*>	m_Sessions;

	QByteArray			m_TargetID;
	QByteArray			m_EntityID;
	CPrivateKey*		m_pEntityKey;
	uint64				m_TimeOut;
	uint64				m_Duration;
};