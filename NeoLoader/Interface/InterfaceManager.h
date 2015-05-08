#pragma once

#include <QProcess>
#include "../../Framework/ObjectEx.h"
#include "../../Framework/IPC/IPCClient.h"

/*class QProcessEx: public QProcess
{
public:
	QProcessEx(QObject* pParent = NULL) : QProcess(pParent) {}

	void	Detache() {setProcessState(QProcess::NotRunning);}
};*/

class CInterfaceManager: public QObjectEx
{
	Q_OBJECT

public:
	CInterfaceManager(QObject* qObject = NULL);
	~CInterfaceManager();

	virtual void				EstablishInterface(QString Interface, QObject* pTarget = NULL);
	virtual bool				IsInterfaceEstablished(const QString& Interface);
	virtual void				TerminateInterface(QString Interface);

	virtual void				Process();

	virtual	bool				SendNotification(const QString& Interface, const QString& Command, const QVariant& Parameters);
	virtual QVariant			RemoteProcedureCall(const QString& Interface, const QString& Command, const QVariant& Parameters);

protected:
	struct SInterface
	{
		SInterface(const QString name) 
		 : pClient(NULL), pProcess(NULL) 
		{
			Name = name;
			StartTime = 0;
			ConnectTime = 0;
			Aux = 0;
		}
		CIPCClient* pClient;
		QObject*	pTarget;
		QProcess*	pProcess;
		QString		Name;
		uint64		Aux;
		QString		FilePath;
		uint64		StartTime;
		uint64		ConnectTime;
	};

	QMap<QString, SInterface*>	m_Interfaces;
};
