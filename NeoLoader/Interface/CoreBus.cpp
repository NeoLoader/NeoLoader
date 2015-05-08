#include "GlobalHeader.h"
#include "CoreServer.h"
#include "CoreBus.h"
#include "../NeoCore.h"
#include <QCoreApplication>

CCoreBus::CCoreBus(QString BusName, quint16 BusPort, bool bPassive)
{
	m_BusName = BusName;
	m_BusPort = BusPort;
	m_bPassive = bPassive; // we are a gui only

     //m_Timer = new QTimer(this);
     //connect(m_Timer, SIGNAL(timeout()), this, SLOT(OnTimer()));
     //m_Timer->start(3000);

	m_BusMaster = NULL;
	m_BusSlave = NULL;

	if(BusPort && bPassive)
	{
		m_Broadcast = new QUdpSocket(this);
		m_Broadcast->bind();
		connect(m_Broadcast, SIGNAL(readyRead()), this, SLOT(OnDatagrams()));
	}
	else
		m_Broadcast = NULL;

	if(!BusName.isEmpty())
		QTimer::singleShot(100, this, SLOT(OnBusOperate()));
}

CCoreBus::~CCoreBus()
{
	//m_Timer->stop();
	//delete m_Timer;

	if(m_BusMaster)
		delete m_BusMaster;

	if(m_BusSlave)
		delete m_BusSlave;

	foreach(QLocalSocket* pBusNode, m_BusNodes.keys())
		delete pBusNode;
	m_BusNodes.clear();

	if(m_Broadcast)
	{
		m_Broadcast->close();
		delete m_Broadcast;
	}
}

bool CCoreBus::ListNodes(bool inLAN)
{
	ASSERT(m_bPassive);

	m_FoundNodes.clear();

	if(inLAN)
	{
		if(m_Broadcast)
		{
			m_Broadcast->writeDatagram("Enum", QHostAddress::Broadcast, m_BusPort);
			return true;
		}
	}
	else if(m_BusSlave)
	{
		m_BusSlave->write("Enum\r\n");
		return true;
	}
	return false;
}

//void CCoreBus::OnTimer()
//{
//}

void CCoreBus::OnBusOperate()
{
	if(m_BusSlave == NULL)
	{
		m_BusSlave = new QLocalSocket(this);
		connect(m_BusSlave, SIGNAL(connected()), this, SLOT(OnConnected()));
		connect(m_BusSlave, SIGNAL(disconnected()), this, SLOT(OnDisconnected()));
		connect(m_BusSlave, SIGNAL(readyRead()), this, SLOT(OnReadyRead()));
		//connect(m_BusSlave, SIGNAL(error(QLocalSocket::LocalSocketError)), this, SLOT(OnError(QLocalSocket::LocalSocketError)));
		m_BusSlave->connectToServer(m_BusName);
	}
}

void CCoreBus::OnBusConnection()
{
	QLocalSocket* pSlave = m_BusMaster->nextPendingConnection();
	connect(pSlave, SIGNAL(readyRead()), this, SLOT(OnReadyRead()));
	connect(pSlave, SIGNAL(disconnected()), this, SLOT(OnDisconnected()));
	m_BusNodes.insert(pSlave, "");
}

void CCoreBus::OnConnected()
{
	if(!m_bPassive)
	{
		QString Description = "Enum ";
		Description += "Client=Neo|";
		Description += "Path=" + QCoreApplication::applicationFilePath() + "|";
		Description += "Name=" + theCore->m_Server->GetName() + "|";
		Description += "Port=" + QString::number(theCore->m_Server->GetPort()) + "|";
		Description += "\r\n";
		m_BusSlave->write(Description.toUtf8());
	}
}


CCoreBus::SCore CCoreBus::ReadDescription(const QString& Description)
{
	SCore Core;
	foreach(const QString& sTag, Description.split("|"))
	{
		int Sep = sTag.indexOf("=");
		if(Sep != -1)
		{
			QString TagName = sTag.left(Sep);
			QString TagValue = sTag.mid(Sep+1);
			if(TagName.compare("Path") == 0)
				Core.Path = TagValue;
			else if(TagName.compare("Name") == 0)
				Core.Name = TagValue;
			else if(TagName.compare("Port") == 0)
				Core.Port = TagValue.toUInt();
			//else if(TagName.compare("Host") == 0)
			//	Core.Host = TagValue;
		}
	}
	return Core;
}


void CCoreBus::OnReadyRead() // read as slave
{
	QLocalSocket* pSocket = (QLocalSocket*)sender();

	for(;;)
	{
		QString Line = QString::fromUtf8(pSocket->readLine()); // Note: do not send multiline requests to the bus, each "packet must be a single line"
		if(Line.isEmpty())
			break;

		StrPair Pair = Split2(Line," ");
		if(pSocket != m_BusSlave)
		{
			if(Pair.first.compare("Enum") == 0)
			{
				if(!Pair.second.isEmpty())
					m_BusNodes.insert(pSocket,Pair.second);
				else // request Node list
				{
					QString List;
					foreach(const QString &Description, m_BusNodes)
					{
						if(!Description.isEmpty())
							List += "Enum " + Description + "\r\n";
					}
					pSocket->write(List.toUtf8());
				}
			}
		}
		else
		{
			ASSERT(m_bPassive);
			if(Pair.first.compare("Enum") == 0)
				m_FoundNodes.append(ReadDescription(Pair.second));
		}
	}
}

void CCoreBus::OnDisconnected()
{
	QLocalSocket* pSocket = (QLocalSocket*)sender();
	if(pSocket == m_BusSlave)
	{
		m_BusSlave->deleteLater();
		m_BusSlave = NULL;
		QTimer::singleShot(1000, this, SLOT(OnBusOperate()));

		if(m_bPassive)
			return;

		m_BusMaster = new QLocalServer(this);
		if(m_BusMaster->listen(m_BusName))
		{
			connect(m_BusMaster, SIGNAL(newConnection()), this, SLOT(OnBusConnection()));

			if(m_BusPort)
			{
				ASSERT(m_Broadcast == NULL);
				m_Broadcast = new QUdpSocket(this);
				m_Broadcast->bind(m_BusPort, QUdpSocket::ShareAddress);
				connect(m_Broadcast, SIGNAL(readyRead()), this, SLOT(OnDatagrams()));
			}
		}
		else
		{
			m_BusMaster->deleteLater();
			m_BusMaster = NULL;
		}
	}
	else
	{
		m_BusNodes.remove(pSocket);
		pSocket->deleteLater();
	}
}

/*void CCoreBus::OnError(QLocalSocket::LocalSocketError socketError)
{
	OnDisconnected();
}*/

void CCoreBus::OnDatagrams()
{
	while (m_Broadcast->hasPendingDatagrams())
	{
		QByteArray Datagram;
		Datagram.resize(m_Broadcast->pendingDatagramSize());
		QHostAddress Sender;
		quint16 SenderPort;

		m_Broadcast->readDatagram(Datagram.data(), Datagram.size(), &Sender, &SenderPort);

		QString Line = Datagram;
		if(Line.isEmpty())
			continue;

		StrPair Pair = Split2(Line," ");
		if(m_bPassive)
		{
			if(Pair.first.compare("Enum") == 0)
			{
				SCore Core = ReadDescription(Pair.second);
				Core.Host = Sender.toString();
				Core.Remote = true;
				m_FoundNodes.append(Core);
			}
		}
		else
		{
			if(Pair.first.compare("Enum") == 0)
			{
				QString List;
				foreach(const QString &Description, m_BusNodes)
				{
					if(!Description.isEmpty())
						List += "Enum " + Description + "\r\n";
				}
				m_Broadcast->writeDatagram(List.toUtf8(),Sender,SenderPort);
			}
		}
	}
}