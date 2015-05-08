#include "GlobalHeader.h"
#include "BandwidthLimit.h"
#include "BandwidthLimiter.h"
#include "BandwidthManager.h"
#include "../SocketThread.h"
#include "../../NeoCore.h"

CBandwidthLimiter::CBandwidthLimiter(CSocketThread* pSocketThread)
{
	ASSERT(pSocketThread);

	m_pManager[eUpChannel] = pSocketThread->GetUpManager();
	m_pManager[eDownChannel] = pSocketThread->GetDownManager();

	// Install default global bandwidth limit
	AddLimit(pSocketThread->GetUpLimit(), eUpChannel);
	AddLimit(pSocketThread->GetDownLimit(), eDownChannel);
}

CBandwidthLimiter::~CBandwidthLimiter()
{
	ASSERT(m_Limits[eUpChannel].isEmpty());
	ASSERT(m_Limits[eDownChannel].isEmpty());
}

void CBandwidthLimiter::Dispose()
{
	for(int i=0; i < eCount; i++)
	{
		m_pManager[i]->ReturnBandwidth(this);

		for(QList<CBandwidthLimit*>::iterator I = m_Limits[i].begin(); I != m_Limits[i].end(); I = m_Limits[i].erase(I))
			(*I)->Remove(this, i);
	}
}

int	CBandwidthLimiter::GetQuota(UINT Channel)
{
#if QT_VERSION < 0x050000
	return m_Quota[Channel];
#else
	return m_Quota[Channel].load();
#endif
}

void CBandwidthLimiter::RequestBandwidth(UINT Channel, int iAmount)
{
	QMutexLocker Locker(&m_LimitMutex); 

	int Quota = GetQuota(Channel);
	if(Quota < iAmount)
		m_pManager[Channel]->RequestBandwidth(m_Limits[Channel], this, iAmount - Quota);
}

void CBandwidthLimiter::AssignBandwidth(UINT Channel, int iAmount)
{
	m_Quota[Channel].fetchAndAddOrdered(iAmount);
};

void CBandwidthLimiter::CountBandwidth(UINT Channel, int iAmount, CBandwidthCounter::EType Type)
{
	QMutexLocker Locker(&m_LimitMutex); 

	if(theCore->m_Network->UseTransportLimiting() || Type >= CBandwidthCounter::eProtocol)
		m_Quota[Channel].fetchAndAddOrdered(-iAmount);

	foreach(CBandwidthLimit* pLimit, m_Limits[Channel])
		pLimit->CountBytes(iAmount, Type);

	int Quota = GetQuota(Channel);
	if(Quota < 0) // if we are in det request imminetly
		m_pManager[Channel]->RequestBandwidth(m_Limits[Channel], this, -Quota);
}

void CBandwidthLimiter::AddLimit(CBandwidthLimit* pLimit, UINT Channel)
{
	// Note: the mutex must already be locked
	pLimit->Add(this, Channel);
	m_Limits[Channel].push_back(pLimit);
	m_pManager[Channel]->UpdateLimits(m_Limits[Channel], this);
}

void CBandwidthLimiter::RemoveLimit(CBandwidthLimit* pLimit, UINT Channel)
{
	// Note: the mutex must already be locked
	QList<CBandwidthLimit*>::iterator I = find(m_Limits[Channel].begin(), m_Limits[Channel].end(), pLimit);
	if(I != m_Limits[Channel].end())
	{
		pLimit->Remove(this, Channel);
		m_Limits[Channel].erase(I);
		m_pManager[Channel]->UpdateLimits(m_Limits[Channel], this);
	}
}

////////////////////////////////////////////////////////////////////////////////
//

CBandwidthProxy::CBandwidthProxy(QIODevice* pDevice, CSocketThread* pSocketThread)
: CBandwidthLimiter(pSocketThread)
{
	ASSERT(pDevice->isOpen());
	m_pDevice = pDevice;
	m_pDevice->seek(0);
}

CBandwidthProxy::~CBandwidthProxy()
{
	CBandwidthLimiter::Dispose();
}

#ifdef MSS
void AddTCPOverhead(CBandwidthProxy* pProxy, uint64 uSize, CBandwidthCounter::EType TypeDown, CBandwidthCounter::EType TypeUp)
{
	// Note: for now we assume always IPv4 for hosters overhead is not that important anyways
	uint64 FrameOH = 20 + 20; // IPv4 Header + TCP Header

	// Note: TCP does not fragment frames, so sach frame is IP Header + TCP Header
	uint64 uMSS = MSS - FrameOH;
	int iFrames = (uSize + (uMSS-1))/uMSS; // count if IP frames

	FrameOH += theCore->m_Network->GetFrameOverhead(); // Ethernet
	pProxy->CountBandwidth(CBandwidthLimiter::eDownChannel, iFrames * FrameOH, TypeDown);
	pProxy->CountBandwidth(CBandwidthLimiter::eUpChannel, iFrames * FrameOH, TypeUp);
}
#endif

qint64 CBandwidthProxy::readData(char *data, qint64 maxlen)
{
	if(!m_pDevice)
		return -1;

	RequestBandwidth(CBandwidthLimiter::eUpChannel, Min(maxlen, m_pDevice->bytesAvailable()));

	qint64 uToRead = Min(maxlen, GetQuota(CBandwidthLimiter::eUpChannel));
	if(uToRead <= 0)
		return 0;
	if(m_pDevice->pos() != pos())
		m_pDevice->seek(pos());
	qint64 uRead = m_pDevice->read(data, uToRead);
	if(uRead == -1)
		return -1;
	ASSERT(uToRead >= uRead);

	CountBandwidth(CBandwidthLimiter::eUpChannel, uRead, CBandwidthCounter::ePayload);
#ifdef MSS
	AddTCPOverhead(this, uRead, CBandwidthCounter::eAck, CBandwidthCounter::eHeader);
#endif

	emit BytesWritten(uRead);
	return uRead;
}

qint64 CBandwidthProxy::writeData(const char *data, qint64 len)	
{
	if(!m_pDevice)
		return -1;

	RequestBandwidth(CBandwidthLimiter::eDownChannel, len);

	qint64 uToWrite = Min(len, GetQuota(CBandwidthLimiter::eDownChannel));
	if(uToWrite <= 0)
		return 0;
	if(m_pDevice->pos() != pos())
		m_pDevice->seek(pos());
	qint64 uWriten = m_pDevice->write(data, uToWrite);
	if(uWriten == -1)
		return -1;
	ASSERT(uToWrite >= uWriten);

	CountBandwidth(CBandwidthLimiter::eDownChannel, uWriten, CBandwidthCounter::ePayload);
#ifdef MSS
	AddTCPOverhead(this, uWriten, CBandwidthCounter::eHeader, CBandwidthCounter::eAck);
#endif

	emit BytesReceived(uWriten);
	return uWriten;
}
