#include "GlobalHeader.h"
#include "FileDetails.h"
#include "File.h"
#include "../NeoCore.h"
#include "../FileTransfer/NeoShare/NeoManager.h"
#include "../FileTransfer/ed2kMule/MuleManager.h"
#include "../FileTransfer/ed2kMule/MuleKad.h"
#ifndef NO_HOSTERS
#include "../FileTransfer/HosterTransfer/WebManager.h"
#include "../FileTransfer/HosterTransfer/WebCrawler.h"
#endif
#ifdef _DEBUG
#include "../../Framework/Xml.h"
#endif

CFileDetails::CFileDetails(QObject* qObject)
 : QObjectEx(qObject)
{
	m_Availability = 0;

#ifndef NO_HOSTERS
	m_Collect = false;
#endif
}

CFileDetails::~CFileDetails()
{
#ifndef NO_HOSTERS
	Collect(false);
#endif
}

#ifndef NO_HOSTERS
void CFileDetails::Collect(bool bCollect)
{
	if(bCollect == m_Collect)
		return;
	if(bCollect)
		theCore->m_WebManager->GetCrawler()->CollectDetails(this);
	else
		theCore->m_WebManager->GetCrawler()->ReleaseDetails(this);
	m_Collect = bCollect;
}
#endif

void CFileDetails::Find()
{
	if(CFile* pFile = qobject_cast<CFile*>(parent()))
	{
		if(GetTime() - pFile->GetProperty("NeoLastFindNotes").toULongLong() > MIN2S(15))
			theCore->m_NeoManager->GetKad()->FindRating(pFile);

		if(GetTime() - pFile->GetProperty("MuleLastFindNotes").toULongLong() > MIN2S(15))
			theCore->m_MuleManager->GetKad()->FindNotes(pFile);
	}
}

bool CFileDetails::IsSearching()
{
	if(CFile* pFile = qobject_cast<CFile*>(parent()))
	{
		if(theCore->m_NeoManager->GetKad()->IsFindingRating(pFile))
			return true;
		if(theCore->m_MuleManager->GetKad()->IsFindingNotes(pFile))
			return true;
	}
#ifndef NO_HOSTERS
	if(m_Collect && theCore->m_WebManager->GetCrawler()->IsCollecting(this))
		return true;
#endif
	return false;
}

void CFileDetails::Add(const QString& SourceID, const QVariantMap& Details)
{
	// Note: we may crawl multiple times th same source, for example search results first and than each result in detail
	//			so we must make sure we wil not loose informations ot if  its doen in the wrong order we wil keep the best data
	QVariantMap &CurDetails = Get(SourceID);
	bool KeepOld = CurDetails[PROP_LEVEL].toInt() > Details[PROP_LEVEL].toInt();
	for(QVariantMap::const_iterator I = Details.begin(); I != Details.end(); I++)
	{
		if(CurDetails.contains(I.key()) && KeepOld)
			continue;
		CurDetails.insert(I.key(), I.value());
	}

	m_Availability = 0;
	int Count = 0;
	for(QMap<QString, QVariantMap>::iterator I = m_FileDetails.begin(); I != m_FileDetails.end(); I++)
	{
		if(double Availability = I->value("Availability").toDouble())
		{
			m_Availability += Availability;
			Count ++;
		}
	}
	if(Count)
		m_Availability /= Count;

	emit Update();
}

QVariantMap& CFileDetails::Get(const QString& SourceID)
{
	for(QMap<QString, QVariantMap>::iterator I = m_FileDetails.find(SourceID); I != m_FileDetails.end() && I.key() == SourceID; I++)
	{
		if(I->value(PROP_TEMP).toBool())
			continue;
		return I.value();
	}
	return m_FileDetails.insert(SourceID, QVariantMap()).value();
}

QVariant CFileDetails::GetProperty(const QString& Name)
{
	QVariant Value;
	int Priority = LEAST_PRIORITY + 2; // smallest priority value means best result content recognizer is MAX_PRIORITY + 0
	int Repetition = 0;
	for(QMap<QString, QVariantMap>::iterator I = m_FileDetails.begin(); I != m_FileDetails.end(); I++)
	{
		if(!I->contains(Name))
			continue;

		int CurPrio = I->value(PROP_PRIORITY).toInt();
		if(CurPrio <= 0)
			CurPrio = LEAST_PRIORITY + 1;
		int CurRep = I->value(PROP_REPETITION, 1).toInt();

		if(Priority > CurPrio || (Priority == CurPrio && CurRep > Repetition)) 
		{
			Value = I->value(Name);
			Priority = CurPrio;
			Repetition = CurRep;
		}
	}
	return Value;
}

void CFileDetails::Merge(CFileDetails* pDetails)
{
	for(QMap<QString, QVariantMap>::iterator I = pDetails->m_FileDetails.begin(); I != pDetails->m_FileDetails.end(); I++)
		Add(I.key(), I.value());
}

void CFileDetails::Clear()
{
	m_FileDetails.clear();
}

QVariantList CFileDetails::Dump()
{
	QVariantList DetailsList;
	for(QMap<QString, QVariantMap>::iterator I = m_FileDetails.begin(); I != m_FileDetails.end(); I++)
	{
		QVariantMap& Details = *I;
		if(!Details.contains("!ID"))
			Details["ID"] = GetRand64() & MAX_FLOAT;
		Details["SourceID"] = I.key();
		DetailsList.append(Details);
	}
	return DetailsList;
}

QVariantMap CFileDetails::Extract()
{
	QVariantMap Details;
	for(QMap<QString, QVariantMap>::iterator I = m_FileDetails.begin(); I != m_FileDetails.end(); I++)
	{
		for(QVariantMap::iterator J = I->begin(); J != I->end(); J++)
		{
			if(!Details.contains(J.key()))
				Details[J.key()] = GetProperty(J.key());
		}
	}
	return Details;
}

QString CFileDetails::GetID(const QString& Type)
{
	QMap<QString, int> IDs;
	for(QMap<QString, QVariantMap>::iterator I = m_FileDetails.begin(); I != m_FileDetails.end(); I++)
	{
		QString Name = "ID_" + Type;
		QString ID = I->value(Name).toString();
		if(!ID.isEmpty())
			IDs[ID] ++;
	}

	QString ID;
	int BestCount = 0;
	for(QMap<QString, int>::iterator I = IDs.begin(); I != IDs.end(); I++)
	{
		if(BestCount >= I.value())
			continue;
		BestCount = I.value();
		ID = I.key();
	}
	return ID;
}

bool CFileDetails::IsEmpty()
{
	for(QMap<QString, QVariantMap>::iterator I = m_FileDetails.begin(); I != m_FileDetails.end(); I++)
	{
		if(!I->value(PROP_TEMP).toBool())
			return false;
	}
	return true;
}

void CFileDetails::Collect(const QSet<uint64>& Files)
{
	// this function is for collections only

	for(QMap<QString, QVariantMap>::iterator I = m_FileDetails.begin(); I != m_FileDetails.end(); )
	{
		if(I->value(PROP_TEMP).toBool())
			I = m_FileDetails.erase(I);
		else
			I++;
	}

	QMultiMap<QString, QPair<int, QVariantMap> > FileDetails;

	foreach(uint64 FileID, Files)
	{
		if(CFile* pFile = CFileList::GetFile(FileID))
		{
			QMap<QString, QVariantMap> CurDetails = pFile->GetDetails()->GetAll();
			for(QMap<QString, QVariantMap>::iterator I = CurDetails.begin(); I != CurDetails.end(); I++)
			{
				bool bFound = false;
				for(QMap<QString, QPair<int, QVariantMap> >::iterator J = FileDetails.find(I.key()); J != FileDetails.end() && J.key() == I.key(); J++)
				{
					if(J->second == I.value())
					{
						J->first++;
						bFound = true;
					}
				}
				if(!bFound)
					FileDetails.insert(I.key(), qMakePair(1, I.value()));
			}
		}
	}

	for(QMultiMap<QString, QPair<int, QVariantMap> >::iterator I = FileDetails.begin(); I != FileDetails.end(); I++)
	{
		QVariantMap &Details = m_FileDetails.insert(I.key(), I->second).value();
		Details[PROP_TEMP] = true;
		Details[PROP_REPETITION] = I->first;
	}
}

#ifndef NO_HOSTERS
void CFileDetails::Add(const QString& SourceUrl)
{
	m_DetailUrls.insert(SourceUrl);

	if(m_Collect)
		theCore->m_WebManager->GetCrawler()->CollectDetails(this);
}

void CFileDetails::Clr(const QString& SourceUrl)
{
	m_DetailUrls.remove(SourceUrl);
}

QStringList CFileDetails::GetSourceUrl()
{
	return m_DetailUrls.toList();
}
#endif

// Load/Store
QVariantMap CFileDetails::Store()
{
	QVariantMap Details;
	
	Details["Availability"] = m_Availability;

	QVariantList DetailsList;
	for(QMap<QString, QVariantMap>::iterator I = m_FileDetails.begin(); I != m_FileDetails.end(); I++)
	{
		QVariantMap Details;
		Details["SourceID"] = I.key();
		Details["Details"] = I.value();
		DetailsList.append(Details);
	}
	Details["Details"] = DetailsList;

	//Details["Properties"] = m_Properties;

	return Details;
}

void CFileDetails::Load(const QVariantMap& Details)
{
	m_Availability = Details["Availability"].toDouble();

	foreach(const QVariant& vDetails, Details["Details"].toList())
	{
		QVariantMap Details = vDetails.toMap();
		m_FileDetails.insert(Details["SourceID"].toString(), Details["Details"].toMap());
	}

	//m_Properties = Details["Properties"].toMap();
}

#include "../../MediaInfo/MediaInfoDLL.h"

size_t MI_ = MediaInfoDLL_Load();

QString CFileDetails::ReadMediaInfo(QString FilePath)
{
#ifdef WIN32
	FilePath.replace("/", "\\");
#endif

	MediaInfoDLL::MediaInfo MI;
	MI.Option(L"CharSet", L"UTF-8");
	MI.Open(FilePath.toStdWString().c_str());
	if (MI.IsReady())
		return QString::fromWCharArray(MI.Inform().data()).toUtf8();
	return "";
}

void CFileDetails::MediaInfoRead(const QString& Data)
{
	if (Data.isEmpty())
		return;

	QString file_format = "";
	QString file_size = "";
	QString duration = "";

	QString video_format = "";
	QString video_width = "";
	QString video_height = "";
	QString video_aspectratio = "";

	QString audio_format = "";
	QString audio_bitratemode = "";
	QString audio_bitrate = "";
	QString audio_samplingrate = "";

	QString info_album = "";
	QString info_artist = "";
	QString info_genre = "";
	QString info_title = "";
	QString info_year = "";

	QByteArray metainfo = Data.toLatin1();

	QList<QByteArray> all_lines = metainfo.split(*"\n");
	int section = 1;
	for (int i = 0; i < all_lines.size(); ++i) {

		QString line = QString::fromLatin1(all_lines.at(i));

		// The output consists of three parts: "General", "Video" and "Audio"
		if (line.trimmed() == "Video")
			section = 2;
		else if (line.trimmed() == "Audio")
			section = 3;

		// If it's an empty line or start block of block (i.e. just the words "General", "Video" or "Audio) -> skip and go to next line
		if (!line.contains(" : "))
			continue;

		// Some general information
		if (section == 1) {

			// What video container format has the file
			if (line.startsWith("Format    ")) {
				file_format = QString(line.split("    : ").at(1));
			}

			// The file size
			if (line.startsWith("File size   ")) {
				file_size = QString(line.split("    : ").at(1));
			}

			// The duration of the video
			if (line.startsWith("Duration"))
			{
				duration = QString(line.split("    : ").at(1));

				// If it's more than 60 mins -> mediainfo gives the info in hours and minutes
				if (duration.contains("h")) 
				{
					QStringList parts = duration.split("h ");
					bool ok;
					QString mins = parts.at(1);
					int mins_int = mins.replace("mn", "").toInt(&ok, 10);
					mins_int += 60 * parts.at(0).toInt(&ok, 10);
					duration = QString("%1:00").arg(mins_int);

					// If it's at least one minute, then mediainfo gives the info in minutes and seconds
				}
				else if (duration.contains("mn")) 
				{
					duration = duration.replace("mn ", ":");
					duration = duration.replace("s", "");
					// If it's less than a minute, the duration info is already in seconds
				}
				else 
				{
					duration = "00:" + duration.split("s ").at(0);
				}

				QStringList dur = duration.split(":");
				duration = dur.at(0) + ":";
				if (dur.at(1).size() == 1)
				{
					duration += "0";
				}
				duration += dur.at(1);

			}

			// The Album
			if (line.startsWith("Album   ")) {
				info_album = QString(line.split("    : ").at(1));
			}

			// The Performer
			if (line.startsWith("Performer   ")) {
				info_artist = QString(line.split("    : ").at(1));
			}

			// The Genre
			if (line.startsWith("Genre   ")) {
				info_genre = QString(line.split("    : ").at(1));
			}

			// The Track name
			if (line.startsWith("Track name   ")) {
				info_title = QString(line.split("    : ").at(1));
			}
			
			//if (line.startsWith("Track name/Position   ")) {
			//}

			//if (line.startsWith("Track name/Total   ")) {
			//}

			// The Year
			if (line.startsWith("Recorded date   ")) {
				info_year = QString(line.split("    : ").at(1));
			}

			// The "Video" part
		}
		else if (section == 2) {

			// The video codec used
			if (line.startsWith("Format   ")) {
				video_format = QString(line.split("     : ").at(1));
			}

			// The width of the file (together with height -> resolution)
			if (line.startsWith("Width")) {
				video_width = QString(line.split("     : ").at(1));
				video_width = video_width.replace(" pixels", "");
			}

			// The height of the file (together with width -> resolution)
			if (line.startsWith("Height")) {
				video_height = QString(line.split("     : ").at(1));
				video_height = video_height.replace(" pixels", "");
			}

			// The aspect ratio of the video
			if (line.startsWith("Display aspect ratio")) {
				video_aspectratio = QString(line.split("     : ").at(1));
			}

			// The "Audio" part
		}
		else if (section == 3) {

			// What audio codec was used
			if (line.startsWith("Format   ")) {
				audio_format = QString(line.split("     : ").at(1));
			}

			// The bitrate mode of the audio (usually either "Variable" or "Constant")
			if (line.startsWith("Bit rate mode")) {
				audio_bitratemode = QString(line.split("     : ").at(1));
			}

			// The bitrate of the audio
			if (line.startsWith("Bit rate   ")) {
				audio_bitrate = QString(line.split("     : ").at(1));
			}

			// The audio sampling rate
			if (line.startsWith("Sampling rate")) {
				audio_samplingrate = QString(line.split("     : ").at(1));
			}
		}
	}

	QVariantMap Details;
	if (!info_artist.isEmpty()) Details["Artist"] = info_artist;
	if (!info_album.isEmpty()) Details["Album"] = info_album;
	if (!info_title.isEmpty()) Details["Title"] = info_title;
	if (!duration.isEmpty()) Details["Length"] = (60 * Split2(duration, ":").first.toInt()) + (Split2(duration, ":").second.toInt());

	Add("mediainfo://", Details);
}