#pragma once

#include "../../Framework/ObjectEx.h"
//#include "../FileList/FileList.h"
#include "../FileList/File.h"
#include "../FileTransfer/Transfer.h"
#include "StatusMap.h"


class CFileStats: public QObjectEx
{
	Q_OBJECT

public:
	CFileStats(CFile* pFile);

	//void							Process(UINT Tick);

	void							SetupAvailMap();

	void							AddTransfer(CTransfer* pTransfer);
	void							RemoveTransfer(CTransfer* pTransfer);

	CAvailMap*						GetAvailMap()			{return m_Availability.data();}
#ifndef NO_HOSTERS
	CCacheMap*						GetCacheMap()			{return m_HosterCache.data();}
	CHosterMap*						GetHosterMap()			{return m_HosterMap.data();}
#endif 

	void							UpdateAvail(CTransfer* pTransfer, const CAvailDiff& AvailDiff, bool b1st = false);

	double							GetAvailStats();
	double							GetAvailStatsRaw(bool bUpdate = false);
	int								GetTransferCount(ETransferType eType);

	struct STemp
	{
		int Checked;
		int Connected;
		int Complete;
		int All;
	};
	STemp&							GetTemp(ETransferType eType)			{return m_TempMap[eType];}
	void							UpdateTempMap(QMap<ETransferType, STemp>& TempMap)	{m_TempMap = TempMap;}
	STemp							GetTempCount(ETransferType eType);

#ifndef NO_HOSTERS
	void							UpdateHosters()			{m_HosterShare.clear();}

	bool							HasHosters();

	QMap<QString, QMap<QString, double> > GetHostingInfo();
	QStringList						GetHosterList();
#endif

	CFile*							GetFile() const			{CFile* pFile = qobject_cast<CFile*>(parent()); ASSERT(pFile); return pFile;}

protected:
#ifndef NO_HOSTERS
	void							AddRange(uint64 uBegin, uint64 uEnd, bool bTest, bool bUpdateHosted, bool bUpdateCache, CHosterLink* pHosterLink);
	void							DelRange(uint64 uBegin, uint64 uEnd, bool bTest, bool bUpdateHosted, bool bUpdateCache, CHosterLink* pHosterLink);
#endif

	CAvailMapPtr					m_Availability;
#ifndef NO_HOSTERS
	CCacheMapPtr					m_HosterCache;
	CHosterMapPtr					m_HosterMap;
	QMap<QString, int>				m_HosterCount;
#endif

	struct SAvailStat
	{
		SAvailStat() 
		 : Availability(0)
		 , uRevision(0), uInvalidate(0) {}

		double Availability;

		uint64 uRevision;
		uint64 uInvalidate;
	};
	SAvailStat						m_AvailStat;

	QMap<ETransferType, int>		m_CountMap;
	QMap<ETransferType, STemp>		m_TempMap;
#ifndef NO_HOSTERS
	QMap<QString, QMap<QString, double> > m_HosterShare;
#endif
};
