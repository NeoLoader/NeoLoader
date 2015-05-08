#pragma once

#include <QMainWindow>
#include <QWidget>
#include <QHBoxLayout>
#include <QMenu>
#include <QAction>
#include <QSplitter>
#include <QTabWidget>
#include <QTextEdit>
#include <QLabel>
#include <QMenuBar>
#include <QStatusBar>
#include <QCloseEvent>
#include <QFileDialog>
#include <QMessageBox>

#include "../Framework/ObjectEx.h"
#include "Common/Pointer.h"

class CSettings;
class CSmartSocket;
class CKademlia;
class CPrivateKey;
class CRoutingZone;
class CKadLookup;
class CNeoKad;

class CRoutingWidget;
class CLookupWidget;
class CIndexWidget;
class CSearchList;
class CSearchView;


class CNeoKadGUI : public QMainWindow
{
	Q_OBJECT

public:
#if QT_VERSION < 0x050000
	CNeoKadGUI(QWidget *parent = 0, Qt::WFlags flags = 0);
#else
	CNeoKadGUI(QWidget *parent = 0, Qt::WindowFlags flags = 0);
#endif
	~CNeoKadGUI();

	void				Process();

private slots:
	void				OnShowGUI();

	void				OnConnect();
	void				OnDisconnect();
	void				OnBootstrap();

	void				OnConsolidate();
	//void				OnAddNode();
	void				OnTerminate();
	void				OnDebugger();
	void				OnAuthenticate();

	void				OnAbout();

	void				OnTab(int Index);

protected:
	void				timerEvent(QTimerEvent* pEvent)
	{
		if(pEvent->timerId() == m_uTimerID)
			Process();
	}
	void				closeEvent(QCloseEvent *e);

	void				GetLogLines();
	void				AddLog(uint32 uFlag, const QString &Pack);
	uint64				m_uLastLog;

	uint16				m_uTimerCounter;
	int					m_uTimerID;

	QWidget				*m_pMainWidget;
	QHBoxLayout			*m_pMainLayout;

	QMenu				*m_pKadMenu;
	QAction				*m_pConnect;
	QAction				*m_pDisconnect;
	QAction				*m_pBootstrap;

	/*QMenu				*m_pViewMenu;
	QAction				*m_pShowAll;*/

	QMenu				*m_pDevMenu;
	QAction				*m_pConsolidate;
	//QAction				*m_pAddNode;
	QAction				*m_pTerminate;
	QAction				*m_pDebugger;
	QAction				*m_pAuthenticate;

	QMenu				*m_pHelpMenu;
	QAction				*m_pAbout;

	QSplitter			*m_pSplitter;

	QTabWidget			*m_pKadTab;
	CRoutingWidget		*m_pRoutingWidget;
	CSearchView			*m_pSearchView;
	CSearchList			*m_pSearchList;
	CLookupWidget		*m_pLookupWidget;
	CIndexWidget		*m_pIndexWidget;

	QTabWidget			*m_pLogTab;
	QTextEdit			*m_pUserLog;
	QTextEdit			*m_pDebugLog;

	QLabel				*m_pNode;		// NodeID
	QLabel				*m_pAddress;	// IP:Port (Fiewalled)[ - Buddy: IP:Port] [; next]
	QLabel				*m_pNetwork;	// Node Count
};
