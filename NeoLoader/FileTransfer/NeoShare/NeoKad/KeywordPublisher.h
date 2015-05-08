#pragma once

#include "KadPublisher.h"

class CKeywordPublisher: public QObjectEx
{
	Q_OBJECT
public:
	CKeywordPublisher(CKadAbstract* pItf);
	~CKeywordPublisher();

	virtual void			Process(UINT Tick);

	QVariant 				PublishEntrys(const QString& Keyword, const QList<uint64>& Files);

protected:
	CKadAbstract*			Itf()					{return qobject_cast<CKadAbstract*>(parent());}

	struct SPub: CKadAbstract::SPub
	{
		SPub(const QByteArray& ID): CKadAbstract::SPub(ID) {}
		QList<uint64>	Files;
	};
	QMap<QString, SPub*>	m_Publishments;

	QString					m_LastKeyword;
};