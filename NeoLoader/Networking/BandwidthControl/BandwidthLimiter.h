#pragma once

#include "BandwidthCounter.h"
#include "../SocketThread.h"
class CBandwidthLimit;
class CBandwidthManager;
class CSocketThread;

//#define MSS	1300
#define MSS 1500

class CBandwidthLimiter
{
public:
	CBandwidthLimiter(CSocketThread* pSocketThread);
	~CBandwidthLimiter();
	virtual void		Dispose();
	
	enum EChannel
	{
		eUpChannel = 0,
		eDownChannel = 1,
		eCount
	};

	int					GetQuota(UINT Channel);
	void				RequestBandwidth(UINT Channel, int iAmount);
	//virtual void		BandwidthAssigned(UINT Channel) {}
	virtual void		AssignBandwidth(UINT Channel, int iAmount);
	void				CountBandwidth(UINT Channel, int iAmount, CBandwidthCounter::EType Type);

	virtual void		AddLimit(CBandwidthLimit* pUpLimit, UINT Channel);
	virtual void		AddUpLimit(CBandwidthLimit* pUpLimit)		{QMutexLocker Locker(&m_LimitMutex); AddLimit(pUpLimit, eUpChannel);}
	virtual void		AddDownLimit(CBandwidthLimit* pDownLimit)	{QMutexLocker Locker(&m_LimitMutex); AddLimit(pDownLimit, eDownChannel);}

	virtual void		RemoveLimit(CBandwidthLimit* pLimit, UINT Channel);
	virtual void		RemoveUpLimit(CBandwidthLimit* pUpLimit)	{QMutexLocker Locker(&m_LimitMutex); RemoveLimit(pUpLimit, eUpChannel);}
	virtual void		RemoveDownLimit(CBandwidthLimit* pDownLimit){QMutexLocker Locker(&m_LimitMutex); RemoveLimit(pDownLimit, eDownChannel);}

	QMutex*				GetMutex()									{return &m_LimitMutex;}

protected:
	friend class CBandwidthManager;

	QAtomicInt			m_Quota[eCount];
	CBandwidthManager*  m_pManager[eCount];
	QMutex				m_LimitMutex; // this mutex locks the limits
	QList<CBandwidthLimit*>	m_Limits[eCount];
};

////////////////////////////////////////////////////////////////////////////////
//

class CBandwidthProxy: public QIODevice, public CBandwidthLimiter
{
	Q_OBJECT

public:
	CBandwidthProxy(QIODevice* pDevice, CSocketThread* pSocketThread);
	~CBandwidthProxy();

	virtual bool		isSequential() const					{return false;}
    virtual qint64		size() const							{return m_pDevice ? m_pDevice->size() : 0;}
	virtual bool		atEnd() const							{return pos() == size();}

signals:
	void				BytesWritten(qint64 Bytes);
    void				BytesReceived(qint64 Bytes);

protected:
	virtual qint64		readData(char *data, qint64 maxlen);
	virtual qint64		writeData(const char *data, qint64 len);

protected:
	QPointer<QIODevice>	m_pDevice;
};
