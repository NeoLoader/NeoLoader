#pragma once
#include "../../Framework/ObjectEx.h"

class CFile;
class CFileList;
class CFileDetails;
class CSearch;

#define CAT_SERIES		"Series"
#define CAT_MOVIES		"Movies"
#define CAT_VIDEO		"Video"
#define CAT_AUDIO		"Audio"
#define CAT_PICTURES	"Pictures"
#define CAT_DOCUMNTS	"Documents"
#define CAT_BOOKS		"Books"
#define CAT_MUSIC		"Music"
#define CAT_SOFTWARE	"Software"
#define CAT_GAMES		"Games"
#define CAT_ADULT		"Adult"
#define CAT_OTHER		"Other"

#define TYP_SERIES		"Series"
#define TYP_SEASON		"Season"
#define TYP_EPISODE		"Episode"
#define TYP_MOVIE		"Movie"

//#define TYP_SOFTWARE	"Software"

// Generic
#define PROP_NAME		"Name"
#define PROP_EXT_NAME	"ExtName"
#define PROP_CATEGORY	"Category"
#define PROP_TYPE		"Type"
#define PROP_PART		"Part"

// CAT_SOFTWARE
#define PROP_EDITION	"Edition"
#define PROP_VERSION	"Version"

// CAT_MOVIE
#define PROP_MOVIE		"Movie"

// CAT_SERIES
#define PROP_SERIES		"Series"
#define PROP_SEASON		"Season"
#define PROP_EPISODE	"Episode"

// CAT_MUSIC
//#define PROP_ARTIST		"Artist"
//#define PROP_ALBUM		"Album"
//#define PROP_TRACK		"Track"

// CAT_MOVIES CAT_VIDEO
#define PROP_YEAR		"Year"

class CCollection: public QObjectEx
{
	Q_OBJECT

public:
	enum EType
	{
		eGeneric = 0,
		eRoot,
		eCategory,
		eSoftware,
		eMovie,
		eSeries,
		eSeason,
		eEpisode,
		eOther,
		//eMusic,
	};

	CCollection(EType Type = eGeneric, QObject* qObject = NULL);
	~CCollection();

	virtual void					SetName(const QString& Name);
	virtual EType					GetType()												{return m_Type;}
	virtual QString					GetName()												{return m_Name;}
	virtual uint64					GetID()													{return m_ID;}
	static CCollection*				GetCollection(uint64 ID);

	virtual void					AddFile(CFile* pFile);
	virtual void					ArrangeFile(CFile* pFile);
	virtual QSet<uint64>			GetFiles()												{return m_Files;}
	virtual QSet<uint64>			GetAllFiles();

	virtual void					AddCollection(CCollection* pCollection)					{pCollection->setParent(this); m_Subs.insert(pCollection->GetName(), pCollection);}
	virtual QMap<QString, CCollection*> GetCollections()									{return m_Subs;}

	virtual QStringList				GetPath();
	virtual CCollection*			GetCollection(const QStringList& Path);
	virtual CCollection*			GetCollection(const QString& Key, EType Type = eGeneric);
	virtual CCollection*			GetCollection(const QVariantMap& Collection);

	//virtual void					Clear();
	//virtual int						CleanUp();
	bool							IsEmpty();

	//virtual void					MergeMatches(CFileList* pList, const QString& Property, EType Type = eGeneric);
	//virtual void					ArrangeFiles(CFileList* pList);

	//virtual void					SetFileDirs(CFileList* pList, const QString& Path);

	virtual CFileDetails*			GetDetails()						{return m_FileDetails;}

#ifndef NO_HOSTERS
	virtual void					AddCrawlUrls(const QStringList& CrawlUrls) {m_CrawlUrls.unite(CrawlUrls.toSet());}
	virtual bool					HasCrawlUrls()						{return !m_CrawlUrls.isEmpty();}
	virtual void					CrawlUrls();
#endif

	virtual QVariantMap				GetContent(bool& IsCrawling);

	virtual bool					IsCollecting();

	//virtual QVariant				GetProperty(const QString& Name);

	/*virtual void					SetProperty(const QString& Name, const QVariant& Value)	{if(Value.isValid()) m_Properties.insert(Name, Value); else m_Properties.remove(Name);}
	virtual QVariant				GetProperty(const QString& Name, const QVariant& Default = QVariant())	{return m_Properties.value(Name, Default);}
	virtual bool					HasProperty(const QString& Name)						{return m_Properties.contains(Name);}
	//virtual void					SetProperty(const QString& Name, const QVariant& Value)	{setProperty(Name.toLatin1(), Value);}
	//virtual QVariant				GetProperty(const QString& Name)						{return property(Name.toLatin1());}
	virtual QList<QString>			GetAllProperties()										{return m_Properties.uniqueKeys();}
	virtual QVariantMap&			GetProperties()											{return m_Properties;}*/

	virtual CSearch*				GetSearch();

	virtual void					ScheduleCollect();

	// Load/Store
	virtual QVariantMap				Store();
	virtual void					Load(const QVariantMap& Collection);

private slots:
	void							OnCollect();

protected:
	QString							m_Name;
	EType							m_Type;

	QMap<QString, CCollection*>		m_Subs;
	QSet<uint64>					m_Files;

	//QVariantMap						m_Properties;

	CFileDetails*					m_FileDetails;
	bool							m_bCollect;

#ifndef NO_HOSTERS
	QSet<QString>					m_CrawlUrls;
#endif 

	uint64							m_ID;
	static QMap<uint64, CCollection*>m_Map;
};
