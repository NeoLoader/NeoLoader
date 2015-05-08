#pragma once

#include <QtNetwork>

//#if !defined(WIN32) && !defined(__APPLE__)
//#include </usr/include/qt4/QtWebKit/qwebview.h>
//#include </usr/include/qt4/QtWebKit/qwebframe.h>
//#else
#if QT_VERSION < 0x050000
#include <QtWebKit/QWebView>
#include <QtWebKit/QWebFrame>
#else
#include <QWebView>
#include <QWebFrame>
#endif
//#endif

#include "webview_global.h"

class WEBVIEW_EXPORT QNetworkCookieJarEx : public QNetworkCookieJar
{
	 Q_OBJECT
public:
	QNetworkCookieJarEx(QObject* qObject = NULL);

    virtual QList<QNetworkCookie>	cookiesForUrl(const QUrl &url) const;
    virtual bool					setCookiesFromUrl(const QList<QNetworkCookie> &cookieList, const QUrl &url);

	virtual void					Load(const QVariantList& Cookies);
	virtual QVariantList			Store();
};

//////////////////////////////////////////////
//

class WEBVIEW_EXPORT QNetworkAccessManagerEx : public QNetworkAccessManager
{
    Q_OBJECT
public:
	QNetworkAccessManagerEx(QObject* parent = NULL);
	virtual ~QNetworkAccessManagerEx();

	virtual void	AddDownload(QNetworkReply *pReply);

private slots:
	virtual void	OnFetchPending();
	virtual void	OnRequestFinished();
	virtual void	OnReadyRead();

#ifndef QT_NO_OPENSSL
	virtual void	sslErrors(QNetworkReply *pReply, const QList<QSslError> &error);
#endif

private:
	QList<QNetworkReply*>			m_Pending;
	QMap<QNetworkReply*, QFile*>	m_Downloads;
};

//////////////////////////////////////////////
//

class WEBVIEW_EXPORT QWebViewEx: public QWebView
{
	Q_OBJECT

public:
	QWebViewEx(QNetworkAccessManagerEx* pNetAccess = NULL, bool bCanClose = false, QWidget *parent = NULL);
	~QWebViewEx() {}

	QWebView*		createWindow (QWebPage::WebWindowType type);

	void			closeEvent(QCloseEvent *event)	{deleteLater();}

public slots:
	void			SetUrl(const QUrl& Url)						{setUrl(Url);}
	void			SetHtml(const QString& Html)				{setHtml(Html);}

private slots:
	virtual void	OnSetGeometry(const QRect &geometry)		{setGeometry(geometry);}

	virtual void	OnDownload(const QNetworkRequest &pRequest)	{OnUnsupportedContent(page()->networkAccessManager()->get(pRequest));}
	virtual void	OnUnsupportedContent(QNetworkReply *pReply)	{((QNetworkAccessManagerEx*)page()->networkAccessManager())->AddDownload(pReply);}

	virtual void	OnClose()									{hide(); deleteLater();}

protected:
	virtual QWebView*	MakeNew()								{return new QWebViewEx((QNetworkAccessManagerEx*)page()->networkAccessManager(), true);}

};

//////////////////////////////////////////////
//

class WEBVIEW_EXPORT QWebPageEx: public QWebPage
{
	Q_OBJECT

public:
	QWebPageEx(QWidget *parent = NULL)
	 : QWebPage(parent) {}

	const QStringList&	GetLog()	{return m_Log;}

protected:
	virtual void	javaScriptConsoleMessage(const QString & message, int lineNumber, const QString & sourceID)
	{m_Log.append(message);}
	
	QStringList		m_Log;
};

//////////////////////////////////////////////
//

extern "C" { 
	WEBVIEW_EXPORT QWidget* CreateWebView(bool bJS);
};
