#include "GlobalHeader.h"
#include "KadGUI.h"
#include "MuleKad.h"
#include "Kad/Types.h"
#include "Kad/UDPSocket.h"
#include "Kad/KadHandler.h"
#include "Kad/routing/Contact.h"
#include "Kad/routing/RoutingZone.h"
#include "Kad/kademlia/Search.h"
#include "Kad/kademlia/SearchManager.h"
#include "Kad/kademlia/Indexed.h"
#include "Kad/FileTags.h"
#include "Kad/kademlia/UDPFirewallTester.h"
#include <QInputDialog>
#include <QApplication>
#include "../Framework/Settings.h"

#if QT_VERSION < 0x050000
CKadGUI::CKadGUI(CMuleKad* pKad, QWidget *parent, Qt::WFlags flags)
#else
CKadGUI::CKadGUI(CMuleKad* pKad, QWidget *parent, Qt::WindowFlags flags)
#endif
 : QMainWindow(parent, flags)
{
	m_uTimerCounter = 0;
	m_uTimerID = startTimer(10);

	m_uLastLog = 0;

	m_pKad = pKad;

	QString Title = "MuleKad v" + QString::number(MULE_KAD_VERSION_MJR) + "." + QString::number(MULE_KAD_VERSION_MIN).rightJustified(2, '0');
#if MULE_KAD_VERSION_UPD > 0
	Title.append('a' + MULE_KAD_VERSION_UPD - 1);
#endif
#ifdef _DEBUG
	Title += " - DEBUG";
	//Title += " (" __DATE__ ")";
#endif
	setWindowTitle(Title);

	m_pMainWidget = new QWidget(this);
	setCentralWidget(m_pMainWidget);

	m_pKadMenu = menuBar()->addMenu(tr("&Kad"));

		m_pConnect = new QAction(tr("&Connect"), this);
		connect(m_pConnect, SIGNAL(triggered()), this, SLOT(OnConnect()));
		m_pKadMenu->addAction(m_pConnect);

		m_pDisconnect = new QAction(tr("&Disconnect"), this);
		connect(m_pDisconnect, SIGNAL(triggered()), this, SLOT(OnDisconnect()));
		m_pKadMenu->addAction(m_pDisconnect);

		m_pKadMenu->addSeparator();

		m_pBootstrap = new QAction(tr("&Bootstrap"), this);
		connect(m_pBootstrap, SIGNAL(triggered()), this, SLOT(OnBootstrap()));
		m_pKadMenu->addAction(m_pBootstrap);

		m_pRecheck = new QAction(tr("&Recheck FW"), this);
		connect(m_pRecheck, SIGNAL(triggered()), this, SLOT(OnRecheck()));
		m_pRecheck->setEnabled(false);
		m_pKadMenu->addAction(m_pRecheck);

	m_pHelpMenu = menuBar()->addMenu(tr("&Help"));

		m_pAbout = new QAction(tr("&About"), this);
		connect(m_pAbout, SIGNAL(triggered()), this, SLOT(OnAbout()));
		m_pHelpMenu->addAction(m_pAbout);


	// log Begin
		m_pLogTab = new QTabWidget(NULL);

		QIcon logIcon;
		logIcon.addFile(":/Log");
		m_pUserLog = new QTextEdit(m_pLogTab);
		m_pUserLog->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard | Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
		m_pUserLog->setUndoRedoEnabled(false);
		m_pLogTab->addTab(m_pUserLog, logIcon, tr("Log"));

		QIcon debugLogIcon;
		debugLogIcon.addFile(":/DebugLog");
		m_pDebugLog = new QTextEdit(m_pLogTab);
		m_pDebugLog->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard | Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
		m_pDebugLog->setUndoRedoEnabled(false);
		m_pLogTab->addTab(m_pDebugLog, debugLogIcon, tr("Debug"));

#ifdef _DEBUG
		m_pLogTab->setCurrentWidget(m_pDebugLog);
#endif
	// logEnd

		m_pRoutingTable = new QTreeWidget(NULL);
		m_pRoutingTable->setHeaderLabels(QString("ID|Type|Distance|IP").split("|"));
		m_pRoutingTable->setSortingEnabled(true);

		m_pLookupList = new QTreeWidget(NULL);
		m_pLookupList->setHeaderLabels(QString("Number|Key|Type|Name|Status|Load|Packets Sent|Responses").split("|"));
		m_pLookupList->setSortingEnabled(true);


	m_pSplitter = new QSplitter(m_pMainWidget);
	m_pSplitter->setOrientation(Qt::Vertical);

	m_pSplitter->addWidget(m_pRoutingTable);
	m_pSplitter->addWidget(m_pLookupList);
	m_pSplitter->addWidget(m_pLogTab);

	m_pMainLayout = new QHBoxLayout(m_pMainWidget);
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);
	m_pMainLayout->addWidget(m_pSplitter);
	m_pMainWidget->setLayout(m_pMainLayout);

	QStatusBar* pStatusBar = statusBar();
	m_pAddress = new QLabel(this);
	pStatusBar->addPermanentWidget(m_pAddress, 200);
	m_pClient = new QLabel(this);
	pStatusBar->addPermanentWidget(m_pClient, 200);
	

	restoreGeometry(m_pKad->Cfg()->GetBlob("Gui/Window"));
	m_pSplitter->restoreState(m_pKad->Cfg()->GetBlob("Gui/Main"));
	m_pRoutingTable->header()->restoreState(m_pKad->Cfg()->GetBlob("Gui/RoutingTable"));
	m_pLookupList->header()->restoreState(m_pKad->Cfg()->GetBlob("Gui/LookupList"));
}

CKadGUI::~CKadGUI()
{
	killTimer(m_uTimerID);

	m_pKad->Cfg()->SetBlob("Gui/Window",saveGeometry());
	m_pKad->Cfg()->SetBlob("Gui/Main",m_pSplitter->saveState());
	m_pKad->Cfg()->SetBlob("Gui/RoutingTable",m_pRoutingTable->header()->saveState());
	m_pKad->Cfg()->SetBlob("Gui/LookupList",m_pLookupList->header()->saveState());
}

void CKadGUI::Process()
{
	m_uTimerCounter++;

	if(m_uTimerCounter % 10 == 0) // 10 times a second
	{
		GetLogLines();
		if(Kademlia::CKademlia::IsRunning())
		{
			DumpRoutignTable();
			DumpSearchList();
		}

		if(m_uTimerCounter % 100 == 0) // once a second
		{
			if(CKadHandler* KadHandler = m_pKad->Kad())
			{
				if(m_pConnect->isEnabled())
				{
					m_pConnect->setEnabled(false);
					m_pDisconnect->setEnabled(true);
					m_pBootstrap->setEnabled(true);
					m_pRecheck->setEnabled(true);
					m_pDisconnect->setEnabled(true);
				}

				m_pAddress->setText(tr("Kad: %1 - %2:%3 %4")
					.arg(Kademlia::CKademlia::IsConnected() ? "Connected" : "Connecting")
					.arg(QHostAddress(KadHandler->GetKadIP()).toString()).arg(KadHandler->GetKadPort())
					.arg(KadHandler->IsFirewalledUDP() ? tr(" (Firewalled)") : "")
					+ (KadHandler->HasBuddy() ? tr(" - Buddy: %1:%2").arg(QHostAddress(KadHandler->GetBuddyIP()).toString()).arg(KadHandler->GetBuddyPort()) : "") );

				if(KadHandler->GetTCPPort() != 0)
					m_pClient->setText(tr("Client: %1 - %2 %3").arg(QString::fromStdWString(KadHandler->GetEd2kHash().ToHexString())).arg(KadHandler->GetTCPPort()).arg(KadHandler->IsFirewalledTCP() ? tr(" (Firewalled)") : ""));
				else
					m_pClient->setText(tr("No Client"));
			}
			else
			{
				if(m_pDisconnect->isEnabled())
				{
					m_pConnect->setEnabled(true);
					m_pDisconnect->setEnabled(false);
					m_pBootstrap->setEnabled(false);
					m_pRecheck->setEnabled(false);
					m_pDisconnect->setEnabled(false);

					m_pAddress->setText(tr("Kad: Disconnected"));
					m_pClient->setText(tr("No Client"));

					m_pLookupList->clear();
					m_pRoutingTable->clear();
				}
			}

			if (m_uTimerCounter % 1000 == 0) // every 10 seconds
			{
				m_uTimerCounter = 0;
			}
		}
	}
}

void CKadGUI::OnConnect()
{
	m_pKad->Connect();
}

void CKadGUI::OnDisconnect()
{
	m_pKad->Disconnect();
}

void CKadGUI::OnBootstrap()
{
	if(CKadHandler* KadHandler = m_pKad->Kad())
	{
		QString Bootstrap = QInputDialog::getText(this, tr("bootstrap kad"),tr("IP:Port"));
		StrPair IPPort = Split2(Bootstrap,":");
		KadHandler->Bootstrap(QHostAddress(IPPort.first), IPPort.second.toUInt());
	}
}

void CKadGUI::OnRecheck()
{
	Kademlia::CKademlia::RecheckFirewalled();
}

void CKadGUI::OnAbout()
{
	QMessageBox::about(this, tr("Mule Kad"), tr("Mule Kad is licenced under the GPL: http://opensource.org/licenses/gpl-license.php and its source code is available to everyone at: https://gitorious.org/mulekad/mulekad have fun with it."));
}

void AddLog(QTextEdit *pLog, uint32 uFlag, const QString &Pack, int Limit)
{
	QColor Color;
	switch(uFlag & LOG_MASK)
	{
		case LOG_ERROR:		Color = Qt::red;		break;
		case LOG_WARNING:	Color = Qt::darkYellow;	break;
		case LOG_SUCCESS:	Color = Qt::darkGreen;	break;
		case LOG_INFO:		Color = Qt::blue;		break;
		default:			Color = Qt::black;		break;
	}

	QTextDocument* Document = pLog->document();
	QTextCursor Cursor = pLog->textCursor();
	Cursor.movePosition(QTextCursor::Start);
	for(int i=Document->lineCount();i > Limit;i--)
		Cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor);
	Cursor.removeSelectedText();
	Cursor.movePosition(QTextCursor::End);
	pLog->setTextCursor(Cursor);

	pLog->setTextColor(Color);
	pLog->append(Pack);
}

void CKadGUI::AddLog(uint32 uFlag, const QString &Pack)
{
	int Limit = m_pKad->Cfg()->GetInt("Gui/LogLength");
	::AddLog(m_pDebugLog, uFlag, Pack, Limit);
	if ((uFlag & LOG_DEBUG) == 0)  
		::AddLog(m_pUserLog, uFlag, Pack, Limit);
}

void CKadGUI::GetLogLines()
{
	QList<CLog::SLine> Log = m_pKad->GetLog();

	int Index = 0;
	if(uint64 uLast = m_uLastLog)
	{
		for(int i=0;i < Log.count(); i++)
		{
			if(uLast == Log.at(i).uID)
			{
				Index = i+1;
				break;
			}
		}
	}

	if(Index >= Log.count())
		return;

	int Flags = Log.at(Index).uFlag;
	QString Pack = QDateTime::fromTime_t((time_t)Log.at(Index).uStamp).toLocalTime().time().toString() + ": " + Log.at(Index).Line.Print();
	for(Index++; Index < Log.count(); Index++)
	{
		if(Flags != Log.at(Index).uFlag)
		{
			AddLog(Flags, Pack);
			Pack.clear();
			Flags = Log.at(Index).uFlag;
		}
		if(!Pack.isEmpty())
			Pack += "\n";
		Pack += QDateTime::fromTime_t((time_t)Log.at(Index).uStamp).toLocalTime().time().toString() + ": " + Log.at(Index).Line.Print();
	}
	AddLog(Flags, Pack);
	m_uLastLog = Log.at(Index-1).uID;
}

void CKadGUI::DumpRoutignTable()
{
	Kademlia::ContactList Contacts;
	Kademlia::CKademlia::GetRoutingZone()->GetAllEntries(&Contacts);

	QMap<uint64, QTreeWidgetItem*> RoutingMap;
	for(int i=0; i<m_pRoutingTable->topLevelItemCount();i++)
	{
		QTreeWidgetItem* pItem = m_pRoutingTable->topLevelItem(i);
		uint64 ID = pItem->data(0, Qt::UserRole).toULongLong();
		ASSERT(!RoutingMap.contains(ID));
		RoutingMap.insert(ID,pItem);
	}

	for (Kademlia::ContactList::iterator I = Contacts.begin(); I != Contacts.end(); I++)
	{
		Kademlia::CContact* Contact = *I;
		//ID|Type|Distance|IP
		uint64 ID = (uint64)Contact;
		QTreeWidgetItem* pItem = RoutingMap.take(ID);
		if(!pItem)
		{
			pItem = new QTreeWidgetItem();
			pItem->setData(0, Qt::UserRole, ID);
			pItem->setText(0, QString::fromStdWString(Contact->GetClientID().ToHexString()));
			pItem->setText(2, QString::fromStdWString(Contact->GetDistance().ToBinaryString()));
			pItem->setText(3, QString::fromStdWString(IPToStr(Contact->GetIPAddress(), Contact->GetUDPPort())));
			m_pRoutingTable->addTopLevelItem(pItem);
			//pItem->setExpanded(true);
		}
		pItem->setText(1, tr("%1 (%2)").arg(Contact->GetType()).arg(Contact->GetVersion()));
	}

	// whats left is to be deleted 
	foreach(QTreeWidgetItem* pItem, RoutingMap)
		delete pItem;
}

void CKadGUI::DumpSearchList()
{
	Kademlia::SearchMap Searches;
	Kademlia::CSearchManager::GetAllSearches(&Searches);

	QMap<uint64, QTreeWidgetItem*> SearchList;
	for(int i=0; i<m_pLookupList->topLevelItemCount();i++)
	{
		QTreeWidgetItem* pItem = m_pLookupList->topLevelItem(i);
		uint64 ID = pItem->data(0, Qt::UserRole).toULongLong();
		ASSERT(!SearchList.contains(ID));
		SearchList.insert(ID,pItem);
	}

	for (Kademlia::SearchMap::iterator I = Searches.begin(); I != Searches.end(); I++)
	{
		Kademlia::CSearch* Search = I->second;
		//Number|Key|Type|Status|Load|Packets Sent|Responses
		uint64 ID = (uint64)Search;
		QTreeWidgetItem* pItem = SearchList.take(ID);
		if(!pItem)
		{
			pItem = new QTreeWidgetItem();
			pItem->setData(0, Qt::UserRole, ID);
			pItem->setText(0, QString::number((int)Search->GetSearchID()));
			pItem->setText(1, QString::fromStdWString(Search->GetTarget().ToHexString()));
			switch (Search->GetSearchTypes())
			{
				case Kademlia::CSearch::FILE:			pItem->setText(2, "Search Sources");	break;
				case Kademlia::CSearch::KEYWORD:		pItem->setText(2, "Search Keywords");	break;
				case Kademlia::CSearch::NODE:
				case Kademlia::CSearch::NODECOMPLETE:
				case Kademlia::CSearch::NODESPECIAL:
				case Kademlia::CSearch::NODEFWCHECKUDP:	pItem->setText(2, "Node Lookup");		break;
				case Kademlia::CSearch::STOREFILE:		pItem->setText(2, "Store File");		break;
				case Kademlia::CSearch::STOREKEYWORD:	pItem->setText(2, "Store Keyword");		break;
				case Kademlia::CSearch::FINDBUDDY:		pItem->setText(2, "Find Buddy");		break;
				case Kademlia::CSearch::STORENOTES:		pItem->setText(2, "Store Notes");		break;
				case Kademlia::CSearch::NOTES:			pItem->setText(2, "Notes");				break;
				case Kademlia::CSearch::FINDSOURCE:		pItem->setText(2, "Callback");			break;
				default:								pItem->setText(2, "Unknown");
			}
			m_pLookupList->addTopLevelItem(pItem);
			//pItem->setExpanded(true);
		}
		pItem->setText(3, QString::fromStdWString(Search->GetName()));
		pItem->setText(4, Search->Stopping() ? "Stopping" : "Active");
		pItem->setText(5, QString("%1 (%2|%3)").arg(Search->GetNodeLoad()).arg(Search->GetNodeLoadResponse()).arg(Search->GetNodeLoadTotal()));
		pItem->setText(6, QString("(%1|%2)").arg(Search->GetKadPacketSent()).arg(Search->GetRequestAnswer()));
		pItem->setText(7, QString("%1").arg(Search->GetAnswers()));
	}

	// whats left is to be deleted 
	foreach(QTreeWidgetItem* pItem, SearchList)
		delete pItem;
}
