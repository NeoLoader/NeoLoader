#pragma once

#include "../../Framework/HttpServer/HttpServer.h"

class CWebRoot : public QObjectEx, public CHttpHandler
{
	Q_OBJECT

public:
	CWebRoot(QObject* qObject = NULL);
	~CWebRoot();

	static bool		TestLogin(CHttpSocket* pRequest);

private slots:
	virtual void	OnRequestCompleted();

protected:
	virtual void	HandleRequest(CHttpSocket* pRequest);
	virtual void	ReleaseRequest(CHttpSocket* pRequest);
};