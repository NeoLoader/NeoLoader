#include "GlobalHeader.h"
#include "SearchManager.h"
#include "Search.h"
#include "SearchAgent.h"
#include "../NeoCore.h"
#include "../FileTransfer/NeoShare/NeoManager.h"
#include "../FileTransfer/NeoShare/NeoKad.h"
#include "../FileTransfer/ed2kMule/MuleManager.h"
#include "../FileTransfer/ed2kMule/MuleKad.h"
#include "../FileTransfer/ed2kMule/ServerClient/ServerList.h"
#ifndef NO_HOSTERS
#include "../FileTransfer/HosterTransfer/WebManager.h"
#include "../FileTransfer/HosterTransfer/WebCrawler.h"
#endif
#include "../Common/TuringTools.h"
#include "../../Framework/Xml.h"

CSearchManager::CSearchManager(QObject* qObject)
 : QObjectEx(qObject)
{
}

void CSearchManager::Process(UINT Tick)
{
	foreach(CSearch* pSearch, m_SearchList)
		pSearch->Process(Tick);
}

uint64 CSearchManager::AddSearch(CSearch* pSearch)
{
	uint64 SearchID = 0;
#ifdef _DEBUG
	do SearchID++;
#else
	do SearchID = GetRand64() & MAX_FLOAT;
#endif
	while(m_SearchList.contains(SearchID)); // statisticaly almost impossible but in case
	ASSERT(SearchID);

	m_SearchList.insert(SearchID, pSearch);
	return SearchID;
}

uint64 CSearchManager::StartSearch(ESearchNet SearchNet, const QString& Expression, const QVariantMap& Criteria)
{
	CSearch* pSearch;
	if(SearchNet == eSmartAgent)
		pSearch = new CSearchAgent(SearchNet, Expression, Criteria, this);
#ifndef NO_HOSTERS
	else if(SearchNet == eWebSearch)
		pSearch = new CWebSearch(SearchNet, Expression, Criteria, this);
#endif
	else 
		pSearch = new CSearch(SearchNet, Expression, Criteria, this);

	uint64 ID = AddSearch(pSearch);
	StartSearch(ID);
	return ID;
}

bool CSearchManager::StartSearch(uint64 SearchID, bool bMore)
{
	CSearch* pSearch = m_SearchList.value(SearchID);
	if(!pSearch)
		return false;

	if(!pSearch->IsRunning())
	{
		switch(pSearch->GetSearchNet())
		{
			case eSmartAgent:	((CSearchAgent*)pSearch)->Start(bMore);							break;
			case eNeoKad:		theCore->m_NeoManager->GetKad()->StartSearch(pSearch);			break;
			case eMuleKad:		theCore->m_MuleManager->GetKad()->StartSearch(pSearch);			break;
			case eEd2kServer:	theCore->m_MuleManager->GetServerList()->StartSearch(pSearch);	break;
#ifndef NO_HOSTERS
			case eWebSearch:	theCore->m_WebManager->GetCrawler()->StartSearch(pSearch);		break;
#endif
			default:		ASSERT(0);
		}

		if(pSearch->HasError())
		{
			LogLine(LOG_ERROR, tr("Couldnt start Search: %1").arg(pSearch->GetError()));
			return false;
		}
		ASSERT(pSearch->IsRunning());
	}

	return true;
}

void CSearchManager::StopSearch(uint64 SearchID)
{
	CSearch* pSearch = m_SearchList.take(SearchID);
	if(!pSearch)
		return;

	if(pSearch->IsRunning()) // is the search stilla ctive
	{
		switch(pSearch->GetSearchNet())
		{
			case eSmartAgent:	pSearch->SetStarted(0);											break;
			case eNeoKad:		theCore->m_NeoManager->GetKad()->StopSearch(pSearch);			break;
			case eMuleKad:		theCore->m_MuleManager->GetKad()->StopSearch(pSearch);			break;
			case eEd2kServer:	theCore->m_MuleManager->GetServerList()->StopSearch(pSearch);	break;
#ifndef NO_HOSTERS
			case eWebSearch:	theCore->m_WebManager->GetCrawler()->StopSearch(pSearch);		break;
#endif
			default:		ASSERT(0);
		}
	}

	delete pSearch;
}

uint64 CSearchManager::DiscoverContent(const QVariantMap& Request)
{
	QString Expression	= Request["Expression"].toString();
	QString Category	= Request["Category"].toString();
	QString SubCategory	= Request["SubCategory"].toString();
	QString Genre		= Request["Genre"].toString();
	QString Type		= Request["Type"].toString();
	QString Language	= Request["Language"].toString();
	QString Sorting		= Request["Sorting"].toString();
	QString Order		= Request["Order"].toString();
	bool Shallow		= Request["Shallow"].toBool();
	bool StreamsOnly	= Request["Streams"].toBool();

	QString DiscoveryStr =	"EXP:" + Expression + 
							"|CAT:" + Category + (SubCategory.isEmpty() ? "" : (";" + SubCategory)) + "|GEN:" + Genre + "|TYP:" + Type +
							"|LNG:" + Language + "|SRT:" + Sorting + "|ORD:" + Order;

	uint64 &ID = m_DiscoveryMap[DiscoveryStr];
	if(ID)
	{
		if(CSearch* pSearch = m_SearchList.value(ID)) // Note: the search may have been terminated
		{
			if(!pSearch->IsRunning() && Request["More"].toBool())
				StartSearch(ID, true); // find More
			return ID;
		}
	}

	QVariantMap Criteria;
	Criteria["Category"] = Category;
	Criteria["SubCategory"] = SubCategory;
	Criteria["Genre"] = Genre;
	Criteria["Type"] = Type;
	Criteria["Language"] = Language;
	Criteria["Sorting"] = Sorting;
	Criteria["Order"] = Order;
	Criteria["Shallow"] = Shallow;
	Criteria["Streams"] = StreamsOnly;
	ID = StartSearch(eSmartAgent, Expression, Criteria);

	return ID;
}

bool CSearchManager::FindMore(uint64 SearchID)
{
	if(CSearch* pSearch = m_SearchList.value(SearchID)) // Note: the search may have been terminated
	{
		if(pSearch->IsRunning())
			return true;
		return StartSearch(SearchID, true); // find More
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////////////
// Load/Store

void CSearchManager::StoreToFile()
{
	QVariantList SearchList;

	//for(QMap<uint64, CSearch*>::iterator I = m_SearchList.begin(); I != m_SearchList.end(); I++)
	foreach(CSearch* pSearch, m_SearchList)
	{
//#ifndef _DEBUG
		if(pSearch->HasError() || pSearch->GetExpression().isEmpty())
			continue;
//#endif

		SearchList.append(pSearch->Store());
	}

	CXml::Write(SearchList, theCore->Cfg()->GetSettingsDir() + "/SearchLists.xml");
}

void CSearchManager::LoadFromFile()
{
	QVariantList SearchList = CXml::Read(theCore->Cfg()->GetSettingsDir() + "/SearchLists.xml").toList();

	foreach(const QVariant& vSearch, SearchList)
	{
		QVariantMap Search = vSearch.toMap();
		ESearchNet SearchNet = CSearch::GetSearchNet(Search["SearchNet"].toString());

		CSearch* pSearch;
		if(SearchNet == eSmartAgent)
			pSearch = new CSearchAgent(SearchNet, this);
#ifndef NO_HOSTERS
		else if(SearchNet == eWebSearch)
			pSearch = new CWebSearch(SearchNet, this);
#endif
		else 
			pSearch = new CSearch(SearchNet, this);

		pSearch->Load(Search);
		foreach(CFile* pFile, pSearch->GetFiles())
		{
			if(pFile->IsStarted())
				pFile->Enable();
		}
		AddSearch(pSearch);
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// Search Expression Parsing, we use a Snippet from the Turing Engine and a few hacks to parse the query //
///////////////////////////////////////////////////////////////////////////////////////////////////////////

wstring GetString(SExpression** Expression)
{
	if(Expression && !(*Expression)->IsMulti())
	{
		if((*Expression)->IsString()) // Note: a String is wrapped in ""
			return (*Expression)->Exp.Word->substr(1, (*Expression)->Exp.Word->length() - 2);
		else
			return *(*Expression)->Exp.Word;
	}
	return L"";
}

void MakeBooleanTree(CExpressions* Expressions)
{
	if(Expressions->Count() == 0)
		return;

	int Index = 1;
	while(Expressions->Count() > Index)
	{
		SExpression** Expression = Expressions->GetExp(Index);
		if((*Expression)->IsOperator())
			Expressions->Del(Index); // all actual operators are irrelevant
		else if(!(*Expression)->IsMulti())
		{
			wstring Word = GetString(Expression);
			if(Word == L"AND" || Word == L"OR" || Word == L"NOT")
			{
				Index++;
				continue;
			}
		}
		int ToGo = Expressions->Count() - Index;
		if(ToGo > 1)
			Expressions->SubordinateExp(Index, ToGo);
		break;
	}

	SExpression** ppExpL = Expressions->GetExp(0);
	if((*ppExpL)->IsMulti())
		MakeBooleanTree((*ppExpL)->Exp.Multi);
	else
		Expressions->SubordinateExp(0, 1);
	SExpression** ppExpR = Expressions->GetExp(Index);
	if(ppExpR && (*ppExpR)->IsMulti())
		MakeBooleanTree((*ppExpR)->Exp.Multi);
	else
		Expressions->SubordinateExp(Index, 1);
}

SSearchTree* ConvertExpressions(CExpressions* Expressions)
{
	SSearchTree* pSearchTree = new SSearchTree();
	SExpression** ppExpL = 0;
	SExpression** ppExpR = 0;
	switch(Expressions->Count())
	{
		case 1:
		{
			SExpression** ppExp = Expressions->GetExp(0);
			while((*ppExp)->IsMulti())
				ppExp = (*ppExp)->Exp.Multi->GetExp(0);
			if(ppExp)
			{
				pSearchTree->Type = SSearchTree::String;
				pSearchTree->Strings.push_back(GetString(ppExp));
			}
			break;
		}
		case 2: // implicit AND
		{
			ppExpL = Expressions->GetExp(0);
			pSearchTree->Type = SSearchTree::AND;
			ppExpR = Expressions->GetExp(1);
			break;
		}
		case 3:
		{
			ppExpL = Expressions->GetExp(0);
			wstring Word = GetString(Expressions->GetExp(1));
			if(Word == L"AND")
				pSearchTree->Type = SSearchTree::AND;
			else if(Word == L"OR")
				pSearchTree->Type = SSearchTree::OR;
			else if(Word == L"NOT")
				pSearchTree->Type = SSearchTree::NAND;
			ppExpR = Expressions->GetExp(2);
			break;
		}
		default:
		ASSERT(0);
	}
	if(ppExpL)
		pSearchTree->Left = ConvertExpressions((*ppExpL)->Exp.Multi);
	if(ppExpR)
		pSearchTree->Right = ConvertExpressions((*ppExpR)->Exp.Multi);
	return pSearchTree;
}

bool SimplifyTree(SSearchTree* pSearchTree)
{
	if(!pSearchTree)
		return false;
	if(pSearchTree->Type == SSearchTree::String)
		return true;
	
	bool L = SimplifyTree(pSearchTree->Left);
	bool R = SimplifyTree(pSearchTree->Right);
	if(!L || pSearchTree->Type != SSearchTree::AND || !R)
		return false;
	
	pSearchTree->Type = SSearchTree::String;
	pSearchTree->Strings.insert(pSearchTree->Strings.end(), pSearchTree->Left->Strings.begin(), pSearchTree->Left->Strings.end());
	pSearchTree->Strings.insert(pSearchTree->Strings.end(), pSearchTree->Right->Strings.begin(), pSearchTree->Right->Strings.end());
	return true;
}

wstring PrintSearchTree(SSearchTree* pSearchTree)
{
	if(!pSearchTree)
		return L"";
	wstring String;
	if(pSearchTree->Type == SSearchTree::String)
	{
		String += L" ";
		for(int i = 0; i < pSearchTree->Strings.size(); i++)
			String += pSearchTree->Strings[i] + L" ";
	}
	else
	{
		String += L" (";
		String += PrintSearchTree(pSearchTree->Left);
		if(pSearchTree->Type == SSearchTree::AND)
			String += L"AND";
		else if(pSearchTree->Type == SSearchTree::OR)
			String += L"OR";
		else if(pSearchTree->Type == SSearchTree::NAND)
			String += L"NOT";
		String += PrintSearchTree(pSearchTree->Right);
		String += L") ";
	}
	return String;
}

SSearchTree* CSearchManager::MakeSearchTree(const QString& Expression)
{
	CExpressions* Expressions = CTuringParser::GetExpressions(Expression.toStdWString());
	if(!Expressions)
		return NULL;

	MakeBooleanTree(Expressions);
	//wstring Debug = CTuringParser::PrintExpressions(Expressions);

	SSearchTree* pSearchTree = ConvertExpressions(Expressions);
	delete Expressions; Expressions = NULL;

	//wstring Temp = PrintSearchTree(pSearchTree);
	SimplifyTree(pSearchTree);
	//wstring Final = PrintSearchTree(pSearchTree);

	return pSearchTree;
}

QVariant SSearchTree::ToVariant()
{
	QVariantMap Map;

	if(Type == AND)
		Map["Type"] = "AND";
	else if(Type == OR)
		Map["Type"] = "OR";
	else if(Type == NAND)
		Map["Type"] = "NOT";
	else
	{
		Map["Type"] = "String";

		QStringList StringList;
		for(int i = 0; i < Strings.size(); i++)
			StringList.append(QString::fromStdWString(Strings[i]));
		Map["Value"] = StringList;
	}

	if(Left)
		Map["Left"] = Left->ToVariant();
	if(Right)
		Map["Right"] = Right->ToVariant();

	return Map;
}

void SSearchTree::FromVariant(QVariant Variant)
{
	QVariantMap Map = Variant.toMap();

	if(Map["Type"] == "AND")
		Type = AND;
	else if(Map["Type"] == "OR")
		Type = OR;
	else if(Map["Type"] == "NOT")
		Type = NAND;
	else if(Map["Type"] == "String")
	{
		Type = String;
		foreach(const QString& Value, Map["Value"].toStringList())
			Strings.push_back(Value.toStdWString());
	}

	if(Map.contains("Left"))
	{
		Left = new SSearchTree();
		Left->FromVariant(Map["Left"]);
	}
	if(Map.contains("Right"))
	{
		Right = new SSearchTree();
		Right->FromVariant(Map["Right"]);
	}
}
