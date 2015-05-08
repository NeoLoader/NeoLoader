#pragma once

#include "../FileList/PartMap.h"
#include "../FileList/File.h"
#include "../../Framework/ObjectEx.h"
#include "../../Framework/Address.h"

class CFile;
class CHosterLink;
class CTorrentPeer;
class CBandwidthLimit;
class CP2PClient;

struct SAddressCombo
{
	SAddressCombo() 
	 : IPv4(CAddress::IPv4), IPv6(CAddress::IPv6), Prot(CAddress::None) 
	{}

	void			SetIP(const SAddressCombo& From);
	void			SetIP(const CAddress& IP);
	void			AddIP(const CAddress& IP);
	CAddress		GetIP() const;

	bool			CompareTo(const CAddress& Address) const;
	bool			CompareTo(const SAddressCombo &Other) const;

	bool			HasV4() const {return !IPv4.IsNull();}
	bool			HasV6()	const {return !IPv6.IsNull();}

	CAddress		IPv4;
	CAddress		IPv6;
	CAddress::EAF	Prot;
};

template <size_t size>
struct SUID
{
	SUID()
	{
		Empty = true;
	}
	SUID(const QByteArray& Array)
	{
		if(!(Empty = Array.size() != size))
			memcpy(Data, Array.data(), size);
	}

	void Set(byte* pData)
	{
		Empty = false;
		memcpy(Data, pData, size);
	}
	QByteArray ToArray(bool bStatic = false) const
	{
		if(Empty)
			return QByteArray();
		return bStatic ? QByteArray((char*)Data, size) : QByteArray::fromRawData((char*)Data, size);
	}

	bool IsEmpty() const {return Empty;}
	bool operator!=(const SUID &Other) const {return !(*this == Other);}
	bool operator==(const SUID &Other) const 
	{
		if(Empty != Other.Empty)
			return false;
		if(Empty)
			return true;
		return memcmp(Data, Other.Data, size) == 0;
	}
	bool operator<(const SUID &Other) const
	{
		if(Empty != Other.Empty)
			return Empty; // if this one is empty is the smaller one
		if(Empty) // thise are booth empty so non is smaller
			return false;
		return memcmp(Data, Other.Data, size) < 0;
	}

	bool Empty;
	byte Data[size];
};

enum EFoundBy
{
	eOther = 0,
	eSelf,
	eStored,
	eNeo,
	eKad,
	eDHT,
	eXS,
	ePEX,
	eEd2kSvr,
	eTracker,
	eGrabber
};

enum ETransferType
{
	eTypeUnknown = 0,
	eNeoShare,
	eHosterLink,
	eBitTorrent,
	eEd2kMule
};

struct STransferStats
{
	STransferStats() 
	 : UploadedTotal(0), UploadedSession(0)
	 , DownloadedTotal(0), DownloadedSession(0) {}

	void AddUpload(uint64 Amount) {UploadedTotal += Amount; UploadedSession += Amount;}
	void AddDownload(uint64 Amount) {DownloadedTotal += Amount; DownloadedSession += Amount;}

	uint64		UploadedTotal;
	uint64		UploadedSession;
	uint64		DownloadedTotal;
	uint64		DownloadedSession;
};

class CTransfer: public QObjectEx
{
	Q_OBJECT

public:
	static QString					GetTypeStr(const CTransfer* pTransfer);
	static QString					GetTypeStr(const CP2PClient* pClient);
	static CTransfer*				New(const QString& Type);
	static CTransfer*				FromUrl(const QString& Url, CFile* pFile);

	CTransfer(CFile* pFile = NULL);
	virtual ~CTransfer();

	virtual bool					Process(UINT Tick) = 0;

	virtual bool					IsConnected() const = 0;

	virtual QString 				GetUrl() const = 0;	// contact data; http link, ip address+port, tor address, etc
	virtual QString 				GetDisplayUrl() const		{return GetUrl();}
	
	virtual void					Hold(bool bDelete) = 0;

	virtual	bool					IsUpload() const = 0;
	virtual	bool					IsWaitingUpload() const = 0;
	virtual	bool					IsActiveUpload() const = 0;

	virtual	bool					IsDownload() const = 0;
	virtual bool					IsInteresting() const = 0;
	virtual	bool					IsWaitingDownload() const = 0;
	virtual	bool					IsActiveDownload() const = 0;

	virtual ETransferType			GetType() = 0;
	virtual QString					GetTypeStr();

	virtual void					SetError(const QString& Error) {m_Error = Error;}
	virtual bool					HasError() const			{return !m_Error.isEmpty();}
	virtual const QString&			GetError() const			{return m_Error;} // If an unrecoverable error is set, all transfers are forced to deactivated untill clerance
	virtual void					ClearError()				{m_Error.clear();}

	virtual void					SetFile(CFile* pFile);
	virtual CFile*					GetFile() const				{CFile* pFile = qobject_cast<CFile*>(parent()); return pFile;}

	virtual CShareMap*				GetPartMap()				{return m_Parts.data();}
	virtual CShareMapPtr			GetPartMapPtr()				{return m_Parts;}

	virtual CBandwidthLimit*		GetUpLimit() = 0;
	virtual CBandwidthLimit*		GetDownLimit() = 0;

	virtual bool					SheduleRange(uint64 uBegin = 0, uint64 uEnd = -1, bool bStream = false);
	virtual void 					ReleaseRange(uint64 uBegin = 0, uint64 uEnd = -1, bool bAll = true);

	virtual void					RangeReceived(uint64 uBegin, uint64 uEnd);

	virtual CFileHashPtr			GetHash() = 0;

	virtual bool					StartUpload() = 0;

	virtual qint64					UploadedBytes()				{return 0;}	// uploaded bytes to client (write to socket)
	virtual qint64					DownloadedBytes()			{return 0;} // downloaded bytes form client (read form socket)

	virtual QString					GetFoundByStr() const;
	virtual	void					SetFoundBy(EFoundBy FoundBy){m_FoundBy = FoundBy;}
	virtual	EFoundBy				GetFoundBy()				{return m_FoundBy;}

	virtual bool					IsSeed() const = 0;

	virtual bool					IsChecked() const = 0;

	virtual void					ResetSchedule() = 0;

	// Load/Store
	virtual QVariantMap				Store() = 0;
	virtual bool					Load(const QVariantMap& Transfer) = 0;

protected:
	friend class CDownloadManager;
	friend class CWebDownload;
	virtual void					ReserveRange(uint64 uBegin, uint64 uEnd, uint32 uState);
	virtual void					UnReserveRange(uint64 uBegin, uint64 uEnd, uint32 uState);

	virtual bool					CheckInterest();

	CShareMapPtr					m_Parts;

	EFoundBy						m_FoundBy;

	QString							m_Error;
};

class CP2PTransfer: public CTransfer
{
	Q_OBJECT

public:
	CP2PTransfer();
	virtual ~CP2PTransfer();

	virtual bool					Process(UINT Tick);

	virtual bool					Connect() = 0;
	virtual void					Disconnect() = 0;

	virtual QString					GetConnectionStr() const = 0;

	virtual bool					SheduleDownload();

	virtual bool					SheduleRange(uint64 uBegin = 0, uint64 uEnd = -1, bool bStream = false);
	virtual void 					ReleaseRange(uint64 uBegin = 0, uint64 uEnd = -1, bool bAll = true);

	virtual void					RangeReceived(uint64 uBegin, uint64 uEnd, QByteArray Data);
	virtual	void					CancelRequest(uint64 uBegin, uint64 uEnd);

	virtual void					ResetSchedule()						{ReleaseRange(0, -1, false);}

	virtual void 					PartsAvailable(const QBitArray &Parts, uint64 uPartSize, uint64 uBlockSize = 0);
	//virtual void					RangesAvailable(const CShareMap* Ranges);

	virtual double					GetProbabilityRange();

	virtual uint64					GetAvailableSize()					{return m_AvailableBytes;}

	virtual void					StopUpload(bool bStalled = false) = 0;

	virtual qint64					UploadedBytes()						{return m_UploadedBytes;}
	virtual qint64					DownloadedBytes()					{return m_DownloadedBytes;}

	virtual qint64					GetFirstSeen()						{return m_FirstSeen;}

	virtual qint64					PendingBytes() const = 0;
	virtual qint64					PendingBlocks() const = 0;
	virtual qint64					LastUploadedBytes() const = 0;
	virtual qint64					UploadDuration() const				{return m_UploadStartTime ? GetCurTick() - m_UploadStartTime : 0;}
	virtual bool					IsBlocking() const = 0;
	virtual qint64					ReservedBytes() const				{return m_ReservedSize;}
	virtual qint64					RequestedBlocks() const = 0;
	virtual qint64					LastDownloadedBytes() const = 0;
	virtual qint64					DownloadDuration() const			{return m_DownloadStartTime ? GetCurTick() - m_DownloadStartTime : 0;}

	virtual bool					SupportsHostCache() const = 0;
	virtual QPair<const byte*, size_t> GetID() const = 0;

	virtual QString					GetSoftware() const					{return "";}
	
public slots:
	virtual void					OnBytesWritten(qint64 Bytes)		{m_UploadedBytes += Bytes; if(CFile* pFile = GetFile()) pFile->OnBytesWritten(Bytes);}
    virtual void					OnBytesReceived(qint64 Bytes)		{m_DownloadedBytes += Bytes; if(CFile* pFile = GetFile()) pFile->OnBytesReceived(Bytes);}

protected:
	virtual void					ReserveRange(uint64 uBegin, uint64 uEnd, uint32 uState);
	virtual void					UnReserveRange(uint64 uBegin, uint64 uEnd, uint32 uState);

	virtual void					RequestBlocks(bool bRetry = false) = 0;

	uint64							m_FirstSeen;

	uint64							m_ReservedSize;
	//uint64							m_NextReserve;
	//int								m_EndGameLevel;

	uint64							m_AvailableBytes;

	uint64							m_UploadedBytes;
	uint64							m_UploadStartTime;
	uint64							m_DownloadedBytes;
	uint64							m_DownloadStartTime;
};

//#define REQ_DEBUG
