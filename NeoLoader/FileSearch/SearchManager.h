#pragma once

#include "../../Framework/ObjectEx.h"
class CSearch;
class CSearchAgent;

enum ESearchNet
{
	eInvalid,
	eSmartAgent,
	eNeoKad,
	eMuleKad,
	eEd2kServer,
	eWebSearch
};

struct SSearchTree
{
	SSearchTree(){
		Type = AND;
		Left = NULL;
		Right = NULL;
	}
	~SSearchTree()
	{
		delete Left;
		delete Right;
	}
	
	enum ESearchTermType {
		AND,
		OR,
		NAND,
		String,
	}				Type;
	vector<wstring>	Strings;
	SSearchTree*	Left;
	SSearchTree*	Right;

	QVariant ToVariant();
	void	 FromVariant(QVariant Variant);
};

class CSearchManager: public QObjectEx
{
	Q_OBJECT

public:
	CSearchManager(QObject* qObject = NULL);

	void			Process(UINT Tick);

	uint64			StartSearch(ESearchNet SearchNet, const QString& Expression, const QVariantMap& Criteria);
	bool			StartSearch(uint64 SearchID, bool bMore = false);
	void			StopSearch(uint64 SearchID);

	QList<uint64>	GetSearchIDs()				{return m_SearchList.uniqueKeys();}
	CSearch*		GetSearch(uint64 SearchID)	{return m_SearchList.value(SearchID);}

	uint64			DiscoverContent(const QVariantMap& Request);

	bool			FindMore(uint64 SearchID);

	// Load/Store
	void			StoreToFile();
	void			LoadFromFile();

	static SSearchTree*	MakeSearchTree(const QString& Expression);

protected:
	uint64			AddSearch(CSearch* pSearch);

	QMap<QString, uint64>	m_DiscoveryMap;

	QMap<uint64, CSearch*>	m_SearchList;
};
