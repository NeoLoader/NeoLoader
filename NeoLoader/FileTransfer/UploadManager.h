#pragma once

#include "../../Framework/ObjectEx.h"

class CFile;
class CArchiveThread;
class CPartMap;
struct SCachePart;
class CCacheMap;

#include "Transfer.h"

class CUploadManager: public QObjectEx
{
	Q_OBJECT

public:
	CUploadManager(QObject* qObject = NULL);
	~CUploadManager() {}

	void							Process(UINT Tick);

	double							GetProbabilityRange()			{if(m_ProbabilityRange == 0) return 1; 
																	return m_ProbabilityRange;}
	uint64							GetAverageStartTime()			{if(m_StartHistory.count() < 2) return MIN2MS(1); 
																	return (m_StartHistory.last() - m_StartHistory.first()) / m_StartHistory.size();}

#ifndef NO_HOSTERS
	bool							OrderUpload(CFile* pFile, QStringList Hosts, CPartMap* pMap = NULL);
	bool							OrderSolUpload(CFile* pFile, QStringList Hosts);
	bool							OrderReUpload(CHosterLink* pHosterLink);
	bool							UploadsPending(CFile* pFile);

	uint64							GetPartSizeForHost(const QString& Host);
	bool							NativeCryptoHost(const QString& Host);
#endif

	bool							OfferHorde(CP2PTransfer* pP2PTransfer);
	bool							AcceptHorde(CP2PTransfer* pP2PTransfer);
	bool							AcceptHorde();

	int								GetActiveCount()				{return m_Uploads.count();}
	int								GetWaitingCount()				{return m_WaitingUploads;}

	const QList<QPointer<CTransfer> >& GetTransfers()				{return m_Uploads;}

	struct SUploadSlot
	{
		enum EType
		{
			eTrickle = 0,
			eFull,
			eFocus,
			eBlocking
		};

		SUploadSlot(CP2PTransfer* pSlot = 0)
		{
			pTransfer = pSlot;

			eType = eTrickle;

			StalledTimeOut = -1;
			BlockingTimeOut = -1;
			StrikeCount = 0;
		}

		CP2PTransfer*	pTransfer;

		uint64			StalledTimeOut;
		uint64			BlockingTimeOut;
		int				StrikeCount;

		EType			eType;
	};

protected:
#ifndef NO_HOSTERS
	void							InspectRange(QMultiMap<int, SCachePart>& CacheQueue, uint64 uBegin, uint64 uEnd, bool bAvail, uint32 uCached, uint32 uNeeded, uint64 FileSize, uint64 PartSize);
	void							InspectCache(CFile* pFile);
	QString							SelectUploadHost(CFile* pFile, CShareMap* pPartMap);
#endif

	int								m_WaitingUploads;
	uint64							m_NextUploadStart;
	uint64							m_LastUploadDowngrade;
	uint64							m_LastUploadUpgrade;
	int								m_LastUpLimit;
	int								m_LastFullSlotCount;
	int								m_LastTricklSlotCount;

	double							m_ProbabilityRange;
	QList<uint64>					m_StartHistory;

	QList<QPointer<CTransfer> >		m_Uploads;

	QVector<SUploadSlot>			m_UploadSlots;
};
