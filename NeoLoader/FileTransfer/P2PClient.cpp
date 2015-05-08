#include "GlobalHeader.h"
#include "P2PClient.h"
#include "Transfer.h"
#include "../Networking/BandwidthControl/BandwidthLimit.h"

CP2PClient::CP2PClient(QObject* qObject)
 : QObjectEx(qObject) 
{
	m_UpLimit = new CBandwidthLimit(this);
	m_DownLimit = new CBandwidthLimit(this);
}

CP2PClient::~CP2PClient() 
{
}

QString CP2PClient::GetTypeStr()
{
	return CTransfer::GetTypeStr(this);
}