#include "GlobalHeader.h"
#include "RequestManager.h"
#include "Functions.h"

CRequestManager::CRequestManager(int TimeOut, QObject* parent)
:QNetworkAccessManager(parent)
{
	m_TimeOut = TimeOut;
	connect(this, SIGNAL(finished(QNetworkReply*)), this, SLOT(finishedRequest(QNetworkReply*))); 
#ifndef QT_NO_OPENSSL
    connect(this, SIGNAL(sslErrors(QNetworkReply*, const QList<QSslError>&)),SLOT(sslErrors(QNetworkReply*, const QList<QSslError>&)));
#endif
}

void CRequestManager::Process(UINT Tick)
{
	foreach(QNetworkReply *pReply, m_Requests.keys())
	{
		if(m_Requests[pReply] < GetCurTick())
			pReply->abort();
	}
}

void CRequestManager::SetTimeOut(QNetworkReply *pReply)
{
	m_Requests[pReply] = GetCurTick() + SEC2MS(m_TimeOut);
}

void CRequestManager::StopTimeOut(QNetworkReply *pReply)
{
	m_Requests.remove(pReply);
}

void CRequestManager::Abbort(QNetworkReply* &refReply)
{
	if(refReply)
	{
		QNetworkReply* pReply = refReply;
		refReply = NULL;
		pReply->abort(); // this will call OnRequestFinished, therefore we have to set m_pReply = NULL befoure
		StopTimeOut(pReply);
		pReply->deleteLater();
	}
}

QNetworkReply* CRequestManager::createRequest ( Operation op, const QNetworkRequest & req, QIODevice * outgoingData )
{
	QNetworkReply* pReply = QNetworkAccessManager::createRequest(op, req, outgoingData);
	connect(pReply, SIGNAL(downloadProgress (qint64, qint64)), this, SLOT(OnData(qint64, qint64)));
	connect(pReply, SIGNAL(uploadProgress (qint64, qint64)), this, SLOT(OnData(qint64, qint64)));
	SetTimeOut(pReply);
	return pReply;
}

void CRequestManager::finishedRequest(QNetworkReply *pReply)
{
	StopTimeOut(pReply);
}

void CRequestManager::OnData(qint64 bytesSent, qint64 bytesTotal)
{
	// Reset TimeOut, as long as data are being transferred its not a timeout
	SetTimeOut((QNetworkReply*)sender());
}

#ifndef QT_NO_OPENSSL
void CRequestManager::sslErrors(QNetworkReply *pReply, const QList<QSslError> &error)
{
	pReply->ignoreSslErrors(); // ToDo-Now: check for a valid certificate on neo domains !
}
#endif
