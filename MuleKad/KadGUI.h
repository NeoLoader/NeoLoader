#pragma once
//#include "GlobalHeader.h"

#include <QMainWindow>
#include <QSplitter>
#include <QTextEdit>
#include <QHBoxLayout>
#include <QStatusBar>
#include <QMenuBar>
#include <QMessageBox>
#include <QLabel>
#include <QListWidget>
#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QTimerEvent>

#include "../Framework/ObjectEx.h"

class CMuleKad;

class CKadGUI : public QMainWindow
{
	Q_OBJECT

public:
#if QT_VERSION < 0x050000
	CKadGUI(CMuleKad* pKad, QWidget *parent = 0, Qt::WFlags flags = 0);
#else
	CKadGUI(CMuleKad* pKad, QWidget *parent = 0, Qt::WindowFlags flags = 0);
#endif
	~CKadGUI();

	void				Process();

protected:
	void				timerEvent(QTimerEvent* pEvent)
	{
		if(pEvent->timerId() == m_uTimerID)
			Process();
	}

	void				GetLogLines();
	void				AddLog(uint32 uFlag, const QString &Pack);
	void				DumpRoutignTable();
	void				DumpSearchList();
	uint64				m_uLastLog;

	uint16				m_uTimerCounter;
	int					m_uTimerID;

	CMuleKad*			m_pKad;

private slots:
	void				OnConnect();
	void				OnDisconnect();
	void				OnBootstrap();
	void				OnRecheck();

	void				OnAbout();

private:
	QWidget				*m_pMainWidget;
	QHBoxLayout			*m_pMainLayout;

	QMenu				*m_pKadMenu;
	QAction				*m_pConnect;
	QAction				*m_pDisconnect;
	QAction				*m_pBootstrap;
	QAction				*m_pRecheck;

	QMenu				*m_pHelpMenu;
	QAction				*m_pAbout;

	QSplitter			*m_pSplitter;

	QTreeWidget 		*m_pRoutingTable;
	QTreeWidget 		*m_pLookupList;

	QTabWidget			*m_pLogTab;
	QTextEdit			*m_pUserLog;
	QTextEdit			*m_pDebugLog;

	QLabel				*m_pAddress; // IP:Port (Fiewalled) - Buddy: IP:Port
	QLabel				*m_pClient; // Client: Port - UserHash (Firewalled)
};
