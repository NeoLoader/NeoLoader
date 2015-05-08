#pragma once

#include "../FileList/FileList.h"
#include "SearchManager.h"

class CCollection;

class CAbstractSearch
{
public:
	virtual void					SetStarted(const QVariant& SearchID)					{m_SearchID = SearchID;}
	virtual void					SetStopped()											{m_SearchID.clear();}
	virtual const QVariant&			GetSearchID()											{return m_SearchID;}
	virtual bool					IsRunning()												{return m_SearchID.isValid();}

	virtual QString&				GetExpression() = 0;

	virtual void					SetCriteria(const QString& Name, const QVariant& Value)	{GetCriterias().insert(Name, Value);}
	virtual QVariant 				GetCriteria(const QString& Name, const QVariant& Default = QVariant())	{return GetCriterias().value(Name, Default);}
	virtual QVariantMap&			GetCriterias() = 0;
	virtual QList<QString>			GetAllCriterias()										{return GetCriterias().uniqueKeys();}
	virtual bool					IsAliasSearch()											{return GetCriterias().contains("Hash");}

	virtual bool					AddFoundFile(CFile* pFile) = 0;
	virtual int						GetResultCount() = 0;

	//virtual void					AddUrls(const QStringList& Urls) = 0;
	virtual void					AddCollection(const QString& SourceID, const QVariantMap& Collection, int Priority) = 0;

	virtual void					SetError(const QString& Error) {m_Error = Error;}
	virtual bool					HasError()					{return !m_Error.isEmpty();} 
	virtual const QString&			GetError()					{return m_Error;}
	virtual void					ClearError()				{m_Error.clear();}

	virtual void					SetResumePoint(const QVariant& ResumePoint)				{m_ResumePoint = ResumePoint;}
	virtual const QVariant&			GetResumePoint()										{return m_ResumePoint;}

protected:
	QVariant						m_SearchID; // Note this is not the local search ID but a remote one

	QVariant						m_ResumePoint;

	QString							m_Error;
};

class CSearch: public CFileList, public CAbstractSearch
{
	Q_OBJECT

public:
	CSearch(ESearchNet SearchNet, QObject* qObject = NULL);
	CSearch(ESearchNet SearchNet, const QString& Expression, const QVariantMap& Criteria, QObject* qObject = NULL);

	virtual void					Process(UINT Tick);

	virtual bool					AddFoundFile(CFile* pFile);
	virtual int						GetResultCount()										{return m_FileMap.size();}

	//virtual void					AddUrls(const QStringList& Urls) {}
	// Note: Normal searches does not handle collections yet, this needs to be added when neo kad will get collections support
	virtual void					AddCollection(const QString& SourceID, const QVariantMap& Collection, int Priority) {}

	virtual uint64					GrabbFile(CFile* pFile);

	virtual QString&				GetExpression()											{return m_Expression;}

	virtual ESearchNet				GetSearchNet()											{return m_SearchNet;}
	static QString					GetSearchNetStr(ESearchNet SearchNet);
	static ESearchNet				GetSearchNet(const QString& SearchNetStr);
	virtual QVariantMap&			GetCriterias()											{return m_Criteria;}
	virtual void					SetCriterias(const QVariantMap& Criteria)				{m_Criteria = Criteria;}

	// Load/Store
	virtual QVariant				Store();
	virtual int						Load(const QVariantMap& Data);

private slots:
	virtual void OnUpdate()			{}

protected:
	virtual void					MergeFiles(CFile* pFromFile, CFile* pToFile);

	ESearchNet						m_SearchNet;

	QString							m_Expression;
	QVariantMap						m_Criteria; // FileExt, Min/MaxSize, Ed2kFileType, artist, codec, bitrate, etc...
};

class CWebSearch: public CSearch
{
	Q_OBJECT
public:
	CWebSearch(ESearchNet SearchNet, QObject* qObject = NULL);
	CWebSearch(ESearchNet SearchNet, const QString& Expression, const QVariantMap& Criteria, QObject* qObject = NULL);

#ifndef NO_HOSTERS
	virtual void					AddCollection(const QString& SourceID, const QVariantMap& Collection, int Priority);

	//virtual void					AddUrls(const QStringList& Urls);
	virtual void					CrawlUrls(const QStringList& Urls, CCollection* pCollection = NULL);

protected:

	QSet<QString>					m_CrawledUrls;
#endif
};