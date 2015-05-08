#pragma once

#include "../../../../Framework/ObjectEx.h"
#include "../../../FileList/Hashing/FileHash.h"

class CFile;
struct SNeoEntity;
struct SMuleSource;
struct STorrentPeer;

class CKeywordPublisher;
class CHashPublisher;
class CRatingPublisher;
class CSourcePublisher;
#ifndef NO_HOSTERS
class CLinkPublisher;
#endif

struct SKadFile
{
	SKadFile(uint64 ID) {FileID = ID;}

	uint64						FileID;
	QMap<QString,QPair<time_t, int> > Keywords;
};

struct SFileInto
{
	QString						FileName;
	uint64						FileSize;
	QString						FileDirectory;
	QVariantMap					HashMap;
};

struct SKadKeyword
{	
	SKadKeyword(const QString& keyword) {Keyword = keyword;}

	QString						Keyword;
	QList<SKadFile*>			Files;
};

class CKadAbstract: public QObjectEx
{
	Q_OBJECT
public:
	CKadAbstract(QObject* qObject = NULL);
	~CKadAbstract();

	virtual void				Process(UINT Tick);

	uint64						MkPubID(uint64 FileID);
	static QByteArray			MkTargetID(const QByteArray& Hash);

	QString						MkTopFileKeyword();

	static QMultiMap<EFileHashType, CFileHashPtr> ReadHashMap(const QVariantMap& HashMap, uint64 uFileSize);

protected:
	friend class CKeywordPublisher;
	friend class CFilePublisher;
	friend class CHashPublisher;
	friend class CRatingPublisher;
	friend class CSourcePublisher;
#ifndef NO_HOSTERS
	friend class CLinkPublisher;
#endif


	enum EIndex
	{
		eKeyword,
		eHashes,
		eRating,
		eSources,
#ifndef NO_HOSTERS
		eLinks,
#endif
		eHubs
	};

	struct SPub
	{
		SPub(const QByteArray& ID): StoreCount(0), ExpirationTime(0) {LookupID = ID;}
		QByteArray		LookupID;
		int				StoreCount;
		time_t			ExpirationTime;
	};

	void						AddFile(SKadFile* pFile);
	void						RemoveFile(uint64 FileID);

	const QMap<uint64, SKadFile*>&	GetFiles()			{return m_Files;}
	const QMap<QString, SKadKeyword*>& GetKeywords()	{return m_Keywords;}

	virtual int					MaxLookups();

	QByteArray					StartLookup(const QVariant& Request);
	void						StopLookup(const QByteArray& LookupID);
	bool						HandleLookup(const QByteArray& LookupID, time_t& ExpirationTime, int& StoreCount);
		
	// Interface BEGIN
	virtual QPair<time_t, int>	GetKwrdInitStats(uint64 ID) = 0;
    typedef QMultiMap<EFileHashType, CFileHashPtr> THashMultiMap;
    virtual THashMultiMap   	IsOutdated(uint64 ID, EIndex eIndex) = 0;
    virtual void				Update(uint64 ID, EIndex eIndex, time_t ExpirationTime, int StoreCount, const THashMultiMap& Hashes = THashMultiMap()) = 0;

	virtual SFileInto			GetFileInfo(uint64 ID, uint64 ParentID = 0) = 0;

	virtual bool				IsComplete(uint64 ID) = 0;

	virtual QVariantMap			GetRating(uint64 ID) = 0;
	virtual	void				AddRatings(uint64 ID, const QVariantList& Ratings, bool bDone) = 0;

#ifndef NO_HOSTERS
	virtual QStringList			GetLinks(uint64 ID, CFileHashPtr pHash) = 0;
	virtual void				AddLinks(uint64 ID, const QStringList& Urls) = 0;
#endif

	virtual void				AddSources(uint64 ID, const QMultiMap<QString, SNeoEntity>& Neos, const QList<SMuleSource>& Mules, const QMultiMap<QByteArray, STorrentPeer>& Peers) = 0;

	virtual QMultiMap<EFileHashType, CFileHashPtr> GetHashes(uint64 ID) = 0;
	virtual QVariantMap			GetHashMap(uint64 ID);
	virtual void				AddAuxHashes(uint64 ID, uint64 uFileSize, const QVariantMap& HashMap) = 0;

	virtual QList<uint64>		GetSubFiles(uint64 ID) = 0; // Note: this must always return in metadata order!!!
	virtual QList<uint64>		GetParentFiles(uint64 ID) = 0;
	virtual void				SetupIndex(uint64 ID, const QString& FileName, uint64 uFileSize, const QList<SFileInto>& SubFiles) = 0;
	// Interface END

	QMap<uint64, SKadFile*>		m_Files;
	QMap<QString, SKadKeyword*>	m_Keywords;

	CKeywordPublisher*			m_KeywordPublisher;
	CHashPublisher*				m_HashPublisher;
	CRatingPublisher*			m_RatingPublisher;
	CSourcePublisher*			m_SourcePublisher;
#ifndef NO_HOSTERS
	CLinkPublisher*				m_LinkPublisher;
#endif
};

/////////////////////////////////////////////////////////////////////////////////////////////////////
//

class CKadPublisher: public CKadAbstract
{
	Q_OBJECT
public:
	CKadPublisher(QObject* qObject = NULL);

	virtual void				Process(UINT Tick);

	void						ResetFile(CFile* pFile);

	bool						FindHash(CFile* pFile, bool bForce = false);
	bool						FindIndex(CFile* pFile, bool bForce = false);
	bool						FindAliases(CFile* pFile, bool bForce = false);
	bool						FindSources(CFile* pFile, bool bForce = false);
	bool						FindRating(CFile* pFile, bool bForce = false);
	bool						IsFindingRating(CFile* pFile);

	static CFileHash*			SelectTopHash(CFile* pFile);

protected:
	friend class CCoreServer;

	// Interface BEGIN
	virtual QPair<time_t, int>	GetKwrdInitStats(uint64 ID);
    virtual THashMultiMap       IsOutdated(uint64 ID, EIndex eIndex);
    virtual void				Update(uint64 ID, EIndex eIndex, time_t ExpirationTime, int StoreCount, const THashMultiMap& Hashes = THashMultiMap());

	virtual SFileInto			GetFileInfo(uint64 ID, uint64 ParentID = 0);

	virtual bool				IsComplete(uint64 ID);

	virtual QVariantMap			GetRating(uint64 ID);
	virtual	void				AddRatings(uint64 ID, const QVariantList& Ratings, bool bDone);

#ifndef NO_HOSTERS
	virtual QStringList			GetLinks(uint64 ID, CFileHashPtr pHash);
	virtual void				AddLinks(uint64 ID, const QStringList& Urls);
#endif

	virtual void				AddSources(uint64 ID, const QMultiMap<QString, SNeoEntity>& Neos, const QList<SMuleSource>& Mules, const QMultiMap<QByteArray, STorrentPeer>& Peers);

	virtual QMultiMap<EFileHashType, CFileHashPtr> GetHashes(uint64 ID);
	virtual void				AddAuxHashes(uint64 ID, uint64 uFileSize, const QVariantMap& HashMap);

	virtual QList<uint64>		GetSubFiles(uint64 ID); // Note: this must always return in metadata order!!!
	virtual QList<uint64>		GetParentFiles(uint64 ID);
	virtual void				SetupIndex(uint64 ID, const QString& FileName, uint64 uFileSize, const QList<SFileInto>& SubFiles);
	// Interface END
		
	QMap<uint64, QPointer<CFile> >	m_FileList;

	time_t						m_LastTopPublishment;
	QByteArray					m_TopLookupID;
};
