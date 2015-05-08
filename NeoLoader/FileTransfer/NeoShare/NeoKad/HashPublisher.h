#pragma once

#include "FilePublisher.h"

class CHashPublisher: public CFilePublisher
{
	Q_OBJECT
public:
	CHashPublisher(CKadAbstract* pItf);

protected:
	virtual CKadAbstract::EIndex Index()	{return CKadAbstract::eHashes;}
	virtual char*			CodeID()		{return "FileRepository";}

	virtual QVariant		PublishEntrys(uint64 FileID, CFileHashPtr pHash);
	virtual bool			EntrysFound(uint64 FileID, const QVariantList& Results, bool bDone);
};