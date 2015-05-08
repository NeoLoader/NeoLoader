#include "GlobalHeader.h"
#include "NeoFS.h"
#include "../NeoCore.h"
#include "../FileList/File.h"
#include "../FileList/FileManager.h"

#ifndef __APPLE__

CNeoFS::CNeoFS(const QString& MountPoint, QObject* parent)
 : CFuse(MountPoint, parent)
{
}


CNeoFS::~CNeoFS()
{
}

QStringList CNeoFS::ListDirectory(const QString& Path)
{
	QStringList Files;
	if(Path == "/") // NeoFS has only root
        QMetaObject::invokeMethod(this, "ReadDir", Qt::BlockingQueuedConnection, Q_ARG(QStringList&, Files));
	return Files;
}

bool CNeoFS::IsDirectory(const QString& Path)
{
    return Path == "/";
}

uint64 CNeoFS::GetFileSize(const QString& Path)
{
	quint64 Size = -1;
    quint64 FileID = Split2(Path.mid(1), "_").first.toULongLong();
	QMetaObject::invokeMethod(this, "GetFile", Qt::BlockingQueuedConnection, Q_ARG(quint64, FileID), Q_ARG(quint64&, Size));
	return Size;
}

uint64 CNeoFS::OpenFile(const QString& Path)
{
    SFileHandle* pHandle = new SFileHandle;
	pHandle->Refs = 1;
	pHandle->Open = true;
	pHandle->FileID = Split2(Path.mid(1), "_").first.toULongLong();
    quint64 Size = -1;
    QMetaObject::invokeMethod(this, "GetFile", Qt::BlockingQueuedConnection, Q_ARG(quint64, pHandle->FileID), Q_ARG(quint64&, Size), Q_ARG(quint64, (quint64)pHandle));

    // is the file available
    if(Size != -1)
        return (uint64)pHandle;

    delete pHandle;
    return 0;
}

uint64 CNeoFS::ReadFile(uint64 Handle, uint64 Offset, char* DataPtr, uint64 DataSize)
{
	SFileHandle* pHandle = (SFileHandle*)Handle;
    if(!pHandle)
        return -1;
	++pHandle->Refs;

    // Check if the requested file data is available
retry:
	if(!pHandle->Open)
		return -1;

    CPartMapPtr PartMap = pHandle->PartMap; // handle keeps a week pointer if its null file is completed or gone
    if(PartMap && (PartMap->GetRange(Offset, Min(PartMap->GetSize(), Offset + DataSize)) & Part::Available) == 0)
    {
        QThread::currentThread()->msleep(100);
        goto retry;
    }

    QFile File(pHandle->FilePath);

	if(!--pHandle->Refs)
		delete pHandle;

    if(!File.open(QFile::ReadOnly))
        return -1; // file does not longer exist

    if(!File.seek(Offset))
        return 0;

    return File.read(DataPtr, DataSize);
}

void CNeoFS::CloseFile(uint64 Handle)
{
	SFileHandle* pHandle = (SFileHandle*)Handle;
	pHandle->Open = false;
	if(!--pHandle->Refs)
		delete pHandle;
}

//////////////////////////////////////

void CNeoFS::ReadDir(QStringList& Files)
{
    foreach(CFile* pFile, theCore->m_FileManager->GetFiles())
    {
        if(pFile->IsRemoved())
            continue;
        Files.append(QString("%1_%2").arg(pFile->GetFileID()).arg(pFile->GetFileName()));
    }
}

void CNeoFS::GetFile(quint64 FileID, quint64& Size, quint64 Ptr)
{
    if(CFile* pFile = CFileList::GetFile(FileID))
    {
        Size = pFile->GetFileSize();
        if(Ptr)
        {
            SFileHandle* pHandle = (SFileHandle*)Ptr;
            pHandle->FilePath = pFile->GetFilePath();
            pHandle->PartMap = pFile->GetPartMapPtr();
        }
    }
}

QString CNeoFS::GetVirtualPath(CFile* pFile)
{
    QString Path = m_MountPoint;
    if(Path.right(1) != "/")
        Path += "/";
    Path += QString("%1_%2").arg(pFile->GetFileID()).arg(pFile->GetFileName());
    return Path ;
}

#endif
