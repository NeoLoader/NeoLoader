#include "GlobalHeader.h"
#include "NeoSummary.h"
#include "FileListWidget.h"
#include "FileListView.h"
#include "SearchWindow.h"
#include "../NeoGUI.h"
#include "../Common/PieChart.h"
#include "StatisticsWindow.h"
#include "../Common/Common.h"

CNeoSummary::CNeoSummary(QWidget *parent)
:QWidget(parent)
{
	m_SearchID = 0;
	m_LastSearch = 0;

	m_pMainLayout = new QGridLayout(this);
	m_pMainLayout->setMargin(3);

	m_pTopGroup = new QGroupBoxEx<QVBoxLayout>(tr("Top Files"));
	m_pTopGroup->setToolTip(tr("List of most recently shared files in NeoKad using HosterCache"));

	m_pTopFiles = new CFileListWidget(CFileListView::eFilesSearch);
	m_pTopFiles->ShowDetails(false);
	m_pTopGroup->layout()->addWidget(m_pTopFiles);

	m_pMainLayout->addWidget(m_pTopGroup, 0, 0, 4, 1);

	m_pDownloadGroup = new QGroupBoxEx<QVBoxLayout>(tr("Download Volume"));
	m_pDownloadVolume = new QPieChart();
	m_pDownloadVolume->AddPiece("NeoShare",Qt::gray,25);
	m_pDownloadVolume->AddPiece("BitTorrent",Qt::gray,25);
	m_pDownloadVolume->AddPiece("ed2k/eMule",Qt::gray,25);
	m_pDownloadVolume->AddPiece("Hosters",Qt::gray,25);
	m_pDownloadGroup->layout()->addWidget(m_pDownloadVolume);
	m_pDownloadGroup->setMaximumWidth(250);
	m_pDownloadGroup->setMaximumHeight(350);
	m_pDownloadGroup->setMinimumHeight(200);
	m_pMainLayout->addWidget(m_pDownloadGroup, 0, 1);

	m_pUploadGroup = new QGroupBoxEx<QVBoxLayout>(tr("Upload Volume"));
	m_pUploadVolume = new QPieChart();
	m_pUploadVolume->AddPiece("NeoShare",Qt::gray,25);
	m_pUploadVolume->AddPiece("BitTorrent",Qt::gray,25);
	m_pUploadVolume->AddPiece("ed2k/eMule",Qt::gray,25);
	m_pUploadVolume->AddPiece("Hosters",Qt::gray,25);
	m_pUploadGroup->layout()->addWidget(m_pUploadVolume);
	m_pUploadGroup->setMaximumWidth(250);
	m_pUploadGroup->setMaximumHeight(350);
	m_pUploadGroup->setMinimumHeight(200);
	m_pMainLayout->addWidget(m_pUploadGroup, 0, 2);

	m_pFWGroup = new QGroupBoxEx<QHBoxLayout>(tr("Firewall/NAT Status"));
	m_pFWGroup->layout()->setMargin(0);
	m_pFWGroup->layout()->setAlignment(Qt::AlignLeft);
	m_pFWIcon = new QLabel();
	m_pFWIcon->setPixmap(QPixmap(":/Icons/Shield-Blue"));
	m_pFWGroup->layout()->addWidget(m_pFWIcon);
	m_pFWText = new QLabel(tr("Firewall Status"));
	m_pFWGroup->layout()->addWidget(m_pFWText);
	m_pFWGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
	//m_pFWGroup->setMaximumHeight(50);
	m_pMainLayout->addWidget(m_pFWGroup, 1, 1, 1, 2);

	setLayout(m_pMainLayout);

	m_TimerId = startTimer(500);
}

CNeoSummary::~CNeoSummary()
{
	killTimer(m_TimerId);
}

class CStartTopSearchJob: public CInterfaceJob
{
public:
	CStartTopSearchJob(CNeoSummary* pView)
	{
		m_pView = pView;
		m_Request["SearchNet"] = "NeoKad";
		m_Request["Expression"] = "";
	}

	virtual QString			GetCommand()	{return "StartSearch";}
	virtual void			HandleResponse(const QVariantMap& Response) 
	{
		if(m_pView)
		{
			if(m_pView->m_SearchID)
				CSearchWindow::StopSearch(m_pView->m_SearchID);

			m_pView->m_SearchID = Response["ID"].toULongLong();
			if(!m_pView->m_SearchID) // we had an error try search again
				m_pView->m_LastSearch = 0;
			else
				m_pView->m_pTopFiles->SetID(m_pView->m_SearchID);
		}
	}

protected:
	QPointer<CNeoSummary> m_pView;
};

void CNeoSummary::timerEvent(QTimerEvent *e)
{
    if (e->timerId() != m_TimerId) 
	{
        QObject::timerEvent(e);
		return;
    }

	const QVariantMap& Data = theGUI->GetInfoData();
	QVariantMap Networks = Data["Networks"].toMap();
	QVariantMap NeoShare = Networks["NeoShare"].toMap();

	if((!m_LastSearch || GetCurTick() - m_LastSearch > HR2MS(24)) && NeoShare["KadStatus"].toString() == "Connected")
	{
		m_LastSearch = GetCurTick();

		CStartTopSearchJob* pStartSearchJob = new CStartTopSearchJob(this);
		theGUI->ScheduleJob(pStartSearchJob);
	}

	/////////////////////////////////////
	// Transfer Volume

	QVariantMap Bandwidth = Data["Bandwidth"].toMap();
	uint64 uTotalDownload = 0; //Bandwidth["DownloadedTotal"].toULongLong();
	uint64 uTotalUpload = 0; //Bandwidth["UploadedTotal"].toULongLong();

	uint64 uNeoDownload = 0;
	uint64 uNeoUpload = 0;
	if(Networks.contains("NeoShare"))
	{
		uNeoDownload = NeoShare["DownloadedTotal"].toULongLong();
		uTotalDownload += uNeoDownload;
		uNeoUpload = NeoShare["UploadedTotal"].toULongLong();
		uTotalUpload += uNeoUpload;
	}
	
	uint64 uTorrentDownload = 0;
	uint64 uTorrentUpload = 0;
	if(Networks.contains("BitTorrent"))
	{
		QVariantMap BitTorrent = Networks["BitTorrent"].toMap();
		uTorrentDownload = BitTorrent["DownloadedTotal"].toULongLong();
		uTotalDownload += uTorrentDownload;
		uTorrentUpload = BitTorrent["UploadedTotal"].toULongLong();	
		uTotalUpload += uTorrentUpload;
	}

	uint64 uMuleDownload = 0;
	uint64 uMuleUpload = 0;
	if(Networks.contains("Ed2kMule"))
	{
		QVariantMap Ed2kMule = Networks["Ed2kMule"].toMap();
		uMuleDownload = Ed2kMule["DownloadedTotal"].toULongLong();
		uTotalDownload += uMuleDownload;
		uMuleUpload = Ed2kMule["UploadedTotal"].toULongLong();
		uTotalUpload += uMuleUpload;
	}

	QVariantMap Hosters = Data["Hosters"].toMap();
	uint64 uHosterDownload = Hosters["DownloadedTotal"].toULongLong();
	uTotalDownload += uHosterDownload;
	uint64 uHosterUpload = Hosters["UploadedTotal"].toULongLong();
	uTotalUpload += uHosterUpload;

	uint64 uSummDownload = uNeoDownload + uTorrentDownload + uMuleDownload + uHosterDownload;
	uint64 uSummUpload = uNeoUpload + uTorrentUpload + uMuleUpload + uHosterUpload;
	if(uSummDownload > uTotalDownload)
		uTotalDownload = uSummDownload;
	if(uSummUpload > uTotalUpload)
		uTotalUpload = uSummUpload;

	if(uTotalDownload)
		m_pDownloadVolume->Reset();
	if(uTotalUpload)
		m_pUploadVolume->Reset();

	if(Networks.contains("NeoShare"))
	{
		if(uTotalDownload)
		{
			double dNeoDownload = 100.0 * uNeoDownload / uTotalDownload;
			m_pDownloadVolume->AddPiece("NeoShare " + FormatSize(uNeoDownload),Qt::blue,dNeoDownload);
		}
		if(uTotalUpload)
		{
			double dNeoUpload = 100.0 * uNeoUpload / uTotalUpload;
			m_pUploadVolume->AddPiece("NeoShare " + FormatSize(uNeoUpload),Qt::blue,dNeoUpload);
		}
	}

	if(Networks.contains("BitTorrent"))
	{
		if(uTotalDownload)
		{
			double dTorrentDownload = 100.0 * uTorrentDownload / uTotalDownload;
			m_pDownloadVolume->AddPiece("BitTorrent " + FormatSize(uTorrentDownload),Qt::green,dTorrentDownload);
		}
		if(uTotalUpload)
		{
			double dTorrentUpload = 100.0 * uTorrentUpload / uTotalUpload;
			m_pUploadVolume->AddPiece("BitTorrent " + FormatSize(uTorrentUpload),Qt::green,dTorrentUpload);
		}
	}

	if(Networks.contains("Ed2kMule"))
	{
		if(uTotalDownload)
		{
			double dMuleDownload = 100.0 * uMuleDownload / uTotalDownload;
			m_pDownloadVolume->AddPiece("ed2k/eMule " + FormatSize(uMuleDownload),Qt::red,dMuleDownload);
		}
		if(uTotalUpload)
		{
			double dMuleUpload = 100.0 * uMuleUpload / uTotalUpload;
			m_pUploadVolume->AddPiece("ed2k/eMule " + FormatSize(uMuleUpload),Qt::red,dMuleUpload);
		}
	}

	if(uTotalDownload)
	{
		double dHosterDownload = 100.0 * uHosterDownload / uTotalDownload;
		m_pDownloadVolume->AddPiece("Hosters " + FormatSize(uHosterDownload),Qt::yellow,dHosterDownload);
	}
	if(uTotalUpload)
	{
		double dHosterUpload = 100.0 * uHosterUpload / uTotalUpload;
		m_pUploadVolume->AddPiece("Hosters " + FormatSize(uHosterUpload),Qt::yellow,dHosterUpload);
	}

	m_pDownloadVolume->repaint();
	m_pUploadVolume->repaint();


	/////////////////////////////////////
	// Firewall Status

	QString FWStatus;
	int FWIcon = 1; // tip top

	if(Networks.contains("NeoShare"))
	{
		FWStatus += tr("NeoShare: ");
		if(NeoShare["Port"].toInt() == 0)
		{
			FWStatus += tr("Port ERROR !!!");
			if(FWIcon < 2) FWIcon = 4;
		}
		else if(!NeoShare["Firewalled"].toBool())
			FWStatus += tr("Ok");
		else if(NeoShare["NATed"].toBool())
		{
			FWStatus += tr("Firewalled with NAT Open");
			if(FWIcon < 2) FWIcon = 2;
		}
		else
		{
			FWStatus += tr("Firewalled");
			if(FWIcon < 3) FWIcon = 3;
		}
		FWStatus += "\r\n";
	}

	if(Networks.contains("BitTorrent"))
	{
		QVariantMap BitTorrent = Networks["BitTorrent"].toMap();
		FWStatus += tr("BitTorrent: ");
		if(BitTorrent["Port"].toInt() == 0)
		{
			FWStatus += tr("Port ERROR !!!");
			if(FWIcon < 2) FWIcon = 4;
		}
		else if(!BitTorrent["Firewalled"].toBool())
			FWStatus += tr("Ok");
		else if(BitTorrent["NATed"].toBool())
		{
			FWStatus += tr("Firewalled with NAT Open");
			if(FWIcon < 2) FWIcon = 2;
		}
		else
		{
			FWStatus += tr("Firewalled");
			if(FWIcon < 3) FWIcon = 3;
		}
		FWStatus += "\r\n";
	}

	if(Networks.contains("Ed2kMule"))
	{
		QVariantMap Ed2kMule = Networks["Ed2kMule"].toMap();
		QVariantMap BitTorrent = Networks["BitTorrent"].toMap();
		FWStatus += tr("ed2kMule: ");
		if(Ed2kMule["TCPPort"].toInt() == 0)
		{
			FWStatus += tr("Port ERROR !!!");
			if(FWIcon < 2) FWIcon = 4;
		}
		else if(!Ed2kMule["Firewalled"].toBool())
			FWStatus += tr("Ok");
		else if(Ed2kMule["NATed"].toBool())
		{
			FWStatus += tr("Firewalled with NAT Open");
			if(FWIcon < 2) FWIcon = 2;
		}
		else
		{
			FWStatus += tr("Firewalled");
			if(FWIcon < 3) FWIcon = 3;
		}
		FWStatus += "\r\n";
	}

	switch(FWIcon)
	{
	case 1: m_pFWIcon->setPixmap(QPixmap(":/Icons/Shield-Green")); break;
	case 2: m_pFWIcon->setPixmap(QPixmap(":/Icons/Shield-Yelow")); break;
	case 3: m_pFWIcon->setPixmap(QPixmap(":/Icons/Shield-Red")); break;
	case 4: m_pFWIcon->setPixmap(QPixmap(":/Icons/Shield-Magenta")); break;
	}
	m_pFWText->setText(FWStatus);
}