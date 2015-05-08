#include "GlobalHeader.h"
#include "SearchAgent.h"
#include "Search.h"
#include "SearchManager.h"
#include "../FileList/File.h"
#include "../FileList/FileDetails.h"
#include "../FileList/FileStats.h"
#include "../FileTransfer/HashInspector.h"
#include "../FileList/FileManager.h"
#include "../FileList/Hashing/UntrustedFileHash.h"
#include "../NeoCore.h"
#include "../FileTransfer/BitTorrent/Torrent.h"
#include "../FileTransfer/BitTorrent/TorrentInfo.h"
#include "../FileTransfer/NeoShare/NeoManager.h"
#include "../FileTransfer/NeoShare/NeoKad.h"
#include "../FileTransfer/ed2kMule/MuleManager.h"
#include "../FileTransfer/ed2kMule/MuleKad.h"
#include "../FileTransfer/ed2kMule/ServerClient/ServerList.h"
#ifndef NO_HOSTERS
#include "../FileTransfer/HosterTransfer/WebManager.h"
#include "../FileTransfer/HosterTransfer/WebCrawler.h"
#endif
#include "FileTypes.h"
#include "Collection.h"

CSearchAgent::CSearchAgent(ESearchNet SearchNet, QObject* qObject)
 : CWebSearch(SearchNet, qObject)
{
	Init();
}

CSearchAgent::CSearchAgent(ESearchNet SearchNet, const QString& Expression, const QVariantMap& Criteria, QObject* qObject)
 : CWebSearch(SearchNet, Expression, Criteria, qObject)
{
	Init();
}

void CSearchAgent::Init()
{
	//m_LastFileCount = 0;

	InitRegExp();
	m_RootCollection = new CCollection(CCollection::eRoot, this);
}

CSearchAgent::~CSearchAgent()
{
	// this agent is the parent of all the search proxys, those we need to stop al stilla ctive searches if we delete it
	Reset();
}

// foFx
bool ListContainsList(const QStringList& Where, const QStringList& What)
{
	foreach(const QString& Str, What)
	{
		if(Where.contains(Str, Qt::CaseInsensitive))
			return true;
	}
	return false;
}

void CSearchAgent::Process(UINT Tick)
{
	CSearch::Process(Tick);

	//if(m_LastFileCount != m_FileMap.size() && (Tick & EPerSec) != 0)
	//{
	//	Recognize();
	//
	//	m_LastFileCount = m_FileMap.size();
	//}

	if(IsRunning())
	{
		int ActiveCount = 0;
		foreach(CSearchProxy* pSearch, m_Searches)
		{
			if(pSearch->IsRunning())
				ActiveCount++;
		}

		if(ActiveCount == 0)
			SetStopped();
	}
}

bool CSearchAgent::AddFoundFile(CFile* pFile)
{
	if(!CSearch::AddFoundFile(pFile))
		return false;

	pFile->GetDetails()->Add("", Recognize(pFile)); // this triggers OnUpdate and arangement
	return true;
}

void CSearchAgent::OnUpdate()
{
	CFileDetails* pDetails = (CFileDetails*)sender();
	CFile* pFile = (CFile*)pDetails->parent();

	m_RootCollection->ArrangeFile(pFile);
}

void CSearchAgent::Start(bool bMore)
{
	if(!bMore)
		Reset();

	SetStarted((uint64)(void*)this);

	QStringList Type = SplitStr(GetCriteria("Type").toString(), "|");

	CSearchProxy* pNeoSearch = m_Searches.value("NeoKad");
	if(!pNeoSearch && theCore->m_NeoManager->GetKad()->IsConnected())
		pNeoSearch = StartSearch(eNeoKad);

	if(Type.isEmpty() || Type.contains("ed2k"))
	{
		CSearchProxy* pMuleSearch = m_Searches.value("MuleKad");
		if(!pMuleSearch && theCore->m_MuleManager->GetKad()->IsConnected())
			pMuleSearch = StartSearch(eMuleKad);

		CSearchProxy* pEd2kSearch = m_Searches.value("Ed2kServer");
		if(!pEd2kSearch) // && theCore->m_MuleManager->GetServerList()->GetConnectedServer() != NULL)
			pEd2kSearch = StartSearch(eEd2kServer);
	}
	
#ifndef NO_HOSTERS
	foreach(CWebScriptPtr pScript, theCore->m_WebManager->GetAllScripts())
	{
		if(!pScript->GetAPIs().contains("Search"))
			continue;

		QStringList CurTypes = SplitStr(pScript->GetHeaderTag("Search;Type"), ","); // if no type is specifyed the script is excluded from agent search
		if(CurTypes.isEmpty() || (!Type.isEmpty() && !ListContainsList(CurTypes, Type)))
			continue;

		QString Site = pScript->GetScriptName();

		CSearchProxy* pWebSearch = m_Searches.value(Site);
		if(!pWebSearch)
			pWebSearch = StartSearch(Site);
		else if(!pWebSearch->IsRunning()) // get more
			theCore->m_WebManager->GetCrawler()->StartSearch(pWebSearch, Site);
	}
#endif
}

CSearchProxy* CSearchAgent::StartSearch(ESearchNet SearchNet)
{
	CSearchProxy* pSearch = new CSearchProxy(SearchNet, this);

	switch(pSearch->GetSearchNet())
	{
		case eNeoKad:		theCore->m_NeoManager->GetKad()->StartSearch(pSearch);			break;
		case eMuleKad:		theCore->m_MuleManager->GetKad()->StartSearch(pSearch);			break;
		case eEd2kServer:	theCore->m_MuleManager->GetServerList()->StartSearch(pSearch);	break;
		default:		ASSERT(0);
	}

	QString SearchNetStr = CSearch::GetSearchNetStr(pSearch->GetSearchNet());
	LogLine(LOG_DEBUG | LOG_INFO, tr("Starting '%1' Search for: %2").arg(SearchNetStr).arg(this->GetExpression()));
	m_Searches.insert(SearchNetStr, pSearch);

	return pSearch;
}

#ifndef NO_HOSTERS
CSearchProxy* CSearchAgent::StartSearch(const QString& Site)
{
	CSearchProxy* pSearch = new CSearchProxy(eWebSearch, this);

	theCore->m_WebManager->GetCrawler()->StartSearch(pSearch, Site);

	LogLine(LOG_DEBUG | LOG_INFO, tr("Starting '%1' Search for: %2").arg(Site).arg(this->GetExpression()));
	m_Searches.insert(Site, pSearch);

	return pSearch;
}
#endif

void CSearchAgent::Reset()
{
	foreach(CSearchProxy* pSearch, m_Searches)
	{
		if(pSearch->IsRunning())
		{
			switch(pSearch->GetSearchNet())
			{
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
	m_Searches.clear();
}

// Load/Store
QVariant CSearchAgent::Store()
{
	QVariantMap Data = CSearch::Store().toMap();
	Data["Collections"] = m_RootCollection->Store();
	return Data;		
}

int CSearchAgent::Load(const QVariantMap& Data)
{
	int iRet = CSearch::Load(Data);
	m_RootCollection->Load(Data["Collections"].toMap());
	return iRet;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Content Recognition System
//

QStringList ExtractBrackets(QString& Name)
{
	QString BackName = Name;

	QStringList Brackets;
	QRegExp BracketExp("[\\[\\(\\{](.*)[\\]\\)\\}]");
	BracketExp.setMinimal(true);
	int CutPos = 0;
	int Pos = 0;
	while ((Pos = BracketExp.indexIn(Name)) != -1) {
		QString Bracket = BracketExp.cap(1).trimmed();
		if(!Bracket.isEmpty())
			Brackets.append(Bracket);
		Name.remove(Pos, BracketExp.matchedLength());
		if(Pos > 5 && !CutPos)
			CutPos = Pos;
	}
	if(CutPos)
		Name.truncate(CutPos);

	if(Name.size() < 5)
	{
		Name = BackName;
		Name.replace(QRegExp("[\\[\\]\\(\\)\\{\\}]"), " ");
	}

	Name.replace(QRegExp("[ \t\r\n]+"), " ");
	Name = Name.trimmed();
	return Brackets;
}

QString FormatName(QString Name)
{
	if(Name.isEmpty())
		return Name;
	Name.replace(QRegExp("[^A-Za-z0-9]"), " ");
	Name.replace(QRegExp("[ \t\r\n]+"), " ");
	Name = Name.trimmed().toLower();
	for(int i=0; i != -1; i = Name.indexOf(" ", i))
	{
		while(Name[i] == ' ') i++;
		Name[i] = Name[i].toUpper();
	}
	return Name;
}

QString FormatVersion(QString Version)
{
	Version = Version.replace(QRegExp("[^A-Za-z0-9]"), " ").trimmed().replace(QRegExp("[ ]+"), ".").toLower();
	if(Version.left(1) == "v")
		Version.remove(0,1);
	return Version;
}

// foFx
StrPair Split2(const QString& String, const QRegExp& Separator, bool Back = false)
{
	int Sep = Back ? String.lastIndexOf(Separator) : String.indexOf(Separator);
	if(Sep != -1)
		return qMakePair(String.left(Sep).trimmed(), String.mid(Sep + Separator.matchedLength()).trimmed());
	return qMakePair(String.trimmed(), QString());
}


void CSearchAgent::InitRegExp()
{
	ExtExp = QRegExp("\\.[a-zA-Z0-9]{1,8}$|\\.[a-zA-Z0-9]{1,4}\\.[a-zA-Z0-9]{1,4}$", Qt::CaseInsensitive);

	// Video
	VidExps.append(QRegExp("(^|[^a-zA-Z0-9]+)(" "extended|edition|limited|REPACK" ")([^a-zA-Z0-9]+|$)", Qt::CaseInsensitive));

	CamExp = QRegExp("(^|[^a-zA-Z0-9]+)(" "CAM([ -]?R(ip)?)?|TS([ -]?R(ip)?)?|TELESYNC|PDVD|TC|TELECINE"				")([^a-zA-Z0-9]+|$)", Qt::CaseInsensitive);	VidExps.append(CamExp);
	ScrExp = QRegExp("(^|[^a-zA-Z0-9]+)(" "WP|WORKPRINT|(DVD|BD)?SCR|(DVD|BD)SCREENER|DDC"								")([^a-zA-Z0-9]+|$)", Qt::CaseInsensitive);	VidExps.append(ScrExp);
	DvdExp = QRegExp("(^|[^a-zA-Z0-9]+)(" "R(5|6)([ -]?R(ip)?)?|DVD[0-9]?[ -]?R(ip)?|DVD[0-9]?[ -]?Scr([ -]?R(ip)?)?"	")([^a-zA-Z0-9]+|$)", Qt::CaseInsensitive);	VidExps.append(DvdExp);
	BDExp = QRegExp( "(^|[^a-zA-Z0-9]+)(" "BD[0-9]?([ -]?R(ip)?)?|BR([ -]?R(ip)?)|BLU[ -]?RAY([ -]?R(ip)?)?"			")([^a-zA-Z0-9]+|$)", Qt::CaseInsensitive);	VidExps.append(BDExp);
	VodExp = QRegExp("(^|[^a-zA-Z0-9]+)(" "VOD([ -]?R(ip)?)?|WEB(([ -]?DL)|([ -]?R(ip)?)|([ -]?CAP))"					")([^a-zA-Z0-9]+|$)", Qt::CaseInsensitive);	VidExps.append(VodExp);
	TVExp = QRegExp( "(^|[^a-zA-Z0-9]+)(" "PPV([ -]?R(ip)?)?|DS([ -]?R(ip)?)?|DTH([ -]?R(ip)?)?|DVB([ -]?R(ip)?)?|"
											"HDTV([ -]?R(ip)?)?|PDTV([ -]?R(ip)?)?|TV([ -]?R(ip)?)?|HDTV([ -]?R(ip)?)?|"
											"HD([ -]?R(ip)?)?"															")([^a-zA-Z0-9]+|$)", Qt::CaseInsensitive);	VidExps.append(TVExp);

	ResExp = QRegExp("(^|[^a-zA-Z0-9]+)("	"480(p|i)|720(p|i)|1080(p|i)|x26(4|5)|AC3|5\\.1CH"							")([^a-zA-Z0-9]+|$)", Qt::CaseInsensitive);	VidExps.append(ResExp);
																																		
	
	SeriesExp = QRegExp("(S[0-9]{1,3} ?E[0-9]{1,4})|([0-9]{1,3}x[0-9]{1,4})", Qt::CaseInsensitive);
	SeasonsExp = QRegExp("([^a-zA-Z0-9]s[0-9]{1,3})|((complete )?season ?[0-9]{1,3})|([0-9]{1,3} ?complete)", Qt::CaseInsensitive);
	AnimeExp = QRegExp("([^a-zA-Z0-9]+)(e|ep)?([0-9]{2,3})([^a-zA-Z0-9]+|v[0-9]|$)", Qt::CaseInsensitive);	
	MovieExp = QRegExp("([^a-zA-Z0-9]+)((19|20)[0-9]{2})([^a-zA-Z0-9]+|$)", Qt::CaseInsensitive);	
	PartExp = QRegExp("([^a-zA-Z0-9]+)((Part|Cd|Dvd)[^a-zA-Z0-9]?[0-9]{1,3})([^a-zA-Z0-9]+|$)", Qt::CaseInsensitive);	
	//
	
	// Software
	//VerExp = QRegExp("[^a-zA-Z0-9]+([a-zA-Z]*[0-9]+([\\. 0-9])*([0-9][a-zA-Z])?)", Qt::CaseInsensitive);
	VerExp = QRegExp("[^a-zA-Z0-9]+([a-zA-Z]*[0-9]+([\\.0-9])*([0-9][a-zA-Z])?)", Qt::CaseInsensitive);
	BitExp = QRegExp("(x64|x86(_64)?|[IA]?32[ -]?bit|[IA]?64[ -]?bit|AMD64|EM64T)", Qt::CaseInsensitive);
	EdExp = QRegExp("(^|[^a-zA-Z0-9]+)(Prof|Pro|Professional|Home|Premium|Plus|Ultimate)(.*Edition)?([^a-zA-Z0-9]+|$)", Qt::CaseInsensitive);	

	// Other
	//VolExp = QRegExp("[^a-zA-Z0-9]+Vol(ume)*[^a-zA-Z]*([0-9]+))", Qt::CaseInsensitive);
	
	//DiscExp = QRegExp("[^a-zA-Z0-9]+Discography)", Qt::CaseInsensitive);

	//AlbumExp = QRegExp("[^a-zA-Z0-9]+Album)", Qt::CaseInsensitive);
	//
}

QVariantMap CSearchAgent::Recognize(CFile* pFile)
{
	QVariantMap Details;
	Details[PROP_PRIORITY] = LEAST_PRIORITY;

	QString FileName = pFile->GetFileName();

	QString Category = FormatName(pFile->GetDetails()->GetProperty(PROP_CATEGORY).toString());
	if(Category == CAT_OTHER)
		Category.clear(); // other means none
		

	int iExt = FileName.indexOf(ExtExp);
	QString FileExt;
	if(iExt != -1)
	{
		FileExt = FileName.mid(iExt+1).toLower();
		FileName.truncate(iExt);
	}
		
	EFileTypes Type;
	if((FileName.contains(CamExp) || FileName.contains(ScrExp) || FileName.contains(DvdExp) || FileName.contains(BDExp) 
		|| FileName.contains(VodExp) || FileName.contains(TVExp) || FileName.contains(ResExp)))
		Type = eVideoExt;
	else
		Type = GetFileTypeByExt(FileExt);

	QStringList Brackets = ExtractBrackets(FileName);

	if(Type == eVideoExt) // if this was a videotry to remove obsolete informations
	{
		int CutPos = FileName.size();
		for(int i=0; i < VidExps.size(); i++)
		{
			for(int Pos = 0;;)
			{
				Pos = FileName.indexOf(VidExps[i], Pos);
				if(Pos == -1)
					break;
				int Len = VidExps[i].matchedLength();
					
				// adjust pos and length for 
				if(Pos != 0) // the leading charakter
				{
					Pos++;
					Len--;
				}
				if(Pos + Len != FileName.length()) //the tailing charakter
					Len--;

				if(Pos < 5)
				{
					CutPos -= Len;
					FileName.remove(Pos, Len);
				}
				else 
				{
					if(Pos < CutPos)
						CutPos = Pos;
					Pos += Len;
				}
			}
		}
		FileName.truncate(CutPos);
	}

	// after all this cutting cleanUp
	FileName.replace(QRegExp("^[^a-zA-Z0-9]*"), "");
	FileName.replace(QRegExp("[^a-zA-Z0-9]*$"), "");

	QString SimpleName = FileName;
	SimpleName.replace(QRegExp("[^A-Za-z0-9]"), " "); // repalce all special cahrs with a space
	SimpleName.replace(QRegExp("[ \t\r\n]+"), " "); // resude all multiple spacess to 1
	SimpleName = SimpleName.trimmed(); // trimm leading and backing spaces
	if(SimpleName.isEmpty())
		return Details;

	///////////////////////////
	// Start Recognizion 
	//

	if(!Type || Type == eVideoExt)
	{
		int SeriesPos = SimpleName.indexOf(SeriesExp);
		if(SeriesPos != -1)
		{
			int SeriesLen = SeriesExp.matchedLength();
				
			QString Title = SimpleName.mid(SeriesPos + SeriesLen);

			QString Serie = SimpleName.left(SeriesPos).trimmed();
			QString Nr = SimpleName.mid(SeriesPos, SeriesLen);
				
			int NrPos;
			if(Nr.left(1).compare("s", Qt::CaseInsensitive) == 0)
			{
				Nr.remove(0,1);
				NrPos = Nr.indexOf("e", 0, Qt::CaseInsensitive);
			}
			else
				NrPos = Nr.indexOf("x", 0, Qt::CaseInsensitive);
			ASSERT(NrPos != -1);
			int Season = Nr.left(NrPos).toInt();
			int Episode = Nr.mid(NrPos+1).toInt();

			Details.insert(PROP_CATEGORY, CAT_SERIES);
			Details.insert(PROP_SERIES, FormatName(Serie));
			Details.insert(PROP_SEASON, Season);
			Details.insert(PROP_EPISODE, Episode);
			//FormatName(Title) // Episode Title
			return Details; // we found an episode of a series
		}
	}

	if(!Type || Type == eVideoExt)
	{
		int SeasonsPos = SimpleName.indexOf(SeasonsExp);
		if(SeasonsPos != -1)
		{
			QString Serie = SimpleName.left(SeasonsPos).trimmed();
			
			QRegExp SeasonsExp("[0-9]{1,3}", Qt::CaseInsensitive);
			SeasonsPos = SimpleName.indexOf(SeasonsExp, SeasonsPos);

			int SeasonLen = SeasonsExp.matchedLength();
			int Season = SimpleName.mid(SeasonsPos, SeasonLen).toInt();

			Details.insert(PROP_CATEGORY, CAT_SERIES);
			Details.insert(PROP_SERIES, FormatName(Serie));
			Details.insert(PROP_SEASON, Season);
			return Details; // we found a complete season of a series
		}
	}

	if(Type == eVideoExt || Category == CAT_MOVIES || Category == CAT_SERIES)
	{
		if(!Category.isEmpty() && Category != CAT_MOVIES && Category != CAT_SERIES) 
			Category.clear(); // this is a video file, not all categorys are value

		int PartPos = SimpleName.indexOf(PartExp);
		if(PartPos != -1)
		{
			int PartLen = PartExp.matchedLength();

			QString Part = SimpleName.mid(PartPos, PartLen);
			
			SimpleName.truncate(PartPos);

			Details.insert(PROP_PART, FormatName(Part));
		}

		QString Title;

		int AnimePos = SimpleName.indexOf(AnimeExp);
		if(AnimePos != -1)
		{
			int Episode = AnimeExp.cap(3).toInt();
			Title = SimpleName.left(AnimePos).trimmed();

			if(Category.isEmpty())
				Category = CAT_SERIES; // Anime?
			Details.insert(PROP_EPISODE, Episode);
		}
		else
		{
			int Year;
			int MoviePos = SimpleName.indexOf(MovieExp);
			if(MoviePos != -1)
			{
				Year = MovieExp.cap(2).toInt();
				Title = SimpleName.left(MoviePos).trimmed();
			}
			else
			{
				foreach(const QString& Bracket, Brackets)
				{
					int iYear = Bracket.toInt();
					if(iYear > 1900 && iYear < 2100)
						Year = iYear;
				}
				Title = SimpleName.trimmed();
			}
			if(Year)
				Details.insert(PROP_YEAR, Year);

			if(Category.isEmpty())
				Category = Year ? CAT_MOVIES : CAT_VIDEO;
		}

		Details.insert(PROP_CATEGORY, Category);
			
		if(Category == CAT_SERIES)
			Details.insert(PROP_SERIES, FormatName(Title));
		else if(Category == CAT_MOVIES)
			Details.insert(PROP_MOVIE, FormatName(Title));
		else
			Details.insert(PROP_NAME, FormatName(Title));
			
		return Details; // we found a movie or a video file
	}

	if((!Type || Type == eArchiveExt || Type == eProgramExt) && (Category.isEmpty() || Category == CAT_SOFTWARE || Category == CAT_GAMES))
	{
		QString Name = FileName;
			
		QString Bits;
		int BitPos = -1;
		for(;;)
		{
			int Pos = Name.indexOf(BitExp);
			if(Pos == -1)
				break;
			if(BitPos == -1)
				BitPos = Pos;

			Bits = BitExp.cap(1);
			Name.remove(Pos, BitExp.matchedLength());
		}

		int VerPos = Name.indexOf(VerExp);
		if(VerPos != -1 || BitPos != -1)
		{
			QString Version;
			QString Ext;
			if(VerPos != -1)
			{
				Name.truncate(VerPos);
				Version = VerExp.cap(1);
				Ext = FileName.mid(VerPos + VerExp.matchedLength()).trimmed();
			}
			else if(BitPos != -1)
			{
				Name.truncate(BitPos);
			}

			QString Edition;
			int EdPos = Name.indexOf(EdExp);
			if(EdPos != -1)
			{
				Edition = EdExp.cap(2);
				if(EdPos / 2 >= Name.length() - (EdPos + EdExp.matchedLength())) // if there isnt much afterwards kill it
					Name.truncate(EdPos);
				else
					Name.remove(EdPos + 1, EdExp.matchedLength() - 1); // keep first spacer
			}
			else 
			{
				int EdPos = Ext.indexOf(EdExp);
				if(EdPos != -1)
				{
					Edition = EdExp.cap(2);
					Ext.remove(EdPos ? EdPos + 1 : 0, EdPos ? EdExp.matchedLength() - 1 : EdExp.matchedLength()); // keep first spacer but not at the very begin
				}
			}
				
			Details.insert(PROP_CATEGORY, Category.isEmpty() ? CAT_SOFTWARE : Category);
			Details.insert(PROP_NAME, FormatName(Name));
			if(!Version.isEmpty())
				Details.insert(PROP_VERSION, FormatVersion(Version));
			if(!Edition.isEmpty())
			{
				if(Edition.compare("Pro", Qt::CaseInsensitive) == 0 || Edition.compare("Prof", Qt::CaseInsensitive) == 0)
					Edition = "Professional";
				Details.insert(PROP_EDITION, FormatName(Edition));
			}
			return Details; // we found some software
		}
	}

	// we have not found any of the above, apply most generic treatment

	FileName.replace(QRegExp("[^\\-:;A-Za-z0-9 ]"), "");

	StrPair Name = Split2(FileName, QRegExp("[\\-:;]"));

	Details.insert(PROP_NAME, Name.first);
	Details.insert(PROP_EXT_NAME, Name.second);
	if(Category.isEmpty())
	{
		switch(Type)
		{
		case ePictureExt:	Category = CAT_PICTURES;	break;
		case eDocumentExt:	Category = CAT_DOCUMNTS;	break;
		case eProgramExt:	Category = CAT_SOFTWARE;	break;
		case eAudioExt:		Category = CAT_AUDIO;		break;
		case eArchiveExt:	
		default:			Category = CAT_OTHER;		break;
		}
	}
	Details.insert(PROP_CATEGORY, Category);

	return Details;
}

/*void CSearchAgent::Recognize()
{
	foreach(CFile* pFile, GetFiles())
		pFile->GetDetails()->Add("", Recognize(pFile));

	m_RootCollection->Clear();

	foreach(CFile* pFile, GetFiles())
	{
//#ifdef _DEBUG
//		pFile->SetFileDir("");
//#endif

		m_RootCollection->AddFile(pFile);
	}

	m_RootCollection->ArrangeFiles(this);
#ifndef _DEBUG
	m_RootCollection->CleanUp();
#endif

	//m_RootCollection->SetFileDirs(this, ""); // now done on the fly
}*/

void CSearchAgent::AddCollection(const QString& SourceID, const QVariantMap& Collection, int Priority)
{
	QVariantMap Details = Collection["details"].toMap();
	Details[PROP_PRIORITY] = Priority;

	CCollection* pCollection = m_RootCollection->GetCollection(Details);
	if(!pCollection)
	{
		LogLine(LOG_WARNING, tr("Invalid Collection"));
		return;
	}
	
	pCollection->GetDetails()->Add(SourceID, Details);

#ifndef NO_HOSTERS
	if(GetCriteria("Shallow").toBool())
		pCollection->AddCrawlUrls(Collection["crawlUrls"].toStringList());
	else
#endif
		CWebSearch::AddCollection(SourceID, Collection, Priority);
}

QList<CCollection*> CSearchAgent::GetCollections()
{
	QString Category = GetCriteria("Category").toString();

	QList<CCollection*> Collections;
	foreach(CCollection* pCategory, m_RootCollection->GetCollections())
	{
		if(Category.isEmpty() && !(Category.compare(pCategory->GetName(), Qt::CaseInsensitive) == 0))
			continue;
		Collections.append(pCategory->GetCollections().values());
	}

	QString Sorting = GetCriteria("Sorting").toString();
	QString Order = GetCriteria("Order").toString();
	// TODO: Sort

	return Collections;
}

QList<CCollection*> CSearchAgent::GetAllCollections()
{
	QList<CCollection*> Collections;
	foreach(CCollection* pCategory, m_RootCollection->GetCollections())
		Collections.append(pCategory->GetCollections().values());
	return Collections;
}