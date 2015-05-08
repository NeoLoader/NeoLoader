#pragma once

#include "FilePublisher.h"

class CSourcePublisher: public CFilePublisher
{
	Q_OBJECT
public:
	CSourcePublisher(CKadAbstract* pItf);

protected:
	virtual CKadAbstract::EIndex Index()	{return CKadAbstract::eSources;}
	virtual char*			CodeID()		{return "SourceTracker";}

	virtual QVariant		PublishEntrys(uint64 FileID, CFileHashPtr pHash);
	virtual bool			EntrysFound(uint64 FileID, const QVariantList& Results, bool bDone);
};