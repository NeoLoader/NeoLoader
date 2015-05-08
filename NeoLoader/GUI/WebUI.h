#pragma once

#include "../../Framework/HttpServer/HttpServer.h"
#include "../../Framework/Archive/CachedArchive.h"

class CWebUI : public QObjectEx, public CHttpHandler
{
	Q_OBJECT

public:
	CWebUI(QObject* qObject = NULL);
	~CWebUI();

	static void		GetProgress(QIODevice* pDevice, const QString& sMode, uint64 ID, int iWidth, int iHeight, int iDepth = 5);
	static void		GetProgress(QIODevice* pDevice, uint64 ID, uint64 SubID, int iWidth, int iHeight, int iDepth = 5);
	static void		GetProgress(QIODevice* pDevice, uint64 ID, const QString& Groupe, const QString& Hoster, const QString& User, int iWidth, int iHeight, int iDepth = 5);

private slots:
	void			OnRequestCompleted();

protected:
	virtual void	HandleRequest(CHttpSocket* pRequest);
	virtual void	ReleaseRequest(CHttpSocket* pRequest);

#ifdef USE_7Z
	CCachedArchive*	m_WebUI;
#endif
	QString			m_WebUIPath;
};