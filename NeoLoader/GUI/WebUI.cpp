#include "GlobalHeader.h"
#include "../NeoCore.h"
#include "WebUI.h"
#include "../Interface/WebRoot.h"
#include "../Interface/CoreServer.h"
#include "../../Framework/HttpServer/HttpSocket.h"
#include "../FileList/FileManager.h"
#include "../FileList/Hashing/HashingThread.h"
#include "../FileTransfer/FileGrabber.h"
#ifndef NO_HOSTERS
#include "../FileTransfer/HosterTransfer/HosterLink.h"
#include "../FileTransfer/HosterTransfer/ArchiveDownloader.h"
#include "../FileTransfer/HosterTransfer/ArchiveSet.h"
#endif
#include "../FileTransfer/BitTorrent/Torrent.h"
#include "../FileTransfer/Transfer.h"
#include "../FileTransfer/PartDownloader.h"
#include "../FileList/File.h"
#include "../FileList/FileStats.h"
#include "BarShader.h"
#include <QPainter>

CWebUI::CWebUI(QObject* qObject)
: QObjectEx(qObject)
{
	theCore->m_HttpServer->RegisterHandler(this,"/WebUI");

	m_WebUIPath = theCore->Cfg(false)->GetString("HttpServer/WebUIPath");
	if(m_WebUIPath.isEmpty())
		m_WebUIPath = theCore->Cfg()->GetAppDir() + "/WebUI";
#ifdef USE_7Z
	m_WebUI = NULL;
	if(!QFile::exists(m_WebUIPath))
	{
		m_WebUI = new CCachedArchive(theCore->Cfg()->GetAppDir() + "/WebUI.7z");
		if(!m_WebUI->Open())
		{
			LogLine(LOG_ERROR, tr("WebUI.7z archive is damaged or missing!"));
			delete m_WebUI;
			m_WebUI = NULL;
		}
	}
#endif
}

CWebUI::~CWebUI()
{
#ifdef USE_7Z
	if(m_WebUI)
	{
		m_WebUI->Close();
		delete m_WebUI;
	}
#endif
}

template <class T>
bool IterateRanges(uint64 PixelSize, T* pMap, typename T::SIterator& SrcIter, uint64 uFileSize)
{
	if(SrcIter.uEnd >= uFileSize)
		return false;

	SrcIter.uBegin = SrcIter.uEnd;
	SrcIter.uEnd += PixelSize;
	if(SrcIter.uEnd > uFileSize)
		SrcIter.uEnd = uFileSize;
	SrcIter.uState = pMap->GetRange(SrcIter.uBegin, SrcIter.uEnd, T::eUnion);

	return true;
}

void FillAvailBar(uint64 PixelSize, CAvailMap &Avail, uint64 uFileSize, uint32 uCompleteAvail, bool IsComplete, CBarShader &BarShader)
{
	CAvailMap::SIterator AvailIter;
	while(PixelSize ? IterateRanges(PixelSize, &Avail, AvailIter, uFileSize) : Avail.IterateRanges(AvailIter))
	{
		uint32 uAvail = AvailIter.uState;
		if(!uAvail)
			continue;
				
		if(uCompleteAvail && uCompleteAvail != -1)
		{
			if(uAvail > uCompleteAvail)
				uAvail -= uCompleteAvail;
			else // uCompleteAvail is out of date
				uAvail = 1;
		}

		if(IsComplete)
			BarShader.FillRange(AvailIter.uBegin, AvailIter.uEnd, qRgb(0, (uAvail > 5) ? 244 : 114 + 22*uAvail, 0));
		else 
			BarShader.FillRange(AvailIter.uBegin, AvailIter.uEnd, qRgb(0, (uAvail > 10) ? 0 : 210 - 22*(uAvail - 1), 255));
	}
}

void CWebUI::GetProgress(QIODevice* pDevice, const QString& sMode, uint64 ID, int iWidth, int iHeight, int iDepth)
{
	QImage Progress(iWidth, iHeight, QImage::Format_RGB32);

	CFile* pFile = CFileList::GetFile(ID);
	if(!pFile)
	{
		QPainter Painter;
		Painter.begin(&Progress);
		Painter.fillRect(QRect(0,0,Progress.width(),Progress.height()), Qt::gray);
		Painter.end();

#ifdef _DEBUG
		Progress.save(pDevice, "bmp");
#else
		Progress.save(pDevice, "png");
#endif
		return; 
	}


	StrPair MM = Split2(sMode, "-");

	/*enum eMode
	{
		ePartFile = 1,
		eArchive = 2,
		eBooth   = 3
	}	Mode = eBooth;
	if(MM.first == "Archive")
		Mode = eArchive;
	else if(MM.first == "PartFile")
		Mode = ePartFile;
	else
	{
		if(pFile && !pTransfer && pFile->IsRawArchive())
			Mode = eArchive;
	}*/

	enum eSubMode
	{
		eProgress = 1,
		ePlan = 2,
		eNone   = 3
	}	SubMode = eProgress;
	if(MM.second == "Plan")
		SubMode = ePlan;
	else if(MM.second == "NoProgress")
		SubMode = eNone;


	if(iWidth == 0)	iWidth = 300;
	if(iHeight == 0)iHeight = 12;

	uint cAvailable = iDepth ? qRgb(134, 134, 134) : qRgb(70, 70, 70);
	uint cVerified = iDepth ? qRgb(104, 104, 104) : qRgb(50, 50, 50);
	uint cConfirmed = qRgb(0, 224, 0);
	uint cScheduled = qRgb(255, 210, 0);
	uint cMarked = qRgb(128, 0, 255);
	uint cRequested = qRgb(255, 127, 0);
	uint cCached = qRgb(127, 210, 0);
	uint cHave = qRgb(0, 0, 160);
	uint cMissing = qRgb(255, 0, 0);
	uint cUnavailable = qRgb(224, 224, 224);
	uint cDisabled = qRgb(136, 0, 21);
	uint cEmpty = qRgb(224, 224, 224);

	uint64 uFileSize = pFile->GetFileSize();
	if(uFileSize == 0)
		uFileSize = pFile->GetProperty("EstimatedSize").toULongLong();

	bool IsComplete = pFile->IsComplete(true);

	uint32 uCompleteAvail = pFile->GetStats()->GetAvailStatsRaw(true);

	uint64 PixelSize = 0;
#ifdef _DEBUG
	PixelSize = DivUp(uFileSize, iWidth*4);
	ASSERT(PixelSize >= 0);
#endif

	if(pFile->HasError())
	{
		QPainter Painter;
		Painter.begin(&Progress);
		Painter.fillRect(QRect(0,0,Progress.width(),Progress.height()),QBrush(QImage(":/ErrPattern")));
		Painter.end();
	}
	else if(uFileSize == 0)
	{
		CBarShader BarShader(1, iWidth, iHeight, iDepth);
		BarShader.Fill(cEmpty);
		BarShader.DrawImage(Progress);
	}
	else
	{
		///////////////////////////////////////////////////////////////
		// Draw File Status
		//

		CBarShader BarShader(uFileSize, iWidth, iHeight, iDepth);
		BarShader.Fill(IsComplete ? cVerified : cMissing);
		////

#ifndef NO_HOSTERS
		// Archive Avail Calculation BEGIN
		CPartMap Status(uFileSize);
		CAvailMap Avail(uFileSize);
		const QMap<QByteArray, CArchiveSet*>& Archives = pFile->GetArchives();

		bool bIsArchive = pFile->IsArchive();

		if(bIsArchive)
		{
			foreach(CArchiveSet* pArchive, Archives)
			{
				if(pArchive->GetTotalSize() == 0)
					continue; // added links and no check has yet been performed

				double Stretch = (double)uFileSize / pArchive->GetTotalSize();

				uint64 uBegin = 0;
				uint64 uEnd = 0;
				for(int Part = 1; Part <= pArchive->GetPartCount(); Part++)
				{
					uint64 uPartSize = 0;
					uint32 uCount = 0;

					bool bCompleted = false;
					bool bRequested = false;
					uint64 uDownloaded = 0;
					foreach(CHosterLink* pHosterLink, pArchive->GetAvailable(Part))
					{
						if(!pFile->GetTransfers().contains(pHosterLink) || pHosterLink->HasError())
							continue;

						uCount++;
						if(pHosterLink->GetFileSize() > uPartSize)
							uPartSize = pHosterLink->GetFileSize();

						switch(pHosterLink->GetStatus(CHosterLink::eDown))
						{
							case CHosterLink::eTransferred:	// successfuly completed
								bCompleted = true;
								break;
							case CHosterLink::eTransfering:	// active waiting for transfer or transfering
								//ASSERT(bRequested == false); // there should be only one request per part at any one time
								bRequested = true;
								break;
						}
						uDownloaded = pHosterLink->GetTransferredSize(CHosterLink::eDown); 
					}

					bool bCached = false;
					uint64 uUploaded = 0;
					foreach(CHosterLink* pHosterLink, pArchive->GetPending(Part))
					{
						if(!pFile->GetTransfers().contains(pHosterLink))
							continue;

						if(pHosterLink->GetFileSize() > uPartSize)
							uPartSize = pHosterLink->GetFileSize();

						switch(pHosterLink->GetStatus(CHosterLink::eUp))
						{
							case CHosterLink::eTransfering:	// active waiting for transfer or transfering
								//ASSERT(bCached == false); // there should be only one request per part at any one time
								bCached = true;
								uUploaded = pHosterLink->GetTransferredSize(CHosterLink::eUp); 
								break;
						}
					}

					uEnd += pArchive->GetPartSize() * Stretch;
					if(uEnd > uFileSize)
						uEnd = uFileSize;

					if(uBegin < uEnd)
						Avail.SetRange(uBegin, uEnd, uCount, CAvailMap::eAdd);

					if(bCompleted)
						Status.SetRange(uBegin, uEnd, Part::Verified);

					if(bCached && uUploaded > 0)
					{
						uint64 uUlEnd = uBegin + uUploaded;
						if(uUlEnd > uFileSize)
							uUlEnd = uFileSize;
						Avail.SetRange(uBegin, uUlEnd, 1, CAvailMap::eAdd);
					}
					if(bRequested)
						Status.SetRange(uBegin, uEnd, Part::Scheduled);
					if(uDownloaded > 0)
					{
						uint64 uDlEnd = uBegin + uDownloaded;
						if(uDlEnd > uFileSize)
							uDlEnd = uFileSize;
						Status.SetRange(uBegin, uDlEnd, Part::Available);
					}

					uBegin = uEnd;
				}
			}
		}
		// Archive Avail Calculation END

		// Fill source informations
		if(CAvailMap* pAvailMap = pFile->GetStats()->GetAvailMap())
		{
			if(Archives.isEmpty())
				Avail.Assign(pAvailMap);
			else
				Avail.Merge(pAvailMap);
		}

		//CAvailMap* pAvail = NULL;
		CPartMap* pStatus = NULL;
		if(bIsArchive)
		{
			//pAvail = &Avail;
			pStatus = &Status;
		}
		else 
		{
			//pAvail = pFile->GetStats()->GetAvailMap();
			pStatus = pFile->GetPartMap();
		}
#else
		CAvailMap Avail(uFileSize);

		if(CAvailMap* pAvailMap = pFile->GetStats()->GetAvailMap())
			Avail.Assign(pAvailMap);

		//CAvailMap* pAvail = NULL;
		CPartMap* pStatus = pFile->GetPartMap();
#endif

		FillAvailBar(PixelSize, Avail, uFileSize, uCompleteAvail, IsComplete, BarShader);

		// Fill file informations
		if(pStatus && !pFile->IsComplete())
		{
			if(!IsComplete) // this is the complete value from IsCompleteAux
			{
				CPartMap::SIterator FileIter;
				while(PixelSize ? IterateRanges(PixelSize, pStatus, FileIter, uFileSize) : pStatus->IterateRanges(FileIter))
				{
					if((FileIter.uState & Part::Verified) != 0)
						BarShader.FillRange(FileIter.uBegin, FileIter.uEnd, cVerified);
					else if((FileIter.uState & Part::Available) != 0)
						BarShader.FillRange(FileIter.uBegin, FileIter.uEnd, cAvailable);
					else if((FileIter.uState & Part::Requested) != 0)
						BarShader.FillRange(FileIter.uBegin, FileIter.uEnd, cRequested);
					else if((FileIter.uState & Part::Scheduled) != 0)
						BarShader.FillRange(FileIter.uBegin, FileIter.uEnd, cScheduled);
					else if((FileIter.uState & Part::Marked) != 0)
						BarShader.FillRange(FileIter.uBegin, FileIter.uEnd, cMarked);
					else if((FileIter.uState & Part::Disabled) != 0)
						BarShader.FillRange(FileIter.uBegin, FileIter.uEnd, cDisabled);
				}
			}
			else // special case for multifiles with halted subfiles
			{
				CPartMap::SIterator FileIter;
				while(PixelSize ? IterateRanges(PixelSize, pStatus, FileIter, uFileSize) : pStatus->IterateRanges(FileIter))
				{
					if((FileIter.uState & Part::Disabled) != 0)
					{
						if((FileIter.uState & Part::Verified) != 0)
							BarShader.FillRange(FileIter.uBegin, FileIter.uEnd, cVerified);
						else if((FileIter.uState & Part::Available) != 0)
							BarShader.FillRange(FileIter.uBegin, FileIter.uEnd, cAvailable);
						else 
							BarShader.FillRange(FileIter.uBegin, FileIter.uEnd, cDisabled);
					}
				}
			}
		}

		////
		BarShader.DrawImage(Progress);

		//////////// Sub Bars ...

		if(!IsComplete)
		{
			// Not so small top progress sub bar
#ifndef NO_HOSTERS
			if(SubMode == ePlan && pFile->IsStarted() && pFile->IsIncomplete() && !pFile->IsRawArchive() && pFile->GetPartMap() != NULL)
#else
			if(SubMode == ePlan && pFile->IsStarted() && pFile->IsIncomplete() && pFile->GetPartMap() != NULL)
#endif
			{
				CValueMap<uint32> PlanMap(uFileSize);
				EFileHashType Types[3] = {HashNeo, HashTorrent, HashEd2k};
				for(int i=0; i < ARRSIZE(Types); i++)
				{
					CFileHash* pHash = NULL;
					EFileHashType eType = Types[i];
					if(eType == HashNeo && pFile->IsMultiFile())
						eType = HashXNeo;
					if(eType != HashTorrent)
						pHash = pFile->GetHash(eType);
					else if(CTorrent* pTorrent = pFile->GetTopTorrent())
						pHash = pTorrent->GetHash().data();
					if(!pHash)
						continue;

					QVector<CPartDownloader::SPart> Plan = pFile->GetDownloader()->GetDownloadPlan(pHash);
					int Index = 0;
					for(QVector<CPartDownloader::SPart>::const_iterator I = Plan.constBegin(); I != Plan.constEnd(); I++, Index++) // Note: each range is exactly one entire part/piece/block
					{
						uint32 Color;
						if(Index <= 8)
							Color = 63 + 128 + ((8 - Index) * 8); // + 8 - 64
						else
							Color = 63 + ((Plan.count() - Index) * 128 / Plan.count());
						uint32 Colors = (Color & 0xFF) << (i * 8);

						PlanMap.SetRange(I->uBegin, I->uEnd, Colors, CValueMap<uint32>::eAdd);
					}
				}

				CBarShader BarShader(uFileSize, iWidth, iHeight/3, iDepth);
				BarShader.Fill(qRgb(63, 63, 63));
				CValueMap<uint32>::SIterator PlanIter;
				while(PlanMap.IterateRanges(PlanIter))
				{
					if(PlanIter.uState)
						BarShader.FillRange(PlanIter.uBegin, PlanIter.uEnd, PlanIter.uState);
				}
				BarShader.DrawImage(Progress);
			}

			// small top progress sub bar
			if(SubMode == eProgress)
			{
				// Fill progress informations
				uint64 uAvailable = pFile->GetStatusStats(Part::Available); // pFile->DownloadedBytes();
				uint64 uVerifyed = pFile->GetStatusStats(Part::Verified);
				uint64 uRequested = pFile->GetStatusStats(Part::Requested);
				uint64 uHashed = theCore->m_Hashing->GetProgress(pFile);
				if(uHashed == -1)
					uHashed = 0;
				uint64 uAllocated = pFile->GetStatusStats(Part::NotAvailable);
				int iProgress = (double)iWidth*(max(uAllocated, max(uHashed, uAvailable+uRequested)))/uFileSize;
				//int iProgress = (double)iWidth*(max(uHashed, uAvailable+uRequested))/uFileSize;
				if(iProgress > iWidth)
					iProgress = iWidth;
				CBarShader BarShader(max(uAllocated, max(uHashed, max(uAvailable, uVerifyed)+uRequested)),iProgress,iHeight/4,iDepth);
				//CBarShader BarShader(max(uHashed, max(uAvailable, uVerifyed)+uRequested),iProgress,iHeight/4,iDepth);
				BarShader.Fill(cRequested);
				BarShader.FillRange(0, uVerifyed, cConfirmed);
				BarShader.FillRange(uVerifyed, max(uAvailable, uVerifyed), cScheduled);
				BarShader.FillRange(0, uHashed, qRgb(0, 210, 255));
				BarShader.FillRange(0, uAllocated, qRgb(255, 0, 255));
				BarShader.DrawImage(Progress);
			}
		}
	}

#ifdef _DEBUG
	Progress.save(pDevice, "bmp");
#else
	Progress.save(pDevice, "png");
#endif
}

void CWebUI::GetProgress(QIODevice* pDevice, uint64 ID, uint64 SubID, int iWidth, int iHeight, int iDepth)
{
	QImage Progress(iWidth, iHeight, QImage::Format_RGB32);

	CFile* pFile = CFileList::GetFile(ID);
	CTransfer* pTransfer = (SubID && pFile) ? pFile->GetTransfer(SubID) : NULL;
	if(!pTransfer)
	{
		QPainter Painter;
		Painter.begin(&Progress);
		Painter.fillRect(QRect(0,0,Progress.width(),Progress.height()), Qt::gray);
		Painter.end();

#ifdef _DEBUG
		Progress.save(pDevice, "bmp");
#else
		Progress.save(pDevice, "png");
#endif
		return; 
	}

	if(iWidth == 0)	iWidth = 300;
	if(iHeight == 0)iHeight = 12;

	uint cAvailable = iDepth ? qRgb(134, 134, 134) : qRgb(70, 70, 70);
	uint cVerified = iDepth ? qRgb(104, 104, 104) : qRgb(50, 50, 50);
	uint cConfirmed = qRgb(0, 224, 0);
	uint cScheduled = qRgb(255, 210, 0);
	uint cMarked = qRgb(128, 0, 255);
	uint cRequested = qRgb(255, 127, 0);
	uint cCached = qRgb(127, 210, 0);
	uint cHave = qRgb(0, 0, 160);
	uint cMissing = qRgb(255, 0, 0);
	uint cUnavailable = qRgb(224, 224, 224);
	uint cDisabled = qRgb(136, 0, 21);
	uint cEmpty = qRgb(224, 224, 224);

	uint64 uFileSize = pFile->GetFileSize();
	if(uFileSize == 0)
		uFileSize = pFile->GetProperty("EstimatedSize").toULongLong();

	bool IsComplete = pFile->IsComplete(true);

	uint64 PixelSize = 0;
#ifdef _DEBUG
	PixelSize = DivUp(uFileSize, iWidth*4);
	ASSERT(PixelSize >= 0);
#endif

	/*if(pTransfer->HasError())
	{
		QPainter Painter;
		Painter.begin(&Progress);
		Painter.fillRect(QRect(0,0,Progress.width(),Progress.height()),QBrush(QImage(":/ErrPattern")));
		Painter.end();
	}
	else*/ if(uFileSize == 0)
	{
		CBarShader BarShader(1, iWidth, iHeight, iDepth);
		BarShader.Fill(cEmpty);
		BarShader.DrawImage(Progress);
	}
	else
	{
		///////////////////////////////////////////////////////////////
		// Draw Transfer Status
		//

		CBarShader BarShader(uFileSize, iWidth, iHeight, iDepth);
		BarShader.Fill(cUnavailable);

		if(CShareMap* pPartMap = pTransfer->GetPartMap())
		{
			CPartMap* pMap = pFile->GetPartMap();

			CShareMap::SIterator SrcIter;
			while(PixelSize ? IterateRanges(PixelSize, pPartMap, SrcIter, uFileSize) : pPartMap->IterateRanges(SrcIter))
			{
				if((SrcIter.uState & Part::Requested) != 0)
					BarShader.FillRange(SrcIter.uBegin, SrcIter.uEnd, cRequested);
				else if((SrcIter.uState & Part::Scheduled) != 0)
					BarShader.FillRange(SrcIter.uBegin, SrcIter.uEnd, cScheduled);
				else if((SrcIter.uState & Part::Marked) != 0)
					BarShader.FillRange(SrcIter.uBegin, SrcIter.uEnd, cMarked);
				else if((SrcIter.uState & Part::Available) != 0)
					BarShader.FillRange(SrcIter.uBegin, SrcIter.uEnd, qRgb(0, 210, 255));
				/*{
					if(pMap)
					{
						CPartMap::SIterator FileIter(SrcIter.uBegin, SrcIter.uEnd);
						while(pMap->IterateRanges(FileIter))
						{
							if((FileIter.uState & Part::Available) != 0)
								BarShader.FillRange(FileIter.uBegin, FileIter.uEnd, cHave); // qRgb(0, 210, 255)
							else
								BarShader.FillRange(FileIter.uBegin, FileIter.uEnd, qRgb(0, 210, 255));
						}
					}
					else // complete file
						BarShader.FillRange(SrcIter.uBegin, SrcIter.uEnd, cHave); // qRgb(0, 210, 255)
				}*/
				else if((SrcIter.uState & Part::Selected) != 0)
					BarShader.FillRange(SrcIter.uBegin, SrcIter.uEnd, cCached);
			}
		}
#ifndef NO_HOSTERS
		else if(CHosterLink* pHosterLink = qobject_cast<CHosterLink*>(pTransfer))
		{
			int PartNumber = 0;
			uint64 PartSize = 0;
			bool bPending = false;
			foreach(CArchiveSet* pArchive, pFile->GetArchives())
			{
				PartNumber = pHosterLink->GetPartNumber();
				if(PartNumber)
				{
					double Stretch = (double)uFileSize / pArchive->GetTotalSize();
					if(pArchive->GetPartSize() == -1)
						PartSize = -1;
					else
						PartSize = pArchive->GetPartSize() * Stretch;
					break;
				}
			}

			if(PartSize != -1)
			{
				uint64 uBegin = PartSize * (PartNumber - 1);
				uint64 uEnd = uBegin + PartSize;
				if(uEnd > uFileSize)
					uEnd = uFileSize;

				if(pHosterLink->GetStatus(CHosterLink::eUp) == CHosterLink::eTransfering)
				{
					BarShader.FillRange(uBegin, uEnd, cCached);
					uint64 uUlEnd = uBegin + pHosterLink->GetTransferredSize(CHosterLink::eUp);
					if(uUlEnd > uFileSize)
						uUlEnd = uFileSize;
					BarShader.FillRange(uBegin, uUlEnd, cHave);
				}
				else if (IsComplete)
					BarShader.FillRange(uBegin, uEnd, cHave); // qRgb(0, 210, 255)
				else if(pHosterLink->IsDownload())
				{
					if(pHosterLink->GetStatus(CHosterLink::eDown) == CHosterLink::eTransfering)
						BarShader.FillRange(uBegin, uEnd, cScheduled);
					else
						BarShader.FillRange(uBegin, uEnd, qRgb(0, 210, 255));
					uint64 uDlEnd = uBegin + pHosterLink->GetTransferredSize(CHosterLink::eDown);
					if(uDlEnd > uFileSize)
						uDlEnd = uFileSize;
					BarShader.FillRange(uBegin, uDlEnd, cHave);
				}
				else
					BarShader.FillRange(uBegin, uEnd, qRgb(0, 210, 255));
			}
		}
#endif

		BarShader.DrawImage(Progress);
	}
	
	if (pTransfer->HasError())
	{
		uchar* r = (Progress.bits());
		uchar* g = (Progress.bits() + 1);
		uchar* b = (Progress.bits() + 2);

#if QT_VERSION < 0x050000
		uchar* end = (Progress.bits() + Progress.numBytes ());
#else
		uchar* end = (Progress.bits() + Progress.byteCount ());
#endif
		while (r != end)
		{
			*r = *g = *b = (((*r + *g) >> 1) + *b) >> 1; // (r + b + g) / 3

			r += 4;
			g += 4;
			b += 4;
		}
	}

#ifdef _DEBUG
	Progress.save(pDevice, "bmp");
#else
	Progress.save(pDevice, "png");
#endif
}

void CWebUI::GetProgress(QIODevice* pDevice, uint64 ID, const QString& Groupe, const QString& Hoster, const QString& User, int iWidth, int iHeight, int iDepth)
{
	QImage Progress(iWidth, iHeight, QImage::Format_RGB32);

	CFile* pFile = CFileList::GetFile(ID);

#ifndef NO_HOSTERS
	CArchiveSet* pArchive = NULL;
	if(pFile && !Groupe.isEmpty())
		pArchive = pFile->GetArchive(QByteArray::fromBase64(QString(Groupe).replace("-","+").replace("_","/").toLatin1()));
#endif

	if(!pFile || (!Groupe.isEmpty()
#ifndef NO_HOSTERS
		&& !pArchive
#endif
		))
	{
		QPainter Painter;
		Painter.begin(&Progress);
		Painter.fillRect(QRect(0,0,Progress.width(),Progress.height()), Qt::gray);
		Painter.end();

#ifdef _DEBUG
		Progress.save(pDevice, "bmp");
#else
		Progress.save(pDevice, "png");
#endif
		return; 
	}

	if(iWidth == 0)	iWidth = 300;
	if(iHeight == 0)iHeight = 12;

	uint cAvailable = iDepth ? qRgb(134, 134, 134) : qRgb(70, 70, 70);
	uint cVerified = iDepth ? qRgb(104, 104, 104) : qRgb(50, 50, 50);
	uint cConfirmed = qRgb(0, 224, 0);
	uint cScheduled = qRgb(255, 210, 0);
	uint cMarked = qRgb(128, 0, 255);
	uint cRequested = qRgb(255, 127, 0);
	uint cCached = qRgb(127, 210, 0);
	uint cHave = qRgb(0, 0, 160);
	uint cMissing = qRgb(255, 0, 0);
	uint cUnavailable = qRgb(224, 224, 224);
	uint cDisabled = qRgb(136, 0, 21);
	uint cEmpty = qRgb(224, 224, 224);

	uint64 uFileSize = pFile->GetFileSize();
	if(uFileSize == 0)
		uFileSize = pFile->GetProperty("EstimatedSize").toULongLong();

	bool IsComplete = pFile->IsComplete(true);

	uint64 PixelSize = 0;
#ifdef _DEBUG
	PixelSize = DivUp(uFileSize, iWidth*4);
	ASSERT(PixelSize >= 0);
#endif

	uint32 uCompleteAvail = -1;

	if(uFileSize == 0)
	{
		CBarShader BarShader(1, iWidth, iHeight, iDepth);
		BarShader.Fill(cEmpty);
		BarShader.DrawImage(Progress);
	}
	else
	{
		///////////////////////////////////////////////////////////////
		// Draw Hosting Status
		//

		CBarShader BarShader(uFileSize, iWidth, iHeight, iDepth);
		BarShader.Fill(cUnavailable);

		CAvailMap Avail(uFileSize);

#ifndef NO_HOSTERS
		if(!pArchive)
		{
			if(CHosterMap* pHosters = pFile->GetStats()->GetHosterMap())
			{
				CHosterMap::SIterator Iterator;
				while(pHosters->IterateRanges(Iterator))
				{
					uint32 uCount = 0;
					for(QMap<QString, SHosterRange>::iterator I = Hoster.isEmpty() ? Iterator.uState.begin() : Iterator.uState.find(Hoster); I != Iterator.uState.end(); I++) // for each hoster
					{
						if(User.isEmpty())
						{
							uCount += I.value().PubCount;
							for(QMap<QString, int>::iterator K = I.value().MyCounts.begin(); K != I.value().MyCounts.end(); K++) // for each account
								uCount += K.value();
						}
						else if(User == "Pub")
							uCount += I.value().PubCount;
						else
							uCount += I.value().MyCounts.value(User, 0);

						if(!Hoster.isEmpty())
							break; // only one hoster matches
					}

					Avail.SetRange(Iterator.uBegin, Iterator.uEnd, uCount);
					if(uCompleteAvail > uCount)
						uCompleteAvail = uCount;
				}
			}
		}
		else if(pArchive->GetTotalSize())
		{
			double Stretch = (double)uFileSize / pArchive->GetTotalSize();

			uint64 uBegin = 0;
			uint64 uEnd = 0;
			for(int Part = 1; Part <= pArchive->GetPartCount(); Part++)
			{
				uint64 uPartSize = 0;
				uint32 uCount = 0;

				uint64 uDownloaded = 0;
				foreach(CHosterLink* pHosterLink, pArchive->GetAvailable(Part))
				{
					if(!pFile->GetTransfers().contains(pHosterLink) || pHosterLink->HasError())
						continue;

					if(!Hoster.isEmpty() && pHosterLink->GetHoster() != Hoster)
						continue;
					if(!User.isEmpty() && pHosterLink->GetUploadAcc() != (User == "Pub" ? "" : User))
						continue;

					uCount++;
					if(pHosterLink->GetFileSize() > uPartSize)
						uPartSize = pHosterLink->GetFileSize();

					uDownloaded = pHosterLink->GetTransferredSize(CHosterLink::eDown); 
				}

				bool bCached = false;
				uint64 uUploaded = 0;
				foreach(CHosterLink* pHosterLink, pArchive->GetPending(Part))
				{
					if(!pFile->GetTransfers().contains(pHosterLink))
						continue;

					if(pHosterLink->GetFileSize() > uPartSize)
						uPartSize = pHosterLink->GetFileSize();

					switch(pHosterLink->GetStatus(CHosterLink::eUp))
					{
						case CHosterLink::eTransfering:	// active waiting for transfer or transfering
							//ASSERT(bCached == false); // there should be only one request per part at any one time
							bCached = true;
							uUploaded = pHosterLink->GetTransferredSize(CHosterLink::eUp); 
							break;
					}
				}

				uEnd += pArchive->GetPartSize() * Stretch;
				if(uEnd > uFileSize)
					uEnd = uFileSize;

				if(uBegin < uEnd)
				{
					Avail.SetRange(uBegin, uEnd, uCount);
					if(uCompleteAvail > uCount)
						uCompleteAvail = uCount;
				}

				uBegin = uEnd;
			}
		}
#endif

		FillAvailBar(PixelSize, Avail, uFileSize, uCompleteAvail, IsComplete, BarShader);

		BarShader.DrawImage(Progress);
	}

#ifdef _DEBUG
	Progress.save(pDevice, "bmp");
#else
	Progress.save(pDevice, "png");
#endif
}

//#define BASIC_GUI

void CWebUI::OnRequestCompleted()
{
	CHttpSocket* pRequest = (CHttpSocket*)sender();
	ASSERT(pRequest->GetState() == CHttpSocket::eHandling);
	QString Path = pRequest->GetPath();
	TArguments Cookies = GetArguments(pRequest->GetHeader("Cookie"));
	TArguments Arguments = GetArguments(pRequest->GetQuery().mid(1),'&');

	if(Path.right(1) != "/" && !Path.mid(Path.lastIndexOf("/")).contains("."))
	{
		pRequest->SetHeader("Location", QString(Path + "/" + pRequest->GetQuery()).toUtf8());
		pRequest->RespondWithError(303);
	}

	else if(!CWebRoot::TestLogin(pRequest))
	{
		pRequest->SetHeader("Location", QString("/Login" + Path).toUtf8());
		pRequest->RespondWithError(303);
	}

	else if(Path.compare("/WebUI/Progress.png") == 0)
	{
		pRequest->SetCaching(0);
		GetProgress(pRequest, Arguments["Mode"], Arguments["ID"].toULongLong(), Arguments["SubID"].toULongLong(), Arguments["Width"].toInt(), Arguments["Height"].toInt());
	}
	else if(Path.left(7).compare("/WebUI/") == 0)
	{
		QString FilePath = Path.mid(6);
		if(FilePath == "/")
		{
#ifdef USE_7Z
			if(m_WebUI || QFile::exists(m_WebUIPath + "/index.html"))
#else
			if(QFile::exists(m_WebUIPath + "/index.html"))
#endif
				FilePath = "/index.html";
#ifdef BASIC_GUI
			else
				FilePath = "/Main/";
#endif
		}

#ifdef BASIC_GUI
		if(FilePath.right(1) == "/")
		{
			QStringList Paths = FilePath.split("/", QString::SkipEmptyParts);

			if(Paths.first() == "Main")
				pRequest->write(GetTemplate(":/Templates/Main",Arguments.contains("Frame") ? Arguments["Frame"] : "Frame").toUtf8());
			else if(Paths.first() == "Log")
			{
				QString Lines;

				QVariantMap Request;
				if(Arguments.contains("File"))
					Request["File"] = Arguments["File"];
				if(Arguments.contains("Task"))
					Request["Task"] = Arguments["Task"];
				QVariantMap Response = theCore->m_Server->GetLog(Request);

				foreach (const QVariant LogEntryV, Response["Lines"].toList())
				{
					QVariantMap LogEntry = LogEntryV.toMap();

					TArguments Variables;
					switch(LogEntry["Flag"].toUInt() & LOG_MASK)
					{
						case LOG_ERROR:		Variables["Color"] = "red";			break;
						case LOG_WARNING:	Variables["Color"] = "darkyellow";	break;
						case LOG_SUCCESS:	Variables["Color"] = "darkgreen";	break;
						case LOG_INFO:		Variables["Color"] = "blue";		break;
						default:			Variables["Color"] = "black";		break;
					}
					Variables["Time"] = QDateTime::fromTime_t(LogEntry["Stamp"].toUInt()).toString();
					Variables["Line"] = CLogMsg(LogEntry["Line"]).Print();
					Lines.append(FillTemplate(GetTemplate(":/Templates/Main", "Line"),Variables));
				}

				TArguments Variables;
				Variables["Lines"] = Lines;
				Variables["Log"] = pRequest->GetQuery();
				pRequest->write(FillTemplate(GetTemplate(":/Templates/Main","Log"), Variables).toUtf8());
			}
			else if(Paths.first() == "Files")
			{
				if(Paths.size() == 1)
				{
					TArguments Variables;
					if(Arguments.contains("Status"))
						Variables["List"] = "Status=" + Arguments["Status"];
					else if(Arguments.contains("GrabberID"))
						Variables["List"] = "GrabberID=" + Arguments["GrabberID"];
					else if(Arguments.contains("SearchID"))
						Variables["List"] = "SearchID=" + Arguments["SearchID"];
					pRequest->write(FillTemplate(GetTemplate(":/Templates/Files","Frame"), Variables).toUtf8());
				}
				else if(Paths[1] == "List")
				{
					QString Files;

					QVariantMap Request;
					if(Arguments.contains("Status"))
						Request["Status"] = Arguments["Status"];
					else if(Arguments.contains("GrabberID"))
						Request["GrabberID"] = Arguments["GrabberID"];
					else if(Arguments.contains("SearchID"))
						Request["SearchID"] = Arguments["SearchID"];
					QVariantMap Response = theCore->m_Server->FileList(Request);

					foreach (const QVariant FileV, Response["Files"].toList())
					{
						QVariantMap File = FileV.toMap();
						TArguments Variables;
						Variables["ID"] = File["ID"].toString();
						Variables["Name"] = File["FileName"].toString();
						Variables["Size"] = QString::number(File["FileSize"].toDouble()/(1024*1024), 'f', 2) + "Mb";
						Variables["Type"] = File["Type"].toString();
						Variables["Status"] = File["Status"].toString();
						Variables["Transfer"] = QString::number(File["DownRate"].toDouble() / 1024, 'f', 2) + "Kb / " + QString::number(File["UpRate"].toDouble() / 1024, 'f', 2) + "Kb";
						Files.append(FillTemplate(GetTemplate(":/Templates/Files", "Entry"),Variables));
					}

					TArguments Variables;
					Variables["Files"] = Files;
					pRequest->write(FillTemplate(GetTemplate(":/Templates/Files","List"), Variables).toUtf8());
				}
				else
				{
					if(!Arguments.isEmpty() || pRequest->IsPost())
					{
						QVariantMap File;
						File["ID"] = Paths[1];

						if(Arguments.contains("Action"))
							File["Action"] = Arguments["Action"];

						QVariantMap Properties;

						if(Arguments.contains("Password"))
							Properties["Passwords"] = QUrl::fromPercentEncoding(Arguments["Password"].toLatin1()).split("\r\n");

						File["Properties"] = Properties;

						QString Trackers = pRequest->GetPostValue("Trackers");
						if(!Trackers.isEmpty())
						{
							int Tier = 0;
							QVariantList TrackerList;
							foreach(const QString& TrackerEntry, Trackers.split("\r\n\r\n"))
							{
								if(TrackerEntry.trimmed().isEmpty())
									continue;

								foreach(const QString& TrackerSubEntry, TrackerEntry.split("\r\n"))
								{
									if(TrackerSubEntry.trimmed().isEmpty())
										continue;

									QVariantMap Tracker;
									Tracker["Url"] = TrackerSubEntry.trimmed();
									Tracker["Tier"] = Tier;
									TrackerList.append(Tracker);
								}
								Tier++;
							}
							File["Trackers"] = TrackerList;
						}
						theCore->m_Server->SetFile(File);
					}


					QVariantMap Request;
					Request["ID"] = Paths[1];
					QVariantMap File = theCore->m_Server->GetFile(Request);

					if(Paths.size() == 2)
					{
						TArguments Variables;
						Variables["Status"] = File["Status"].toString();
						pRequest->write(FillTemplate(GetTemplate(":/Templates/Files","FileInfo"), Variables).toUtf8());
					}
					else if(Paths[2] == "Toolbox")
					{
						TArguments Variables;
						Variables["ID"] = File["ID"].toString();
						if(File["Status"].toString().split(" ").contains("Pending"))
							pRequest->write(FillTemplate(GetTemplate(":/Templates/Files","Grabbox"), Variables).toUtf8());
						else
							pRequest->write(FillTemplate(GetTemplate(":/Templates/Files","Toolbox"), Variables).toUtf8());
					}
					else if(Paths[2] == "Details")
					{
						QString FileList;
						if(File.contains("SubFiles"))
						{
							QString Files;
							foreach (const QVariant SubFileID, File["SubFiles"].toList()) // XXX - thats broken!
							{
								QVariantMap Request;
								Request["ID"] = SubFileID;
								QVariantMap SubFile = theCore->m_Server->GetFile(Request);

								TArguments Variables;
								Variables["ID"] = SubFile["ID"].toString();
								Variables["Name"] = SubFile["FileName"].toString();
								Variables["Size"] = QString::number(SubFile["FileSize"].toDouble()/(1024*1024), 'f', 2) + "Mb";
								Variables["Type"] = SubFile["Type"].toString();
								Variables["Status"] = SubFile["Status"].toString();
								Variables["Transfer"] = QString::number(SubFile["DownRate"].toDouble() / 1024, 'f', 2) + "Kb / " + QString::number(SubFile["UpRate"].toDouble() / 1024, 'f', 2) + "Kb";
								Files.append(FillTemplate(GetTemplate(":/Templates/Files", "Entry"),Variables));
							}

							TArguments Variables;
							Variables["Files"] = Files;
							FileList = FillTemplate(GetTemplate(":/Templates/Files","FileList"), Variables);
						}

						QString HashList;
						if(File.contains("HashMap"))
						{
							QString Hashes;
							foreach (const QVariant& vHash, File["FileHash"].toList())
							{
								QVariantMap Hash = vHash.toMap();

								TArguments Variables;
								Variables["Type"] = Hash["Type"].toString();
								Variables["Hash"] = Hash["Value"].toString();
								Hashes.append(FillTemplate(GetTemplate(":/Templates/Files", "FileHash"),Variables));
							}

							TArguments Variables;
							Variables["Hashes"] = Hashes;
							HashList = FillTemplate(GetTemplate(":/Templates/Files","HashList"), Variables);
						}

						QString Details;
						if (File.contains("Trackers"))
						{
							QString Trackers;
							foreach (const QVariant TrackerV, File["Trackers"].toList())
							{
								QVariantMap Tracker = TrackerV.toMap();
								TArguments Variables;
								Variables["Url"] = Tracker["Url"].toString();
								Variables["Status"] = Tracker["Status"].toString();
								Trackers.append(FillTemplate(GetTemplate(":/Templates/Files", "Tracker"),Variables));
							}

							TArguments Variables;
							Variables["ID"] = File["ID"].toString();
							Variables["UrlName"] = QString(QUrl::toPercentEncoding(File["FileName"].toString()));
							Variables["Trackers"] = Trackers;
							Details = FillTemplate(GetTemplate(":/Templates/Files","TorrentDetails"), Variables);
						}

						TArguments Variables;
						Variables["ID"] = File["ID"].toString();
						Variables["Name"] = File["FileName"].toString();
						Variables["Size"] = QString::number(File["FileSize"].toDouble()/(1024*1024), 'f', 2) + "Mb";
						//Variables["Ext"] = GetFileExt(Variables["Name"]);
						Variables["HashList"] = HashList;
						Variables["FileList"] = FileList;
						Variables["Details"] = Details;
						pRequest->write(FillTemplate(GetTemplate(":/Templates/Files","FileDetails"), Variables).toUtf8());
					}
					else if(Paths[2] == "Transfers")
					{
						QString Transfers;

						foreach (const QVariant TransferV, File["Transfers"].toList())
						{
							QVariantMap Transfer = TransferV.toMap();
							TArguments Variables;
							Variables["Url"] = Transfer["Url"].toString();
							Variables["Type"] = Transfer["Type"].toString();
							Variables["Status"] = Transfer["Status"].toString();
							Variables["ID"] = File["ID"].toString();
							Variables["SubID"] = Transfer["ID"].toString();
							Variables["Transfer"] = QString::number(Transfer["DownRate"].toDouble() / 1024, 'f', 2) + "Kb / " + QString::number(Transfer["UpRate"].toDouble() / 1024, 'f', 2) + "Kb";
							Transfers.append(FillTemplate(GetTemplate(":/Templates/Files", "Transfer"),Variables));
						}

						TArguments Variables;
						Variables["Transfers"] = Transfers;
						pRequest->write(FillTemplate(GetTemplate(":/Templates/Files","Transfers"), Variables).toUtf8());
					}
					else if(Paths[2] == "Link")
					{
						TArguments Variables;
						Variables["ID"] = Paths[1];

						if(!pRequest->GetQuery().isEmpty())
						{
							QVariantMap Request;
							Request["ID"] = Paths[1];
							Request["Links"] = Arguments["Links"];
							Request["Encoding"] = Arguments["Encoding"];
							QVariantMap Response = theCore->m_Server->MakeLink(Request);

							QString Link = Response["Link"].toString();
							if(Link == "Status:InProgress")
							{
								Variables["Result"] = GetTemplate(":/Templates/Files","LinkWait");
								Variables["Refresh"] = GetTemplate(":/Templates/Files","Refresh");
							}
							else
							{
								Variables["Link"] = Link;
								Variables["EncLink"] = QUrl::toPercentEncoding(Link);
								Variables["Result"] = FillTemplate(GetTemplate(":/Templates/Files","LinkResult"), Variables);
								Variables["Refresh"] = "";
							}
						}
						else
						{
							Variables["Result"] = "";
							Variables["Refresh"] = "";
						}
						
						pRequest->write(FillTemplate(GetTemplate(":/Templates/Files","Link"), Variables).toUtf8());
					}
					else if(Paths[2] == "Password")
					{
						TArguments Variables;
						Variables["ID"] = File["ID"].toString();
						Variables["Name"] = File["FileName"].toString();
						Variables["Size"] = QString::number(File["FileSize"].toDouble()/(1024*1024), 'f', 2) + "Mb";
						QVariantMap Properties = File["Properties"].toMap();
						Variables["Password"] = Properties["Passwords"].toStringList().join("\r\n");

						pRequest->write(FillTemplate(GetTemplate(":/Templates/Files","Password"), Variables).toUtf8());
					}
					else if(Paths[2] == "Trackers")
					{
						QMultiMap<int, QString> TrackerMap;
						foreach(const QVariant& vTracker, File["Trackers"].toList())
						{
							QVariantMap Tracker = vTracker.toMap();
							TrackerMap.insert(Tracker["Tier"].toInt(), Tracker["Url"].toString());
						}

						QString Trackers;
						foreach(int Tier, TrackerMap.uniqueKeys())
							Trackers.append(QStringList(TrackerMap.values(Tier)).join("\r\n") + "\r\n");

						TArguments Variables;
						Variables["ID"] = File["ID"].toString();
						Variables["Name"] = File["FileName"].toString();
						Variables["Size"] = QString::number(File["FileSize"].toDouble()/(1024*1024), 'f', 2) + "Mb";
						Variables["Trackers"] = Trackers;

						pRequest->write(FillTemplate(GetTemplate(":/Templates/Files","Trackers"), Variables).toUtf8());
					}
					else
						pRequest->RespondWithError(404);
				}
			}
			else if(Paths.first() == "Search")
			{
				if(pRequest->IsPost())
				{
					QVariantMap Request;
					Request["Expression"] = pRequest->GetPostValue("Expression");
					Request["SearchNet"] = pRequest->GetPostValue("SearchNet");
					QVariantMap Response = theCore->m_Server->StartSearch(Request);
					//Response["ID"]
				}
				else if(Arguments.contains("Close"))
				{
					QVariantMap Request;
					Request["ID"] = Arguments["Close"];
					theCore->m_Server->StopSearch(Request);
				}

				if(Paths.size() == 1)
					pRequest->write(GetTemplate(":/Templates/Search","Frame").toUtf8());
				else if(Paths[1] == "List")
				{
					QVariantMap Request;
					QVariantMap Response = theCore->m_Server->SearchList(Request);

					QString List;

					foreach (const QVariant SearchV, Response["Searches"].toList())
					{
						QVariantMap Search = SearchV.toMap();

						TArguments Variables;
						Variables["ID"] = Search["ID"].toString();
						QVariantMap Criteria = Search["Criteria"].toMap();
						if(Criteria.contains("Hash"))
							Variables["Expression"] = Criteria["Hash"].toByteArray().toHex();
						else
							Variables["Expression"] = Search["Expression"].toString();

						List.append(FillTemplate(GetTemplate(":/Templates/Search","Entry"), Variables));
					}

					TArguments Variables;
					Variables["List"] = List;
					pRequest->write(FillTemplate(GetTemplate(":/Templates/Search","List"), Variables).toUtf8());
				}
				else
					pRequest->RespondWithError(404);
			}
			else if(Paths.first() == "Grabber")
			{
				if(pRequest->IsPost())
				{
					QVariantMap Request;
					Request["Links"] = pRequest->GetPostValue("Links");
					QVariantMap Response = theCore->m_Server->GrabLinks(Request);
				}

				if(Paths.size() == 1)
				{
					QString Tasks;
					
					if(Arguments.contains("Action"))
					{
						QVariantMap Request;
						if(Arguments.contains("ID"))
							Request["ID"] = Arguments["ID"];
						Request["Action"] = Arguments["Action"];
						theCore->m_Server->GrabberList(Request);
					}

					QVariantMap Request;
					QVariantMap Response = theCore->m_Server->GrabberList(Request);

					foreach (const QVariant TaskV, Response["Tasks"].toList())
					{
						QVariantMap Task = TaskV.toMap();

						QString Status;
						if(Task.contains("TasksPending"))
						{
							TArguments Variables;
							Status = GetTemplate(":/Templates/Grabber","TasksPending");
							Variables["Count"] = Task["TasksPending"].toString();
							if(Task.contains("TasksFailed"))
							{
								Variables["FailedCount"] = Task["TasksFailed"].toString();
								Status += GetTemplate(":/Templates/Grabber","TasksFailed"),Variables;
							}
							Variables["ID"] = Task["ID"].toString();
							Status = FillTemplate(Status,Variables);
						}
						else
							Status = GetTemplate(":/Templates/Grabber","Done");

						QString Result;
						if(Task.contains("FileCount"))
						{
							TArguments Variables;
							Variables["ID"] = Task["ID"].toString();
							Variables["Count"] = Task["FileCount"].toString();
							Result = FillTemplate(GetTemplate(":/Templates/Grabber","FileResult"),Variables);
						}
						if(Task.contains("IndexCount"))
						{
							TArguments Variables;
							Variables["ID"] = Task["ID"].toString();
							Variables["Count"] = Task["IndexCount"].toString();
							Result = FillTemplate(GetTemplate(":/Templates/Grabber","SiteResult"),Variables);
						}

						TArguments Variables;
						Variables["ID"] = Task["ID"].toString();
						Variables["Uris"] = Task["Uris"].toStringList().join(" ");
						Variables["Status"] = Status;
						Variables["Result"] = Result;
						Tasks.append(FillTemplate(GetTemplate(":/Templates/Grabber", "Task"),Variables));
					}

					TArguments Variables;
					Variables["Tasks"] = Tasks;
					pRequest->write(FillTemplate(GetTemplate(":/Templates/Grabber","Grabber"), Variables).toUtf8());
				}
				else if(Paths[1] == "Reader")
				{
					pRequest->write(GetTemplate(":/Templates/Grabber","Reader").toUtf8());
				}
				else
				{
					uint64 ID = Paths[1].toULongLong();

					QString Sites;

					QVariantMap Request;
					Request["ID"] = ID;
					QVariantMap Response = theCore->m_Server->GrabberList(Request);

					if(!Response["Tasks"].toList().isEmpty())
					{
						QVariantMap Task = Response["Tasks"].toList().first().toMap();

						foreach (const QVariant SiteV, Task["Index"].toList())
						{
							QVariantMap Site = SiteV.toMap();
							TArguments Variables;
							Variables["Title"] = Site["Title"].toString();
							Variables["Url"] = Site["Url"].toString();
							Sites.append(FillTemplate(GetTemplate(":/Templates/Grabber", "Site"),Variables));
						}
					}

					TArguments Variables;
					Variables["Sites"] = Sites;
					pRequest->write(FillTemplate(GetTemplate(":/Templates/Grabber","Index"), Variables).toUtf8());
				}
			}
			else if(Paths.first() == "Maker")
			{
				if(pRequest->IsPost())
				{
					QVariantMap Request;
					Request["FileName"] = pRequest->GetPostValue("FileName");
					
					QVariantList SubFiles;
					foreach(const QString& FileID, pRequest->GetPostValue("SubFiles").split(","))
					{
						if(uint64 ID = FileID.trimmed().toULongLong())
							SubFiles.append(ID);
					}
					Request["SubFiles"] = SubFiles;
					
					QVariantMap Response = theCore->m_Server->AddFile(Request);

					TArguments Variables;
					Variables["ID"] = Response["ID"].toString();
					pRequest->write(FillTemplate(GetTemplate(":/Templates/Maker","Result"), Variables).toUtf8());
				}
				else
					pRequest->write(GetTemplate(":/Templates/Maker","Maker").toUtf8());
			}
			else if(Paths.first() == "Tasks")
			{
				QString Tasks;

				QVariantMap Request;
				bool bGrabber = false;
				if(Arguments.contains("GrabberID"))
				{
					bGrabber = true;
					Request["GrabberID"] = Arguments["GrabberID"];
				}
				if(Arguments.contains("Hold"))
					Request["Hold"] = Arguments["Hold"];
				QVariantMap Response = theCore->m_Server->WebTaskList(Request);

				foreach (const QVariant TaskV, Response["Tasks"].toList())
				{
					QVariantMap Task = TaskV.toMap();
					TArguments Variables;
					Variables["ID"] = Task["ID"].toString();
					Variables["Url"] = Task["Url"].toString();
					Variables["Status"] = Task["Status"].toString();
					Tasks.append(FillTemplate(GetTemplate(":/Templates/Tasks", "Entry"),Variables));
				}

				TArguments Variables;
				Variables["Tasks"] = Tasks;
				Variables["Hold"] = bGrabber ? "" : GetTemplate(":/Templates/Tasks", Response["Halted"].toBool() ? "Resume" : "Hold");
				pRequest->write(FillTemplate(GetTemplate(":/Templates/Tasks","List"), Variables).toUtf8());
			}
			else
				pRequest->RespondWithError(404);
		}
		else 
#endif
		if(!pRequest->IsGet())
			pRequest->RespondWithError(405);
		else
		{
			pRequest->SetCaching(HR2S(12));

#ifdef USE_7Z
			if(m_WebUI)
			{
				int ArcIndex = m_WebUI->FindByPath(FilePath);
				if(ArcIndex != -1)
				{
					QByteArray Buffer;
					QMap<int, QIODevice*> Files;
					Files.insert(ArcIndex, new QBuffer(&Buffer));
					if(m_WebUI->Extract(&Files))
						pRequest->write(Buffer);
					else
						pRequest->RespondWithError(500);
				}
				else
					pRequest->RespondWithError(404);
			}
#endif
			else
			{
				FilePath.prepend(m_WebUIPath);
				pRequest->RespondWithFile(FilePath);
			}
		}
	}
	else
		pRequest->RespondWithError(404);

	pRequest->SendResponse();
}

void CWebUI::HandleRequest(CHttpSocket* pRequest)
{
	connect(pRequest, SIGNAL(readChannelFinished()), this, SLOT(OnRequestCompleted()));
}

void CWebUI::ReleaseRequest(CHttpSocket* pRequest)
{
	disconnect(pRequest, SIGNAL(readChannelFinished()), this, SLOT(OnRequestCompleted()));
}
