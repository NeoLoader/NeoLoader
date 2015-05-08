#pragma once

#include "../../Framework/ObjectEx.h"

class CCoreBus: public QObjectEx
{
	Q_OBJECT

public:
	CCoreBus(QString BusName, quint16 BusPort, bool bPassive = false);
	~CCoreBus();

	bool				ListNodes(bool inLAN = false);
	struct SCore
	{
		SCore(){Remote = false;}
		QString Path;
		QString Name;
		QString Host;
		quint16 Port;
		bool	Remote;
	};
	const QList<SCore>	GetNodeList()	{return m_FoundNodes;}

	QString				GetBusName()	{return m_BusName;}
	quint16				GetBusPort()	{return m_BusPort;}

	void				close()			{if(m_Broadcast) m_Broadcast->close();}

private slots:
//	void				OnTimer();

	void				OnBusOperate();

	void				OnBusConnection();

	void				OnConnected();
	void				OnReadyRead();
	void				OnDisconnected();
	//void				OnError(QLocalSocket::LocalSocketError socketError);

	void				OnDatagrams();

protected:
	SCore				ReadDescription(const QString& Description);

	QString				m_BusName;
	quint16				m_BusPort;
	bool				m_bPassive;

	//QTimer*				m_Timer;

	QLocalServer*		m_BusMaster;
	QMap<QLocalSocket*, QString> m_BusNodes;
	QLocalSocket*		m_BusSlave;

	QList<SCore>		m_FoundNodes;

	QUdpSocket*			m_Broadcast;
};