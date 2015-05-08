#pragma once

#include "../../Framework/ObjectEx.h"
//#include "../FileList/FileList.h"
#include "../FileList/File.h"

class CPartDownloader: public QObjectEx
{
	Q_OBJECT

public:
	CPartDownloader(CFile* pFile);

	//void							Process(UINT Tick);

	struct SPart
	{
		SPart(uint64 Begin = 0, uint64 End = 0)
		: uBegin(Begin), uEnd(End), iNumber(-1), bStream(false) {}
		uint64 uBegin;
		uint64 uEnd;
		int iNumber;
		bool bStream;
	};

	const QVector<SPart>&			GetDownloadPlan(CFileHash* pHash);

	CFile*							GetFile() const			{CFile* pFile = qobject_cast<CFile*>(parent()); ASSERT(pFile); return pFile;}


public slots:
	void							OnChange(bool Purge);

protected:

	struct SDownloadPlan
	{
		SDownloadPlan() 
		 : uRevision(0), uInvalidate(0), bWasAllocating(false), Type(HashUnknown) {}

		QVector<SPart>		Plan;

		uint32 uRevision;
		uint64 uInvalidate;
		bool bWasAllocating;
		EFileHashType Type;
	};
	QMap<QByteArray, SDownloadPlan>		m_DownloadPlans;

	void							UpdatePlan(CFileHash* pHash, SDownloadPlan& DownloadPlan, bool bNow = false);
};
