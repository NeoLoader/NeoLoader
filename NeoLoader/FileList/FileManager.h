#pragma once

#include "FileList.h"
class CTorrent;

class CFileManager: public CFileList
{
	Q_OBJECT

public:
	CFileManager(QObject* qObject = NULL);

	virtual void					Resume();
	virtual void					Suspend();
	virtual void					ScanShare();

	virtual void					Process(UINT Tick);

	virtual bool					GrabbFile(CFile* pFile, bool bDirect = false, CFile** ppKnownFile = NULL);

	virtual CFile*					AddFromFile(const QString& FilePath);
	virtual bool					AddUniqueFile(CFile* pFile, bool bDirect = false, CFile** ppKnownFile = NULL);

	// Load/Store
	virtual void					StoreToFile();
	virtual void					LoadFromFile();

protected:
	virtual QStringList				FindSharedFiles();
	virtual QStringList				ListDir(const QString& srcDirPath);
	CFile*							MergeTorrent(CTorrent* pTorrent);

	time_t							m_LastSave;
};
