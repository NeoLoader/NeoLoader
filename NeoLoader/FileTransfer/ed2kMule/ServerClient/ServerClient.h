#pragma once
//#include "GlobalHeader.h"

#include "../../../../Framework/ObjectEx.h"
#include "../../../../Framework/Buffer.h"
#include "../MuleSocket.h"
#include "../MuleTags.h"
#include "Ed2kServer.h"
class CFile;
struct SSearchRoot;

class CServerClient : public QObjectEx
{
    Q_OBJECT

public:
	CServerClient(CEd2kServer* pServer);
	~CServerClient();

	bool				Connect();
	void				Disconnect();

	uint32				GetClientID()		{return m_ClientID;}

	void				SendLoginRequest();
	//void				SendGetServerList();

	void				RequestSources(CFile* pFile);
	void				FindFiles(const SSearchRoot& SearchRoot);
	void				FindMore();

	void				Process(UINT Tick);

	void				PublishFiles(QList<CFile*> Files);

	void				RequestCallback(uint32 ClientID);
	void				RequestNatCallback(uint32 ClientID);

	void				SendPacket(CBuffer& Packet, uint8 Prot);
	void				ProcessPacket(CBuffer& Packet, uint8 Prot);

	CEd2kServer*		GetServer()			{return qobject_cast<CEd2kServer*>(parent());}

	virtual void			AddLogLine(time_t uStamp, uint32 uFlag, const CLogMsg& Line);


private slots:
	void				OnConnected();
	void				OnDisconnected(int Error);

	void				OnReceivedPacket(QByteArray Packet, quint8 Prot);

signals:
	void				Connected();
	void				Disconnected();

protected:

	CMuleSocket*		m_Socket;

	uint32				m_ClientID;

	uint64				m_LastPublish;
};