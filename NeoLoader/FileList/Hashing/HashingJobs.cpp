#include "GlobalHeader.h"
#include "HashingJobs.h"
#include "HashingThread.h"
#include "../File.h"
#include "../FileList.h"
#include "../IOManager.h"
#include "../../NeoCore.h"
#include "FileHash.h"
#include "FileHashSet.h"
#include "FileHashTree.h"
#include "FileHashTreeEx.h"

/////////////////////////////////////////////////////////////////////////////////////
// CHashingJob
//

CHashingJob::CHashingJob(uint64 FileID, const QList<CFileHashPtr>& List, CPartMapPtr Parts)
{
	m_FileID = FileID;
	m_Parts = Parts;
	foreach(const CFileHashPtr& pHash, List)
		m_List.append(pHash);
}


/////////////////////////////////////////////////////////////////////////////////////
// CVerifyPartsJob
//

CVerifyPartsJob::CVerifyPartsJob(uint64 FileID, const QList<CFileHashPtr>& List, CPartMapPtr Parts, EHashingMode Mode)
 : CHashingJob(FileID, List, Parts)
{
	m_Mode = Mode;
	m_uFrom = 0;
	m_uTo = -1;
}

CVerifyPartsJob::CVerifyPartsJob(uint64 FileID, CFileHashPtr pHash, CPartMapPtr Parts, uint64 uOffset, uint64 uSize, EHashingMode Mode)
	: CHashingJob(FileID, QList<CFileHashPtr>(), Parts)
{
	m_List.append(pHash);

	m_Mode = Mode;
	m_uFrom = uOffset;
	m_uTo = uSize != -1 ? uOffset + uSize : -1;
}
	
void CVerifyPartsJob::Execute(CManagedIO* pDevice)
{
	pDevice->open(QIODevice::ReadOnly); // on this kind of devices open should never fail

	foreach(CFileHashPtr pHash, m_List)
	{
		CFileHashEx* pHashEx = qobject_cast<CFileHashEx*>(pHash.data());
		if(!pHashEx)
			continue; // this one is gone

		if(pHashEx->IsLoaded())
			pHashEx->Verify(pDevice, m_Parts.data(), m_uFrom, m_uTo, m_Mode);
		//else // try full file verification
		//{
		//	ASSERT((m_Parts->GetRange(0, -1) & Part::Available) != 0);
		//	CFileHashPtr pNewHash = CFileHashPtr(pHash->SpinOff());
		//	if(pNewHash->Calculate(pDevice))
		//	{
		//		if(pHash->Compare(pNewHash.data()))
		//		{
		//			m_pThread->SaveHash(pNewHash.data());
		//			m_pThread->LoadHash(pHash.data());
		//			pHash->SetResult(true, 0, m_Parts->GetSize(), m_Parts.data());
		//		}
		//	}
		//}
	}

	emit Finished();
}


/////////////////////////////////////////////////////////////////////////////////////
// CVerifyPartsJob
//

CHashFileJob::CHashFileJob(uint64 FileID, const QList<CFileHashPtr>& List)
 : CHashingJob(FileID, List)
{
}
	
void CHashFileJob::Execute(CManagedIO* pDevice)
{
	pDevice->open(QIODevice::ReadOnly); // on this kind of devices open should never fail

	foreach(CFileHashPtr pHash, m_List)
	{
		if(!pHash)
			continue; // this one is gone

		if(!pHash->Calculate(pDevice))
			m_pThread->LogLine(LOG_ERROR, tr("Hashing (%2) of %1 failed").arg(pDevice->fileName()).arg(CFileHash::HashType2Str(pHash->GetType())));
		else if(pHash->IsComplete()) // else means we we need meta data and that will be done by CFile
			m_pThread->SaveHash(pHash.data());
	}

	emit Finished();
}


/////////////////////////////////////////////////////////////////////////////////////
// CVerifyPartsJob
//

CImportPartsJob::CImportPartsJob(uint64 FileID, const QList<CFileHashPtr>& List, CPartMapPtr Parts, const QString& FilePath, bool bDeleteSource)
 : CHashingJob(FileID, List, Parts)
{
	ASSERT(List.count() == 1);

	m_FilePath = FilePath;
	m_bDeleteSource = bDeleteSource;
}
	
void CImportPartsJob::Execute(CManagedIO* pDevice)
{
	pDevice->open(QIODevice::ReadWrite); // on this kind of devices open should never fail

	QFile File(m_FilePath);
	File.open(QFile::ReadOnly);

	CFileHashPtr pHash = m_List.first();
	if(CFileHashEx* pHashEx = qobject_cast<CFileHashEx*>(pHash.data()))
	{
		// Create a duplicate of our hash with an own status map
		CFileHashEx* pHashAux = qobject_cast<CFileHashEx*>(pHashEx->Clone(true));
		pHashAux->Verify(&File, NULL, 0, -1, eVerifyFile); // hashing all partmap is not needed

		uint64 uStatusSize = pHashAux->GetPartSize();
		if(!uStatusSize) // Note: if booth are set it means that blocks dont fit into parts!!!
			uStatusSize = pHashAux->GetBlockSize();
		QBitArray StatusMap = pHashAux->GetStatusMap();
		for(uint32 i=0; i < StatusMap.count(); i++) // for each part
		{
			uint64 uBegin = uStatusSize * i;
			uint64 uEnd = Min(uBegin + uStatusSize, pHashAux->GetTotalSize());

			if(StatusMap.testBit(i) && (m_Parts->GetRange(uBegin, uEnd, CPartMap::eUnion) & Part::Verified) == 0) // do not overwrite verifyed parts
			{
				File.seek(uBegin);
				pDevice->seek(uBegin);

				quint64 uSize = uEnd - uBegin;
				const size_t BuffSize = 16*1024;
				char Buffer[BuffSize];
				quint64 uPos = 0;
				while(uPos < uSize)
				{
					quint64 uToGo = BuffSize;
					if(uPos + uToGo > uSize)
						uToGo = uSize - uPos;
					uPos += uToGo;

					qint64 uRead = File.read(Buffer, uToGo);
					if(uRead < 1)
						break;
					pDevice->write(Buffer, uRead);
					ASSERT(uRead == uToGo);
				}
				ASSERT(uPos <= uSize);

				if(uPos == uSize) // check if no error happened, and set the file range
				{
					pHashEx->SetResult(true, uBegin, uEnd);
					m_Parts->SetRange(uBegin, uEnd, Part::Available, CPartMap::eAdd);
				}
			}
		}

		delete pHashAux;
	}

	File.close();
	if(m_bDeleteSource)
		File.remove();
	emit Finished();
}