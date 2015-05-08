#include "GlobalHeader.h"
#include "LinkDialog.h"
#include "../NeoGUI.h"

CLinkDialog::CLinkDialog(QList<uint64> FileIDs, QWidget *parent)
	: QDialogEx(parent)
{
	m_FileIDs = FileIDs;

	m_pMainLayout = new QVBoxLayout(this);
	m_pMainLayout->setMargin(0);

	m_pLinkWidget = new QWidget();
	m_pLinkLayout = new QGridLayout();
	m_pLinkLayout->setMargin(3);

	m_pAddArc = new QCheckBox(tr("Add Archives"));
	m_pAddArc->setTristate();
	m_pLinkLayout->addWidget(m_pAddArc, 0, 0);
	connect(m_pAddArc, SIGNAL(stateChanged(int)), this, SLOT(OnAddArc(int)));

	m_pEncoding = new QComboBox();
	//m_pEncoding->addItem(tr("Plaintext"));
	//m_pEncoding->addItem(tr("Compressed"));
	//m_pEncoding->addItem(tr("Encrypted"));
	m_pEncoding->addItem(tr("NeoLink"));
	m_pEncoding->addItem(tr("Magnet"));
	m_pEncoding->addItem(tr("ed2k/eMule"));
	m_pLinkLayout->addWidget(m_pEncoding, 0, 1);
	connect(m_pEncoding, SIGNAL(activated(int)), this, SLOT(OnEncoding(int)));

	m_pGetBtn = new QToolButton();
	m_pGetBtn->setText(tr("Copy Link"));
	connect(m_pGetBtn, SIGNAL(released()), this, SLOT(OnGet()));

	m_pGetMenu = new QMenu();

	m_pGetTorent = m_pGetMenu->addAction("Get Torrent", this, SLOT(OnGetTorrent()));
	m_pGetColection = m_pGetMenu->addAction("Get eMuleCollection", this, SLOT(OnGetColection()));

	m_pGetBtn->setPopupMode(QToolButton::MenuButtonPopup);
	m_pGetBtn->setMenu(m_pGetMenu);

	m_pLinkLayout->addWidget(m_pGetBtn, 0, 3);

	m_pLinks = new QTextEdit();
	m_pLinks->setReadOnly(true);
	m_pLinkLayout->addWidget(m_pLinks, 1, 0, 1, 10);

	m_pLinkWidget->setLayout(m_pLinkLayout);

	m_pMainLayout->addWidget(m_pLinkWidget);
	setLayout(m_pMainLayout);

	OnMakeLinks();
}

class CMakeLinkJob: public CInterfaceJob
{
public:
	CMakeLinkJob(CLinkDialog* pView, uint64 ID, const QString& Encoding, const QString& Links)
	{
		m_pView = pView;
		m_Request["ID"] = ID;
		m_Request["Encoding"] = Encoding;
		m_Request["Links"] = Links;
	}

	virtual QString			GetCommand()	{return "MakeLink";}
	virtual void			HandleResponse(const QVariantMap& Response) 
	{
		if(Response.contains("FileData"))
		{
			QString FilePath = QFileDialog::getSaveFileName(0, tr("Save File"), Response["FileName"].toString(), QString("Any File (*.torrent)"));
			if(!FilePath.isEmpty())
			{
				QFile File(FilePath);
				if (File.open(QIODevice::ReadWrite))
					File.write(Response["FileData"].toByteArray());
			}
		}
		else if(Response.contains("Link"))
		{
			if(m_pView)
			{
				m_pView->m_pLinks->append(Response["Link"].toString() + "\r\n");
				m_pView->m_pGetTorent->setEnabled(Response["Torrents"].toInt() > 0);
				bool bMulti = Response["Type"] == "MultiFile";
				m_pView->m_pGetColection->setEnabled(bMulti);
				
				QModelIndex Index = m_pView->m_pEncoding->model()->index(3, 0); 
				m_pView->m_pEncoding->model()->setData(Index, QVariant(bMulti ? 0 : 1 | 32), Qt::UserRole - 1);
			}
			else
				QApplication::clipboard()->setText(QApplication::clipboard()->text() + Response["Link"].toString() + "\r\n");
		}
	}

protected:
	QPointer<CLinkDialog>	m_pView; // Note: this can be deleted at any time
};

void CLinkDialog::CopyLinks(QList<uint64> FileIDs, EModes Mode)
{
	QString Encoding;
	switch(Mode)
	{
	case eMagnet:		Encoding = "Magnet"; break;
	case eNeo:			Encoding = "Plaintext"; break;
	//case eCompressed:	Encoding = "Compressed"; break;
	//case eEncrypted:	Encoding = "Encrypted"; break;
	case ed2k:			Encoding = "ed2k"; break;
	case eTorrent:		Encoding = "Torrent"; break;
	}

	if(Mode != eTorrent)
		QApplication::clipboard()->setText("");

	foreach(uint64 ID, FileIDs)
	{
		CMakeLinkJob* pMakeLinkJob = new CMakeLinkJob(NULL, ID, Encoding, "None");
		theGUI->ScheduleJob(pMakeLinkJob);
	}
}

void CLinkDialog::OnMakeLinks()
{
	m_pLinks->clear();

	QString Links = m_pAddArc->isChecked() ? "Archive" : "None";
	QString Encoding;
	switch(m_pEncoding->currentIndex())
	{
	case eMagnet:		Encoding = "Magnet"; break;
    case eNeo:			Encoding = (m_pAddArc->checkState() == Qt::PartiallyChecked) ? "Compressed" : "Plaintext"; break;
	//case eCompressed:	Encoding = "Compressed"; break;
	//case eEncrypted:	Encoding = "Encrypted"; break;
	case ed2k:			Encoding = "ed2k"; break;
	}

	foreach(uint64 ID, m_FileIDs)
	{
		CMakeLinkJob* pMakeLinkJob = new CMakeLinkJob(this, ID, Encoding, Links);
		theGUI->ScheduleJob(pMakeLinkJob);
	}
}

void CLinkDialog::OnGet()
{
	QApplication::clipboard()->setText(m_pLinks->toPlainText());
}

void CLinkDialog::OnGetTorrent()
{
	foreach(uint64 ID, m_FileIDs)
	{
		CMakeLinkJob* pMakeLinkJob = new CMakeLinkJob(this, ID, "Torrent", "None");
		theGUI->ScheduleJob(pMakeLinkJob);
	}
}

void CLinkDialog::OnGetColection()
{
	foreach(uint64 ID, m_FileIDs)
	{
		CMakeLinkJob* pMakeLinkJob = new CMakeLinkJob(this, ID, "eMuleCollection", "None");
		theGUI->ScheduleJob(pMakeLinkJob);
	}
}
