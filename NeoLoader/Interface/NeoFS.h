#pragma once

#include "../Common/FUSE/Fuse.h"
#include "../FileList/PartMap.h"
class CFile;

#ifndef __APPLE__

class CNeoFS : public CFuse
{
    Q_OBJECT
public:
    CNeoFS(const QString& MountPoint, QObject* parent = 0);
    ~CNeoFS();

    QString     GetVirtualPath(CFile* pFile);


	virtual QStringList	ListDirectory(const QString& Path);
    virtual bool        IsDirectory(const QString& Path);
	virtual uint64		GetFileSize(const QString& Path);
	virtual uint64		OpenFile(const QString& Path);
	virtual uint64		ReadFile(uint64 Handle, uint64 Offset, char* DataPtr, uint64 DataSize);
	virtual void		CloseFile(uint64 Handle);

public slots:
    void ReadDir(QStringList& Files);
    void GetFile(quint64 FileID, quint64& Size, quint64 Ptr = 0);

protected:

	struct SFileHandle
    {
        uint64      FileID;
        CPartMapRef PartMap;
        QString     FilePath;
		int			Refs;
		bool		Open;
    };
};

#endif
