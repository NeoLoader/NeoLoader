#pragma once

#include "neogui_global.h"

#include "../Framework/Settings.h"
#include "../Framework/IPC/JobManager.h"
class CLogView;
class CCaptchaDialog;
class CModernGUI;
class CStatsUpdateJob;
class CHosterIconJob;
class CSettingsWindow;
#include <QSystemTrayIcon>
class QVBoxLayout;
class QSplitter;
class QSpinBoxEx;
class CConnector;

class NEOGUI_EXPORT CNeoGUI: public QMainWindow
{
	Q_OBJECT

public:
	CNeoGUI(CSettings* Settings, bool bLocal);
	~CNeoGUI();

	void				Suspend(bool bSet);

	CSettings*			Cfg()		{return m_Settings;}
	void				ScheduleJob(CInterfaceJob* pJob);
	void				UnScheduleJob(CInterfaceJob* pJob);
	bool				IsSchedulerBlocking();
	bool				IsLocal()		{return m_bLocal;}
	bool				IsConnected();

	QVariantMap			GetInfoData()					{return m_Data;}

	void				LogLine(uint32 uFlag, const QString &sLine);

	QIcon				GetHosterIcon(const QString& Hoster, bool bDefault = true);

	static void			MakeFileIcon(const QString& Ext);

	static void			DefaultSettings(QMap<QString, CSettings::SSetting>& Settings);

signals:
	void				SendRequest(const QString& Command, const QVariantMap& Request);

	void				OpenDebugger();

	void				CheckUpdates();

	void				Connect();
	void				Disconnect();

public slots:
	void				DoReset();


private slots:
	void				DispatchResponse(const QString& Command, const QVariantMap& Response);
	
	void				OnSysTray(QSystemTrayIcon::ActivationReason Reason);

	void				UpdatesFound(int Code);

	void				ShowWizard();

	void				OnSettings();
	void				OnConnector();
	void				OnWebUI();
	void				OnExit();

	void				OnUpLimit(int Limit);
	void				OnDownLimit(int Limit);

	void				OnHelp();
	void				OnNews();
	void				OnAbout();

protected:
	friend class CStatsUpdateJob;
	friend class CHosterIconJob;
	virtual void		timerEvent(QTimerEvent *e);
	virtual void		closeEvent(QCloseEvent *e);
	virtual void		changeEvent(QEvent* e);

	void				MakeGUI();
	void				ClearGUI();

	void				PromptExit();

	void				LoadLanguage(QByteArray& Translation, QTranslator& Translator, const QString& Prefix);

	int					m_TimerId;
	CStatsUpdateJob*	m_pStatsUpdateJob;
	QVariantMap			m_Data;

	bool				m_bLocal;

	uint64				m_PingTime;
	QList<uint64>		m_PingStat;

	QMap<QString, QIcon>m_Icons;

private:
	CSettings*			m_Settings;
	CJobManager*		m_Manager;

	QWidget*			m_pMainWidget;
	QVBoxLayout*		m_pMainLayout;

	QWidget*			m_pGUI;

	QSplitter*			m_pSplitter;

	CLogView*			m_pLogView;

	CCaptchaDialog*		m_pCaptchaDialog;

	CSettingsWindow*	m_pSettingsWnd;
	CConnector*			m_pConnector;
	QLabel*				m_pConnection;

	QLabel*				m_pTransfer;

	QSystemTrayIcon*	m_pTrayIcon;

	QMenu*				m_pNeoMenu;
	QAction*			m_pConnect;
	QAction*			m_pOpenWebUI;
	QAction*			m_pShowSettings;
#ifndef NO_HOSTERS
	QAction*			m_pDebugger;
#endif
	QAction*			m_pExit;

	QMenu*				m_pHelpMenu;
	QAction*			m_pHelp;
	QAction*			m_pWizard;
	QAction*			m_pUpdate;
	QAction*			m_pNews;
	QAction*			m_pAbout;

	QSpinBoxEx*			m_pUpLimit;
	QSpinBoxEx*			m_pDownLimit;

	QTranslator			m_Translator;
	QByteArray			m_Translation;
};

extern CNeoGUI* theGUI;
