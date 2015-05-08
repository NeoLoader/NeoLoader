#pragma once

#include "./NeoHelper/neohelper_global.h"

#include <QNetworkAccessManager>

class NEOHELPER_EXPORT CRequestManager : public QNetworkAccessManager
{
    Q_OBJECT
public:
	 CRequestManager(int TimeOut, QObject* parent = NULL);
   
	void				Process(UINT Tick);

	void				Abbort(QNetworkReply* &refReply);

private slots:
	void				finishedRequest(QNetworkReply *pReply);
	void				OnData(qint64 bytesSent, qint64 bytesTotal);
#ifndef QT_NO_OPENSSL
	void				sslErrors(QNetworkReply *pReply, const QList<QSslError> &error);
#endif

protected:
	QNetworkReply*		createRequest ( Operation op, const QNetworkRequest & req, QIODevice * outgoingData = 0 );

	void				SetTimeOut(QNetworkReply *pReply);
	void				StopTimeOut(QNetworkReply *pReply);

	int					m_TimeOut;
	QMap<QNetworkReply*, uint64>	m_Requests;
};
