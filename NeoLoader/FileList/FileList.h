#pragma once

#include "../../Framework/ObjectEx.h"
#include "./Hashing/FileHash.h"

class CFile;
class CFileHash;
class CFileList;
class CPartMap;

class CFileList: public QObjectEx
{
	Q_OBJECT

public:
	CFileList(QObject* qObject = NULL);
	~CFileList();

	virtual void					Process(UINT Tick);

	virtual void					AddFile(CFile* pFile);
	virtual QList<CFile*>			GetFiles()							{return m_FileMap.values();}
	virtual CFile*					GetFileByID(uint64 FileID);
	virtual CFile*					GetFileByHash(const CFileHash* pFileHash, bool bAlsoRemoved = false);
	virtual QList<CFile*>			FindDuplicates(CFile* pFile, bool bNoDuplicates = false);
	virtual QList<CFile*>			GetFilesByHash(const QList<CFileHashPtr>& Hashes, uint64 uFileSize, bool bNoDuplicates = false);
	virtual QList<CFile*>			GetFilesByName(const QString& FileName, bool bArchives = false);
	virtual CFile*					GetFileByProperty(const QString& Name, const QVariant& Value);
	virtual CFile*					GetArchiveFile(const QString& FileName);
	virtual CFile*					GetFileByPath(const QString& FilePath);
	virtual void					RemoveFile(CFile* pFile);
	virtual quint64					GetFileID(CFile* pFile)				{return m_FileMap.key(pFile,0);}

	virtual QList<CFile*>			GetFilesBySourceUrl(const QString& sUrl);

	static	uint64					AllocID(uint64 FileID = 0);
	static	void					ReleaseID(uint64 FileID);
	static	CFile*					GetFile(uint64 FileID);
	static	QList<CFile*>			GetAllFiles();

	// Load/Store
	virtual QVariant				Store();
	virtual int						Load(const QVariantList& Data);

protected:
	virtual void					ListFile(CFile* pFile);
	virtual void					UnlistFile(CFile* pFile);

	QMap<uint64, CFile*>			m_FileMap;
	uint64							m_LastID;

	static QSet<uint64>				m_FileIDs;
	static QSet<CFileList*>			m_AllLists;
};
