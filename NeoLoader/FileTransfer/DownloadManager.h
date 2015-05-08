#pragma once

#include "../../Framework/ObjectEx.h"

class CFile;

#include "Transfer.h"

class CDownloadManager: public QObjectEx
{
public:
	CDownloadManager(QObject* qObject = NULL);
	~CDownloadManager() {}

	void							Process(UINT Tick);
	void							UpdateQueue();

	const QList<QPointer<CTransfer> >& GetTransfers()					{return m_Downloads;}

	int								GetActiveCount()				{return m_Downloads.count();}
	int								GetWaitingCount()				{return m_WaitingDownloads;}

protected:

	int								m_WaitingDownloads;

	QList<QPointer<CTransfer> >		m_Downloads;
};