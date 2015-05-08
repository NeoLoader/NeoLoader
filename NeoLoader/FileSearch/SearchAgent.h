#pragma once
#include "Search.h"

class CCollection;

class CSearchProxy: public QObjectEx, public CAbstractSearch
{
	Q_OBJECT
public:
	CSearchProxy(ESearchNet	SearchNet, CSearch* pSearch)
	:QObjectEx(pSearch){
		m_SearchNet = SearchNet;
	}

	virtual ESearchNet				GetSearchNet()											{return m_SearchNet;}

	virtual QString&				GetExpression()											{return GetSearch()->GetExpression();}
	virtual QVariantMap&			GetCriterias()											{return GetSearch()->GetCriterias();}
	virtual bool					AddFoundFile(CFile* pFile)								{return GetSearch()->AddFoundFile(pFile);}
	virtual int						GetResultCount()										{return GetSearch()->GetResultCount();}

	//virtual void					AddUrls(const QStringList& Urls)						{return GetSearch()->AddUrls(Urls);}
	virtual void					AddCollection(const QString& SourceID, const QVariantMap& Collection, int Priority) {GetSearch()->AddCollection(SourceID, Collection, Priority);}

protected:
	CSearch*						GetSearch()												{return qobject_cast<CSearch*>(parent());}

	ESearchNet						m_SearchNet;
};


// Note: the Search Agent is derived from a WebSearch as we need to manager crawling peoperly
class CSearchAgent: public CWebSearch
{
	Q_OBJECT
public:
	CSearchAgent(ESearchNet SearchNet, QObject* qObject = NULL);
	CSearchAgent(ESearchNet SearchNet, const QString& Expression, const QVariantMap& Criteria, QObject* qObject = NULL);
	virtual void					Init();
	virtual void					InitRegExp();
	~CSearchAgent();

	virtual void					Process(UINT Tick);

	virtual bool					AddFoundFile(CFile* pFile);

	virtual void					Start(bool bMore = false);

	virtual void					AddCollection(const QString& SourceID, const QVariantMap& Collection, int Priority);

	virtual QVariantMap				Recognize(CFile* pFile);
	//virtual void					Recognize();

	virtual QList<CCollection*>		GetCollections();
	virtual QList<CCollection*>		GetAllCollections();

	// Load/Store
	virtual QVariant				Store();
	virtual int						Load(const QVariantMap& Data);

private:
	virtual void OnUpdate();//		{m_LastFileCount = 0;}

protected:
	void							Reset();

	CSearchProxy*					StartSearch(ESearchNet SearchNet);
#ifndef NO_HOSTERS
	CSearchProxy*					StartSearch(const QString& Site);
#endif

	QMap<QString, CSearchProxy*>	m_Searches;
	//int								m_LastFileCount;

	CCollection*					m_RootCollection;

private:
	QRegExp ExtExp;
	QVector<QRegExp> VidExps;

	QRegExp CamExp;
	QRegExp ScrExp;
	QRegExp DvdExp;
	QRegExp BDExp;
	QRegExp VodExp;
	QRegExp TVExp;

	QRegExp ResExp;
																																		
	
	QRegExp SeriesExp;
	QRegExp SeasonsExp;
	QRegExp AnimeExp;
	QRegExp MovieExp;
	QRegExp PartExp;
	//
	
	// Software
	//QRegExp VerExp;
	QRegExp VerExp;
	QRegExp BitExp;
	QRegExp EdExp;

	// Other
	//QRegExp VolExp;
	
	//QRegExp DiscExp;

	//QRegExp AlbumExp;
	//
};

QString FormatName(QString Name);