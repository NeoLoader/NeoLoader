#pragma once

#if QT_VERSION < 0x050000
#include <QtGui>
#else
#include <QMenu>
#include <QAction>
#include <QSystemTrayIcon>
#include <QLabel>
#include <QSplashScreen>
#include <QTranslator>
#endif
#include <QMainWindow>
#include "../../Framework/Settings.h"

class CNeoGUI;
class CCoreClient;

class CNeoLoader : public QObject
{
	Q_OBJECT

public:
	enum ENeoMode
	{
		eNoNeo = 0x00,					// 0000 0000

		eNeoCore = 0x01,				// 0000 0001
		eNeoGUI = 0x02,					// 0000 0010

		eUnified = eNeoCore | eNeoGUI,	// 0000 0011 - Single Process
		eCoreProcess = eUnified | 0x0C,	// 0000 1111 - Split Process - Core - with GUI for Tray
		eGUIProcess = eNeoGUI | 0x0C,	// 0000 1110 - Split Process - GUI
		eLocalGUI = eNeoGUI | 0x04,		// 0000 0110 - Local GUI
		eRemoteGUI = eNeoGUI | 0x08,	// 0000 1010 - Remote GUI

		eMask = 0x0F,					// 0000 1111

		eMinimized = 0x10				// 0001 0000
	};

	CNeoLoader(CSettings* Settings, int eMode);
	~CNeoLoader();

	int					GetMode()					{return m_NeoMode;}

	CSettings*			Cfg()						{return m_Settings;}
	CCoreClient*		Itf()						{return m_Client;}

	void				Dispatch(const QString& Command, const QVariantMap& Response);

	bool				NeoCoreRunning();

	static QMap<QString, CSettings::SSetting> GetDefaultSettings();

signals:
	void				DispatchResponse(const QString& Command, const QVariantMap& Response);

	void				UpdatesFound(int Code);

public slots:
	void				ConnectCore();
	void				DisconnectCore();

	void				StartNeoCore();
	void				StopNeoCore();

	void				StartNeoGUI();

private slots:
	void				SendRequest(const QString& Command, const QVariantMap& Request);

	void				OnSysTray(QSystemTrayIcon::ActivationReason Reason);

	void				OnDebugger();
	void				OnExit();

	void				CheckUpdates();
	void				OnUpdate(int exitCode, QProcess::ExitStatus exitStatus);

	void				OnAbout();

	void				OnSplashMessage(QString Message);

	void				CreateFileIcon(QString Ext);

	void				OnMessage(const QString& Message);

protected:
	CSettings*			m_Settings;

	int					m_NeoMode;

	int					m_TimerId;
	void				timerEvent(QTimerEvent *e);

	CCoreClient*		m_Client;
	sint32				m_ConnectTimer;
	bool				m_IsConnected;
	QProcess*			m_CoreProcess;

	QProcess*			m_GUIProcess;

	QString				m_Title;

	time_t				m_LastCheck;
	QProcess*			m_Updater;

private:
	QSplashScreen*		m_pSplash;

	QSystemTrayIcon*	m_pTrayIcon;
	QMenu*				m_pTrayMenu;

	CNeoGUI*			m_pWnd;
};

extern CNeoLoader* theLoader;
