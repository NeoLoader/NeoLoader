#pragma once

#include "FilePublisher.h"

class CRatingPublisher: public CFilePublisher
{
	Q_OBJECT
public:
	CRatingPublisher(CKadAbstract* pItf);

protected:
	virtual CKadAbstract::EIndex Index()	{return CKadAbstract::eRating;}
	virtual char*			CodeID()		{return "RatingAgent";}

	virtual QVariant		PublishEntrys(uint64 FileID, CFileHashPtr pHash);
	virtual bool			EntrysFound(uint64 FileID, const QVariantList& Results, bool bDone);
};