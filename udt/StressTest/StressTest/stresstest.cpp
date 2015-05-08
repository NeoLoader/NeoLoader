#include "stresstest.h"



StressTest::StressTest(QWidget *parent, Qt::WFlags flags)
	: QMainWindow(parent, flags)
{
	m_uTimerID = startTimer(10);
	m_uTimerCounter = 0;
	m_Timer.start();


	m_SvrBasePort = 10000;
	m_TestRange = 100;
	m_TestTime = 10000000;
	m_SvrPortCounter = m_SvrBasePort;

	UDT::startup();
}

StressTest::~StressTest()
{
	killTimer(m_uTimerID);

	UDT::cleanup();
}

void StressTest::Process()
{
	qint64 uNow = m_Timer.elapsed();

	QList<UDTSOCKET> Servers = m_Servers.keys();
	for(int i=0; i < m_Servers.count() ; i++)
	{
		UDTSOCKET Server = Servers.at(i);

		for(;;)
		{
			sockaddr sa; // sockaddr_in is smaller
			int sa_len = sizeof(sa); 
			UDTSOCKET Client = UDT::accept(Server, (sockaddr*)&sa, &sa_len);
			if (UDT::INVALID_SOCK == Client)
				break;
			else if(Client == NULL)
				break;

			bool blocking = false;
			UDT::setsockopt(Client, 0, UDT_RCVSYN, &blocking, sizeof(blocking));
			UDT::setsockopt(Client, 0, UDT_SNDSYN, &blocking, sizeof(blocking));

			SInfo Info;
			Info.StartTime = uNow;
			Info.Connected = true;
			m_Clients.insert(Client, Info);

			printf("accepted connection\r\n");
		}
	}

	QList<UDTSOCKET> Clients = m_Clients.keys();
	for(int i=0; i < Clients.count() ; i++)
	{
		UDTSOCKET Client = Clients.at(i);

		UDTSTATUS Value = UDT::getsockstate(Client);
		if(Value == CONNECTING && !(uNow - m_Clients.value(Client).StartTime > m_TestTime))
			continue;
		else if(Value != CONNECTED || (uNow - m_Clients.value(Client).StartTime > m_TestTime))
		{
			UDT::close(Client);
			m_Clients.remove(Client);

			printf("closed connection\r\n");
		}
		else if(!m_Clients.value(Client).Connected)
		{
			UDT::send(Client, ">>>> bla blup", 14 , 0);
			/*static char* x = NULL;
			if(!x)
				x = new char[1024*1024];
			UDT::send(Client, x, 1024*1024 , 0);*/
			m_Clients[Client].Connected = true;
			printf("established connection\r\n");
		}
		else
		{
			const qint64 Size = 16 * 1024;
			char Buffer[Size];
			int Recived = UDT::recv(Client, (char*)Buffer, Size, 0);
			if (UDT::ERROR != Recived && Recived > 0)
			{
				Buffer[Recived] = 0;
				//printf("resived %s\r\n", Buffer);	
			}
			//UDT::send(Client, "<<<< bla blup", 14 , 0);
		}
	}

	m_uTimerCounter++;
	if((m_uTimerCounter % 10) != 0) // 10 times a second
		return;
	m_uTimerCounter = 0;

	if(m_Servers.count() < m_TestRange)
	{
		sockaddr* sa = (sockaddr*)new sockaddr_in;
		memset(sa, 0, sizeof(sockaddr_in));
		((sockaddr_in*)sa)->sin_family = AF_INET;
		((sockaddr_in*)sa)->sin_addr.s_addr = INADDR_ANY;	
		((sockaddr_in*)sa)->sin_port = htons((u_short)m_SvrPortCounter++);

		UDTSOCKET Server = UDT::socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

		bool reuse_addr = true; // Note: this is true by default anyways
		UDT::setsockopt(Server, 0, UDT_REUSEADDR, &reuse_addr, sizeof(reuse_addr));
		if (UDT::ERROR == UDT::bind(Server, sa, sizeof(sockaddr_in)))
			return;
		
		if (UDT::ERROR == UDT::listen(Server, 1024))
		{
			UDT::close(Server);
			return;
		}
		
		bool blockng = false;
		UDT::setsockopt(Server, 0, UDT_RCVSYN, &blockng, sizeof(blockng));

		m_Servers.insert(Server, sa);

		printf("started listening\r\n");

		return;
	}

	for(int i=0; i < 10; i++)
	{
		int port = m_SvrBasePort + (qrand() % m_TestRange);
	if(m_Clients.count() < m_TestRange*10)
	{
		

		UDTSOCKET Client = UDT::socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

		bool blocking = false;
		UDT::setsockopt(Client, 0, UDT_RCVSYN, &blocking, sizeof(blocking));
		UDT::setsockopt(Client, 0, UDT_SNDSYN, &blocking, sizeof(blocking));

		sockaddr* my_sa = m_Servers.values().at(qrand() % m_Servers.count());

		bool reuse_addr = true; // Note: this is true by default anyways
		UDT::setsockopt(Client, 0, UDT_REUSEADDR, &reuse_addr, sizeof(reuse_addr));

		if((qrand()%2) == 0)
		{
			bool value = true;
			UDT::setsockopt(Client, 0, UDT_RENDEZVOUS, &value, sizeof(value));
		}

		if (UDT::ERROR == UDT::bind(Client, my_sa, sizeof(sockaddr_in)))
			return;
		
		sockaddr* sa = (sockaddr*)new sockaddr_in;
		memset(sa, 0, sizeof(sockaddr_in));
		((sockaddr_in*)sa)->sin_family = AF_INET;
		((sockaddr_in*)sa)->sin_addr.S_un.S_addr = inet_addr("10.70.0.14");
		((sockaddr_in*)sa)->sin_port = htons((u_short)port);

		if (UDT::ERROR == UDT::connect(Client, (sockaddr*)sa, sizeof(sockaddr_in)))
			return;

		delete sa;

		SInfo Info;
		Info.StartTime = uNow;
		Info.Connected = false;
		m_Clients.insert(Client, Info);

		printf("opened connection %i\r\n", port);
	}
	}
}