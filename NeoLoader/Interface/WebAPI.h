#pragma once

#include "../../Framework/HttpServer/HttpServer.h"
#include "../../Framework/Archive/CachedArchive.h"

class CWebAPI : public QObjectEx, public CHttpHandler
{
	Q_OBJECT

public:
	CWebAPI(QObject* qObject = NULL);
	~CWebAPI();

private slots:
	virtual void	OnRequestCompleted();
	virtual void	OnFilePosted(QString Name, QString File, QString Type);

protected:
	virtual void	HandleRequest(CHttpSocket* pRequest);
	virtual void	ReleaseRequest(CHttpSocket* pRequest);

	//typedef bool (*qrencode)(const QByteArray& inData, QIODevice* outCode, bool Short, int Ecc, int iSize, bool bMicro);
	//qrencode		m_QrEncode;

	//typedef bool (*decodeqr)(QIODevice* inCode, QByteArray& outData);
	//decodeqr		m_DecodeQr;

	QMap<CHttpSocket*, QFile*>	m_Files;

	CCachedArchive*	m_WebAPI;
	QString			m_WebAPIPath;
};