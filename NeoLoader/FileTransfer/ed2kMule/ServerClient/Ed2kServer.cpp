#include "GlobalHeader.h"
#include "Ed2kServer.h"
#include "ServerClient.h"
#include "../../../NeoCore.h"
#include "../MuleManager.h"
#include "../MuleServer.h"
#include "../MuleSource.h"
#include "../../../FileList/FileManager.h"
#include "../../../FileList/FileDetails.h"
#include "../../../FileList/Hashing/FileHashSet.h"

#define MAX_FILES_PER_FRAME 15

CEd2kServer::CEd2kServer(QObject* qObject)
 : QObjectEx(qObject)
{
	m_Port = 0;
	m_CryptoPort = 0;
	m_UDPPort = 0;
	m_UDPKey = 0;

	m_Challenge = 0;
	m_FlagsTCP.Bits = 0;
	m_FlagsUDP.Bits = 0;

	m_SoftLimit = 0;
	m_HardLimit = 0;
	m_FileCount = 0;

	m_UserLimit = 0;
	m_LowIDCount = 0;
	m_UserCount = 0;

	m_bStatic = false;
	m_uTimeOut = 0;

	m_pClient = NULL;

	m_LastConnectAttempt = 0;
	m_ConnectFailCount = 0;
	m_LastDisconnected = 0 ;

	m_LastPingAnswer = 0;
	m_LastPingAttempt = 0;
	m_PingAttemptCount = 0;

	m_NextRequestFrame = -1; // never
}

void CEd2kServer::Process(UINT Tick)
{
	if(m_pClient)
		m_pClient->Process(Tick);

	if(m_Files.isEmpty())
	{
		if(m_pClient && HasTimmedOut())
			Disconnect();
		return;
	}
		
	if(!m_pClient)
	{
		if(GetLastPingAnswer() // we want to have the server info first
		  && (m_LastConnectAttempt == 0 
		   || (GetCurTick() - m_LastConnectAttempt > SEC2MS(10) * m_ConnectFailCount && m_ConnectFailCount < 5)
		   || (GetCurTick() - m_LastConnectAttempt > MIN2MS(30) && m_ConnectFailCount < 10)
		  )
		 )
			Connect(); // Note: this updates the timeout
	}
	else if(m_pClient->GetClientID() != 0)
	{
		UpdateTimeOut();

		int ReqCount = 0;
		QList<CFile*> PubFiles;
		uint64 uNow = GetCurTick();
		// Note: This list is _NOT_ guaranteed to hold valid pointers !!!
		// Howe Ever: we ente this function just after updating it so it should be fine -_-
		for(QMap<CFile*, SFile>::iterator I = m_Files.begin(); I != m_Files.end(); I++)
		{
			CFile* pFile = I.key();
			SFile& File = I.value();

			if(IsLargeEd2kMuleFile(pFile->GetFileSize()) && !SupportsLargeFiles())
				continue; // We should do this check befoure scheduling the file for this server :/

			///////////////////////////////////////////////////////////////
			// Publish File
			if(!(pFile->IsPending() || pFile->MetaDataMissing()))
			{
				if(!File.bPulished && PubFiles.count() < GetSoftLimit())
				{
					File.bPulished = true;
					PubFiles.append(pFile);
				}
			}

			///////////////////////////////////////////////////////////////
			// Find Sources
			if(!pFile->IsComplete())
			{
				if(File.uNextReask < uNow && m_NextRequestFrame < uNow && ReqCount < MAX_FILES_PER_FRAME)
				{
					ReqCount++;
					m_pClient->RequestSources(pFile);
				}
			}
		}

		if(PubFiles.count() > 0)
			m_pClient->PublishFiles(PubFiles);
		if(ReqCount > 0)
			m_NextRequestFrame = uNow + SEC2MS(MAX_FILES_PER_FRAME * (16 + 4));
	}
}

void CEd2kServer::UpdateTimeOut()
{
	m_uTimeOut = GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("Ed2kMule/KeepServers"));
}

bool CEd2kServer::Connect()
{
	if(m_pClient)
		return false;

	UpdateTimeOut();

	CServerClient* pClient = new CServerClient(this);

	connect(pClient, SIGNAL(Connected()), this, SLOT(OnConnected()));
	connect(pClient, SIGNAL(Disconnected()), this, SLOT(OnDisconnected()));

	m_LastConnectAttempt = GetCurTick();
	if(pClient->Connect())
	{
		LogLine(LOG_INFO, tr("Connecting to ed2k server: ed2k://%1:%2/").arg(GetIP().ToQString()).arg(GetPort()));
		m_pClient = pClient;
	}
	else // connect failed probably due to crypto issues
		delete pClient;
	
	return m_pClient != NULL;
}

void CEd2kServer::OnConnected()
{
	LogLine(LOG_SUCCESS, tr("Connected to Ed2k Server: ed2k://%1:%2/, ID: %3 (%4)").arg(GetIP().ToQString()).arg(GetPort())
		.arg(m_pClient->GetClientID()).arg(SMuleSource::IsLowID(m_pClient->GetClientID()) ? tr("Low") : tr("High")));

	m_ConnectFailCount = 0;

	m_NextRequestFrame = 0;
}

void CEd2kServer::Disconnect()
{
	if(!m_pClient)
		return;

	m_Files.clear();

	m_LastDisconnected = GetCurTick();

	m_pClient->Disconnected();
}

void CEd2kServer::OnDisconnected()
{
	LogLine(LOG_WARNING, tr("Disconnected from Ed2k Server: ed2k://%1:%2/").arg(GetIP().ToQString()).arg(GetPort()));

	ASSERT(m_pClient);
	if(m_pClient->GetClientID() == 0)
		m_ConnectFailCount++;

	disconnect(m_pClient, 0, 0, 0);
	m_pClient->deleteLater();
	m_pClient = NULL;
}

void CEd2kServer::AddSources(CBuffer& Packet, bool bWithObfu)
{
	CFileHash Hash(HashEd2k);
	Hash.SetHash(Packet.ReadQData(Hash.GetSize()));
	CFile* pFile = theCore->m_FileManager->GetFileByHash(&Hash);

	m_Files[pFile].uNextReask = GetCurTick() + MIN2MS(15); // Customise

	bool bLowID = SMuleSource::IsLowID(m_pClient->GetClientID());

	int Count = Packet.ReadValue<uint8>();
	for(;Count > 0;Count--)
	{
		SMuleSource Mule;
		uint32 uClientID = Packet.ReadValue<uint32>(); // ID is IP in network order
		if(SMuleSource::IsLowID(uClientID))
			Mule.ClientID = uClientID;
		else
			Mule.ClientID = _ntohl(uClientID); // switch to eMules hybrid format
		if(Mule.ClientID == 0xFFFFFFFF) // 0xFFFFFFFF means HighID with IPv6 only should not be listed here
		{
			Mule.SetIP(Packet.ReadData(16));
			Mule.OpenIPv6 = true; // HighID IPv6 :)
		}
		else if(!Mule.HasLowID()) 
			Mule.SetIP(Mule.ClientID);
		Mule.TCPPort = Packet.ReadValue<uint16>();
		if(bWithObfu)
		{
			uint8 uConOpts = Packet.ReadValue<uint8>();
			if((uConOpts & 0x80) != 0)
				Mule.UserHash.Set(Packet.ReadData(16));
			Mule.ConOpts.Bits = uConOpts & 0x7F;
			Mule.ConOpts.Fields.SupportsNatTraversal = (Mule.HasLowID() && bLowID); // If we are LowID and got a LowID it means the other node supports NatT
		}
		if(pFile) // Note: for global requests we must read through it
			theCore->m_MuleManager->AddToFile(pFile, Mule, eEd2kSvr);
	}
}

CFile* CEd2kServer::ReadFile(CBuffer& Packet)
{
	QByteArray Hash = Packet.ReadQData(16);
	Packet.ReadValue<uint32>(); // ClientID - depricated
	Packet.ReadValue<uint16>(); // ClientPort - depricated

	QVariantMap Tags = CMuleTags::ReadTags(&Packet);

	QString FileName = Tags.take(TO_NUM(FT_FILENAME)).toString();
	uint64	uFileSize = Tags.take(TO_NUM(FT_FILESIZE)).toUInt();
			uFileSize |= (uint64)Tags.take(TO_NUM(FT_FILESIZE)).toUInt() << 32;

	CScoped<CFileHashSet> pHashSet = new CFileHashSet(HashEd2k, uFileSize);
	if(uFileSize == 0 || !pHashSet->SetHash(Hash))
		return NULL;

	CFile* pFile = new CFile();
	pFile->AddHash(CFileHashPtr(pHashSet.Detache()));
	pFile->AddEmpty(HashEd2k, FileName, uFileSize);

	QString ServerUrl = QString("ed2k://|server,%1:%2|").arg(GetIP().ToQString(true)).arg(GetPort());
	pFile->SetProperty("Ed2kServers", QStringList(ServerUrl));

	QVariantMap Details;
	Details["Name"] = FileName;
	for(QVariantMap::iterator I = Tags.begin(); I != Tags.end(); ++I)
	{
		const QVariant& Value = I.value();
		switch(FROM_NUM(I.key()))
		{
			case FT_FILERATING:
			{
				uint16 nPackedRating = Value.toUInt();

				// Percent of clients (related to 'Availability') which rated on that file
                uint16 uPercentClientRatings = ((byte)((nPackedRating >> 8) & 0xff));
				(void)uPercentClientRatings;

				// Average rating used by clients
                uint16 uAvgRating = ((byte)(nPackedRating & 0xff));
				int Rating = uAvgRating / (255/5/*RatingExcellent*/);

				Details["Rating"] = Rating;
				break;
			}
			case FT_SOURCES:			Details["Availability"] = Value;	break;
			case FT_COMPLETE_SOURCES:										break;

			case FT_MEDIA_ARTIST:		Details["Artist"] = Value;			break;
			case FT_MEDIA_ALBUM:		Details["Album"] = Value;			break;
			case FT_MEDIA_TITLE:		Details["Title"] = Value;			break;
			case FT_MEDIA_LENGTH:		Details["Length"] = Value;			break;
			case FT_MEDIA_BITRATE:		Details["Bitrate"] = Value;			break;
			case FT_MEDIA_CODEC:		Details["Codec"] = Value;			break;
		}
	}
	pFile->GetDetails()->Add(ServerUrl, Details);

	return pFile;
}

void CEd2kServer::WriteFile(CFile* pFile, CBuffer& Packet)
{
	CFileHash* pHash = pFile->GetHash(HashEd2k);
	ASSERT(pHash);

	Packet.WriteQData(pHash->GetHash());

	// Comment copyed form eMule (Fair Use):
	// *) This function is used for offering files to the local server and for sending
	//    shared files to some other client. In each case we send our IP+Port only, if
	//    we have a HighID.
	// *) Newer eservers also support 2 special IP+port values which are used to hold basic file status info.
	uint32 nClientID = 0;
	uint16 nClientPort = 0;
	// we use the 'TCP-compression' server feature flag as indicator for a 'newer' server.
	if (SupportsCompression())
	{
		if (pFile->IsIncomplete())
		{
			// publishing an incomplete file
			nClientID = 0xFCFCFCFC;
			nClientPort = 0xFCFC;
		}
		else
		{
			// publishing a complete file
			nClientID = 0xFBFBFBFB;
			nClientPort = 0xFBFB;
		}
	}
	Packet.WriteValue<uint32>(nClientID);
	Packet.WriteValue<uint16>(nClientPort);
	
	QVariantMap Tags;
	Tags[TO_NUM(FT_FILENAME)]		= pFile->GetFileName();
	if(!IsLargeEd2kMuleFile(pFile->GetFileSize()))
		Tags[TO_NUM(FT_FILESIZE)]	= (uint32)pFile->GetFileSize();
	else
	{
		ASSERT(SupportsLargeFiles());
		Tags[TO_NUM(FT_FILESIZE)]	= (uint32)pFile->GetFileSize();
		Tags[TO_NUM(FT_FILESIZE_HI)]= (uint32)(pFile->GetFileSize() >> 32);
	}

	if(int Rating = pFile->GetProperty("Rating").toInt())
		Tags[TO_NUM(FT_FILERATING)]	= Rating;

	foreach(const QString& Name, QString("Title|Artist|Album|Length|Bitrate|Codec").split("|"))
	{
		if(pFile->HasProperty(Name))
		{
			if(Name == "Title")			Tags[TO_NUM(FT_MEDIA_TITLE)] = pFile->GetProperty(Name).toString();
			else if(Name == "Artist")	Tags[TO_NUM(FT_MEDIA_ARTIST)] = pFile->GetProperty(Name).toString();
			else if(Name == "Album")	Tags[TO_NUM(FT_MEDIA_ALBUM)] = pFile->GetProperty(Name).toString();
			else if(Name == "Length")	Tags[TO_NUM(FT_MEDIA_LENGTH)] = pFile->GetProperty(Name).toInt();
			else if(Name == "Bitrate")	Tags[TO_NUM(FT_MEDIA_BITRATE)] = pFile->GetProperty(Name).toInt();
			else if(Name == "Codec")	Tags[TO_NUM(FT_MEDIA_CODEC)] = pFile->GetProperty(Name).toString();
		}
	}

	CMuleTags::WriteTags(Tags, &Packet, SupportsNewTags());
}

QString CEd2kServer::GetStatusStr()
{
	//ping = GetLastPingAnswer() > GetLastPingAttempt() ? GetLastPingAnswer() - GetLastPingAttempt() : 0;
	if(!m_pClient)
		return "Disconnected";
	else if(m_pClient->GetClientID() == 0)
		return "Connecting";
	else
		return "Connected";
}