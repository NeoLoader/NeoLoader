#include "GlobalHeader.h"
#include "Fuse.h"
#include "../../NeoCore.h"

#ifndef __APPLE__

#ifdef WIN32
#include <windows.h>
#include <winbase.h>
#include <stdio.h>
#include <stdlib.h>
#include "dokan.h"

QLibrary CFuse::m_Dokan("dokan");

typedef BOOL (*TDokanRemoveMountPoint)(LPCWSTR);
TDokanRemoveMountPoint DokanRemoveMountPointFkt = NULL;

typedef int (*TDokanMain)(PDOKAN_OPTIONS, PDOKAN_OPERATIONS);
TDokanMain DokanMainFkt = NULL;


QString ToPath(LPCWSTR FileName)
{
	QString Path = QString::fromWCharArray(FileName);
	Path.replace("\\", "/");
	return Path;
}

static int __stdcall
FxCreateFile(
	LPCWSTR					FileName,
	DWORD					AccessMode,
	DWORD					ShareMode,
	DWORD					CreationDisposition,
	DWORD					FlagsAndAttributes,
	PDOKAN_FILE_INFO		DokanFileInfo)
{
	CFuse* pFuse = ((CFuse*)DokanFileInfo->DokanOptions->GlobalContext);

	if(pFuse->IsDirectory(ToPath(FileName)))
		return 0;

	DokanFileInfo->Context = pFuse->OpenFile(ToPath(FileName));
	if(!DokanFileInfo->Context)
		return -1;

	return 0;
}


static int __stdcall
FxOpenDirectory(
	LPCWSTR					FileName,
	PDOKAN_FILE_INFO		DokanFileInfo)
{
	CFuse* pFuse = ((CFuse*)DokanFileInfo->DokanOptions->GlobalContext);

	if(!pFuse->IsDirectory(ToPath(FileName)))
		return -1;

	return 0;
}


static int __stdcall
FxCloseFile(
	LPCWSTR					FileName,
	PDOKAN_FILE_INFO		DokanFileInfo)
{
	CFuse* pFuse = ((CFuse*)DokanFileInfo->DokanOptions->GlobalContext);

	if(DokanFileInfo->Context)
	{
		pFuse->CloseFile(DokanFileInfo->Context);
		DokanFileInfo->Context = NULL;
	}

	return 0;
}


static int __stdcall
FxReadFile(
	LPCWSTR				FileName,
	LPVOID				Buffer,
	DWORD				BufferLength,
	LPDWORD				ReadLength,
	LONGLONG			Offset,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	CFuse* pFuse = ((CFuse*)DokanFileInfo->DokanOptions->GlobalContext);

	uint64 Ret = pFuse->ReadFile(DokanFileInfo->Context, Offset, (char*)Buffer, BufferLength);
	if(Ret < 0)
		return -1;

	*ReadLength = Ret;

	return 0;
}

static int __stdcall
FxGetFileInformation(
	LPCWSTR							FileName,
	LPBY_HANDLE_FILE_INFORMATION	HandleFileInformation,
	PDOKAN_FILE_INFO				DokanFileInfo)
{
	CFuse* pFuse = ((CFuse*)DokanFileInfo->DokanOptions->GlobalContext);

	if (wcslen(FileName) == 1) {
		HandleFileInformation->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
	} else {
		uint64 Size = pFuse->GetFileSize(ToPath(FileName));
		HandleFileInformation->nFileSizeHigh = (Size >> 32);
		HandleFileInformation->nFileSizeLow = Size & 0xFFFFFFFF;
	}
	return 0;
}

static int __stdcall
FxFindFiles(
	LPCWSTR				FileName,
	PFillFindData		FillFindData, // function pointer
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	CFuse* pFuse = ((CFuse*)DokanFileInfo->DokanOptions->GlobalContext);

	QString Path = ToPath(FileName);
	QStringList Files = pFuse->ListDirectory(Path);

	foreach(const QString& File, Files)
	{
		WIN32_FIND_DATAW findData;
		ZeroMemory(&findData, sizeof(WIN32_FIND_DATAW));
		wcscpy(findData.cFileName, File.toStdWString().c_str());
		wcscpy(findData.cAlternateFileName, File.left(8).toStdWString().c_str());

		uint64 Size = pFuse->GetFileSize(Path + File);
		findData.nFileSizeHigh = (Size >> 32);
		findData.nFileSizeLow = Size & 0xFFFFFFFF;

		FillFindData(&findData, DokanFileInfo);
	}

	return 0;
}

static int __stdcall
FxGetVolumeInformation(
	LPWSTR		VolumeNameBuffer,
	DWORD		VolumeNameSize,
	LPDWORD		VolumeSerialNumber,
	LPDWORD		MaximumComponentLength,
	LPDWORD		FileSystemFlags,
	LPWSTR		FileSystemNameBuffer,
	DWORD		FileSystemNameSize,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	wcscpy_s(VolumeNameBuffer, VolumeNameSize / sizeof(WCHAR), L"NeoFS");
	*VolumeSerialNumber = 0x19831116;
	*MaximumComponentLength = 256;
	*FileSystemFlags = FILE_CASE_SENSITIVE_SEARCH | 
						FILE_CASE_PRESERVED_NAMES | 
						FILE_SUPPORTS_REMOTE_STORAGE |
						FILE_UNICODE_ON_DISK;

	wcscpy_s(FileSystemNameBuffer, FileSystemNameSize / sizeof(WCHAR), L"NeoFS");
	return 0;
}

#else
#include <fuse.h>
#include <fuse/fuse_lowlevel.h>

QAtomicPointer <void> m_new_inst;

void* FxInit(struct fuse_conn_info *conn)
{
    void* inst = m_new_inst.fetchAndStoreOrdered(NULL);
    ASSERT(inst);
    return inst; // set fuse_context->private_data
}

int FxOpen(const char *path, struct fuse_file_info *fileInfo)
{
    fuse_context* fuse_ctx = fuse_get_context();
    CFuse* pFuse = (CFuse*)fuse_ctx->private_data;

    // we allow only read access to the files
    if ((fileInfo->flags & 3) != O_RDONLY)
        return -EACCES;

	fileInfo->fh = pFuse->OpenFile(path);

    if(fileInfo->fh == 0)
        return -ENOENT;
    return 0;
}

int FxRead(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo)
{
   fuse_context* fuse_ctx = fuse_get_context();
   CFuse* pFuse = (CFuse*)fuse_ctx->private_data;

   uint64 Ret = pFuse->ReadFile(fileInfo->fh, offset, buf, size);
   if(Ret == -1)
	   return -ENOENT;
   return Ret;
}

int FxClose(const char *path, struct fuse_file_info *fileInfo)
{
    fuse_context* fuse_ctx = fuse_get_context();
    CFuse* pFuse = (CFuse*)fuse_ctx->private_data;

    pFuse->CloseFile(fileInfo->fh);
    return 0;
}

int FxGetAttr(const char *path, struct stat *statbuf)
{
    fuse_context* fuse_ctx = fuse_get_context();
    CFuse* pFuse = (CFuse*)fuse_ctx->private_data;

    memset(statbuf, 0, sizeof(struct stat));

    if (pFuse->IsDirectory(path))
    {
        statbuf->st_mode = S_IFDIR | 0755;
        statbuf->st_nlink = 2;
    }
    else
    {
        uint64 Size = pFuse->GetFileSize(path);
        if(Size == -1)
            return -ENOENT;

        statbuf->st_mode = S_IFREG | 0444;
        statbuf->st_nlink = 1;
        statbuf->st_size = Size;
    }
    return 0;
}

int FxReadDir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fileInfo)
{
    fuse_context* fuse_ctx = fuse_get_context();
    CFuse* pFuse = (CFuse*)fuse_ctx->private_data;

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    QStringList Files = pFuse->ListDirectory(path);
    foreach(const QString& File, Files)
        filler(buf, File.toStdString().c_str(), NULL, 0);

    return 0;
}
#endif



CFuse::CFuse(const QString& MountPoint, QObject* pObject)
 : QThreadEx(pObject)
{
	// Note: this is accessed without a mutex, it must be considdered const after start()
    m_MountPoint = MountPoint;

#ifdef WIN32
	if(!m_Dokan.isLoaded())
	{
		if(!m_Dokan.load())
		{
			LogLine(LOG_ERROR, tr("Dokan library not installed, Fuse not available!"));
			return;
		}

		DokanRemoveMountPointFkt = (TDokanRemoveMountPoint)m_Dokan.resolve("DokanRemoveMountPoint");
		DokanMainFkt = (TDokanMain)m_Dokan.resolve("DokanMain");
	}

    start();
#else
    m_fuse = NULL;
    m_ch = NULL;

    struct fuse_operations fusefs_oper;
    memset(&fusefs_oper, 0, sizeof(fusefs_oper));

    fusefs_oper.init = FxInit;
    //fusefs_oper.destroy = FxDestroy;

    fusefs_oper.open = FxOpen;
    fusefs_oper.read = FxRead;
    fusefs_oper.release = FxClose;

    fusefs_oper.getattr = FxGetAttr;

    fusefs_oper.readdir = FxReadDir;

    /*
general options:
    -o opt,[opt...]        mount options
    -h   --help            print help
    -V   --version         print version

FUSE options:
    -d   -o debug          enable debug output (implies -f)
    -f                     foreground operation
    -s                     disable multi-threaded operation

mountpoint: ExampleFS::ExampleFS
setting FS root to: (null)
    -o allow_other         allow access to other users
    -o allow_root          allow access to root
    -o auto_unmount        auto unmount on process termination
    -o nonempty            allow mounts over non-empty file/dir
    -o default_permissions enable permission checking by kernel
    -o fsname=NAME         set filesystem name
    -o subtype=NAME        set filesystem type
    -o large_read          issue large read requests (2.4 only)
    -o max_read=N          set maximum size of read requests

fuse_mount worked
    -o hard_remove         immediate removal (don't hide files)
    -o use_ino             let filesystem set inode numbers
    -o readdir_ino         try to fill in d_ino in readdir
    -o direct_io           use direct I/O
    -o kernel_cache        cache files in kernel
    -o [no]auto_cache      enable caching based on modification times (off)
    -o umask=M             set file permissions (octal)
    -o uid=N               set file owner
    -o gid=N               set file group
    -o entry_timeout=T     cache timeout for names (1.0s)
    -o negative_timeout=T  cache timeout for deleted names (0.0s)
    -o attr_timeout=T      cache timeout for attributes (1.0s)
    -o ac_attr_timeout=T   auto cache timeout for attributes (attr_timeout)
    -o noforget            never forget cached inodes
    -o remember=T          remember cached inodes for T seconds (0s)
    -o intr                allow requests to be interrupted
    -o intr_signal=NUM     signal to send on interrupt (10)
    -o modules=M1[:M2...]  names of modules to push onto filesystem stack

    -o max_write=N         set maximum size of write requests
    -o max_readahead=N     set maximum readahead
    -o max_background=N    set number of maximum background requests
    -o congestion_threshold=N  set kernel's congestion threshold
    -o async_read          perform reads asynchronously (default)
    -o sync_read           perform reads synchronously
    -o atomic_o_trunc      enable atomic open+truncate support
    -o big_writes          enable larger than 4kB writes
    -o no_remote_lock      disable remote file locking
    -o no_remote_flock     disable remote file locking (BSD)
    -o no_remote_posix_lock disable remove file locking (POSIX)
    -o [no_]splice_write   use splice to write to the fuse device
    -o [no_]splice_move    move data while splicing to the fuse device
    -o [no_]splice_read    use splice to read from the fuse device
    */

    QStringList l("");
    l.append(theCore->Cfg()->GetString("Content/FuseOptions").split("|"));
    int argc = l.size();
    char** argv = new char*[argc];
    unsigned index = 0;
    foreach (const QString s, l)
    {
        QByteArray b = QFile::encodeName(s);
        char* tmp = b.data();
        char* z = (char*) malloc(strlen(tmp)+1);
        memcpy(z, tmp, strlen(tmp)+1);
        argv[index] = z;
        index++;
    }

    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

    // parses command line options (mountpoint, -s, foreground(which is ignored) and -d as well as other fuse specific parameters)
    //int res = fuse_parse_cmdline(&args, &mountpoint, &multithreaded, &foreground);
    //if (res == -1)
    //    goto err_out;
    //LogLine(LOG_INFO, tr("mountpoint: %1").arg(m_MountPoint));

    //to remove leftovers from previous crashes
    fuse_unmount(m_MountPoint.toStdString().c_str(), m_ch);

    m_ch = fuse_mount(m_MountPoint.toStdString().c_str(), &args);
    if(!m_ch)
	{
		LogLine(LOG_ERROR, tr("fuse_mount failed; Unable to mount FUSE on directory %1").arg(m_MountPoint));
        goto err_out;
	}
    LogLine(LOG_SUCCESS, tr("fuse_mount worked"));

    m_fuse = fuse_new(m_ch, &args, &fusefs_oper, sizeof(fusefs_oper), NULL);
    fuse_opt_free_args(&args);
    if(!m_fuse)
	{
		LogLine(LOG_ERROR, tr("fuse_new failes; Unable to mount FUSE on directory %1").arg(m_MountPoint));
        goto err_unmount;
	}
    LogLine(LOG_SUCCESS, tr("fuse_new worked"));

    start();

    goto end;

err_unmount:
    fuse_unmount(m_MountPoint.toStdString().c_str(), m_ch);
err_out:
    m_fuse = NULL;

end:
    for(int i=0; i < argc; i++)
        free(argv[i]);
    delete [] argv;

#endif
}

CFuse::~CFuse()
{
#ifdef WIN32
	if(DokanRemoveMountPointFkt)
		DokanRemoveMountPointFkt(m_MountPoint.toStdWString().c_str());
#else
    if (m_fuse)
    {
        LogLine(LOG_INFO, tr("fuse_session_exit"));
        fuse_session_exit (fuse_get_session(m_fuse));

        LogLine(LOG_INFO, tr("fuse_unmount()"));
        fuse_unmount(m_MountPoint.toStdString().c_str(), m_ch);
    }
#endif

    wait();
}

void CFuse::run()
{
#ifdef WIN32

	DOKAN_OPTIONS dokanOptions;	
	ZeroMemory(&dokanOptions, sizeof(DOKAN_OPTIONS));
	dokanOptions.Version = DOKAN_VERSION;
	dokanOptions.ThreadCount = 0; // use default
	//dokanOptions.Options |= DOKAN_OPTION_NETWORK;
	//dokanOptions.Options |= DOKAN_OPTION_REMOVABLE;
	dokanOptions.GlobalContext = (uint64)this;
    wchar_t MountPoint[MAX_PATH];
    wcscpy(MountPoint, m_MountPoint.toStdWString().c_str());
    dokanOptions.MountPoint = MountPoint;
	dokanOptions.Options |= DOKAN_OPTION_KEEP_ALIVE;

	DOKAN_OPERATIONS dokanOperations;
	ZeroMemory(&dokanOperations, sizeof(DOKAN_OPERATIONS));
	dokanOperations.CreateFile = FxCreateFile;
	dokanOperations.OpenDirectory = FxOpenDirectory;
	dokanOperations.CreateDirectory = NULL;
	dokanOperations.Cleanup = NULL;
	dokanOperations.CloseFile = FxCloseFile;
	dokanOperations.ReadFile = FxReadFile;
	dokanOperations.WriteFile = NULL;
	dokanOperations.FlushFileBuffers = NULL;
	dokanOperations.GetFileInformation = FxGetFileInformation;
	dokanOperations.FindFiles = FxFindFiles;
	dokanOperations.FindFilesWithPattern = NULL;
	dokanOperations.SetFileAttributes = NULL;
	dokanOperations.SetFileTime = NULL;
	dokanOperations.DeleteFile = NULL;
	dokanOperations.DeleteDirectory = NULL;
	dokanOperations.MoveFile = NULL;
	dokanOperations.SetEndOfFile = NULL;
	dokanOperations.SetAllocationSize = NULL;	
	dokanOperations.LockFile = NULL;
	dokanOperations.UnlockFile = NULL;
	dokanOperations.GetFileSecurity = NULL;
	dokanOperations.SetFileSecurity = NULL;
	dokanOperations.GetDiskFreeSpace = NULL;
	dokanOperations.GetVolumeInformation = FxGetVolumeInformation;
	dokanOperations.Unmount = NULL;

	int Status = -1;
	if(DokanMainFkt)
		Status = DokanMainFkt(&dokanOptions, &dokanOperations);
	switch (Status)
	{
		case DOKAN_SUCCESS:	break;
		case DOKAN_ERROR:					LogLine(LOG_ERROR, tr("dokan: Error"));							break;
		case DOKAN_DRIVE_LETTER_ERROR:		LogLine(LOG_ERROR, tr("dokan: Bad Drive letter"));				break;
		case DOKAN_DRIVER_INSTALL_ERROR:	LogLine(LOG_ERROR, tr("dokan: Can't install driver"));			break;
		case DOKAN_START_ERROR:				LogLine(LOG_ERROR, tr("dokan: Driver something wrong"));		break;
		case DOKAN_MOUNT_ERROR:				LogLine(LOG_ERROR, tr("dokan: Can't assign a drive letter"));	break;
		case DOKAN_MOUNT_POINT_ERROR:		LogLine(LOG_ERROR, tr("dokan: Mount point error"));				break;
		default:							LogLine(LOG_ERROR, tr("dokan: Unknown error: %1").arg(Status));
	}

#else

    // We have a problem there does not seam to be a way to pass a argumetn to fusefs_oper.init
    //  So we use a static atomic object to pass one argument deterministically...
    while(m_new_inst.testAndSetOrdered(0, this))
        QThread::currentThread()->msleep(10);

    LogLine(LOG_SUCCESS, tr(" fuse server up and running ;-)"));

    // registers the operations
    // calls either the single-threaded or the multi-threaded event loop
    // fuse_loop() or fuse_loop_mt()
    if (fuse_loop_mt(m_fuse) < 0)
        LogLine(LOG_ERROR, tr("problem in fuse_loop_mt :'("));

#endif
}

#endif
