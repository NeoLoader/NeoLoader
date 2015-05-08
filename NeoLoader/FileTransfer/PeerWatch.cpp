#include "GlobalHeader.h"
#include "PeerWatch.h"
#include "../NeoCore.h"
#include "../../Framework/Buffer.h"
#include "../../Framework/Exception.h"
#include "../../Framework/RequestManager.h"
#include "Transfer.h"
#include "../../Framework/HttpServer/HttpHelper.h"
#include "../../Framework/Archive/Archive.h"
#include "../../Framework/qzlib.h"

CPeerWatch::CPeerWatch(QObject* qObject)
 : QObjectEx(qObject)
{
	m_NextCleanUp = 0;
	m_IpFilterDate = 0;
	//LoadIPFilter();
}

void CPeerWatch::Process(UINT Tick)
{
	uint64 uNow =  GetCurTick();

	if(m_NextCleanUp > uNow)
		return;
	m_NextCleanUp = uNow + SEC2MS(theCore->Cfg()->GetInt("PeerWatch/BlockTime"));

	for(map<CAddress, uint64>::iterator I = m_BanList.begin(); I != m_BanList.end(); )
	{
		if(I->second < uNow)
			I = m_BanList.erase(I);
		else
			I++;
	}

	for(map<SPeer, uint64>::iterator I = m_DeadList.begin(); I != m_DeadList.end(); )
	{
		if(I->second < uNow)
			I = m_DeadList.erase(I);
		else
			I++;
	}

	for(map<SPeer, uint64>::iterator I = m_AliveList.begin(); I != m_AliveList.end(); )
	{
		if(I->second < uNow)
			I = m_AliveList.erase(I);
		else
			I++;
	}
}

int CmpSIPFilterByAddr(const void* pvKey, const void* pvElement)
{
	uint32 ip = *(uint32*)pvKey;
	const SIPFilter* pIPFilter = (SIPFilter*)pvElement;
	if (ip < pIPFilter->uStart)
		return -1;
	if (ip > pIPFilter->uEnd)
		return 1;
	return 0;
}

bool CPeerWatch::CheckPeer(const CAddress& Address, uint16 uPort, bool bIncoming)
{
	int iEnable = theCore->Cfg()->GetInt("PeerWatch/Enable");
	if(iEnable == 0)
		return true;

	uint64 uNow =  GetCurTick();

	// test banned cleints
	map<CAddress, uint64>::iterator J = m_BanList.find(Address);
	if(J != m_BanList.end())
	{
		if(J->second > uNow)
			return false;
		//m_BanList.erase(J);
	}

	if(iEnable == 2)
	{
		uint32 uAddress = Address.ToIPv4();

		// check filtered IP's
		if(!m_FilterList.empty())
		{
			SIPFilter* ppFound = (SIPFilter*)bsearch(&uAddress, &m_FilterList[0], m_FilterList.size(), sizeof(SIPFilter), CmpSIPFilterByAddr);
			if (ppFound /*&& ppFound->uLevel < level*/)
				return false;
		}
	}

	// test dead sources
	if(bIncoming)
		return true;

	map<SPeer, uint64>::iterator I = m_DeadList.find(SPeer(Address, uPort));
	if(I != m_DeadList.end())
	{
		if(I->second > uNow)
			return false;
		//m_DeadList.erase(I);
	}
	return true;
}

void CPeerWatch::PeerFailed(const CAddress& Address, uint16 uPort)
{
	if(m_AliveList.find(SPeer(Address, uPort)) != m_AliveList.end())
		return;

	m_DeadList[SPeer(Address, uPort)] = GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("PeerWatch/BlockTime"));
}

void CPeerWatch::PeerConnected(const CAddress& Address, uint16 uPort)
{
	m_AliveList[SPeer(Address, uPort)] = GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("PeerWatch/BlockTime"));

	map<SPeer, uint64>::iterator I = m_DeadList.find(SPeer(Address, uPort));
	if(I != m_DeadList.end())
		m_DeadList.erase(I);
}

void CPeerWatch::BanPeer(const CAddress& Address)
{
	m_BanList[Address] = GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("PeerWatch/BanTime"));
}

///////////////////////////////////////////////////////////////////////////////////////////////
//

/*bool CPeerWatch::LoadIPFilter()
{
	QString IpFilter = theCore->Cfg()->GetString("PeerWatch/IPFilter");
	if(IpFilter.isEmpty())
		return true;
	
	if(IpFilter.left(6) == "ftp://" || IpFilter.left(7) == "http://" || IpFilter.left(8) == "https://")
	{
		QNetworkRequest Request = QNetworkRequest(IpFilter);
		IpFilter.clear();

		QDir Dir(CSettings::GetSettingsDir() + "/");
		foreach(const QString& File, Dir.entryList(QStringList("IpFilter-*"), QDir::Files))
		{
			int pos1 = File.indexOf("-");
			int pos2 = File.indexOf("-",pos1+1);
			if(pos1 == -1 || pos2 == -1)
				continue;

			QString sDate = File.mid(pos1+1, pos2 - (pos1 + 1));
			m_IpFilterDate = sDate.toULongLong();
			IpFilter = File;
			break;
		}

		Request.setRawHeader("Accept-Encoding", "gzip");
		QNetworkReply* pReply = m_IpFilterDate ? theCore->m_RequestManager->head(Request) : theCore->m_RequestManager->get(Request);
		connect(pReply, SIGNAL(finished()), this, SLOT(OnRequestFinished()));

		if(IpFilter.isEmpty())
			return false;
	}

	if(!(IpFilter.contains("/")
#ifdef WIN32
	 || IpFilter.contains("\\")
#endif
	))
		IpFilter.prepend(CSettings::GetSettingsDir() + "/");

	return ImportIPFilter(IpFilter);
}

void CPeerWatch::OnRequestFinished()
{
	QNetworkReply* pReply = (QNetworkReply*)sender();
	QByteArray Data = pReply->readAll();
	pReply->deleteLater();

	int StatusCode = pReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	QString StatusText = pReply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();

	QString Location = pReply->header(QNetworkRequest::LocationHeader).toString();
	if(!Location.isEmpty())
	{
		QNetworkRequest Request = QNetworkRequest(Location);
		Request.setRawHeader("Accept-Encoding", "gzip");
		QNetworkReply* pReply = m_IpFilterDate ? theCore->m_RequestManager->head(Request) : theCore->m_RequestManager->get(Request);
		connect(pReply, SIGNAL(finished()), this, SLOT(OnRequestFinished()));
		return;
	}

	if(StatusCode != 200)
	{
		LogLine(LOG_ERROR, tr("IpFilter Update, failed: %1").arg(StatusText));
		return;
	}

	QString FileName = GetArguments(pReply->rawHeader("Content-Disposition")).value("filename");
	if(FileName.isEmpty())
		FileName = Url2FileName(pReply->url().toString(), false);

	QDateTime Date = GetHttpDate(pReply->rawHeader("Date"));
	QList<QByteArray> list = pReply->rawHeaderList();

	if(m_IpFilterDate)
	{
		QString DateX = Date.toString();
		if(Date.toTime_t() > m_IpFilterDate && Date.toTime_t() - m_IpFilterDate > DAY2S(1))
		{
			m_IpFilterDate = 0;
			QNetworkRequest Request = QNetworkRequest(pReply->url());
			Request.setRawHeader("Accept-Encoding", "gzip");
			QNetworkReply* pReply = theCore->m_RequestManager->get(Request);
			connect(pReply, SIGNAL(finished()), this, SLOT(OnRequestFinished()));
		}
		return;
	}

	if(FileName.isEmpty() || Data.isEmpty())
	{
		LogLine(LOG_ERROR, tr("IpFilter Update, failed"));
		return;
	}

	m_IpFilterDate = Date.toTime_t();

	if(IsgZiped(Data))
	{
		struct z_stream_s* zStream = NULL;
		Data = ungzip_arr(zStream, Data);
		clear_z(zStream);
	}

	SArcInfo Info = GetArcInfo(FileName);
	if(Info.FormatIndex != 0)
	{
		QBuffer Buffer(&Data);
		CArchive IpFilterArc(FileName, &Buffer);
		if(!IpFilterArc.Open())
		{
			LogLine(LOG_ERROR, tr("IpFilter archive is damaged!"));
			return;
		}
		else
		{
			int ArcIndex = -1;
			if(IpFilterArc.FileCount() == 1)
				ArcIndex = 0;
			else
			{
				QStringList IpFilters;
				IpFilters.append("ipfilter.dat");
				IpFilters.append("ipfilter.p2p");
				IpFilters.append("guarding.p2p");
				IpFilters.append("guarding.p2p.txt");
				foreach(const QString& Filter, IpFilters)
				{
					ArcIndex = IpFilterArc.FindByPath(Filter);
					if(ArcIndex != -1)
						break;
				}
			}
			if(ArcIndex == -1)
			{
				LogLine(LOG_ERROR, tr("IpFilter archive does not contain a known ipfilter file!"));
				return;
			}

			FileName = IpFilterArc.FileProperty(ArcIndex, "Path").toString();

			QByteArray Buffer;
			QMap<int, QIODevice*> Files;
			Files.insert(ArcIndex, new QBuffer(&Buffer));
			if(!IpFilterArc.Extract(&Files))
			{
				LogLine(LOG_ERROR, tr("IpFilter archive is damaged!"));
				return;
			}
			IpFilterArc.Close();

			Data = Buffer;
		}
	}
	else 
	{
		StrPair NameExt = Split2(FileName,".", true);
		if(NameExt.second == "gz")
			FileName = NameExt.first + ".p2p";
	}

	QDir Dir(CSettings::GetSettingsDir() + "/");
	foreach(const QString& File, Dir.entryList(QStringList("IpFilter-*"), QDir::Files))
	{
		int pos1 = File.indexOf("-");
		int pos2 = File.indexOf("-",pos1+1);
		if(pos1 == -1 || pos2 == -1)
			continue;
		QFile::remove(CSettings::GetSettingsDir() + "/" + File);
	}

	QString IpFilter = CSettings::GetSettingsDir() + "/IpFilter-" + QString::number(m_IpFilterDate) + "-" + FileName;
	QFile File(IpFilter);
	File.open(QFile::WriteOnly);
	File.write(Data);
	File.close();

	ImportIPFilter(IpFilter);
}

int CmpSIPFilterByStartAddr(const void* p1, const void* p2)
{
	const SIPFilter* rng1 = (SIPFilter*)p1;
	const SIPFilter* rng2 = (SIPFilter*)p2;
	if (rng1->uStart < rng2->uStart)
		return -1;
	if (rng1->uEnd > rng2->uEnd)
		return 1;
	return 0;
}

bool CPeerWatch::ImportIPFilter(const QString& FileName)
{
	LogLine(LOG_DEBUG, tr("Loading IpFilter File: %1").arg(FileName));

	m_FilterList.clear();

	QFile File(FileName);
	if(!File.open(QFile::ReadOnly))
		return false;

	enum EFileType
	{
		Unknown = 0,
		FilterDat = 1,		// ipfilter.dat/ip.prefix format
		PeerGuardian = 2,	// PeerGuardian text format
		PeerGuardian2 = 3	// PeerGuardian binary format
	} eFileType = Unknown;
	
	QFileInfo Info(File);

	if(Info.suffix().compare(".p2p") == 0 || (Info.completeBaseName().right(12).compare("guarding.p2p") == 0 && Info.suffix().compare(".txt") == 0))
		eFileType = PeerGuardian;
	else if (Info.suffix().compare(".prefix") == 0)
		eFileType = FilterDat;
	else
	{
		static const byte _aucP2Bheader[] = "\xFF\xFF\xFF\xFFP2B";
		byte aucHeader[sizeof _aucP2Bheader - 1];
		if (File.read((char*)aucHeader, sizeof(aucHeader)) == sizeof(aucHeader))
		{
			if (memcmp(aucHeader, _aucP2Bheader, sizeof _aucP2Bheader - 1) == 0)
				eFileType = PeerGuardian2;
			else
				File.seek(0);
		}
	}

	uint64 uStartTick = GetCurTick();
	int iEntries = 0;
	if (eFileType == PeerGuardian2) // binary
	{
		// Version 1: strings are ISO-8859-1 encoded
		// Version 2: strings are UTF-8 encoded
		uint8 nVersion;
		if (File.read((char*)&nVersion, sizeof(nVersion)) != sizeof(nVersion) || !(nVersion==1 || nVersion==2))
			return false;
		
		SIPFilter IPFilter;
		IPFilter.uLevel = 100;
		while (!File.atEnd())
		{
			//char szName[256];
			//int iLen = 0;
			for (;;) // read until NUL or EOF
			{
				char iChar = 0;
				if(File.read(&iChar, 1) != 1)
					break;
				//if (iLen < sizeof(szName) - 1)
				//	szName[iLen++] = iChar;
				if (iChar == '\0')
					break;
			}
			//szName[iLen] = '\0';
					
			if (File.read((char*)&IPFilter.uStart, sizeof(IPFilter.uStart)) != sizeof(IPFilter.uStart))
				break;
			IPFilter.uStart = _ntohl(IPFilter.uStart);

			if (File.read((char*)&IPFilter.uEnd, sizeof(IPFilter.uEnd)) != sizeof(IPFilter.uEnd))
				break;
			IPFilter.uEnd = _ntohl(IPFilter.uEnd);

			// (nVersion == 2) ? OptUtf8ToStr(szName, iLen) : 
			m_FilterList.push_back(IPFilter);
			iEntries++;
		}
	}
	else
	{
		char LineBuff[256];
		while (!File.atEnd()) 
		{
			qint64 res = File.readLine(LineBuff, sizeof(LineBuff) - 1);
			if(res == -1)
				break;
			QByteArray Line = QByteArray::fromRawData(LineBuff, res);

			// ignore comments & too short lines
			if (Line.length() < 5 || Line.at(0) == '#' || Line.at(0) == '/')
				continue;

			if (eFileType == Unknown)
			{
				// looks like html
				if (Line.indexOf('>') > -1 && Line.indexOf('<') > -1)
					Line.remove(0, Line.lastIndexOf('>') + 1);

				// check for <IP> - <IP> at start of line
				UINT u1, u2, u3, u4, u5, u6, u7, u8;
				if (sscanf(Line.data(), "%u.%u.%u.%u - %u.%u.%u.%u", &u1, &u2, &u3, &u4, &u5, &u6, &u7, &u8) == 8)
					eFileType = FilterDat;
				else
				{
					// check for <description> ':' <IP> '-' <IP>
					int iColon = Line.indexOf(':');
					if (iColon > -1)
					{
						QByteArray strIPRange = Line.mid(iColon + 1);
						UINT u1, u2, u3, u4, u5, u6, u7, u8;
						if (sscanf(strIPRange.data(), "%u.%u.%u.%u - %u.%u.%u.%u", &u1, &u2, &u3, &u4, &u5, &u6, &u7, &u8) == 8)
							eFileType = PeerGuardian;
					}
				}
			}

			SIPFilter IPFilter;
			UINT u1, u2, u3, u4, u5, u6, u7, u8;
			if (eFileType == FilterDat)
			{
				int iDescStart = 0;
				int iItems = sscanf(Line.data(), "%u.%u.%u.%u - %u.%u.%u.%u , %u , %n", &u1, &u2, &u3, &u4, &u5, &u6, &u7, &u8, &IPFilter.uLevel, &iDescStart);
				if (iItems < 8)
					continue;

				if (iItems == 8)
					IPFilter.uLevel = 100; // set default level
			}
			else if (eFileType == PeerGuardian)
			{
				int iPos = Line.lastIndexOf(':');
				if (iPos < 0)
					continue;

				QByteArray strIPRange = Line.mid(iPos + 1, Line.length() - iPos);
				if (sscanf(strIPRange.data(), "%u.%u.%u.%u - %u.%u.%u.%u", &u1, &u2, &u3, &u4, &u5, &u6, &u7, &u8) != 8)
					continue;

				IPFilter.uLevel = 100;
			}

			((byte*)&IPFilter.uStart)[0] = (byte)u4;
			((byte*)&IPFilter.uStart)[1] = (byte)u3;
			((byte*)&IPFilter.uStart)[2] = (byte)u2;
			((byte*)&IPFilter.uStart)[3] = (byte)u1;

			((byte*)&IPFilter.uEnd)[0] = (byte)u8;
			((byte*)&IPFilter.uEnd)[1] = (byte)u7;
			((byte*)&IPFilter.uEnd)[2] = (byte)u6;
			((byte*)&IPFilter.uEnd)[3] = (byte)u5;

			m_FilterList.push_back(IPFilter);
			iEntries++;
		}
	}
	LogLine(LOG_SUCCESS, tr("Loaded %1 IpFilter entries in %2 seconds").arg(iEntries).arg((double)(GetCurTick() - uStartTick)/1000.0, 0, 'f', 2));

	if(!m_FilterList.empty())
		qsort(&m_FilterList[0], m_FilterList.size(), sizeof(SIPFilter), CmpSIPFilterByStartAddr);

	return true;
}
*/