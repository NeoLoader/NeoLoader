#pragma once

#include "KadPublisher.h"

struct SKadSearch
{
	SKadSearch(uint64 ID, const QByteArray& lookupid, const QString& function) {LookupID = lookupid; FileID = ID; Function = function;}
	QByteArray LookupID;
	uint64 FileID;
	QString Function;
};

class CFilePublisher: public QObjectEx
{
	Q_OBJECT
public:
	CFilePublisher(CKadAbstract* pItf);
	~CFilePublisher();

	virtual void			Process(UINT Tick);

	bool					Find(uint64 FileID, CFileHash* pFileHash, const QString& Function, const QString& FileName);
	bool					IsFinding(uint64 FileID, const QString& Function);

	int						GetSearchCount()		{return m_Searches.size();}

protected:
	CKadAbstract*			Itf()					{return qobject_cast<CKadAbstract*>(parent());}

	virtual CKadAbstract::EIndex Index() = 0;
	virtual char*			CodeID() = 0;

	virtual QVariant		PublishEntrys(uint64 FileID, CFileHashPtr pHash) = 0;
	virtual bool			EntrysFound(uint64 FileID, const QVariantList& Results, bool bDone) = 0;

	struct SHPub: CKadAbstract::SPub
	{
		SHPub(const QByteArray& ID, CFileHashPtr Ptr): CKadAbstract::SPub(ID) {pHash = Ptr;}
		CFileHashPtr pHash;
	};

	struct SPub
	{
		~SPub() {qDeleteAll(Lookups);}
		QList<SHPub*> Lookups;
	};

	QMap<uint64, SPub*>		m_Publishments;

	uint64					m_LastFileID;

	QMultiMap<uint64, SKadSearch*> m_Searches;
};