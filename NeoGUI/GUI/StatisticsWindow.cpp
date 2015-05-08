#include "GlobalHeader.h"
#include "StatisticsWindow.h"
#include "../Common/IncrementalPlot.h"
#include "../NeoGUI.h"
#include "../Common/Common.h"

CStatisticsWindow::CStatisticsWindow(QWidget *parent)
:QWidget(parent)
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(1);
	m_pSplitter = new QSplitter();
	m_pSplitter->setOrientation(Qt::Horizontal);

	m_pStatisticsTree = new QTreeWidget();
	m_pStatisticsTree->header()->hide();
#ifdef WIN32
	m_pStatisticsTree->setStyle(QStyleFactory::create("windowsxp"));
#endif
	m_pStatisticsTree->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pStatisticsTree, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenuRequested(const QPoint &)));

	m_pMenu = new QMenu();

	m_pCopy = new QAction(tr("Copy"), m_pMenu);
	connect(m_pCopy, SIGNAL(triggered()), this, SLOT(OnCopy()));
	m_pMenu->addAction(m_pCopy);

	m_pCopyAll = new QAction(tr("Copy All"), m_pMenu);
	connect(m_pCopyAll, SIGNAL(triggered()), this, SLOT(OnCopyAll()));
	m_pMenu->addAction(m_pCopyAll);

	m_pMenu->addSeparator();

	m_pReset = new QAction(tr("Reset Graphs"), m_pMenu);
	connect(m_pReset, SIGNAL(triggered()), this, SLOT(OnReset()));
	m_pMenu->addAction(m_pReset);

	m_pBandwidth = new QTreeWidgetItem(QStringList(tr("Bandwidth")));
	m_pStatisticsTree->addTopLevelItem(m_pBandwidth);
		m_pUpRate = new QTreeWidgetItem();
		m_pBandwidth->addChild(m_pUpRate);
		m_pDownRate = new QTreeWidgetItem();
		m_pBandwidth->addChild(m_pDownRate);
		m_pUpVolume = new QTreeWidgetItem();
		m_pBandwidth->addChild(m_pUpVolume);
		m_pDownVolume = new QTreeWidgetItem();
		m_pBandwidth->addChild(m_pDownVolume);
		m_pCons = new QTreeWidgetItem();
		m_pBandwidth->addChild(m_pCons);

		m_pPinger = new QTreeWidgetItem(QStringList(tr("Pinger")));
		m_pBandwidth->addChild(m_pPinger);
			m_pPingHost = new QTreeWidgetItem();
			m_pPinger->addChild(m_pPingHost);
			m_pPingValue = new QTreeWidgetItem();
			m_pPinger->addChild(m_pPingValue);

		m_pPeerWatch = new QTreeWidgetItem();
		m_pBandwidth->addChild(m_pPeerWatch);

	m_pBandwidth->setExpanded(true);

	m_pNetworks = new QTreeWidgetItem(QStringList(tr("Networks")));
	m_pStatisticsTree->addTopLevelItem(m_pNetworks);
		m_pNeoShare = new QTreeWidgetItem(QStringList(tr("Neo Share")));
		m_pNetworks->addChild(m_pNeoShare);
			m_pNeoShareUpRate = new QTreeWidgetItem();
			m_pNeoShare->addChild(m_pNeoShareUpRate);
			m_pNeoShareDownRate = new QTreeWidgetItem();
			m_pNeoShare->addChild(m_pNeoShareDownRate);
			m_pNeoShareUpVolume = new QTreeWidgetItem();
			m_pNeoShare->addChild(m_pNeoShareUpVolume);
			m_pNeoShareDownVolume = new QTreeWidgetItem();
			m_pNeoShare->addChild(m_pNeoShareDownVolume);
			m_pNeoShareKadStatus = new QTreeWidgetItem();
			m_pNeoShare->addChild(m_pNeoShareKadStatus);
			m_pNeoSharePort = new QTreeWidgetItem();
			m_pNeoShare->addChild(m_pNeoSharePort);
			m_pNeoShareIP = new QTreeWidgetItem();
			m_pNeoShare->addChild(m_pNeoShareIP);
			m_pNeoShareCons = new QTreeWidgetItem();
			m_pNeoShare->addChild(m_pNeoShareCons);
			m_pNeoShareFirewalled = new QTreeWidgetItem();
			m_pNeoShare->addChild(m_pNeoShareFirewalled);
			m_pNeoShareRoutes = new QTreeWidgetItem(QStringList(tr("Routes")));
			m_pNeoShare->addChild(m_pNeoShareRoutes);
		m_pNeoShare->setExpanded(true);

		m_pBitTorrent = new QTreeWidgetItem(QStringList(tr("BitTorrent")));
		m_pNetworks->addChild(m_pBitTorrent);
			m_pTorrentUpRate = new QTreeWidgetItem();
			m_pBitTorrent->addChild(m_pTorrentUpRate);
			m_pTorrentDownRate = new QTreeWidgetItem();
			m_pBitTorrent->addChild(m_pTorrentDownRate);
			m_pTorrentUpVolume = new QTreeWidgetItem();
			m_pBitTorrent->addChild(m_pTorrentUpVolume);
			m_pTorrentDownVolume = new QTreeWidgetItem();
			m_pBitTorrent->addChild(m_pTorrentDownVolume);
			m_pTorrentPort = new QTreeWidgetItem();
			m_pBitTorrent->addChild(m_pTorrentPort);
			m_pTorrentIP = new QTreeWidgetItem();
			m_pBitTorrent->addChild(m_pTorrentIP);
			m_pTorrentCons = new QTreeWidgetItem();
			m_pBitTorrent->addChild(m_pTorrentCons);
			m_pTorrentFirewalled = new QTreeWidgetItem();
			m_pBitTorrent->addChild(m_pTorrentFirewalled);
			m_pDHTStats = new QTreeWidgetItem(QStringList(tr("DHT Stats")));
			m_pBitTorrent->addChild(m_pDHTStats);
				m_pDHTNodes = new QTreeWidgetItem();
				m_pDHTStats->addChild(m_pDHTNodes);
				m_pDHTGlobalNodes = new QTreeWidgetItem();
				m_pDHTStats->addChild(m_pDHTGlobalNodes);
				m_pDHTLookups = new QTreeWidgetItem();
				m_pDHTStats->addChild(m_pDHTLookups);
		m_pBitTorrent->setExpanded(true);

		m_pEd2kMule = new QTreeWidgetItem(QStringList(tr("ed2k/eMule")));
		m_pNetworks->addChild(m_pEd2kMule);
			m_pMuleUpRate = new QTreeWidgetItem();
			m_pEd2kMule->addChild(m_pMuleUpRate);
			m_pMuleDownRate = new QTreeWidgetItem();
			m_pEd2kMule->addChild(m_pMuleDownRate);
			m_pMuleUpVolume = new QTreeWidgetItem();
			m_pEd2kMule->addChild(m_pMuleUpVolume);
			m_pMuleDownVolume = new QTreeWidgetItem();
			m_pEd2kMule->addChild(m_pMuleDownVolume);
			m_pMulePort = new QTreeWidgetItem();
			m_pEd2kMule->addChild(m_pMulePort);
			m_pMulePortUDP = new QTreeWidgetItem();
			m_pEd2kMule->addChild(m_pMulePortUDP);
			m_pMuleIP = new QTreeWidgetItem();
			m_pEd2kMule->addChild(m_pMuleIP);
			m_pMuleCons = new QTreeWidgetItem();
			m_pEd2kMule->addChild(m_pMuleCons);
			m_pMuleFirewalled = new QTreeWidgetItem();
			m_pEd2kMule->addChild(m_pMuleFirewalled);
			m_pMuleKadStatus = new QTreeWidgetItem();
			m_pEd2kMule->addChild(m_pMuleKadStatus);
			m_pMuleKadPort = new QTreeWidgetItem();
			m_pEd2kMule->addChild(m_pMuleKadPort);
			m_pMuleKadStats = new QTreeWidgetItem(QStringList(tr("Kad Stats")));
			m_pEd2kMule->addChild(m_pMuleKadStats);
				m_pMuleKadTotalUsers = new QTreeWidgetItem();
				m_pMuleKadStats->addChild(m_pMuleKadTotalUsers);
				m_pMuleKadTotalFiles = new QTreeWidgetItem();
				m_pMuleKadStats->addChild(m_pMuleKadTotalFiles);
				m_pMuleKadIndexedSource = new QTreeWidgetItem();
				m_pMuleKadStats->addChild(m_pMuleKadIndexedSource);
				m_pMuleKadIndexedKeyword = new QTreeWidgetItem();
				m_pMuleKadStats->addChild(m_pMuleKadIndexedKeyword);
				m_pMuleKadIndexedNotes = new QTreeWidgetItem();
				m_pMuleKadStats->addChild(m_pMuleKadIndexedNotes);
				m_pMuleKadIndexLoad = new QTreeWidgetItem();
				m_pMuleKadStats->addChild(m_pMuleKadIndexLoad);
			//m_pMuleKadStats->setExpanded(true);
		m_pEd2kMule->setExpanded(true);

	m_pNetworks->setExpanded(true);

	m_pHosters = new QTreeWidgetItem(QStringList(tr("Hosters")));
	m_pStatisticsTree->addTopLevelItem(m_pHosters);
		m_pHostersUpRate = new QTreeWidgetItem();
		m_pHosters->addChild(m_pHostersUpRate);
		m_pHostersDownRate = new QTreeWidgetItem();
		m_pHosters->addChild(m_pHostersDownRate);
		m_pHostersUpVolume = new QTreeWidgetItem();
		m_pHosters->addChild(m_pHostersUpVolume);
		m_pHostersDownVolume = new QTreeWidgetItem();
		m_pHosters->addChild(m_pHostersDownVolume);
		m_pHostersUploads = new QTreeWidgetItem();
		m_pHosters->addChild(m_pHostersUploads);
		m_pHostersDownloads = new QTreeWidgetItem();
		m_pHosters->addChild(m_pHostersDownloads);
	m_pHosters->setExpanded(true);

	m_pIOStats = new QTreeWidgetItem(QStringList(tr("IO Stats")));
	m_pStatisticsTree->addTopLevelItem(m_pIOStats);
		m_pPendingRead = new QTreeWidgetItem();
		m_pIOStats->addChild(m_pPendingRead);
		m_pPendingWrite = new QTreeWidgetItem();
		m_pIOStats->addChild(m_pPendingWrite);
		m_pHashingCount = new QTreeWidgetItem();
		m_pIOStats->addChild(m_pHashingCount);
		m_pAllocationCount = new QTreeWidgetItem();
		m_pIOStats->addChild(m_pAllocationCount);
		
	m_pIOStats->setExpanded(true);


	m_pStatisticsWidget = new QWidget();
	m_pStatisticsLayout = new QVBoxLayout();
	m_pStatisticsLayout->setMargin(0);

	QColor Back = QColor(0,0,64);
	QColor Front = QColor(187,206,239);
	QColor Grid = QColor(46,44,119);

	m_DownRate = new CIncrementalPlot(Back, Front, Grid, tr("Download Rate"), CIncrementalPlot::eBytes);
	m_DownRate->setMinimumHeight(100);
	m_DownRate->AddPlot("Download", Qt::white, Qt::SolidLine, tr("Total"));
	m_DownRate->AddPlot("DownloadEx", Qt::white, Qt::DashLine);
	if(theGUI->Cfg()->GetInt("Gui/AdvancedControls") == 1) //if(theGUI->Cfg()->GetBool("Gui/VerboseGraphs"))
	{
		m_DownRate->AddPlot("DownloadHdr", Qt::white, Qt::DashDotLine);
		m_DownRate->AddPlot("DownloadAck", Qt::white, Qt::DashDotDotLine);
	}
	m_DownRate->AddPlot("DownRate", Qt::white, Qt::DotLine);
	m_DownRate->AddPlot("NeoDownload", Qt::blue, Qt::SolidLine, tr("NeoShare"));
	m_DownRate->AddPlot("NeoDownloadEx", Qt::blue, Qt::DashLine);
	if(theGUI->Cfg()->GetInt("Gui/AdvancedControls") == 1) //if(theGUI->Cfg()->GetBool("Gui/VerboseGraphs"))
	{
	//	m_DownRate->AddPlot("NeoDownloadHdr", Qt::blue, Qt::DashDotLine);
	//	m_DownRate->AddPlot("NeoDownloadAck", Qt::blue, Qt::DashDotDotLine);
		m_DownRate->AddPlot("NeoDownRate", Qt::blue, Qt::DotLine);
	}
	m_DownRate->AddPlot("TorrentDownload", Qt::green, Qt::SolidLine, tr("BitTorrent"));
	m_DownRate->AddPlot("TorrentDownloadEx", Qt::green, Qt::DashLine);
	if(theGUI->Cfg()->GetInt("Gui/AdvancedControls") == 1) //if(theGUI->Cfg()->GetBool("Gui/VerboseGraphs"))
	{
		m_DownRate->AddPlot("TorrentDownloadHdr", Qt::green, Qt::DashDotLine);
		m_DownRate->AddPlot("TorrentDownloadAck", Qt::green, Qt::DashDotDotLine);
		m_DownRate->AddPlot("TorrentDownRate", Qt::green, Qt::DotLine);
	}
	m_DownRate->AddPlot("MuleDownload", Qt::red, Qt::SolidLine, tr("ed2k/eMule"));
	m_DownRate->AddPlot("MuleDownloadEx", Qt::red, Qt::DashLine);
	if(theGUI->Cfg()->GetInt("Gui/AdvancedControls") == 1) //if(theGUI->Cfg()->GetBool("Gui/VerboseGraphs"))
	{
		m_DownRate->AddPlot("MuleDownloadHdr", Qt::red, Qt::DashDotLine);
		m_DownRate->AddPlot("MuleDownloadAck", Qt::red, Qt::DashDotDotLine);
		m_DownRate->AddPlot("MuleDownRate", Qt::red, Qt::DotLine);
	}
	m_DownRate->AddPlot("HosterDownload", Qt::yellow, Qt::SolidLine, tr("Hosters"));
	m_DownRate->AddPlot("DownLimit", Qt::gray, Qt::SolidLine, tr("Limit"));
	//m_DownRate->AddPlot("DownTotal", Qt::magenta, Qt::SolidLine);
	m_pStatisticsLayout->addWidget(m_DownRate);

	m_UpRate = new CIncrementalPlot(Back, Front, Grid, tr("Upload Rate"), CIncrementalPlot::eBytes);
	m_UpRate->setMinimumHeight(100);
	m_UpRate->AddPlot("Upload", Qt::white, Qt::SolidLine, tr("Total"));
	m_UpRate->AddPlot("UploadEx", Qt::white, Qt::DashLine);
	if(theGUI->Cfg()->GetInt("Gui/AdvancedControls") == 1) //if(theGUI->Cfg()->GetBool("Gui/VerboseGraphs"))
	{
		m_UpRate->AddPlot("UploadHdr", Qt::white, Qt::DashDotLine);
		m_UpRate->AddPlot("UploadAck", Qt::white, Qt::DashDotDotLine);
	}
	m_UpRate->AddPlot("UpRate", Qt::white, Qt::DotLine);
	m_UpRate->AddPlot("NeoUpload", Qt::blue, Qt::SolidLine, tr("NeoShare"));
	m_UpRate->AddPlot("NeoUploadEx", Qt::blue, Qt::DashLine);
	if(theGUI->Cfg()->GetInt("Gui/AdvancedControls") == 1) //if(theGUI->Cfg()->GetBool("Gui/VerboseGraphs"))
	{
	//	m_UpRate->AddPlot("NeoUploadHdr", Qt::blue, Qt::DashDotLine);
	//	m_UpRate->AddPlot("NeoUploadAck", Qt::blue, Qt::DashDotDotLine);
		m_UpRate->AddPlot("NeoUpRate", Qt::blue, Qt::DotLine);
	}
	m_UpRate->AddPlot("TorrentUpload", Qt::green, Qt::SolidLine, tr("BitTorrent"));
	m_UpRate->AddPlot("TorrentUploadEx", Qt::green, Qt::DashLine);
	if(theGUI->Cfg()->GetInt("Gui/AdvancedControls") == 1) //if(theGUI->Cfg()->GetBool("Gui/VerboseGraphs"))
	{
		m_UpRate->AddPlot("TorrentUploadHdr", Qt::green, Qt::DashDotLine);
		m_UpRate->AddPlot("TorrentUploadAck", Qt::green, Qt::DashDotDotLine);
		m_UpRate->AddPlot("TorrentUpRate", Qt::green, Qt::DotLine);
	}
	m_UpRate->AddPlot("MuleUpload", Qt::red, Qt::SolidLine, tr("ed2k/eMule"));
	m_UpRate->AddPlot("MuleUploadEx", Qt::red, Qt::DashLine);
	if(theGUI->Cfg()->GetInt("Gui/AdvancedControls") == 1) //if(theGUI->Cfg()->GetBool("Gui/VerboseGraphs"))
	{
		m_UpRate->AddPlot("MuleUploadHdr", Qt::red, Qt::DashDotLine);
		m_UpRate->AddPlot("MuleUploadAck", Qt::red, Qt::DashDotDotLine);
		m_UpRate->AddPlot("MuleUpRate", Qt::red, Qt::DotLine);
	}
	m_UpRate->AddPlot("HosterUpload", Qt::yellow, Qt::SolidLine, tr("Hosters"));
	m_UpRate->AddPlot("UpLimit", Qt::gray, Qt::SolidLine, tr("Limit"));
	//m_UpRate->AddPlot("UpTotal", Qt::magenta, Qt::SolidLine);
	m_pStatisticsLayout->addWidget(m_UpRate);

	m_Connections = new CIncrementalPlot(Back, Front, Grid, tr("Connections"));
	m_Connections->setMinimumHeight(100);
	m_Connections->AddPlot("Connections", Qt::white, Qt::SolidLine, tr("Total"));
	m_Connections->AddPlot("NeoShare", Qt::blue, Qt::SolidLine, tr("NeoShare"));
	m_Connections->AddPlot("BitTorrent", Qt::green, Qt::SolidLine, tr("BitTorrent"));
	m_Connections->AddPlot("Ed2kMule", Qt::red, Qt::SolidLine, tr("ed2k/eMule"));
	m_Connections->AddPlot("WebTasks", Qt::yellow, Qt::SolidLine, tr("Hosters"));
	m_pStatisticsLayout->addWidget(m_Connections);

	m_pStatisticsWidget->setLayout(m_pStatisticsLayout);

	m_pSplitter->addWidget(m_pStatisticsTree);
	m_pSplitter->addWidget(m_pStatisticsWidget);

	m_pMainLayout->addWidget(m_pSplitter);

	setLayout(m_pMainLayout);

	m_pSplitter->restoreState(theGUI->Cfg()->GetBlob("Gui/Widget_Stats_Spliter"));

	m_TimerId = startTimer(1000);
}

CStatisticsWindow::~CStatisticsWindow()
{
	theGUI->Cfg()->SetBlob("Gui/Widget_Stats_Spliter",m_pSplitter->saveState());

	killTimer(m_TimerId);
}

void CStatisticsWindow::UpdateStats(const QVariantMap& Response)
{
	Response;

	QVariantMap Bandwidth = Response["Bandwidth"].toMap();
	//m_pUpRate->setText(0, tr("Upload Rate: %1/s (%2/s +- %3/s) [%4/%5]").arg(FormatSize(Bandwidth["Upload"].toInt())).arg(FormatSize(Bandwidth["UpRate"].toInt())).arg(FormatSize(Bandwidth["UpVar"].toInt()))
	m_pUpRate->setText(0, tr("Upload Rate: %1/s (%2/s) [%3/%4]").arg(FormatSize(Bandwidth["Upload"].toInt())).arg(FormatSize(Bandwidth["UpRate"].toInt()))
																		.arg(Bandwidth["ActiveUploads"].toInt()).arg(Bandwidth["WaitingUploads"].toInt()));
	m_UpRate->AddPlotPoint("UpRate", Bandwidth["UpRate"].toInt());
	m_UpRate->AddPlotPoint("UploadEx", Bandwidth["UploadEx"].toInt());
	m_UpRate->AddPlotPoint("UploadHdr", Bandwidth["UploadHdr"].toInt());
	m_UpRate->AddPlotPoint("UploadAck", Bandwidth["UploadAck"].toInt());
	m_UpRate->AddPlotPoint("Upload", Bandwidth["Upload"].toInt());
	int UpLimit = Bandwidth["UpLimit"].toInt();
	m_UpRate->AddPlotPoint("UpLimit", UpLimit > 0 ? UpLimit : 0);
	//m_UpRate->AddPlotPoint("UpTotal", Bandwidth["UpTotal"].toInt());

	//m_pDownRate->setText(0, tr("Download Rate: %1/s (%2/s +- %3/s) [%4/%5]").arg(FormatSize(Bandwidth["Download"].toInt())).arg(FormatSize(Bandwidth["DownRate"].toInt())).arg(FormatSize(Bandwidth["DownVar"].toInt()))
	m_pDownRate->setText(0, tr("Download Rate: %1/s (%2/s) [%3/%4]").arg(FormatSize(Bandwidth["Download"].toInt())).arg(FormatSize(Bandwidth["DownRate"].toInt()))
																		.arg(Bandwidth["ActiveDownloads"].toInt()).arg(Bandwidth["WaitingDownloads"].toInt()));
	m_DownRate->AddPlotPoint("DownRate", Bandwidth["DownRate"].toInt());
	m_DownRate->AddPlotPoint("DownloadEx", Bandwidth["DownloadEx"].toInt());
	m_DownRate->AddPlotPoint("DownloadHdr", Bandwidth["DownloadHdr"].toInt());
	m_DownRate->AddPlotPoint("DownloadAck", Bandwidth["DownloadAck"].toInt());
	m_DownRate->AddPlotPoint("Download", Bandwidth["Download"].toInt());
	int DownLimit = Bandwidth["DownLimit"].toInt();
	m_DownRate->AddPlotPoint("DownLimit", DownLimit > 0 ? DownLimit : 0);
	//m_DownRate->AddPlotPoint("DownTotal", Bandwidth["DownTotal"].toInt());

/*#ifdef _DEBUG
	int UpErr = Bandwidth["UpTotal"].toInt() - Bandwidth["UpRate"].toInt();
	int UpPr = Bandwidth["UpRate"].toInt() > 0 ? UpErr * 100 / Bandwidth["UpRate"].toInt() : 0;
	int DownErr = Bandwidth["DownTotal"].toInt() - Bandwidth["DownRate"].toInt();
	int DownPr = Bandwidth["DownRate"].toInt() > 0 ? DownErr * 100 / Bandwidth["DownRate"].toInt() : 0;
	m_pBandwidth->setText(0, tr("Bandwidth - Div: %1/s(%2%) - %3/s(%4%)")
		.arg(UpErr > 0 ? FormatSize(UpErr) : "-" + FormatSize(-UpErr)).arg(UpPr)
		.arg(DownErr > 0 ? FormatSize(DownErr) : "-" + FormatSize(-DownErr)).arg(DownPr));
#endif*/

	m_pUpVolume->setText(0, tr("Upload Volume: %1 / %2").arg(FormatSize(Bandwidth["UploadedSession"].toULongLong())).arg(FormatSize(Bandwidth["UploadedTotal"].toULongLong())));
	m_pDownVolume->setText(0, tr("Download Volume: %1 / %2").arg(FormatSize(Bandwidth["DownloadedSession"].toULongLong())).arg(FormatSize(Bandwidth["DownloadedTotal"].toULongLong())));

	m_pCons->setText(0, tr("Active Connections: %1").arg(Bandwidth["Connections"].toString()));
	m_Connections->AddPlotPoint("Connections", Bandwidth["Connections"].toInt());
	
	QVariantMap Pinger = Bandwidth["Pinger"].toMap();
		m_pPingHost->setText(0, tr("Host: %1 (%2)").arg(Pinger["Host"].toString()).arg(Pinger["TTL"].toInt()));
		m_pPingValue->setText(0, tr("Ping: %1ms / %2ms").arg(Pinger["Average"].toUInt()).arg(Pinger["Lowest"].toUInt()));

	QVariantMap PeerWatch = Bandwidth["PeerWatch"].toMap();
	m_pPeerWatch->setText(0, tr("PeerWatch: %1 - %2 - %3").arg(PeerWatch["Alive"].toInt()).arg(PeerWatch["Dead"].toInt()).arg(PeerWatch["Banned"].toInt()));

	QVariantMap Networks = Response["Networks"].toMap();

	if(Networks.contains("NeoShare"))
	{
		m_pNeoShare->setExpanded(true);
		QVariantMap NeoShare = Networks["NeoShare"].toMap();

		m_pNeoShareUpRate->setText(0, tr("Upload Rate: %1/s (%2/s)").arg(FormatSize(NeoShare["Upload"].toInt())).arg(FormatSize(NeoShare["UploadEx"].toInt())));
		m_UpRate->AddPlotPoint("NeoUpRate", NeoShare["UpRate"].toInt());
		m_UpRate->AddPlotPoint("NeoUploadEx", NeoShare["UploadEx"].toInt());
		m_UpRate->AddPlotPoint("NeoUpload", NeoShare["Upload"].toInt());
		m_pNeoShareDownRate->setText(0, tr("Download Rate: %1/s (%2/s)").arg(FormatSize(NeoShare["Download"].toInt())).arg(FormatSize(NeoShare["DownloadEx"].toInt())));
		m_DownRate->AddPlotPoint("NeoDownRate", NeoShare["DownRate"].toInt());
		m_DownRate->AddPlotPoint("NeoDownloadEx", NeoShare["DownloadEx"].toInt());
		m_DownRate->AddPlotPoint("NeoDownload", NeoShare["Download"].toInt());

		m_pNeoShareUpVolume->setText(0, tr("Upload Volume: %1 / %2").arg(FormatSize(NeoShare["UploadedSession"].toULongLong())).arg(FormatSize(NeoShare["UploadedTotal"].toULongLong())));
		m_pNeoShareDownVolume->setText(0, tr("Download Volume: %1 / %2").arg(FormatSize(NeoShare["DownloadedSession"].toULongLong())).arg(FormatSize(NeoShare["DownloadedTotal"].toULongLong())));

		m_pNeoShareKadStatus->setText(0, tr("Kad Status: %1").arg(NeoShare["KadStatus"].toString()));
		NeoShare["KadStatus"].toString() == "Connected";

		m_pNeoSharePort->setText(0, tr("NeoShare Port: %1").arg(NeoShare["Port"].toString()));
		if(NeoShare.contains("AddressV6"))
		{
			m_pNeoShareIP->setText(0, tr("Current IP: %1 & %2").arg(NeoShare["Address"].toString()).arg(NeoShare["AddressV6"].toString()));
			m_pNeoShareFirewalled->setText(0, tr("Firewalled: %1 & %2")
				.arg(NeoShare["Firewalled"].toBool() ? NeoShare["NATed"].toBool() ? tr("Yes (UDP No)") : tr("Yes") : tr("No"))
				.arg(NeoShare["FirewalledV6"].toBool() ? NeoShare["NATedV6"].toBool() ? tr("Yes (UDP No)") : tr("Yes") : tr("No"))
				);
		}	
		else
		{
			m_pNeoShareIP->setText(0, tr("Current IP: %1").arg(NeoShare["Address"].toString()));
			m_pNeoShareFirewalled->setText(0, tr("Firewalled: %1")
				.arg(NeoShare["Firewalled"].toBool() ? NeoShare["NATed"].toBool() ? tr("Yes (UDP No)") : tr("Yes") : tr("No"))
				);
		}
		m_pNeoShareCons->setText(0, tr("Active Sessions: %1").arg(NeoShare["Sessions"].toString()));
		m_Connections->AddPlotPoint("NeoShare", NeoShare["Sessions"].toInt());

		QMap<QByteArray, QTreeWidgetItem*> OldRoutes;
		for(int i=0; i < m_pNeoShareRoutes->childCount(); i++)
		{
			QTreeWidgetItem* pItem = m_pNeoShareRoutes->child(i);
			QByteArray ID = pItem->data(0, Qt::UserRole).toByteArray();
			ASSERT(!OldRoutes.contains(ID));
			OldRoutes.insert(ID, pItem);
		}

		foreach(const QVariant& vRoute, NeoShare["Routes"].toList())
		{
			QVariantMap Route = vRoute.toMap();
			QByteArray ID = Route["EntityID"].toByteArray() + "@" + Route["TargetID"].toByteArray();
			
			QTreeWidgetItem* pItem = OldRoutes.take(ID);
			if(!pItem)
			{
				pItem = new QTreeWidgetItem();
				pItem->setData(0, Qt::UserRole, ID);
				m_pNeoShareRoutes->addChild(pItem);
			}

			QByteArray TargetID = Route["TargetID"].toByteArray();
			std::reverse(TargetID.begin(), TargetID.end());
			pItem->setText(0, QString::fromLatin1(Route["EntityID"].toByteArray().toHex() + "@" + TargetID.toHex().toUpper()) 
				+ (Route["IsStatic"].toBool() ? "" : tr(" (%1)").arg(FormatTime(Route["TimeOut"].toULongLong()))));
		}

		foreach(QTreeWidgetItem* pItem, OldRoutes)
			delete pItem;
	}
	else
	{
		m_pNeoShare->setExpanded(false);

		m_UpRate->AddPlotPoint("NeoUpRate", 0);
		m_UpRate->AddPlotPoint("NeoUploadEx", 0);
		m_UpRate->AddPlotPoint("NeoUpload", 0);
		
		m_DownRate->AddPlotPoint("NeoDownRate", 0);
		m_DownRate->AddPlotPoint("NeoDownloadEx", 0);
		m_DownRate->AddPlotPoint("NeoDownload", 0);

		m_Connections->AddPlotPoint("NeoShare", 0);
	}

	if(Networks.contains("BitTorrent"))
	{
		m_pBitTorrent->setExpanded(true);
		QVariantMap BitTorrent = Networks["BitTorrent"].toMap();

		m_pTorrentUpRate->setText(0, tr("Upload Rate: %1/s (%2/s)").arg(FormatSize(BitTorrent["Upload"].toInt())).arg(FormatSize(BitTorrent["UploadEx"].toInt())));
		m_UpRate->AddPlotPoint("TorrentUpRate", BitTorrent["UpRate"].toInt());
		m_UpRate->AddPlotPoint("TorrentUploadEx", BitTorrent["UploadEx"].toInt());
		m_UpRate->AddPlotPoint("TorrentUploadHdr", BitTorrent["UploadHdr"].toInt());
		m_UpRate->AddPlotPoint("TorrentUploadAck", BitTorrent["UploadAck"].toInt());
		m_UpRate->AddPlotPoint("TorrentUpload", BitTorrent["Upload"].toInt());
		m_pTorrentDownRate->setText(0, tr("Download Rate: %1/s (%2/s)").arg(FormatSize(BitTorrent["Download"].toInt())).arg(FormatSize(BitTorrent["DownloadEx"].toInt())));
		m_DownRate->AddPlotPoint("TorrentDownRate", BitTorrent["DownRate"].toInt());
		m_DownRate->AddPlotPoint("TorrentDownload", BitTorrent["Download"].toInt());
		m_DownRate->AddPlotPoint("TorrentDownloadEx", BitTorrent["DownloadEx"].toInt());
		m_DownRate->AddPlotPoint("TorrentDownloadHdr", BitTorrent["DownloadHdr"].toInt());
		m_DownRate->AddPlotPoint("TorrentDownloadAck", BitTorrent["DownloadAck"].toInt());

		m_pTorrentUpVolume->setText(0, tr("Upload Volume: %1 / %2").arg(FormatSize(BitTorrent["UploadedSession"].toULongLong())).arg(FormatSize(BitTorrent["UploadedTotal"].toULongLong())));
		m_pTorrentDownVolume->setText(0, tr("Download Volume: %1 / %2").arg(FormatSize(BitTorrent["DownloadedSession"].toULongLong())).arg(FormatSize(BitTorrent["DownloadedTotal"].toULongLong())));

		m_pTorrentPort->setText(0, tr("Torrent Port: %1").arg(BitTorrent["Port"].toString()));
		if(BitTorrent.contains("AddressV6"))
		{
			m_pTorrentIP->setText(0, tr("Current IP: %1 & %2").arg(BitTorrent["Address"].toString()).arg(BitTorrent["AddressV6"].toString()));
			m_pTorrentFirewalled->setText(0, tr("Firewalled: %1 & %2")
				.arg(BitTorrent["Firewalled"].toBool() ? BitTorrent["NATed"].toBool() ? tr("Yes (UDP No)") : tr("Yes") : tr("No"))
				.arg(BitTorrent["FirewalledV6"].toBool() ? BitTorrent["NATedV6"].toBool() ? tr("Yes (UDP No)") : tr("Yes") : tr("No"))
				);
		}	
		else
		{
			m_pTorrentIP->setText(0, tr("Current IP: %1").arg(BitTorrent["Address"].toString()));
			m_pTorrentFirewalled->setText(0, tr("Firewalled: %1")
				.arg(BitTorrent["Firewalled"].toBool() ? BitTorrent["NATed"].toBool() ? tr("Yes (UDP No)") : tr("Yes") : tr("No"))
				);
		}
		m_pTorrentCons->setText(0, tr("Active Connections: %1").arg(BitTorrent["Connections"].toString()));

		m_Connections->AddPlotPoint("BitTorrent", BitTorrent["Connections"].toInt());

			QVariantMap DHTStats = BitTorrent["DHTStats"].toMap();
			m_pDHTNodes->setText(0, tr("Nodes: %1").arg(DHTStats["Nodes"].toString()));
			m_pDHTGlobalNodes->setText(0, tr("Global Nodes: %1").arg(DHTStats["GlobalNodes"].toString()));
			m_pDHTLookups->setText(0, tr("Lookups: %1").arg(DHTStats["Lookups"].toString()));
	}
	else
	{
		m_pBitTorrent->setExpanded(false);

		m_UpRate->AddPlotPoint("TorrentUpRate", 0);
		m_UpRate->AddPlotPoint("TorrentUploadEx", 0);
		m_UpRate->AddPlotPoint("TorrentUploadHdr", 0);
		m_UpRate->AddPlotPoint("TorrentUploadAck", 0);
		m_UpRate->AddPlotPoint("TorrentUpload", 0);

		m_DownRate->AddPlotPoint("TorrentDownRate", 0);
		m_DownRate->AddPlotPoint("TorrentDownload", 0);
		m_DownRate->AddPlotPoint("TorrentDownloadEx", 0);
		m_DownRate->AddPlotPoint("TorrentDownloadHdr", 0);
		m_DownRate->AddPlotPoint("TorrentDownloadAck", 0);

		m_Connections->AddPlotPoint("BitTorrent", 0);
	}

	if(Networks.contains("Ed2kMule"))
	{
		m_pEd2kMule->setExpanded(true);
		QVariantMap Ed2kMule = Networks["Ed2kMule"].toMap();

		m_pMuleUpRate->setText(0, tr("Upload Rate: %1/s (%2/s)").arg(FormatSize(Ed2kMule["Upload"].toInt())).arg(FormatSize(Ed2kMule["UploadEx"].toInt())));
		m_UpRate->AddPlotPoint("MuleUpRate", Ed2kMule["UpRate"].toInt());
		m_UpRate->AddPlotPoint("MuleUpload", Ed2kMule["Upload"].toInt());
		m_UpRate->AddPlotPoint("MuleUploadEx", Ed2kMule["UploadEx"].toInt());
		m_UpRate->AddPlotPoint("MuleUploadHdr", Ed2kMule["UploadHdr"].toInt());
		m_UpRate->AddPlotPoint("MuleUploadAck", Ed2kMule["UploadAck"].toInt());
		m_pMuleDownRate->setText(0, tr("Download Rate: %1/s (%2/s)").arg(FormatSize(Ed2kMule["Download"].toInt())).arg(FormatSize(Ed2kMule["DownloadEx"].toInt())));
		m_DownRate->AddPlotPoint("MuleDownRate", Ed2kMule["DownRate"].toInt());
		m_DownRate->AddPlotPoint("MuleDownload", Ed2kMule["Download"].toInt());
		m_DownRate->AddPlotPoint("MuleDownloadEx", Ed2kMule["DownloadEx"].toInt());
		m_DownRate->AddPlotPoint("MuleDownloadHdr", Ed2kMule["DownloadHdr"].toInt());
		m_DownRate->AddPlotPoint("MuleDownloadAck", Ed2kMule["DownloadAck"].toInt());

		m_pMuleUpVolume->setText(0, tr("Upload Volume: %1 / %2").arg(FormatSize(Ed2kMule["UploadedSession"].toULongLong())).arg(FormatSize(Ed2kMule["UploadedTotal"].toULongLong())));
		m_pMuleDownVolume->setText(0, tr("Download Volume: %1 / %2").arg(FormatSize(Ed2kMule["DownloadedSession"].toULongLong())).arg(FormatSize(Ed2kMule["DownloadedTotal"].toULongLong())));

		m_pMulePort->setText(0, tr("TCP Port: %1").arg(Ed2kMule["TCPPort"].toString()));
		m_pMulePortUDP->setText(0, tr("UDP Port: %1").arg(Ed2kMule["UDPPort"].toString()));
		if(Ed2kMule.contains("AddressV6"))
		{
			m_pMuleIP->setText(0, tr("Current IP: %1 & %2").arg(Ed2kMule["Address"].toString()).arg(Ed2kMule["AddressV6"].toString()));
			m_pMuleFirewalled->setText(0, tr("Firewalled: %1 & %2")
				.arg(Ed2kMule["Firewalled"].toBool() ? Ed2kMule["NATed"].toBool() ? tr("Yes (UDP No)") : tr("Yes") : tr("No"))
				.arg(Ed2kMule["FirewalledV6"].toBool() ? Ed2kMule["NATedV6"].toBool() ? tr("Yes (UDP No)") : tr("Yes") : tr("No"))
				);
		}
		else
		{
			m_pMuleIP->setText(0, tr("Current IP: %1").arg(Ed2kMule["Address"].toString()));
			m_pMuleFirewalled->setText(0, tr("Firewalled: %1")
				.arg(Ed2kMule["Firewalled"].toBool() ? Ed2kMule["NATed"].toBool() ? tr("Yes (UDP No)") : tr("Yes") : tr("No"))
				);
		}
		m_pMuleCons->setText(0, tr("Active Connections: %1").arg(Ed2kMule["Connections"].toString()));
		m_Connections->AddPlotPoint("Ed2kMule", Ed2kMule["Connections"].toInt());
		m_pMuleKadStatus->setText(0, tr("Kad Status: %1").arg(Ed2kMule["KadStatus"].toString()));
		m_pMuleKadPort->setText(0, tr("Kad Port: %1").arg(Ed2kMule["KadPort"].toString()));

			QVariantMap KadStats = Ed2kMule["KadStats"].toMap();
			m_pMuleKadTotalUsers->setText(0, tr("Total Users: %1").arg(KadStats["TotalUsers"].toString()));
			m_pMuleKadTotalFiles->setText(0, tr("Total Files: %1").arg(KadStats["TotalFiles"].toString()));
			m_pMuleKadIndexedSource->setText(0, tr("Indexed Sources: %1").arg(KadStats["IndexedSource"].toString()));
			m_pMuleKadIndexedKeyword->setText(0, tr("Indexed Keywords: %1").arg(KadStats["IndexedKeyword"].toString()));
			m_pMuleKadIndexedNotes->setText(0, tr("Indexed Ratings: %1").arg(KadStats["IndexedNotes"].toString()));
			m_pMuleKadIndexLoad->setText(0, tr("Index Load: %1").arg(KadStats["IndexLoad"].toString()));
	}
	else
	{
		m_pEd2kMule->setExpanded(false);

		m_UpRate->AddPlotPoint("MuleUpRate", 0);
		m_UpRate->AddPlotPoint("MuleUpload", 0);
		m_UpRate->AddPlotPoint("MuleUploadEx", 0);
		m_UpRate->AddPlotPoint("MuleUploadHdr", 0);
		m_UpRate->AddPlotPoint("MuleUploadAck", 0);
		
		m_DownRate->AddPlotPoint("MuleDownRate", 0);
		m_DownRate->AddPlotPoint("MuleDownload", 0);
		m_DownRate->AddPlotPoint("MuleDownloadEx", 0);
		m_DownRate->AddPlotPoint("MuleDownloadHdr", 0);
		m_DownRate->AddPlotPoint("MuleDownloadAck", 0);

		m_Connections->AddPlotPoint("Ed2kMule", 0);
	}

	QVariantMap Hosters = Response["Hosters"].toMap();
	m_pHostersUpRate->setText(0, tr("Upload Rate: %1").arg(FormatSize(Hosters["Upload"].toInt()) + "/s"));
	m_UpRate->AddPlotPoint("HosterUpload", Hosters["Upload"].toInt());
	m_pHostersDownRate->setText(0, tr("Download Rate: %1").arg(FormatSize(Hosters["Download"].toInt()) + "/s"));
	m_DownRate->AddPlotPoint("HosterDownload", Hosters["Download"].toInt());

	m_pHostersUpVolume->setText(0, tr("Upload Volume: %1 / %2").arg(FormatSize(Hosters["UploadedTotal"].toULongLong())).arg(FormatSize(Hosters["UploadedSession"].toULongLong())));
	m_pHostersDownVolume->setText(0, tr("Download Volume: %1 / %2").arg(FormatSize(Hosters["DownloadedTotal"].toULongLong())).arg(FormatSize(Hosters["DownloadedSession"].toULongLong())));

	m_pHostersUploads->setText(0, tr("Active Uploads: %1").arg(Hosters["ActiveUploads"].toInt()));
	m_pHostersDownloads->setText(0, tr("Active Downloads: %1").arg(Hosters["ActiveDownloads"].toInt()));
	m_Connections->AddPlotPoint("WebTasks", Hosters["WebTasks"].toInt());

	QVariantMap IOStats = Response["IOStats"].toMap();
	m_pPendingRead->setText(0, tr("Pending Read: %1").arg(FormatSize(IOStats["PendingRead"].toInt())));
	m_pPendingWrite->setText(0, tr("Pending Write: %1").arg(FormatSize(IOStats["PendingWrite"].toInt())));
	m_pHashingCount->setText(0, tr("Hashing Queue: %1").arg(IOStats["HashingCount"].toInt()));
	m_pAllocationCount->setText(0, tr("Allocation Queue: %1").arg(IOStats["AllocationCount"].toInt()));
}

void CStatisticsWindow::timerEvent(QTimerEvent *e)
{
    if (e->timerId() != m_TimerId) 
	{
        QObject::timerEvent(e);
		return;
    }

	UpdateStats(theGUI->GetInfoData());
}

void CStatisticsWindow::OnMenuRequested(const QPoint &point)
{
	m_pMenu->popup(QCursor::pos());	
}

void CStatisticsWindow::OnCopy()
{
	QTreeWidgetItem* pItem = m_pStatisticsTree->currentItem();
	if(pItem)
		QApplication::clipboard()->setText(pItem->text(0));
}

void CopyAll(QTreeWidget* pTree, int Column = -1)
{
	QString Text;
	for(int i=0; i < pTree->topLevelItemCount(); i++)
	{
		QList<QPair<QTreeWidgetItem*, int> > ItemStack;
		ItemStack.append(QPair<QTreeWidgetItem*, int> (pTree->topLevelItem(i), 0));
		while(!ItemStack.isEmpty())
		{
			QTreeWidgetItem* pItem = ItemStack.last().first;
			if(ItemStack.last().second == 0)
			{
				Text.append(QString(ItemStack.size()-1,'\t'));
				if(Column == -1)
				{
					for(int i=0; i < pTree->columnCount(); i++)
					{
						if(i > 0)
							Text.append("\t");
						Text.append(pItem->text(i));
					}
				}
				else
					Text.append(pItem->text(Column));
				Text.append("\r\n");
			}
			if(ItemStack.last().second < pItem->childCount())
				ItemStack.append(QPair<QTreeWidgetItem*, int> (pItem->child(ItemStack.last().second++), 0));
			else
				ItemStack.removeLast();
		}
	}
	QApplication::clipboard()->setText(Text);
}

void CStatisticsWindow::OnCopyAll()
{
	CopyAll(m_pStatisticsTree);
}

void CStatisticsWindow::OnReset()
{
	m_DownRate->Reset();
	m_UpRate->Reset();
	m_Connections->Reset();
}
