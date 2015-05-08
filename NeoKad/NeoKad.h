#pragma once

#include "../Framework/IPC/IPCSocket.h"
#include "../Framework/ObjectEx.h"
#include "Common/Pointer.h"
#include "Networking/SafeAddress.h"
#include <QCoreApplication>

class CSettings;
class CSmartSocket;
class CKademlia;
class CPrivateKey;
class CRoutingZone;
class CKadLookup;
class CRequestManager;

#define KAD_VERSION_MJR		0
#define KAD_VERSION_MIN 	14
#define KAD_VERSION_UPD 	2

#define APP_ORGANISATION	"Neo"
#define APP_NAME			"NeoKad"
#define APP_DOMAIN			"neoloader.com"

class CNeoKad: public CLoggerTmpl<QObject>
{
	Q_OBJECT

public:
	CNeoKad(QObject *parent = 0);
	~CNeoKad();

	void				SetEmbedded()		{m_bEmbedded = true;}
	bool				IsEmbedded()		{return m_bEmbedded;}
	void				Process();

	CSettings*			Cfg()			{return m_Settings;}
	CKademlia*			Kad()			{return m_pKademlia;}

	void				Connect();
	void				Disconnect();

	void				Authenticate(QString ScriptPath);

signals:
	void				ShowGUI();

private slots:
	void				Shutdown()					{m_bEmbedded = false; QCoreApplication::exit(0);}
	void				OnRequestRecived(const QString& Command, const QVariant& Parameters, QVariant& Result);

	void				OnRequestFinished();

protected:
	void				timerEvent(QTimerEvent* pEvent)
	{
		if(pEvent->timerId() == m_uTimerID)
			Process();
	}

	void				CheckAndBootstrap();

	//void				LoadNodes();
	//void				StoreNodes();

	UINT				m_uTimerCounter;
	int					m_uTimerID;

	CIPCServer*			m_pInterface;
	CSettings*			m_Settings;

	CKademlia*			m_pKademlia;
	//uint64				m_uLastSave;

	bool				m_bEmbedded;
	uint64				m_uLastContact;

	CRequestManager*	m_pRequestManager;
	QNetworkReply*		m_pBootstrap;
};


extern CNeoKad* theKad;

template <class T>
QString ListAddr(T AddressMap)
{
	QString Addresses;
	for(typename T::const_iterator J = AddressMap.begin(); J != AddressMap.end(); J++)
	{
		QString Address;
		Address = QString::fromStdWString(J->second.ToString());
		if(!J->second.IsVerifyed())
			Address.prepend("~ ");
		if(const CSafeAddress* pAssistent = J->second.GetAssistent())
		{
			if(pAssistent->IsValid())
				Address.append(CNeoKad::tr(" (Assisted by: %1 )").arg(QString::fromStdWString(pAssistent->ToString())));
			else
				Address.append(CNeoKad::tr(" (Firewalled)"));
		}

		if(!Addresses.isEmpty())
			Addresses.append("; ");
		Addresses.append(Address);
	}
	return Addresses;
}
