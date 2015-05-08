#include "GlobalHeader.h"
#include "../TorrentManager.h"
#include "../../../NeoCore.h"
#include "UdpTrackerClient.h"
#include "../../../../Framework/Buffer.h"
#include "../TorrentPeer.h"
#include "../TorrentServer.h"
#include "../Torrent.h"
#include "../../../../Framework/Exception.h"

CUdpTrackerClient::CUdpTrackerClient(const QUrl& Url, const QByteArray& InfoHash, QObject* qObject)
 : CTrackerClient(Url, InfoHash, qObject)
{
	m_ConnectionID = 0;
	m_IDTimeOut = 0;
	m_TransactionID = 0;
	m_PendingEvent = eNone;
	m_FailCount = 0;
	m_uTimerID = 0;

	m_Socket = NULL;
}

void CUdpTrackerClient::SetupSocket(const CAddress& IPv4)
{
	m_Socket = new QUdpSocket(this);
	if(!IPv4.IsNull()) // Note: Udp trackers are specyfyed only for IPv4
		m_Socket->bind(QHostAddress(IPv4.ToIPv4()), 0);
	else
		m_Socket->bind();
	connect(m_Socket, SIGNAL(readyRead()), this, SLOT(OnDatagrams()));
}

void CUdpTrackerClient::Announce(EEvent Event)
{
	if(!m_ConnectionID || m_IDTimeOut < GetCurTick())
	{
		Connect();
		m_PendingEvent = Event;
		return;
	}

	m_TransactionID = GetRand64();

	CBuffer Packet(8 + 4 + 4 + 20 + 20 + 8 + 8 + 8 + 4 + 4 + 4 + 4 + 2 + 2);

	Packet.WriteValue<uint64>(m_ConnectionID, true);						// connection_id
	Packet.WriteValue<uint32>(eAnnounce, true);								// action
	Packet.WriteValue<uint32>(m_TransactionID, true);						// transaction_id
	Packet.WriteData(m_InfoHash.data(),m_InfoHash.size());					// info_hash
	Packet.WriteData(theCore->m_TorrentManager->GetClientID().Data, 20);	// peer_id

	Packet.WriteValue<uint64>(m_uDownloaded, true);	// downloaded
	Packet.WriteValue<uint64>(m_uLeft, true); // left
	Packet.WriteValue<uint64>(m_uUploaded, true);	// uploaded

	Packet.WriteValue<uint32>(Event, true);								// event
	if (Event == eCompleted)
		m_Completed = true;
	else if(Event == eStarted)
	{
		m_Started = true;
		if(m_uLeft == 0)
			m_Completed = true;
	}
	else if(Event == eStopped)
		m_Stopped = true;

	Packet.WriteValue<uint32>(0, true);									// ip address
	Packet.WriteValue<uint32>(0, true);									// key
	Packet.WriteValue<uint32>(theCore->Cfg()->GetInt("BitTorrent/AnnounceWanted"), true);// num_want
	Packet.WriteValue<uint16>(theCore->m_TorrentManager->GetServer()->GetPort(), true);	// port
	Packet.WriteValue<uint16>(0, true);									// extensions

	ASSERT(Packet.GetLength() == Packet.GetPosition());
	m_Socket->writeDatagram((char*)Packet.GetBuffer(), Packet.GetSize(), m_Address, m_Url.port());
	m_uTimerID = startTimer(SEC2MS(15 * (2 ^ m_FailCount)));
}

void CUdpTrackerClient::Connect()
{
	if(m_Address.isNull())
	{
		m_TransactionID = -1;
		QHostInfo::lookupHost(m_Url.host(), this, SLOT(OnLookupHost(QHostInfo)));
		return;
	}
	
	m_ConnectionID = 0;
	m_TransactionID = GetRand64();

	CBuffer Packet(4 + 4 + 4 + 4);
	Packet.WriteValue<uint32>(0x417, true);					// 
	Packet.WriteValue<uint32>(0x27101980, true);			// connection_id
	Packet.WriteValue<uint32>(eConnect, true);				// action (connect)
	Packet.WriteValue<uint32>(m_TransactionID, true);		// transaction_id
	
	ASSERT(Packet.GetLength() == Packet.GetPosition()); // have we filled all data ? 
	m_Socket->writeDatagram((char*)Packet.GetBuffer(), Packet.GetSize(), m_Address, m_Url.port());
	m_uTimerID = startTimer(SEC2MS(15 * (2 ^ m_FailCount)));
}

void CUdpTrackerClient::OnLookupHost(const QHostInfo &Host)
{
	m_TransactionID = 0;
	if (Host.error() == QHostInfo::NoError && !Host.addresses().isEmpty()) 
	{
		m_Address = Host.addresses().first();
		Connect();
	}
	else
	{
		LogLine(LOG_ERROR, tr("Tracker host name lookup failed: %1").arg(Host.errorString()));
		SetError("Host Name Lookup Failed");
	}
}

void CUdpTrackerClient::TimedOut()
{
	m_TransactionID = 0;
	killTimer(m_uTimerID);
	m_uTimerID = 0;

	if(m_FailCount++ >= 3) // (0)15 + (1)30 + (2)60 + (3)128 + (4)240 + ... +(8)
		SetError("TimeOut");
}

void CUdpTrackerClient::OnDatagrams()
{
	while (m_Socket->hasPendingDatagrams())
	{
		CBuffer Packet(m_Socket->pendingDatagramSize());
		QHostAddress Sender;
		quint16 SenderPort;

		uint64 Ret = m_Socket->readDatagram((char*)Packet.GetBuffer(), Packet.GetLength(), &Sender, &SenderPort);
		if(Ret == -1)
			continue;
		Packet.SetSize(Ret);

		try
		{
			killTimer(m_uTimerID);
			m_uTimerID = 0;
			m_FailCount = 0;

			EAction Action  = (EAction)Packet.ReadValue<uint32>(true);
			uint32 TransactionID = Packet.ReadValue<uint32>(true);
			if(TransactionID != m_TransactionID)
			{
				SetError("Invalid transaction ID");
				continue;
			}
			m_TransactionID = 0;

			switch(Action)
			{
				case eConnect:
				{
					m_IDTimeOut = GetCurTick() + MIN2MS(1);
					m_ConnectionID = Packet.ReadValue<uint64>(true);	// connection_id
					if(m_ConnectionID)
					{
						Announce(m_PendingEvent);
						m_PendingEvent = eNone;
					}
					else
						SetError("Invalid tracker ID");
					break;
				}
				case eAnnounce:
				{
					m_AnnounceInterval = Packet.ReadValue<uint32>(true);	// interval
					Packet.ReadValue<uint32>(true); // incomplete
					Packet.ReadValue<uint32>(true); // complete

					size_t PeerCount = (Packet.GetSize() - 20) / 6;
					if ((Packet.GetSize() - 20) % 6 != 0)
					{
						SetError("invalid udp tracker response length");
						break;
					}

					TPeerList PeerList;

					for (size_t i = 0; i < PeerCount; ++i)
					{
						CAddress Address(Packet.ReadValue<uint32>(true));
						quint16 Port = Packet.ReadValue<uint16>(true);
						PeerList.append(SPeer(Address, Port));
					}

					emit PeersFound(m_InfoHash, PeerList);

					m_NextAnnounce = GetTime() + m_AnnounceInterval;
					break;
				}
				case eError:
				{
					SetError(QString(Packet.ReadQData()));
					break;
				}
				default: 
					throw CException(LOG_ERROR, L"unknown action flag %u", Action);
			}
		}
		catch(const CException& Exception)
		{
			LogLine(Exception.GetFlag(), tr("recived malformated packet form UDP tracker %1; %2").arg(GetUrl()).arg(QString::fromStdWString(Exception.GetLine())));
		}
	}
}
