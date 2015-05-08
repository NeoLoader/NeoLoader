#pragma once
#include "../../Framework/ObjectEx.h"

#define LEAST_PRIORITY		1000

#define	PROP_PRIORITY		"Priority"

#define	PROP_LEVEL			"Level"

#define	PROP_TEMP			"Temp"
#define	PROP_REPETITION		"Repetition"

#define PROP_DESCRIPTION	"Description"
#define PROP_COVERURL		"CoverUrl"
#define PROP_BACKURL		"BackUrl"
#define PROP_BANNERURL		"BannerUrl"

#define PROP_RATING			"Rating"
#define PROP_QUALITY		"Quality"


class CFileDetails: public QObjectEx
{
	Q_OBJECT

public:
	CFileDetails(QObject* qObject = NULL);
	~CFileDetails();

	virtual void					Find();
	virtual bool					IsSearching();
	virtual void					Add(const QString& SourceID, const QVariantMap& Details);
	virtual QVariantMap&			Get(const QString& SourceID);
#ifndef NO_HOSTERS
	virtual void					Collect(bool bCollect = true);
#endif
	virtual bool					HasDetails(const QString& SourceID)						{return m_FileDetails.contains(SourceID);}
	virtual bool					IsEmpty();
	virtual void					Merge(CFileDetails* pDetails);
	virtual void					Clear();
	virtual QVariantList 			Dump();
	virtual QVariantMap 			Extract();

	virtual QMultiMap<QString, QVariantMap>& GetAll()										{return m_FileDetails;}

	virtual QString					GetID(const QString& Type);

	virtual void					Collect(const QSet<uint64>& Files);

#ifndef NO_HOSTERS
	virtual void					Add(const QString& SourceUrl);
	virtual void					Clr(const QString& SourceUrl);
	virtual QStringList				GetSourceUrl();
#endif

	virtual double					GetAvailStats()			{return m_Availability;}

	virtual QVariant				GetProperty(const QString& Name);

	/*virtual void					SetProperty(const QString& Name, const QVariant& Value)	{if(Value.isValid()) m_Properties.insert(Name, Value); else m_Properties.remove(Name);}
	v
	virtual QVariant				GetProperty(const QString& Name, const QVariant& Default = QVariant())	{return m_Properties.value(Name, Default);}
	virtual bool					HasProperty(const QString& Name)						{return m_Properties.contains(Name);}
	//virtual void					SetProperty(const QString& Name, const QVariant& Value)	{setProperty(Name.toLatin1(), Value);}
	//virtual QVariant				GetProperty(const QString& Name)						{return property(Name.toLatin1());}
	virtual QList<QString>			GetAllProperties()										{return m_Properties.uniqueKeys();}
	virtual QVariantMap&			GetProperties()											{return m_Properties;}*/

	// Load/Store
	virtual QVariantMap				Store();
	virtual void					Load(const QVariantMap& Details);

	static QString					ReadMediaInfo(QString FilePath);
	virtual void					MediaInfoRead(const QString& Data);

private slots:

signals:
	void							Update();

protected:
	//QVariantMap						m_Properties;

	QMultiMap<QString, QVariantMap>	m_FileDetails;
	QSet<QString>					m_DetailUrls;

	double							m_Availability;

#ifndef NO_HOSTERS
	bool							m_Collect;
#endif
};

typedef QSharedPointer<CFileDetails> CFileDetailsPtr;
