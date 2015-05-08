#include "GlobalHeader.h"
#include "Collection.h"
#include "SearchAgent.h"
#include "../NeoCore.h"
#include "../FileList/FileList.h"
#include "../FileList/File.h"
#include "../FileList/FileDetails.h"
#ifndef NO_HOSTERS
#include "../FileTransfer/HosterTransfer/WebManager.h"
#include "../FileTransfer/HosterTransfer/WebCrawler.h"
#endif

QMap<uint64, CCollection*> CCollection::m_Map;

CCollection::CCollection(EType Type, QObject* qObject)
 : QObjectEx(qObject)
{
	m_FileDetails = new CFileDetails(this);
	m_bCollect = false;

	m_ID = CFileList::AllocID();
	m_Map.insert(m_ID, this);

	m_Type = Type;
}

CCollection::~CCollection()
{
	CFileList::ReleaseID(m_ID);
	m_Map.remove(m_ID);
}

void CCollection::SetName(const QString& Name)
{
	m_Name = FormatName(Name);
}

CCollection* CCollection::GetCollection(uint64 ID)
{
	return m_Map.value(ID);
}

QStringList CCollection::GetPath()
{
	QStringList Path;
	if(CCollection* pParent = qobject_cast<CCollection*>(parent()))
		Path = pParent->GetPath();
	Path.append(m_Name);
	return Path;
}

CCollection* CCollection::GetCollection(const QStringList& Path)
{
	if(Path.isEmpty())
		return this;
	return GetCollection(Path.first())->GetCollection(Path.mid(1));
}

CCollection* CCollection::GetCollection(const QString& Key, EType Type)
{
	CCollection* &pCollection = m_Subs[FormatName(Key)];
	if(!pCollection)
	{
		pCollection = new CCollection(Type, this);
		pCollection->SetName(Key);
	}
	return pCollection;
}

QSet<uint64> CCollection::GetAllFiles()
{
	QSet<uint64> Files = m_Files;
	foreach(CCollection* pCollection, m_Subs)
		Files.unite(pCollection->GetAllFiles());
	return Files;
}

/*void CCollection::Clear()
{
	m_Files.clear();
	foreach(CCollection* pCollection, m_Subs)
		pCollection->Clear();
}

int CCollection::CleanUp()
{
	int Count = m_Files.count();
	foreach(CCollection* pCollection, m_Subs)
	{
		int Temp = pCollection->CleanUp();
		if(!pCollection->GetDetails()->IsEmpty())
			Temp++;
		if(Temp)
			Count += Temp;
		else
			m_Subs.remove(m_Subs.key(pCollection));
	}
	return Count;
}*/

bool CCollection::IsEmpty()
{
	if(!m_Files.isEmpty() || !m_FileDetails->IsEmpty())
		return false;
	foreach(CCollection* pCollection, m_Subs)
	{
		if(!pCollection->IsEmpty())
			return false;
	}
	return true;
}

/*void CCollection::ArrangeFiles(CFileList* pList)
{
	switch(m_Type)
	{
		case eRoot: // Split by category
		{
			foreach(uint64 FileID, m_Files)
			{
				if(CFile* pFile = pList->GetFileByID(FileID))
				{
					QString Category = pFile->GetDetails()->GetProperty(PROP_CATEGORY).toString();
					CCollection* pCollection = GetCollection(Category, eCategory);
					pCollection->AddFile(pFile);
				}
			}
			m_Files.clear();
			break;
		}
		case eCategory:
		{
			EType Type = eGeneric;
			QString Prop = PROP_NAME;
			if(m_Name.compare(CAT_SOFTWARE, Qt::CaseInsensitive) == 0 || m_Name.compare(CAT_GAMES, Qt::CaseInsensitive) == 0)
				Type = eSoftware;
			else if(m_Name.compare(CAT_SERIES, Qt::CaseInsensitive) == 0)
			{
				Type = eSeries;
				Prop = PROP_SERIES;
			}
			else if(m_Name.compare(CAT_MOVIES, Qt::CaseInsensitive) == 0)
			{
				Type = eMovie;
				Prop = PROP_MOVIE;
			}
			//else if(m_Name.compare(CAT_MUSIC, Qt::CaseInsensitive) == 0)
			//{	Type = eMusic;
			//	Prop = PROP_ARTIST;
			//}
			else
				Type = eOther;
			
			//MergeMatches(pList, Prop, Type);

			foreach(uint64 FileID, m_Files)
			{
				if(CFile* pFile = pList->GetFileByID(FileID))
				{
					QString Value = pFile->GetDetails()->GetProperty(Prop).toString();
					if(!Value.isEmpty())
					{
						CCollection* pCollection = GetCollection(Value, Type);
						pCollection->AddFile(pFile);
						m_Files.remove(FileID);
					}
				}
			}

			// Scrobble: details from all files
			foreach(CCollection* pCollection, m_Subs)
				pCollection->GetDetails()->Collect(pCollection->GetAllFiles());
			

			break;
		}
		case eSoftware:
		{
			foreach(uint64 FileID, m_Files)
			{
				if(CFile* pFile = pList->GetFileByID(FileID))
				{
					QString Version = pFile->GetDetails()->GetProperty(PROP_VERSION).toString();
					if(!Version.isEmpty())
					{
						//QString Edition = pFile->GetDetails()->GetProperty(PROP_EDITION).toString();

						CCollection* pCollection = GetCollection(Version);
						pCollection->AddFile(pFile);
						m_Files.remove(FileID);
					}
				}
			}
			break;
		}
		case eSeries: // this colleciton is one particular series or anime
		{
			foreach(uint64 FileID, m_Files)
			{
				if(CFile* pFile = pList->GetFileByID(FileID))
				{
					QString Season = pFile->GetDetails()->GetProperty(PROP_SEASON).toString();
					if(!Season.isEmpty())
						Season.prepend(tr("Season "));
					else
						Season = tr("Episodes"); // this is liek a season but well all seasons...

					CCollection* pCollection = GetCollection(Season, eSeason);
					pCollection->AddFile(pFile);
					m_Files.remove(FileID);
				}
			}
			break;
		}
		case eSeason:
		{
			foreach(uint64 FileID, m_Files)
			{
				if(CFile* pFile = pList->GetFileByID(FileID))
				{
					QString Episode = pFile->GetDetails()->GetProperty(PROP_EPISODE).toString();
					if(Episode.isEmpty())
						continue;
					Episode.prepend(tr("Episode "));

					CCollection* pCollection = GetCollection(Episode, eEpisode);
					pCollection->AddFile(pFile);
					m_Files.remove(FileID);
				}
			}
			break;
		}
		case eEpisode:
			break;
		case eMovie:
			break;
		//case eMusic:
		//{
		//	MergeMatches(pList, PROP_ALBUM);
		//	break;
		//}
		case eOther:
		{
			//MergeMatches(pList, PROP_EXT_NAME);
			break;
		}
		case eGeneric:
			break;
	}

	foreach(CCollection* pCollection, m_Subs)
		pCollection->ArrangeFiles(pList);
}*/

void CCollection::ArrangeFile(CFile* pFile)
{
	switch(m_Type)
	{
		case eRoot: // Split by category
		{
			// remove if already listed
			uint64 CollectionID = pFile->GetProperty("CollectionID").toULongLong();
			if(CCollection* pCollection = CCollection::GetCollection(CollectionID))
			{
				pCollection->m_Files.remove(pFile->GetFileID());
				for(;;)
				{
					CCollection* pParent = qobject_cast<CCollection*>(pCollection->parent());
					if(!pParent)
						break;
					if(pCollection->IsEmpty())
						delete pParent->m_Subs.take(pCollection->GetName());
					pCollection = pParent;
				}
			}
			//

			QString Category = pFile->GetDetails()->GetProperty(PROP_CATEGORY).toString();
			if(!Category.isEmpty())
			{
				CCollection* pCollection = GetCollection(Category, eCategory);
				pCollection->ArrangeFile(pFile);
			}
			else
				AddFile(pFile);
			break;
		}
		case eCategory:
		{
			EType Type = eGeneric;
			QString Prop = PROP_NAME;
			if(m_Name.compare(CAT_SOFTWARE, Qt::CaseInsensitive) == 0 || m_Name.compare(CAT_GAMES, Qt::CaseInsensitive) == 0)
				Type = eSoftware;
			else if(m_Name.compare(CAT_SERIES, Qt::CaseInsensitive) == 0)
			{
				Type = eSeries;
				Prop = PROP_SERIES;
			}
			else if(m_Name.compare(CAT_MOVIES, Qt::CaseInsensitive) == 0)
			{
				Type = eMovie;
				Prop = PROP_MOVIE;
			}
			//else if(m_Name.compare(CAT_MUSIC, Qt::CaseInsensitive) == 0)
			//{	Type = eMusic;
			//	Prop = PROP_ARTIST;
			//}
			else
				Type = eOther;
			
			QString Value = pFile->GetDetails()->GetProperty(Prop).toString();
			if(!Value.isEmpty())
			{
				CCollection* pCollection = GetCollection(Value, Type);
				pCollection->ArrangeFile(pFile);

				pCollection->ScheduleCollect(); // Scrobble details
			}
			else
				AddFile(pFile);
	
			break;
		}
		case eSoftware:
		{
			QString Version = pFile->GetDetails()->GetProperty(PROP_VERSION).toString();
			if(!Version.isEmpty())
			{
				//QString Edition = pFile->GetDetails()->GetProperty(PROP_EDITION).toString();

				CCollection* pCollection = GetCollection(Version);
				pCollection->ArrangeFile(pFile);
			}
			else
				AddFile(pFile);
			break;
		}
		case eSeries: // this colleciton is one particular series or anime
		{
			QString Season = pFile->GetDetails()->GetProperty(PROP_SEASON).toString();
			if(!Season.isEmpty())
				Season.prepend(tr("Season "));
			else
				Season = tr("Episodes"); // this is liek a season but well all seasons...

			CCollection* pCollection = GetCollection(Season, eSeason);
			pCollection->ArrangeFile(pFile);
			break;
		}
		case eSeason:
		{
			QString Episode = pFile->GetDetails()->GetProperty(PROP_EPISODE).toString();
			if(!Episode.isEmpty())
			{
				Episode.prepend(tr("Episode "));

				CCollection* pCollection = GetCollection(Episode, eEpisode);
				pCollection->ArrangeFile(pFile);
			}
			else
				AddFile(pFile);
			break;
		}
		case eEpisode:
			AddFile(pFile);
			break;
		case eMovie:
			AddFile(pFile);
			break;
		//case eMusic:
		//{
		//	MergeMatches(pList, PROP_ALBUM);
		//	break;
		//}
		default:
		{
			AddFile(pFile);
			break;
		}
	}
}

void CCollection::ScheduleCollect()
{
	if(!m_bCollect)
	{
		m_bCollect = true;
		QTimer::singleShot(250, this, SLOT(OnCollect()));
	}
}

void CCollection::OnCollect()
{
	m_bCollect = false;
	m_FileDetails->Collect(GetAllFiles());
}

/*void CCollection::SetFileDirs(CFileList* pList, const QString& Path)
{
	foreach(uint64 FileID, m_Files)
	{
		//qDebug() << Path;
		if(CFile* pFile = pList->GetFileByID(FileID))
			pFile->SetFileDir(Path);
	}

	for(QMap<QString, CCollection*>::iterator I = m_Subs.begin(); I != m_Subs.end(); I++)
		I.value()->SetFileDirs(pList, Path + "/" + I.key());
}*/

void CCollection::AddFile(CFile* pFile)
{
	pFile->SetFileDir(GetPath().join("/"));
	pFile->SetProperty("CollectionID", GetID());
	m_Files.insert(pFile->GetFileID());
}

CCollection* CCollection::GetCollection(const QVariantMap& Collection)
{
	switch(m_Type)
	{
		case eRoot: // Split by category
		{
			QString Category = FormatName(Collection[PROP_CATEGORY].toString());
			if(CCollection* pCollection = GetCollection(Category, eCategory))
				return pCollection->GetCollection(Collection);
			break;
		}
		case eCategory:
		{
			CCollection* pCollection = NULL;
			if(m_Name.compare(CAT_SERIES, Qt::CaseInsensitive) == 0)
			{
				QString Series = FormatName(Collection[PROP_SERIES].toString());
				pCollection = GetCollection(Series, eSeries);
			}
			else if(m_Name.compare(CAT_MOVIES, Qt::CaseInsensitive) == 0)
			{
				QString Movie = FormatName(Collection[PROP_MOVIE].toString());
				pCollection = GetCollection(Movie, eMovie);
			}
			else if(m_Name.compare(CAT_SOFTWARE, Qt::CaseInsensitive) == 0)
				return NULL; // TODO;
			if(pCollection)
				return pCollection->GetCollection(Collection);
			break;
		}
		//case eSoftware:
		//{
		//	break;
		//}
		case eSeries: // this colleciton is one particular series or anime
		{
			QString Type = FormatName(Collection[PROP_TYPE].toString());
			if(Type.compare(TYP_SERIES, Qt::CaseInsensitive) == 0)
				return this;
			
			QString Season = Collection[PROP_SEASON].toString();
			if(!Season.isEmpty())
				Season.prepend(tr("Season "));
			else
				Season = tr("Episodes"); // this is liek a season but well all seasons...

			if(CCollection* pCollection = GetCollection(Season, eSeason))
				return pCollection->GetCollection(Collection);
			break;
		}
		case eSeason:
		{
			QString Type = FormatName(Collection[PROP_TYPE].toString());
			if(Type.compare(TYP_SEASON, Qt::CaseInsensitive) == 0)
				return this;

			QString Episode = Collection[PROP_EPISODE].toString();
			if(Episode.isEmpty())
				return NULL; // inside a Season only an episode can exist

			Episode.prepend(tr("Episode "));

			if(CCollection* pCollection = GetCollection(Episode, eEpisode))
				return pCollection->GetCollection(Collection);
			break;
		}
		case eEpisode:
		{
			QString Type = FormatName(Collection[PROP_TYPE].toString());
			if(Type.compare(TYP_EPISODE, Qt::CaseInsensitive) == 0)
				return this;
			break;
		}
		case eMovie:
		{
			QString Type = FormatName(Collection[PROP_TYPE].toString());
			if(Type.compare(TYP_MOVIE, Qt::CaseInsensitive) == 0)
				return this;
			break;
		}
		//case eMusic:
		//{
		//	MergeMatches(pList, PROP_ALBUM);
		//	break;
		//}
	}

	return NULL; // Error
}

#ifndef NO_HOSTERS
void CCollection::CrawlUrls()
{
	if(CWebSearch* pSearch = qobject_cast<CWebSearch*>(GetSearch()))
		pSearch->CrawlUrls(m_CrawlUrls.toList(), this);
	m_CrawlUrls.clear();
}
#endif

QVariantMap CCollection::GetContent(bool& IsCrawling)
{
#ifndef NO_HOSTERS
	if(HasCrawlUrls())
		CrawlUrls();
	if(theCore->m_WebManager->GetCrawler()->IsCrawling(this))
		IsCrawling = true;
#endif

	QVariantMap Content = m_FileDetails->Extract();
	Content["Name"] = m_Name;

	QVariantList Files;
	foreach(uint64 FileID, GetAllFiles())
	{
		if(CFile* pFile = CFileList::GetFile(FileID))
		{
			QVariantMap File = pFile->GetDetails()->Extract();
			File["ID"] = FileID;
			File["Name"] = pFile->GetFileName();
			File["Availability"] = pFile->GetDetails()->GetAvailStats();

			Files.append(File);
		}
	}
	Content["Files"] = Files;

	return Content;
}

bool CCollection::IsCollecting()
{
#ifndef NO_HOSTERS
	if(theCore->m_WebManager->GetCrawler()->IsCollecting(m_FileDetails))
		return true;

	foreach(uint64 FileID, m_Files)
	{
		if(CFile* pFile = CFileList::GetFile(FileID))
		{
			if(theCore->m_WebManager->GetCrawler()->IsCollecting(pFile->GetDetails()))
				return true;
		}
	}

	foreach(CCollection* pCollection, m_Subs)
	{
		if(pCollection->IsCollecting())
			return true;
	}
#endif
	return false;
}

CSearch* CCollection::GetSearch()
{
	if(CCollection* pParent = qobject_cast<CCollection*>(parent()))
		return pParent->GetSearch();
	else if(CSearch* pParent = qobject_cast<CSearch*>(parent()))
		return pParent;
	return NULL;
}

//QVariant CCollection::GetProperty(const QString& Name)
//{
//	return m_FileDetails->GetProperty(Name);
//}

// Load/Store
QVariantMap CCollection::Store()
{
	QVariantMap Collection;
	Collection["Name"] = m_Name;
	Collection["Type"] = m_Type;

	QVariantList Collections;
	foreach(CCollection* pCollection, m_Subs)
		Collections.append(pCollection->Store());
	Collection["Collections"] = Collections;
	
	QVariantList Files;
	foreach(uint64 FileID, m_Files)
		Files.append(FileID);
	Collection["Files"] = Files;

	Collection["Details"] = m_FileDetails->Store();

#ifndef NO_HOSTERS
	Collection["CrawlUrls"] = QStringList(m_CrawlUrls.toList());
#endif

	//Collection["Properties"] = m_Properties;

	return Collection;
}

void CCollection::Load(const QVariantMap& Collection)
{
	m_Name = Collection["Name"].toString();
	m_Type = EType(Collection["Type"].toInt());

	foreach(const QVariant& vCollection, Collection["Collections"].toList())
	{
		CCollection* pCollection = new CCollection(CCollection::eGeneric, this);
		pCollection->Load(vCollection.toMap());
		m_Subs.insert(pCollection->GetName(), pCollection);
	}

	foreach(const QVariant& vFileID, Collection["Files"].toList())
		m_Files.insert(vFileID.toULongLong());

	m_FileDetails->Load(Collection["Details"].toMap());

#ifndef NO_HOSTERS
	m_CrawlUrls = Collection["CrawlUrls"].toStringList().toSet();
#endif

	//m_Properties = Collection["Properties"].toMap();
}

////////////////////////////////////////////////////////////
// AI stuff

/*bool IsNonWord(const QString& Word)
{
	if(Word.length() < 3)
		return true;
	QRegExp NonWords("the|for|and|a|an|of", Qt::CaseInsensitive); 
	return NonWords.exactMatch(Word);
}

void CleanUpList(QStringList& Words)
{
	while(!Words.isEmpty() && IsNonWord(Words.first()))
		Words.removeFirst();
	while(!Words.isEmpty() && IsNonWord(Words.last()))
		Words.removeLast();
}*/

/*QStringList FindLongestMatch(const QStringList& WordsI, const QStringList& WordsJ)
{
	QStringList LongestMatch;
	for(int k=0; k < WordsI.size(); k++)
	{
		QString WordI = WordsI[k];
		//if(IsNonWord(WordI))
		//	continue;
		for(int l=-1;;)
		{
			l = WordsJ.indexOf(WordI, l+1);
			if(l == -1)
				break;

			QStringList MatchStr;
			MatchStr.append(WordI);

			int m = 1;
			int n = 1;
			for(; k + m < WordsI.size() && l + n < WordsJ.size(); )
			{
				QString WordIx = WordsI[k + m];
				QString WordJx = WordsJ[l + n];
				//if(IsNonWord(WordIx))
				//{
				//	m++;
				//	continue;
				//}
				//if(IsNonWord(WordJx))
				//{
				//	n++;
				//	continue;
				//}

				if(WordIx != WordJx)
					break;
				MatchStr.append(WordIx);
				m++;
				n++;
			}

			if(MatchStr.size() > LongestMatch.size())
				LongestMatch = MatchStr;
		}
	}
	return LongestMatch;
}

int CalcDistance(const QStringList& s, const QStringList& t) // Levenshtein Distance on word level
{
	//Step 1
	size_t n = s.size();
	size_t m = t.size();
	if (n == 0 || m == 0)
		return INT_MAX; // (max(m, n)); // return the full string cost if one is zero length

	QVector<int> d(++m * ++n);
	//Step 2
	for (int k = 0; k < n; k++)
		d[k] = k;
	for (int k = 0; k < m; k++)
		d[k * n] = k;
	//Step 3 and 4
	for (int i = 1; i < n; i++)
	{
		for (int j = 1; j < m; j++) 
		{
			//Step 5
			//int cost = (s[i - 1] == t[j - 1]) ? 0 : 1;
			int cost = (s[i - 1] == t[j - 1]) ? 0 : 2; // conversion cost is 2 instead of 1
			//Step 6
			int a = d[(j - 1) * n + i] + 1;
			int b = d[j * n + i - 1] + 1;
			int c = d[(j - 1) * n + i - 1] + cost;
			d[j * n + i] = (min(a, (min(b, c))));
		}
	}
	return d[n * m - 1];
}

QVector<QString> GenerateKeyVect(const QMap<QString, QSet<int> >& MatchMap)
{
	QVector<QString> KeyVect;
	QVector<int> CountVect; // this is irrelevant after this step
	for(QMap<QString, QSet<int> >::const_iterator I = MatchMap.begin(); I != MatchMap.end(); I++)
	{
		int Count = I.value().size();
		// sort insert
		int Insert = 0; 
		for(; Insert < CountVect.size(); Insert++)
		{
			if(CountVect[Insert] < Count)
				break;
		}
		KeyVect.insert(Insert, I.key());
		CountVect.insert(Insert, Count);
	}
	return KeyVect;
}

QMap<QString, QSet<int> > ComputeMatchMap(QMap<QString, QSet<int> > ProtoMap)
{
	QVector<QString> StrVect = ProtoMap.keys().toVector();

	// Compute Similarity Map
	QVector<QStringList> WordVec;
	WordVec.resize(StrVect.size());
	for(int i=0; i < StrVect.size(); i++)
	{
		foreach(const QString &Word, StrVect[i].split(" "))
			WordVec[i].append(Word);
	}

	QMap<QString, QSet<int> > MatchMap;
	QSet<int> MatchedGroupes;
	for(int i=0; i < StrVect.size(); i++)
	{
		for(int j=i+1; j < StrVect.size(); j++)
		{
			QStringList LongestMatch = FindLongestMatch(WordVec[i], WordVec[j]);
			//CleanUpList(LongestMatch); // ignore all non words on the begin/end
			//if(LongestMatch.size() > 0 && (LongestMatch.first().length() > 4 || LongestMatch.size() >= 2)) // filter short single words
			if(LongestMatch.size() > 1)
			{
				MatchedGroupes.insert(i);
				MatchedGroupes.insert(j);

				QString Str = LongestMatch.join(" ");
				MatchMap[Str].unite(ProtoMap[StrVect[i]]);
				MatchMap[Str].unite(ProtoMap[StrVect[j]]);
			}
		}

		if(!MatchedGroupes.contains(i))
		{
			MatchedGroupes.insert(i); // just in case but should be irrelevant
			MatchMap[StrVect[i]].unite(ProtoMap[StrVect[i]]);
		}
	}
	//

	// Note: KeyVect is sorted by the amount of items in each grupe starting with the most popular
	QVector<QString> KeyVect = GenerateKeyVect(MatchMap);

	// Consolidate Similarity Map
	for(int i=0; i < KeyVect.size(); i++)
	{
		QString StrI = KeyVect[i];
		QStringList WordsI = StrI.split(" "); 

		for(int j=i+1; j < KeyVect.size(); j++)
		{
			QString StrJ = KeyVect[j];
			QStringList WordsJ = StrJ.split(" ");

			//bool PrefixIJ = StrJ.indexOf(StrI) == 0; // I is prefix of J
			//bool PrefixJI = StrI.indexOf(StrJ) == 0;
			//bool SuffixIJ = StrJ.lastIndexOf(StrI) == StrI.length(); // I is suffix of J
			//bool SuffixJI = StrI.lastIndexOf(StrJ) == StrJ.length();

			int InI = MatchMap[StrI].size();
			int InJ = MatchMap[StrJ].size();
			int InIJ = QSet<int>(MatchMap[StrI]).unite(MatchMap[StrJ]).count();
			int OverlapI = InIJ ? 100 * InI / InIJ : 0;
			int OverlapJ = InIJ ? 100 * InJ / InIJ : 0;

			// this returns the count of words that need to be inserted or removed in order to make the strings same:
			int Distance = CalcDistance(WordsI, WordsJ); 
			int WordCount = Max(WordsI.size(), WordsJ.size());
			int Percentage = WordCount ? 100 * Distance / WordCount : 0;

			int Merge = 0;
			switch(WordCount)
			{
			case 1:	
			case 2:
				continue; // no match 
			case 3:
				if(Distance <= 1)
				{
					if(OverlapI >= 90 && OverlapI >= 90)
						Merge = WordsI.size() < WordsJ.size() ? 1 : -1;
				}
				break;
			case 4:
			case 5:
				if(Distance <= 1) 
					Merge = WordsI.size() < WordsJ.size() ? 1 : -1;
				break;
			default:
				if(Percentage < 20)
					Merge = WordsI.size() < WordsJ.size() ? 1 : -1;
				break;
			}

			if(Merge)
			{
				if(Merge > 0) // we merge in to i ... trivial
				{
					//qDebug() << StrI << " <-> " << StrJ;
					
					MatchMap[StrI].unite(MatchMap.take(StrJ));
				}
				else // we merge into j ... we merge and then we replace i with j
				{
					//qDebug() << StrJ << " <-> " << StrI;

					MatchMap[StrJ].unite(MatchMap.take(StrI));
					KeyVect[i] = KeyVect[j];
				}

				// Note: we dont care for CountVect
				KeyVect.remove(j); // j is gone
				i--; // revisit this entry
				break;
			}
		}
	}
	//

	// Identify Embedded Groupes - Preffix/Suffix search
	for(int i=0; i < KeyVect.size(); i++)
	{
		QString StrI = KeyVect[i];
		for(int j=0; j < KeyVect.size(); j++) // Note we dont use the j=i+1 trick here we want to compare i to j and than j to i
		{
			if(i == j)
				continue;
			QString StrJ = KeyVect[j];

			// Remove files from embedded grupes
			if(StrJ.indexOf(StrI) != -1)
				MatchMap[StrI].subtract(MatchMap[StrJ]);
		}
	}
	//

	// Note: At this point a file should not be contained in two grupes anymore so we can proceed trivially

	return MatchMap;
}

void CCollection::MergeMatches(CFileList* pList, const QString& Property, EType Type)
{
	QVector<CFile*> List;
	QMap<QString, QSet<int> > ProtoMap; // key, file Index'es
	foreach(uint64 FileID, m_Files)
	{
		if(CFile* pFile = pList->GetFileByID(FileID))
		{
			QString Value = FormatName(pFile->GetDetails()->GetProperty(Property).toString());
			if(!Value.isEmpty()) // ignore files when grouping if the target proeprty is not set
			{
				ProtoMap[Value].insert(List.size());
				List.append(pFile);
			}
		}
	}

	//QMap<QString, QSet<int> > MatchMap = ComputeMatchMap(ProtoMap);
	QMap<QString, QSet<int> > MatchMap = ProtoMap;

	for(QMap<QString, QSet<int> >::iterator I = MatchMap.begin(); I != MatchMap.end(); I++)
	{
		QString Key = I.key();
		foreach(int iVect, I.value())
		{
			CFile* pFile = List[iVect];
			uint64 FileID = pFile->GetFileID();

			CCollection* pCollection = GetCollection(Key, Type);
			pCollection->AddFile(pFile);
			m_Files.remove(FileID);
		}
	}
}*/