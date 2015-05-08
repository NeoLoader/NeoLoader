#pragma once

#include "FilePublisher.h"

class CLinkPublisher: public CFilePublisher
{
	Q_OBJECT
public:
#ifndef NO_HOSTERS
	CLinkPublisher(CKadAbstract* pItf);

protected:
	virtual CKadAbstract::EIndex Index()	{return CKadAbstract::eLinks;}
	virtual char*			CodeID()		{return "LinkSafe";}

	virtual QVariant		PublishEntrys(uint64 FileID, CFileHashPtr pHash);
	virtual bool			EntrysFound(uint64 FileID, const QVariantList& Results, bool bDone);
#endif
};