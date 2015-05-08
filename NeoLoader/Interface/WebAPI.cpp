#include "GlobalHeader.h"
#include "../NeoCore.h"
#include "WebAPI.h"
#include "../../Framework/HttpServer/HttpSocket.h"
#include "../GUI/NeoLoader.h"
#include <QLibrary>
#include "CoreServer.h"
#include "../../Framework/OtherFunctions.h"

CWebAPI::CWebAPI(QObject* qObject)
: QObjectEx(qObject)
{
	theCore->m_HttpServer->RegisterHandler(this,"/WebAPI");

//#ifdef __APPLE__
//	m_QrEncode = (qrencode)QLibrary::resolve(QApplication::applicationDirPath() + "/libqrencode.1.dylib","qrencode");
//#else
//	m_QrEncode = (qrencode)QLibrary::resolve("qrencode","qrencode");
//#endif
//	if(!m_QrEncode)
//	{
//		LogLine(LOG_ERROR, tr("qrencode library is missing"));
//		m_QrEncode = NULL;
//	}

	//m_DecodeQr = (decodeqr)QLibrary::resolve("decodeqr","decodeqr");
	//if(!m_DecodeQr)
	//{
	//	LogLine(LOG_ERROR, tr("decodeqr library is missing"));
	//	m_DecodeQr = NULL;
	//}

	m_WebAPIPath = theCore->Cfg(false)->GetString("HttpServer/WebAPIPath");
	if(m_WebAPIPath.isEmpty())
		m_WebAPIPath = theCore->Cfg()->GetAppDir() + "/WebAPI";
#ifdef USE_7Z
	m_WebAPI = NULL;
	if(!QFile::exists(m_WebAPIPath))
	{
		m_WebAPI = new CCachedArchive(theCore->Cfg()->GetAppDir() + "/WebAPI.7z");
		if(!m_WebAPI->Open())
		{
			LogLine(LOG_ERROR, tr("WebAPI.7z archive is damaged or missing!"));
			delete m_WebAPI;
			m_WebAPI = NULL;
		}
	}
#endif
}

CWebAPI::~CWebAPI()
{
#ifdef USE_7Z
	if(m_WebAPI)
	{
		m_WebAPI->Close();
		delete m_WebAPI;
	}
#endif
}

void CWebAPI::OnRequestCompleted()
{
	CHttpSocket* pRequest = (CHttpSocket*)sender();
	ASSERT(pRequest->GetState() == CHttpSocket::eHandling);
	QString Path = pRequest->GetPath();
	TArguments Cookies = GetArguments(pRequest->GetHeader("Cookie"));
	TArguments Arguments = GetArguments(pRequest->GetQuery().mid(1),'&');

	switch(pRequest->GetType())
	{
		case CHttpSocket::eDELETE:
			pRequest->RespondWithError(501);
		case CHttpSocket::eHEAD:
		case CHttpSocket::eOPTIONS:
			pRequest->SendResponse();
			return;
	}

	if(Path.compare("/WebAPI/") == 0)
		pRequest->RespondWithError(403);

	else if(Path.left(14).compare("/WebAPI/Icons/") == 0)
	{
		int Size;
		if(Arguments["Size"] == "Small")
			Size = 16;
		else // if(Arguments["Size"] == "Large")
			Size = 32;
		QString Ext = Split2(Path.mid(14), ".").first;
		QString IconPath = theCore->Cfg()->GetSettingsDir() + "/Cache/Icons/" + Ext + QString::number(Size) + ".png";
		if(!QFile::exists(IconPath))
		{
			if(theLoader)
				QMetaObject::invokeMethod(theLoader, "CreateFileIcon", Qt::BlockingQueuedConnection, Q_ARG(QString, Ext));
			else
				IconPath = ":/Icon" + QString::number(Size) + ".png";
		}
		pRequest->SetCaching(HR2S(48));
		pRequest->RespondWithFile(IconPath);
	}
	else if(Path.left(11).compare("/WebAPI/FS/") == 0)
	{
		StrPair CmdExt = Split2(Path.mid(11),".");

		QVariantMap Result;

		if(CmdExt.first.compare("dir", Qt::CaseInsensitive) == 0)
		{
			QVariantList Entrys;
			QString DirPath = Arguments["Path"];
			QDir Dir(DirPath);
			foreach (const QString& Name, Dir.entryList())
			{
				if (Name.compare(".") == 0 || Name.compare("..") == 0)
					continue;

				QVariantMap Entry;
				QFileInfo Info(DirPath + "/" + Name);
				Entry["Name"] = Info.fileName();
				Entry["Created"] = Info.created();
				Entry["Modifyed"] = Info.lastModified();
				if (Info.isDir())
					Entry["Size"] = "dir";
				else
					Entry["Size"] = Info.size();
				Entrys.append(Entry);
			}
			Result["List"] = Entrys;
		}
		else if(!theCore->Cfg(false)->GetBool("HttpServer/EnableFTP"))
			pRequest->RespondWithError(403);
		else if(CmdExt.first.compare("mkdir", Qt::CaseInsensitive) == 0)
		{
			CreateDir(Arguments["Path"]);
			Result["result"] = "ok";
		}
		else if(CmdExt.first.compare("rename", Qt::CaseInsensitive) == 0)
		{
			QString OldPath = Arguments["OldPath"];
			QFileInfo Info(OldPath);
			if(Info.isDir())
				CopyDir(OldPath, Arguments["NewPath"], true);
			else
				QFile::rename(OldPath, Arguments["NewPath"]);
			Result["result"] = "ok";
		}
		else if(CmdExt.first.compare("copy", Qt::CaseInsensitive) == 0)
		{
			QString OldPath = Arguments["Path"];
			QFileInfo Info(OldPath);
			if(Info.isDir())
				CopyDir(OldPath, Arguments["NewPath"]);
			else
				QFile::copy(OldPath, Arguments["NewPath"]);
			Result["result"] = "ok";
		}
		else if(CmdExt.first.compare("remove", Qt::CaseInsensitive) == 0)
		{
			QString OldPath = Arguments["Path"];
			QFileInfo Info(OldPath);
			if(Info.isDir())
				DeleteDir(OldPath);
			else
				QFile::remove(OldPath);
			Result["result"] = "ok";
		}
		else if(CmdExt.first.compare("upload", Qt::CaseInsensitive) == 0)
		{
			QFile* pFile = m_Files.value(pRequest);
			if(pFile && pFile->isOpen())
			{
				pFile->close();
				Result["result"] = "ok";
			}
			else
				Result["result"] = "error";
		}
		else if(CmdExt.first.compare("download", Qt::CaseInsensitive) == 0)
		{
			QFile* pFile = new QFile(Arguments["Path"]);
			if(pFile->open(QFile::ReadOnly))
			{
				m_Files.insert(pRequest,pFile);
				pRequest->SetResponseBuffer(pFile);
			}
			else
			{
				delete pFile;
				pRequest->RespondWithError(404);
			}
		}
		else
			pRequest->RespondWithError(404);


		if(!Result.isEmpty())
		{
			CIPCSocket::EEncoding Encoding = CIPCSocket::eUnknown;
			if(CmdExt.second == "js")
				Encoding = CIPCSocket::eJson;
			else if(CmdExt.second == "xml")
				Encoding = CIPCSocket::eXML;
			else if(CmdExt.second == "benc")
				Encoding = CIPCSocket::eBencode;

			QString sResult = CIPCSocket::Variant2String(Result, Encoding);

			// send reply
			if(Arguments.contains("callback"))
				pRequest->write(Arguments["callback"].toUtf8() + "(" + sResult.toUtf8() + ");");
			else
				pRequest->write(sResult.toUtf8());
		}
	}

	//else if(Path.compare("/WebAPI/QrEncode.png") == 0)
	//{
	//	if(!m_QrEncode)
	//		pRequest->RespondWithError(503,"QR Encode library not available");
	//	else if(!m_QrEncode(QByteArray::fromPercentEncoding(Arguments["Data"].toLatin1()), pRequest, Arguments["Short"] == "true", Arguments["Ecc"].toInt(), Arguments.contains("Size") ? Arguments["Size"].toInt() : 3, false))
	//		pRequest->RespondWithError(400,"Invalid QR parameters");
	//}

	//else if(Path.compare("/WebAPI/DecodeQr/") == 0)
	//{
	//	if(!m_DecodeQr)
	//		pRequest->RespondWithError(503,"QR Decode library not available");
	//	else if(pRequest->IsPost())
	//	{
	//		QIODevice* pCode = pRequest->GetPostFile("Code");
	//		pCode->seek(0);
	//		QByteArray Data;
	//		if(m_DecodeQr(pCode, Data))
	//			pRequest->write(Data);
	//		else
	//			pRequest->RespondWithError(400,"Invalid Immage");
	//	}
	//}

	else if(Path.left(8).compare("/WebAPI/") == 0)
	{
		QString FilePath = Path.mid(7);

		pRequest->SetCaching(HR2S(12));
#ifdef USE_7Z
		if(m_WebAPI)
		{
			int ArcIndex = m_WebAPI->FindByPath(FilePath);
			if(ArcIndex != -1)
			{
				QByteArray Buffer;
				QMap<int, QIODevice*> Files;
				Files.insert(ArcIndex, new QBuffer(&Buffer));
				if(m_WebAPI->Extract(&Files))
					pRequest->write(Buffer);
				else
					pRequest->RespondWithError(500);
			}
			else
				pRequest->RespondWithError(404);
		}
		else
#endif
		{
			FilePath.prepend(m_WebAPIPath);
			pRequest->RespondWithFile(FilePath);
		}
	}

	pRequest->SendResponse();
}

void CWebAPI::OnFilePosted(QString Name, QString File, QString Type)
{
	CHttpSocket* pRequest = (CHttpSocket*)sender();

	if(!theCore->Cfg(false)->GetBool("HttpServer/EnableFTP"))
		return;

	TArguments Cookies = GetArguments(pRequest->GetHeader("Cookie"));
	TArguments Arguments = GetArguments(pRequest->GetQuery().mid(1),'&');
	if(!theCore->CheckLogin(Cookies["LoginToken"]) && !theCore->CheckLogin(Arguments["LoginToken"]))
		return; // not setting a buffer will timeout the request

	QFile* pFile = new QFile(Arguments["Path"]);
	pFile->open(QFile::WriteOnly);
	m_Files.insert(pRequest,pFile);

	pRequest->SetPostBuffer(pFile);
}

void CWebAPI::HandleRequest(CHttpSocket* pRequest)
{
	connect(pRequest, SIGNAL(readChannelFinished()), this, SLOT(OnRequestCompleted()));
	connect(pRequest, SIGNAL(FilePosted(QString, QString, QString)), this, SLOT(OnFilePosted(QString, QString, QString)));
}

void CWebAPI::ReleaseRequest(CHttpSocket* pRequest)
{
	disconnect(pRequest, SIGNAL(readChannelFinished()), this, SLOT(OnRequestCompleted()));
	disconnect(pRequest, SIGNAL(FilePosted(QString, QString, QString)), this, SLOT(OnFilePosted(QString, QString, QString)));

	if(QFile* pFile = m_Files.take(pRequest))
		delete pFile;
}
