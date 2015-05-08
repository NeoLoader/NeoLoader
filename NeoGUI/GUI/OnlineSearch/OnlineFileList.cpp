#include "GlobalHeader.h"
#include "OnlineFileList.h"
#include "../../NeoGUI.h"
#include "../../../qjson/src/parser.h"
#include "../CoverView.h"
#include "../../Common/Common.h"

COnlineFileList::COnlineFileList(QWidget *parent)
:QWidget(parent)
{
	m_pNet = new QNetworkAccessManager();
	connect(m_pNet, SIGNAL(finished(QNetworkReply*)), this, SLOT(OnFinished(QNetworkReply*)));

	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);

	m_pSplitter = new QSplitter();
	m_pSplitter->setOrientation(Qt::Vertical);

	m_pFileTree = new QTreeWidget();

	m_pSplitter->addWidget(m_pFileTree);
	
	//connect(m_pFileTree, SIGNAL(itemClicked(QTreeWidgetItem*, int)), this, SLOT(OnItemClicked(QTreeWidgetItem*, int)));
	connect(m_pFileTree, SIGNAL(itemSelectionChanged()), this, SLOT(OnSelectionChanged()));
	connect(m_pFileTree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(OnItemDoubleClicked(QTreeWidgetItem*, int)));
#ifdef WIN32
	m_pFileTree->setStyle(QStyleFactory::create("windowsxp"));
#endif
	//m_pFileTree->setSortingEnabled(true);

	m_pFileTree->setHeaderLabels(tr("FileName|Size|Hosters").split("|"));
	//m_pFileTree->setSelectionMode(QAbstractItemView::ExtendedSelection);

	m_pFileTree->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pFileTree, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenuRequested(const QPoint &)));

	m_pMenu = new QMenu();

	m_pGrab = new QAction(tr("Download this File"), m_pMenu);
	connect(m_pGrab, SIGNAL(triggered()), this, SLOT(OnGrab()));
	m_pMenu->addAction(m_pGrab);
	m_pWebPage = new QAction(tr("Open Website"), m_pMenu);
	connect(m_pWebPage, SIGNAL(triggered()), this, SLOT(OnWebPage()));
	m_pMenu->addAction(m_pWebPage);
	m_pSrcPage = new QAction(tr("Open Source"), m_pMenu);
	connect(m_pSrcPage, SIGNAL(triggered()), this, SLOT(OnSrcPage()));
	m_pMenu->addAction(m_pSrcPage);

	m_pDetailWidget = new QWidget();
	m_pDetailLayout = new QFormLayout();

	m_pFileName = new QLineEdit();
	m_pFileName->setReadOnly(true);
	m_pFileName->setMaximumWidth(400);
	m_pDetailLayout->setWidget(0, QFormLayout::LabelRole, new QLabel(tr("FileName:")));
	m_pDetailLayout->setWidget(0, QFormLayout::FieldRole, m_pFileName);

	m_pDescription = new QTextEdit();
	m_pDescription->setMaximumWidth(400);
	m_pDetailLayout->setWidget(1, QFormLayout::LabelRole, new QLabel(tr("Description:")));
	m_pDetailLayout->setWidget(1, QFormLayout::FieldRole, m_pDescription);

	m_pDetailWidget->setLayout(m_pDetailLayout);

	m_pSummary = new QWidget();
	m_pSummaryLayout = new QHBoxLayout();

	m_pSummaryLayout->addWidget(m_pDetailWidget);

	m_pCoverView = new CCoverView();
	m_pSummaryLayout->addWidget(m_pCoverView);

	m_pSummary->setLayout(m_pSummaryLayout);

	m_pTransferTree = new QTreeWidget();
	m_pTransferTree->setHeaderLabels(tr("URL").split("|"));


	m_pFileTabs = new QTabWidget();
	m_pFileTabs->addTab(m_pSummary, tr("Summary"));
	m_pFileTabs->addTab(m_pTransferTree, tr("Links"));

	m_pSplitter->addWidget(m_pFileTabs);

	m_pMainLayout->addWidget(m_pSplitter);
	setLayout(m_pMainLayout);
}

void COnlineFileList::SearchOnline(const QString& Expression, const QVariantMap& Criteria)
{
	QNetworkRequest Resuest(QString("http://downloadstube.net/api/search.php?q=%1").arg(Expression));
	m_pNet->get(Resuest);
}

class CFileAddJob: public CInterfaceJob
{
public:
	CFileAddJob(const QString& FileName, const QString& Description, const QString& CoverUrl, const QStringList& Links)
	{
		m_Request["FileName"] = FileName;

		QVariantMap Properties;
		if(!Description.isEmpty())
			Properties["Description"] = Description;
		if(!CoverUrl.isEmpty())
			Properties["CoverUrl"] = CoverUrl;
		Properties["GrabName"] = true;
		m_Request["Properties"] = Properties;

		m_Request["Links"] = Links;
	}

	virtual QString			GetCommand()	{return "AddFile";}
	virtual void			HandleResponse(const QVariantMap& Response) {}
};

class CHosterIconJob2: public CInterfaceJob
{
public:
	CHosterIconJob2(QLabel* pView, const QString& Hoster)
	{
		m_pView = pView;
		m_Request["Action"] = "LoadIcon";
		m_Request["HostName"] = Hoster;
	}

	virtual QString			GetCommand()	{return "HosterAction";}
	virtual void			HandleResponse(const QVariantMap& Response)
	{
		if(m_pView)
		{
			QByteArray IconData = Response["Icon"].toByteArray();

			QPixmap Pixmap;
			if(!IconData.isEmpty())
				Pixmap.loadFromData(IconData);
			else
				Pixmap.load(":/Icons/Unknown");

			m_pView->setPixmap(Pixmap.scaled(16,16));
		}
	}

protected:
	QPointer<QLabel>m_pView; // Note: this can be deleted at any time
};

QLabel* NewHosterIcon(const QString& Hoster)
{
	QLabel* pLabel = new QLabel();
	pLabel->setMaximumHeight(16);
	CHosterIconJob2* pHosterIconJob2 = new CHosterIconJob2(pLabel, Hoster);
	theGUI->ScheduleJob(pHosterIconJob2);
	return pLabel;
}

void COnlineFileList::OnFinished(QNetworkReply* pReply)
{
	QString Path = pReply->url().path();
	QJson::Parser json;
	bool ok;
	QVariantMap Response = json.parse (pReply->readAll(), &ok).toMap();
	pReply->deleteLater();
	if(!ok)
		return;

	QString API = Split2(Path,"/",true).second;
	if(API == "search.php")
	{
		QList<QTreeWidgetItem*> NewItems;
		QMap<QTreeWidgetItem*, QWidget*> Hosters;
		foreach(const QVariant& vFile, Response["data"].toList())
		{
			QVariantMap File = vFile.toMap();
		
			QTreeWidgetItem* pItem = new QTreeWidgetItem();
			pItem->setData(0, Qt::UserRole, File);
			pItem->setText(0, File["name"].toString());
			uint64 uSize = File["file_size"].toULongLong();
			pItem->setText(1, uSize ? FormatSize(uSize*1024) : "-");
			NewItems.append(pItem);

			QWidget* pHosters = new QWidget();
			pHosters->setMaximumHeight(16);
			QHBoxLayout* pLayout = new QHBoxLayout(pHosters);
			pLayout->setMargin(0);
			pLayout->setAlignment(Qt::AlignLeft);
			foreach(const QString Hoster, File["hoster"].toString().split("|"))
				pLayout->addWidget(NewHosterIcon(Hoster));
			pHosters->setLayout(pLayout);
			Hosters.insert(pItem, pHosters);
		}
		m_pFileTree->addTopLevelItems(NewItems);
		foreach(QTreeWidgetItem* pItem, NewItems)
			m_pFileTree->setItemWidget(pItem, 2, Hosters[pItem]);
	}
	else if(API == "download.php")
	{
		QVariantMap File = Response["data"].toMap();

		QStringList Links;
		foreach(const QString& sLink, File["link"].toString().split("\n"))
		{
			QString Link = sLink.trimmed();
			if(Link.isEmpty())
				continue;
			Links.append(Link);
		}

		if(m_Downloads.contains(File["id"].toString()))
		{
			QString FileName = m_Downloads.take(File["id"].toString());
			if(File["state"].toString() == "download")
			{
				CFileAddJob* pFileAddJob = new CFileAddJob(FileName, File["description"].toString(), File["cover"].toString(), Links);
				theGUI->ScheduleJob(pFileAddJob);
			}
			else
			{
				OnWebPage();
				static bool bMsg = false;
				if(bMsg == false)
				{
					bMsg = true;
					QMessageBox::information(NULL,tr("Go To Grabber"), tr("Get the links from the website and enter them into the grabber window"));
				}
			}
		}
		else
		{
			m_pDescription->setText(File["description"].toString());

			m_pCoverView->ShowCover(0, File["cover"].toString());

			QList<QTreeWidgetItem*> NewItems;
			foreach(const QString& Link, Links)
			{
				QTreeWidgetItem* pItem = new QTreeWidgetItem();
				pItem->setText(0, Link);
				NewItems.append(pItem);
			}
			m_pTransferTree->addTopLevelItems(NewItems);
		}
	}
}

void COnlineFileList::OnMenuRequested(const QPoint &point)
{
	m_pMenu->popup(QCursor::pos());	
}

void COnlineFileList::OnItemClicked(QTreeWidgetItem* pItem, int Column)
{
	QVariantMap File = pItem->data(0, Qt::UserRole).toMap();

	m_pFileName->setText(File["name"].toString());
	m_pTransferTree->clear();

	QNetworkRequest Resuest(QString("http://downloadstube.net/api/download.php?id=%1").arg(File["id"].toString()));
	m_pNet->get(Resuest);
}

void COnlineFileList::OnSelectionChanged()
{
	if(QTreeWidgetItem* pItem = m_pFileTree->currentItem())
		OnItemClicked(pItem, 0);
}

void COnlineFileList::OnItemDoubleClicked(QTreeWidgetItem* pItem, int Column)
{
	QVariantMap File = pItem->data(0, Qt::UserRole).toMap();
	m_Downloads.insert(File["id"].toString(), File["name"].toString());

	QBrush Brush(Qt::blue);
	for(int i=0; i < pItem->columnCount(); i++)
		pItem->setForeground(i, Brush);

	QNetworkRequest Resuest(QString("http://downloadstube.net/api/download.php?id=%1").arg(File["id"].toString()));
	m_pNet->get(Resuest);
}

void COnlineFileList::OnGrab()
{
	if(QTreeWidgetItem* pItem = m_pFileTree->currentItem())
		OnItemDoubleClicked(pItem, 0);
}

void COnlineFileList::OnWebPage()
{
	if(QTreeWidgetItem* pItem = m_pFileTree->currentItem())
	{
		QVariantMap File = pItem->data(0, Qt::UserRole).toMap();
		QDesktopServices::openUrl(QUrl(QByteArray::fromPercentEncoding(File["web_page"].toByteArray())));
	}
}

void COnlineFileList::OnSrcPage()
{
	if(QTreeWidgetItem* pItem = m_pFileTree->currentItem())
	{
		QVariantMap File = pItem->data(0, Qt::UserRole).toMap();
		QDesktopServices::openUrl(QUrl(QByteArray::fromPercentEncoding(File["source"].toByteArray())));
	}
}
