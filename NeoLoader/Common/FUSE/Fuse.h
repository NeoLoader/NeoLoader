#pragma once
#include <QLibrary>

#include "../../../Framework/MT/ThreadEx.h"

#ifndef __APPLE__

class CFuse: public QThreadEx
{
    Q_OBJECT

public:
    CFuse(const QString& MountPoint, QObject* pObject = 0);
	~CFuse();

	virtual void		run();

	virtual QStringList	ListDirectory(const QString& Path) = 0;
    virtual bool        IsDirectory(const QString& Path) = 0;
	virtual uint64		GetFileSize(const QString& Path) = 0;
	virtual uint64		OpenFile(const QString& Path) = 0;
    virtual uint64		ReadFile(uint64 Handle, uint64 Offset, char* DataPtr, uint64 DataSize) = 0;
	virtual void		CloseFile(uint64 Handle) = 0;

protected:
    QString				m_MountPoint;

#ifdef WIN32
	static QLibrary		m_Dokan;
#else
    struct fuse *       m_fuse;
    struct fuse_chan *  m_ch;
#endif
};

#endif
