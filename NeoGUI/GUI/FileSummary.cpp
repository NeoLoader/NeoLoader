#include "GlobalHeader.h"
#include "FileSummary.h"
#include "FileListView.h"
#include "CoverView.h"
#include "../NeoGUI.h"
#include "FileListView.h"
#include "ServicesWidget.h"
#include "StatisticsWindow.h"

CFileSummary::CFileSummary(UINT Mode, QWidget *parent)
:QWidget(parent)
{
	m_ID = 0;
	m_Mode = Mode;

	m_pMainLayout = new QHBoxLayout();
	m_pMainLayout->setMargin(3);

	m_pScrollArea = new QScrollArea();

	m_pBoxWidget = new QResizeWidget();
	m_pBoxLayout = new QVBoxLayout();
	m_pBoxLayout->setMargin(0);
	m_pBoxLayout->setAlignment(Qt::AlignTop);

	AddEntry("Welcome to NeoLoader", ":/Icons/Generic/shield1.png");

	m_pBoxWidget->setLayout(m_pBoxLayout);
	
	m_pScrollArea->setWidget(m_pBoxWidget);
	m_pScrollArea->setWidgetResizable(true);

	m_pMainLayout->addWidget(m_pScrollArea);
	setLayout(m_pMainLayout);
}

int CFileSummary::GetFitness(const QVariantMap& File)
{
	QStringList HashMap = File["Hashes"].toStringList(); // this is for filelist format

	const QVariantMap& Data = theGUI->GetInfoData();
	QVariantMap Networks = Data["Networks"].toMap();

	int Fitness = 0;
	if(HashMap.contains("neo", Qt::CaseInsensitive) || HashMap.contains("neox", Qt::CaseInsensitive))
		Fitness = 4;
	else if(HashMap.contains("btih", Qt::CaseInsensitive) && Networks.contains("BitTorrent"))
		Fitness = 3;
	else if(HashMap.contains("arch", Qt::CaseInsensitive))
	{
		const QStringList& PremHosters = g_Services->GetHosters();

		bool bPremFound = false;
		QVariantMap HosterShare = File["HosterShare"].toMap();
		foreach(const QString& Hoster, HosterShare.keys())
		{
			if(HosterShare.value(Hoster).toDouble() >= 1)
			{
				if(PremHosters.contains(Hoster))
				{
					bPremFound = true;
					break;
				}
			}
		}

		Fitness = bPremFound ? 3 : 2;
	}
	else if(HashMap.contains("ed2k", Qt::CaseInsensitive) && Networks.contains("Ed2kMule"))
		Fitness = 1;

	int Transfers = File["CheckedTransfers"].toInt();
	double Health = File["Availability"].toDouble() / (Transfers ? Transfers : 20);
	//double Health = File["SeedTransfers"].toDouble() / Transfers : 0; // % of seeds

	int Bars = 0;
	if(Health > 0.75)
		Bars = 4;
	else if(Health > 0.50)
		Bars = 3;
	else if(Health > 0.25)
		Bars = 2;
	else if(Health > 0.0)
		Bars = 1;

	int Color = Fitness >= 3 ? 3 : Fitness;
	if(Bars > 3 && Fitness < 4)
		Bars = 3;
	return Bars | (Color << 8);
}

class CSummarySyncJob: public CInterfaceJob
{
public:
	CSummarySyncJob(CFileSummary* pView, uint64 ID)
	{
		m_pView = pView;
		m_Request["ID"] = ID;
		//m_Request["Section"] = "Details";
	}

	virtual QString			GetCommand()	{return "GetFile";}
	virtual void			HandleResponse(const QVariantMap& Response) 
	{
		if(m_pView)
		{
			if(m_pView->m_ID != Response["ID"].toULongLong())
				return;

			bool bEd2k = Response["Properties"].toMap()["Ed2kShare"].toBool();
			bool bTorrent = Response["Properties"].toMap()["Torrent"].toBool();
			QString FileStatus = Response["FileStatus"].toString();
			QString FileState = Response["FileState"].toString();

			if(FileStatus == "Error")
			{
				QString Error = Response["Error"].toString();
				QString Text = "<p>" + tr("File %1 experienced an error: %2").arg(Response["FileName"].toString()).arg(Error) + "</p>";
				if(Error.contains("Inconsistent Archive"))
				{
					Text += "<p>" + tr("This means the hoster links assotiated with it probably belong to more than one disctinct file. ");
					Text += tr("To resolve the issue use <a href=\"#DoInspectLinks\">Inspect Links</a> command in the files context menu.") + "</p>";
				}
				else
					Text += "<p>" + tr("In order to clear the error click <a href=\"#DoStart\">Start</a> in the files context menu.") + "</p>";
				m_pView->AddEntry(Text, ":/Icons/Generic/error5.png");
			}
			else
			{
				QString Text = "<p>" + tr("File %1 ").arg(Response["FileName"].toString());
				QString Icon;

			
				if(FileState == "Removed")
					Text += tr(" is removed from disk.");
				else if(FileState == "Paused")
					Text += tr(" is paused.");
				else if(FileState == "Started")
					Text += tr(" is started.");
				else if(FileState == "Stopped")
					Text += tr(" is stopped.");
				else if(FileState == "Pending")
					Text += tr(" is not added to download list.");
				else if(FileState == "Duplicate")
					Text += tr(" is a duplicate andwill be ignored.");
				else
					Text += tr(" is in an unknown state.");
				Text += "</p>";

				
				if((FileState == "Paused" || FileState == "Started") && FileStatus == "Empty")
				{
					Text += "<p>" + tr("File is being downloaded, but hasnt obtained metadata yet.") + "</p>";
					Icon = ":/Icons/Generic/exclamation1.png";
				}
				if(FileState == "Pending")
				{
					Text += "<p>" + tr("To download the file it must be added to downloads.") + "</p>";
					Icon = ":/Icons/Generic/exclamation2.png";
				}
				if(FileState != "Removed")
				{
					if(FileStatus != "Complete")
						Text += "<p>" + tr("File is being downloaded.") + "</p>";
					else
						Text += "<p>" + tr("File is being shared.") + "</p>";
				}

				/*if(States[1].compare("Packing", Qt::CaseInsensitive) == 0)

				if(States[1].compare("Hashing", Qt::CaseInsensitive) == 0)*/
				
				m_pView->AddEntry(Text, Icon);
			}

			const QStringList& AllHosters = g_Services->GetHosters(true);
			const QStringList& PremHosters = g_Services->GetHosters();
			const QStringList& BestHosters = g_Services->GetBestHosters();

			if((bEd2k || bTorrent) && AllHosters.count() < 3)
			{
				QString Text = "<p>" + tr("You are sharing using P2P technology. You can accelerate the file transfers by enabling Hoster based part caching.") + "</p>";
				Text += "<p>" + tr("To enable Hoster cache you must setup a few Hoster accounts.");
				Text += tr("For obtimal operation it is recommended to have a few accounts set up, of which at least one premium. ");
				Text += tr("You can get new accounts from one of the recommended hosters: ");
				foreach(const QString& Hoster, BestHosters)
				{
					if(!AllHosters.contains(Hoster))
						Text += QString("<a href=\"#Buy:%1\">%1</a>, ").arg(Hoster);
				}
				Text += tr("or you can <a href=\"#AddAcc\">add already existing accounts</a>.") + "</p>";

				m_pView->AddEntry(Text, ":/Icons/Generic/exclamation2.png");
			}

			if(FileStatus != "Complete")
			{
				QVariantMap HosterShare = Response["HosterShare"].toMap();

				QVariantList HashMap = Response["HashMap"].toList();
				bool bBtih = false;
				bool bNeo = false;
				foreach(const QVariant& vHash, HashMap)
				{
					QVariantMap Hash = vHash.toMap();
					if(Hash["Type"] == "btih")
						bBtih = true;
					if(Hash["Type"] == "ed2k")
						bEd2k = true;
				}

				const QVariantMap& Data = theGUI->GetInfoData();
				QVariantMap Networks = Data["Networks"].toMap();
				//bool bNeoOk = HashMap.contains("neo") || HashMap.contains("xneo");
				bool bHosterOk = HosterShare.count(); // (HashMap.contains("arch") || (bNeoOk && ... );
				bool bTorrentOk = bBtih && Networks.contains("BitTorrent");
				bool bMuleOk = bEd2k && Networks.contains("Ed2kMule");

				if(!bTorrentOk && !bMuleOk && !bHosterOk)
				{
					QString Text = "<p>" + tr("The file does not have any sources available, and therefor can NOT be downloaded!\r\n");
					if(bBtih)
						Text += tr("A Bittorent Info Hash is available, to download the file Torrent support must be enabled in settings.");
					if(bEd2k)
						Text += tr("A ed2k/eMule Hash is available, to download the file ed2k/eMule support must be enabled in settings.");
					Text += "</p>";

					m_pView->AddEntry(Text, ":/Icons/Generic/exclamation5.png");
				}

				if(bMuleOk && !bEd2k)
				{
					QString Text = "<p>" + tr("The File can be downloaded from the ed2k network, but this is currently disabled. ");
					Text += tr("In order to enable ed2k sharing click 'ShareEd2k' in the files context menu.") + "</p>";

					m_pView->AddEntry(Text, ":/Icons/Generic/exclamation1.png");
				}

				if(bTorrentOk && !bTorrent)
				{
					QString Text = "<p>" + tr("The File can be downloaded from the BitTorrent network, a info hash is available, but the file doesnt have a torrent yet. ");
					Text += tr("In order to add a torrent fille check 'Torrent' in the files context menu.") + "</p>";

					m_pView->AddEntry(Text, ":/Icons/Generic/exclamation1.png");
				}

				if(HosterShare.count() > 0)
				{
					bool bPremFound = false;
					QStringList FoundFull;
					foreach(const QString& Hoster, HosterShare.keys())
					{
						if(PremHosters.contains(Hoster))
							bPremFound = true;

						if(HosterShare.value(Hoster).toDouble() >= 1)
						{
							if(BestHosters.contains(Hoster))
								FoundFull.prepend(Hoster);
							else
								FoundFull.append(Hoster);
						}
					}

					if(!FoundFull.isEmpty())
					{
						QString Text = "<p>" + tr("The file can be downloaded with premium speed from web hosters.") + "</p>";
						if(!bPremFound)
						{
							Text += tr("However a required premium account is not available, you must add an account for one of the following hosters: ");
							foreach(const QString& Hoster, FoundFull)
								Text += QString("<a href=\"#Buy:%1\">%1</a>, ").arg(Hoster);
							Text += tr(" if you already have a premium account you can <a href=\"#AddAcc\">add it</a> and start downloading right away.") + "</p>";

							m_pView->AddEntry(Text, ":/Icons/Generic/exclamation3.png");
						}
						else
							m_pView->AddEntry(Text, ":/Icons/Generic/shield2.png");
					}
					else if(bEd2k || bTorrent)
					{
						QString Text = "<p>" + tr("Parts of the file are available in the Hoster Cache for download at full speed.") + "</p>";
						if(!bPremFound)
						{
							Text += tr("However a required premium account is not available, you must add an account for one of the following hosters: ");
							foreach(const QString& Hoster, HosterShare.keys())
								Text += QString("<a href=\"#Buy:%1\">%1</a> (%2%), ").arg(Hoster).arg((int)(HosterShare.value(Hoster).toDouble()*100));
							Text += tr(" if you already have a premium account you can <a href=\"#AddAcc\">add it</a> and start downloading right away.") + "</p>";

							m_pView->AddEntry(Text, ":/Icons/Generic/exclamation3.png");
						}
						else
							m_pView->AddEntry(Text, ":/Icons/Generic/shield2.png");
					}
				}
			}

			QTimer::singleShot(0,m_pView->m_pBoxWidget,SLOT(ResizeWidgets()));
		}
	}
protected:
	QPointer<CFileSummary>	m_pView; // Note: this can be deleted at any time
};

void CFileSummary::ShowSummary(uint64 ID)
{
	m_ID = ID;

	foreach(QWidget* pWidget, m_Widgets)
		delete pWidget;
	m_Widgets.clear();

	if(m_ID != 0)
	{
		CSummarySyncJob* pSummarySyncJob = new CSummarySyncJob(this, m_ID);
		theGUI->ScheduleJob(pSummarySyncJob);
	}
}

void CFileSummary::AddEntry(const QString& Text, const QString& Icon, int Size)
{
	QTextBrowser* pEdit = new QTextBrowser();
	QString Color = m_pBoxWidget->palette().color(QPalette::Background).name();
	pEdit->setHtml("<html><body style=\"background-color:" + Color + "\">" +
 	(Icon.isEmpty() ? "" : ("<img src=\"" + Icon + "\" style=\"float: left; margin: 10px;\" " + (Size ? QString("height=\"%1\" ").arg(Size) : "") + "> ")) + 
	 Text + "</body></html>");
	//pEdit->setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred));
	//pEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	pEdit->setFrameShape(QFrame::NoFrame);
	pEdit->setContextMenuPolicy(Qt::NoContextMenu);

	connect(pEdit,SIGNAL(anchorClicked (const QUrl&)),this,SLOT(OnAnchorClicked(const QUrl&)));
	m_pBoxLayout->addWidget(pEdit);
	m_Widgets.append(pEdit);
}

void CFileSummary::OnAnchorClicked(const QUrl& Url)
{
	QString Op = Url.fragment();
	if(Op.left(2) == "Do")
		CFileListView::DoAction(Op.mid(2), m_ID);
	else if(Op == "AddAcc")
		CNewAccount::AddAccount();
	else if(Op.left(3) == "Buy")
		CServicesWidget::BuyAccount(Op.mid(4));
}
