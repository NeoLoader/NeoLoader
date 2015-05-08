#include "GlobalHeader.h"
#include "../NeoCore.h"
#include "WebRoot.h"
#include "CoreClient.h"
#include "CoreServer.h"
#include "../../Framework/HttpServer/HttpSocket.h"
#include "../../Framework/OtherFunctions.h"

CWebRoot::CWebRoot(QObject* qObject)
: QObjectEx(qObject)
{
	theCore->m_HttpServer->RegisterHandler(this);
}

CWebRoot::~CWebRoot()
{
}

bool CWebRoot::TestLogin(CHttpSocket* pRequest)
{
	TArguments Cookies = GetArguments(pRequest->GetHeader("Cookie"));
	TArguments Arguments = GetArguments(pRequest->GetQuery().mid(1),'&');

	bool bLoggedIn = false;
	if(pRequest->GetAddress() == QHostAddress("127.0.0.1") || pRequest->GetAddress() == QHostAddress("::1"))
	{
		if(pRequest->GetHeader("User-Agent") == "NeoClient"
		 || QRegExp(theCore->Cfg(false)->GetString("HttpServer/Whitelist")).exactMatch(pRequest->GetHeader("Referer")))
			bLoggedIn = true;
	}
	if(!bLoggedIn && Arguments.contains("password"))
	{
		bLoggedIn = theCore->TestLogin(Arguments["password"]);
	}
	if(!bLoggedIn)
		bLoggedIn = theCore->CheckLogin(Cookies["LoginToken"]) || theCore->CheckLogin(Arguments["LoginToken"]);
	return bLoggedIn;
}

void CWebRoot::OnRequestCompleted()
{
	CHttpSocket* pRequest = (CHttpSocket*)sender();
	ASSERT(pRequest->GetState() == CHttpSocket::eHandling);
	QString Path = pRequest->GetPath();
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

	bool bLoggedIn = TestLogin(pRequest);

	if(Path.left(11).compare("/Resources/") == 0)
	{
		pRequest->SetCaching(HR2S(12));
		QString Resource = ":" + Path.mid(10);
		if(Resource.compare(":/") == 0)
			Resource = ":/index.html";
		if(!Resource.contains(".")) // Note: we must make sure not all resources can be accessed, thos we allow only acces to resources without aliases
			pRequest->RespondWithError(403);
		else if(pRequest->IsGet())
			pRequest->RespondWithFile(Resource);
		else
			pRequest->RespondWithError(405);
	}
	else if(Path.left(6).compare("/About") == 0)
	{
		pRequest->RespondWithFile(":/Templates/About");
	}

#ifdef _DEBUG
	else if(Path.left(6).compare("/Echo/") == 0)
	{
		if(pRequest->HasRawData())
		{
			QByteArray Data = pRequest->readAll();
			pRequest->write(Data);
		}
	}
#endif

	else if(Path.left(7).compare("/Login/") == 0)
	{
		if(pRequest->IsPost())
		{
			//QString LoginToken = theCore->CheckLogin(pRequest->GetPostValue("UserName"), pRequest->GetPostValue("Password"));
			if(!theCore->TestLogin(pRequest->GetPostValue("Password")))
				pRequest->RespondWithError(401,"Login Failed");
			else
			{
				pRequest->SetHeader("Set-Cookie", QString("LoginToken=" + theCore->GetLoginToken() + "; path=/").toUtf8());  // ToDo-Now: "; Expires= ..."
				pRequest->SetHeader("Location", Path.remove(0,6).toUtf8());
				pRequest->RespondWithError(303);
			}
		}
		else if(Arguments.contains("callback")) // jsonp
		{
			QString Callback = Arguments["callback"];
			QString Parameters = QUrl::fromPercentEncoding(Arguments["data"].toUtf8());

			CIPCSocket::EEncoding Encoding = CIPCSocket::eUnknown;
			QVariantMap Data = CIPCSocket::String2Variant(Parameters.toLatin1(), Encoding).toMap();

			QVariantMap Return;

			if(/*Data.contains("UserName") ||*/ Data.contains("Password"))
			{
				//if(theCore->TestLogin(Data["UserName"].toString(), Data["Password"].toString()))
				if(!theCore->TestLogin(pRequest->GetPostValue("Password")))
				{
					Return["LoginToken"] = "";
					Return["Loggedin"] = false;
				}
				else
				{
					Return["LoginToken"] = theCore->GetLoginToken();
					Return["Loggedin"] = true;
				}
				
			}

			Return["Loggedin"] = bLoggedIn;
		
			QString sReturn = CIPCSocket::Variant2String(Return, Encoding);
			pRequest->SetHeader("Content-Type", "application/javascript; charset=utf-8");
			pRequest->write(Callback.toUtf8() + "(" + sReturn.toUtf8() + ");");
		}
		else
		{
			TArguments Variables;
			//Variables["display"] = theCore->Cfg(false)->GetString("Core/UserName").isEmpty() ? "display:none" : "";
			Variables["display"] = "display:none";
			pRequest->write(FillTemplate(GetTemplate(":/Login", "Login"),Variables).toUtf8());
		}
	}
	else if(!bLoggedIn)
	{
		pRequest->SetHeader("Location", QString("/Login" + Path).toUtf8());
		pRequest->RespondWithError(303);
	}

	else if(Path.compare("/") == 0)
	{
		pRequest->SetHeader("Location", QString("/WebUI/").toUtf8());
		pRequest->RespondWithError(303);
	}

	else if(Path.left(9).compare("/Console/") == 0)
	{
		QString Command;
		QString Parameters;
		QString Callback;

		if(pRequest->HasRawData())
		{
			Command = Arguments["Command"];
			Parameters = pRequest->readAll();
		}
		else if(pRequest->IsPost())
		{
			Command = pRequest->GetPostValue("Command");
			Parameters = pRequest->GetPostValue("Parameters");
		}
		else if(!pRequest->GetQuery().isEmpty()) // jsonp
		{
			Command = Arguments["Command"];
			Parameters = QUrl::fromPercentEncoding(Arguments["data"].toUtf8());
			Callback = Arguments["callback"];
		}

		if(Command.isEmpty())
			pRequest->RespondWithFile(":/Templates/Console");
		else
		{
			CIPCSocket::EEncoding Encoding = CIPCSocket::eJson;
			QVariant Data = CIPCSocket::String2Variant(Parameters.toLatin1(), Encoding);
			QVariant Return = theCore->m_Server->ProcessRequest(Command, Data);
			QByteArray sReturn = CIPCSocket::Variant2String(Return, Encoding);

			if(Encoding == CIPCSocket::eJson)
			{
				if(Callback.isEmpty())
					pRequest->SetHeader("Content-Type", "application/json; charset=utf-8");
				else
					pRequest->SetHeader("Content-Type", "application/javascript; charset=utf-8");
			}
			else if(Encoding == CIPCSocket::eXML)
				pRequest->SetHeader("Content-Type", "text/xml; charset=utf-8");
			else if(Encoding == CIPCSocket::eBencode)
				pRequest->SetHeader("Content-Type", "application/bencode");

			// send reply
			if(Callback.isEmpty())
				pRequest->write(sReturn);
			else
				pRequest->write(Callback.toUtf8() + "(" + sReturn + ");");
		}
	}

	else if(Path.left(8).compare("/Config/") == 0)
	{
		QString FilePath = Path.mid(7);
		if(FilePath.right(1) == "/")
		{
			QString Files;

			QDir srcDir(theCore->Cfg()->GetSettingsDir() + FilePath);
			foreach(const QString& File, srcDir.entryList(QDir::Files))
			{
				TArguments Variables;
				Variables["File"] = File;
				Files.append(FillTemplate(GetTemplate(":/Templates/Config", "File"),Variables));
			}

			TArguments Variables;
			Variables["Files"] = Files;
			pRequest->write(FillTemplate(GetTemplate(":/Templates/Config","Config"), Variables).toUtf8());
		}
		else if(pRequest->HasRawData())
		{
			QByteArray Post = pRequest->readAll();
			if(Post.isEmpty())
				QFile::remove(theCore->Cfg()->GetSettingsDir() + FilePath);
			else
				WriteStringToFile(theCore->Cfg()->GetSettingsDir() + FilePath, Post);
			pRequest->RespondWithError(202);
		}
		else if(pRequest->IsGet())
			pRequest->RespondWithFile(theCore->Cfg()->GetSettingsDir() + FilePath);
		else
			pRequest->RespondWithError(405);
	}

	else
		pRequest->RespondWithError(404, "path not found");

	pRequest->SendResponse();
}

void CWebRoot::HandleRequest(CHttpSocket* pRequest)
{
	connect(pRequest, SIGNAL(readChannelFinished()), this, SLOT(OnRequestCompleted()));
}

void CWebRoot::ReleaseRequest(CHttpSocket* pRequest)
{
	disconnect(pRequest, SIGNAL(readChannelFinished()), this, SLOT(OnRequestCompleted()));
}