#include "WebViewEx.h"
#include <QFileDialog>
//#if !defined(WIN32) && !defined(__APPLE__)
//#include </usr/include/qt4/QtWebKit/QWebSecurityOrigin>
//#else
#include <QWebSecurityOrigin>
//#endif
//#include "../../../Framework/HttpServer/HttpHelper.h"


QNetworkCookieJarEx::QNetworkCookieJarEx(QObject* qObject) 
: QNetworkCookieJar(qObject) 
{
}

QList<QNetworkCookie> QNetworkCookieJarEx::cookiesForUrl(const QUrl &url) const
{
	return QNetworkCookieJar::cookiesForUrl(url);
}

bool QNetworkCookieJarEx::setCookiesFromUrl(const QList<QNetworkCookie> &cookieList, const QUrl &url)
{
	return QNetworkCookieJar::setCookiesFromUrl(cookieList, url);
}

void QNetworkCookieJarEx::Load(const QVariantList& Cookies)
{
	QList<QNetworkCookie> CookieList;
	foreach(const QVariant& cookie, Cookies)
	{
		QVariantMap Cookie = cookie.toMap();
		QNetworkCookie WebCookie;
		WebCookie.setDomain(Cookie["Domain"].toString());
		WebCookie.setExpirationDate(Cookie["ExpirationDate"].toDateTime());
		WebCookie.setHttpOnly(Cookie["HttpOnly"].toBool());
		WebCookie.setName(Cookie["Name"].toByteArray());
		WebCookie.setPath(Cookie["Path"].toString());
		WebCookie.setSecure(Cookie["Secure"].toBool());
		WebCookie.setValue(Cookie["Value"].toByteArray());
		CookieList.append(WebCookie);
	}
	setAllCookies(CookieList);
}

QVariantList QNetworkCookieJarEx::Store()
{
	QVariantList Cookies;
	foreach(const QNetworkCookie& WebCookie, allCookies())
	{
		QVariantMap Cookie;
		Cookie["Domain"] = WebCookie.domain();
		Cookie["ExpirationDate"] = WebCookie.expirationDate();
		Cookie["HttpOnly"] = WebCookie.isHttpOnly();
		Cookie["Name"] = WebCookie.name();
		Cookie["Path"] = WebCookie.path();
		Cookie["Secure"] = WebCookie.isSecure();
		Cookie["Value"] = WebCookie.value();
		Cookies.append(Cookie);
	}
	return Cookies;
}


/////////////////////////////////////////////////////////////////////////////////////////////
//

QNetworkAccessManagerEx::QNetworkAccessManagerEx(QObject* parent)
 :QNetworkAccessManager(parent)
{
	setCookieJar(new QNetworkCookieJarEx());
#ifndef QT_NO_OPENSSL
    connect(this, SIGNAL(sslErrors(QNetworkReply*, const QList<QSslError>&)),SLOT(sslErrors(QNetworkReply*, const QList<QSslError>&)));
#endif
}

QNetworkAccessManagerEx::~QNetworkAccessManagerEx()
{
	foreach(QNetworkReply* pReply, m_Downloads.keys())
	{
		pReply->abort();
		pReply->deleteLater();
		m_Downloads.take(pReply)->deleteLater();
	}

	while(!m_Pending.empty())
	{
		QNetworkReply* pReply = m_Pending.takeFirst();
		pReply->abort();
		pReply->deleteLater();
	}
}

void QNetworkAccessManagerEx::AddDownload(QNetworkReply* pReply)
{
	m_Pending.append(pReply);
	QTimer::singleShot(1000,this,SLOT(OnFetchPending()));
}

QString Url2FileName(QString Url, bool bDecode)
{
	int QueryPos = Url.indexOf("?");
	if(QueryPos != -1)
		Url.truncate(QueryPos);

	if(bDecode)
		Url = QUrl::fromPercentEncoding(Url.toLatin1());

	while(!Url.isEmpty())
	{
		int Pos = Url.lastIndexOf("/");
		if(Pos == -1)
			return Url;
		if(Pos == Url.size()-1)
		{
			Url.truncate(Pos);
			continue;
		}
		return Url.mid(Pos+1);
	}
	return "";
}

void QNetworkAccessManagerEx::OnFetchPending()
{
	while(!m_Pending.empty())
	{
		QNetworkReply* pReply = m_Pending.takeFirst();

		QString FileName;
		//if(pReply->hasRawHeader("Content-Disposition"))
		//	FileName = GetArguments(pReply->rawHeader("Content-Disposition")).value("filename");
		//if(FileName.isEmpty())
			FileName = Url2FileName(pReply->url().toString(), false);

		FileName.remove(QRegExp("[\\\\/:*?<>|\"]")); // make sure its a valid file name
		QString FilePath = QFileDialog::getSaveFileName(0, tr("Save File"), FileName, QString("Any File (*.*)"));
		if (FilePath.isEmpty())
		{
			pReply->abort();
			pReply->deleteLater();
			return;
		}

		QFile* pFile = new QFile(FilePath);
		pFile->open(QFile::WriteOnly);

		if(pReply->isFinished())
		{
			pFile->write(pReply->readAll());
			pFile->deleteLater();
			pReply->deleteLater();
		}
		else
		{
			m_Downloads[pReply] = pFile;
			connect(pReply, SIGNAL(finished()), this, SLOT(OnRequestFinished()));
			connect(pReply, SIGNAL(readyRead()), this, SLOT(OnReadyRead()));
		}
	}
}

void QNetworkAccessManagerEx::OnRequestFinished()
{
	QNetworkReply* pReply = (QNetworkReply*)sender();
	Q_ASSERT(m_Downloads.contains(pReply));
	m_Downloads.take(pReply)->deleteLater();
	pReply->deleteLater();
}

void QNetworkAccessManagerEx::OnReadyRead()
{
	QNetworkReply* pReply = (QNetworkReply*)sender();
	Q_ASSERT(m_Downloads.contains(pReply));
	m_Downloads[pReply]->write(pReply->readAll());
}

#ifndef QT_NO_OPENSSL
void QNetworkAccessManagerEx::sslErrors(QNetworkReply *pReply, const QList<QSslError> &error)
{
	pReply->ignoreSslErrors();
}
#endif

/////////////////////////////////////////////////////////////////////////////////////////////
//

QWebViewEx::QWebViewEx(QNetworkAccessManagerEx* pNetAccess, bool bCanClose, QWidget *parent)
 : QWebView(parent) 
{
	setPage(new QWebPageEx(this));

	if(pNetAccess)
		page()->setNetworkAccessManager(pNetAccess);
	else
		page()->setNetworkAccessManager(new QNetworkAccessManagerEx());

	settings()->setAttribute(QWebSettings::PluginsEnabled, true); 
	settings()->setAttribute(QWebSettings::JavascriptCanOpenWindows, true);
	settings()->setAttribute(QWebSettings::XSSAuditingEnabled, false);
	page()->setForwardUnsupportedContent(true);
	connect(page(), SIGNAL(geometryChangeRequested(const QRect &)), this, SLOT(OnSetGeometry(const QRect &)));
	connect(page(), SIGNAL(unsupportedContent(QNetworkReply *)), this, SLOT(OnUnsupportedContent(QNetworkReply *)));
	connect(page(), SIGNAL(downloadRequested(const QNetworkRequest &)), this, SLOT(OnDownload(const QNetworkRequest &)));
	if(bCanClose)
		connect(page(), SIGNAL(windowCloseRequested()), this, SLOT(OnClose()));
}

QWebView* QWebViewEx::createWindow (QWebPage::WebWindowType type)
{
	QWebView* pView = MakeNew();
	QNetworkCookieJar* CookieJar = page()->networkAccessManager()->cookieJar();
	QObject* CookieParent = CookieJar->parent();
	pView->page()->networkAccessManager()->setCookieJar(CookieJar);
	CookieJar->setParent(CookieParent);
	pView->show();
	return pView;
}

/////////////////////////////////////////////////////////////////////////////////////////////
//

QWidget* CreateWebView(bool bJS)
{
	QWebView* pWebView = new QWebViewEx();
	pWebView->setUrl(QUrl("about:blank"));
	if(!bJS)
		pWebView->settings()->setAttribute(QWebSettings::JavascriptEnabled, 0);
	return pWebView;
}
