#pragma once

#include "../FileList/Hashing/FileHash.h"
#include "../FileList/File.h"
#include "../../Framework/ObjectEx.h"
#include "../../Framework/Address.h"
#include "../FileList/Hashing/HashingThread.h"

class CCorruptionLogger;

class CHashInspector: public QObjectEx
{
	Q_OBJECT

public:
	CHashInspector(QObject* qObject = NULL);
	~CHashInspector() {}

	void				Process(UINT Tick);

	bool				StartValidation(bool bRecovery = false);

	void				ResetRange(uint64 uBegin, uint64 uEnd);

	EFileHashType		GetIndexSource()		{return m_IndexSource;}
	void				SetIndexSource(EFileHashType IndexSource) {m_IndexSource = IndexSource;}

	void				AddAuxHash(CFileHashPtr pFileHash);
	void				BlackListHash(CFileHashPtr pFileHash);
	bool				BadMetaData(CFileHashPtr pFileHash);

	const QMultiMap<EFileHashType,CFileHashPtr>& GetAuxHashes()		{return m_AuxHashMap;}
	const QMultiMap<EFileHashType,CFileHashPtr>& GetBlackedHashes()	{return m_BlackListMap;}

	void				AddUntrustedHash(CFileHashPtr pHash, const CAddress& Address);

	void				OnRecoveryData();

	CCorruptionLogger*	GetLogger(CFileHashPtr pHash);

	CFile*				GetFile() const			{CFile* pFile = qobject_cast<CFile*>(parent()); ASSERT(pFile); return pFile;}

	void				Update()				{m_Update = true;}

	void				SelectHash(EFileHashType Type, const QByteArray& Hash);
	void				UnSelectHash(EFileHashType Type, const QByteArray& Hash);
	void				BanHash(EFileHashType Type, const QByteArray& Hash);
	void				UnBanHash(EFileHashType Type, const QByteArray& Hash);

	void				AddHashesToFile();

	// Load/Store
	QVariantMap			Store();
	void				Load(const QVariantMap& Data);

private slots:
	void				OnPartsVerified();
	void				OnVerifiedAux();
	void				OnPartsRecovered();
	void				OnFileVerified();

protected:
	void				ValidateParts(bool bRecovery = false);
	void				LookForConflicts();
	void				ResetRange(CFile* pFile, uint64 uBegin, uint64 uEnd);
	bool				AddHashesToFile(CFileHashPtr pHash, bool bForce = false);

	bool				AdjustIndexRange(uint64& uFrom, uint64& uTo, const uint64 uFileSize, const uint64 Offset, uint64 SubOffset = 0);
	bool				FindHashConflicts(uint64 uFileSize, CFileHashEx* pMasterHash, uint64 Offset, CFileHashEx* pFileHash, uint64 SubOffset = 0);

	EFileHashType		m_IndexSource;

	uint64				m_LastHashStart;
	uint64				m_LastAvailable;

	bool				m_Update;

	QMultiMap<EFileHashType,CFileHashPtr> m_AuxHashMap;
	QMultiMap<EFileHashType,CFileHashPtr> m_BlackListMap;
	QMap<EFileHashType,CFileHashPtr> m_UntrustedMap;

	QMap<QByteArray, CCorruptionLogger*>	m_Loggers;

	QMap<CHashingJob*, CHashingJobPtr>	m_HashingJobs;
};