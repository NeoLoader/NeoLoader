#include "GlobalHeader.h"
#include "CoverView.h"
#include "../NeoGUI.h"

CCoverView::CCoverView(QWidget *parent)
:QGroupBox(tr("Cover"), parent)
{
	m_ID = 0;

	m_pNet = new QNetworkAccessManager();
	connect(m_pNet, SIGNAL(finished(QNetworkReply*)), this, SLOT(OnFinished(QNetworkReply*)));

	setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenuRequested(const QPoint &)));

	m_pMenu = new QMenu(this);
	m_pSetCover = new QAction(tr("Set Cover URL"), m_pMenu);
	connect(m_pSetCover, SIGNAL(triggered()), this, SLOT(OnSetCover()));
	m_pMenu->addAction(m_pSetCover);


	m_pMainLayout = new QVBoxLayout();

	m_pCover = new QLabel();
	m_pCover->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));

	m_pMainLayout->addWidget(m_pCover);
	setLayout(m_pMainLayout);
}

void CCoverView::ShowCover(uint64 ID, const QString& Url)
{
	m_ID = ID;
	m_Url = Url;

	if(Url.isEmpty())
	{
		m_pCover->clear();
		m_Pixmap = QPixmap();
	}
	else
	{
		QNetworkRequest Resuest(m_Url);
		m_pNet->get(Resuest);
	}
}

void CCoverView::OnFinished(QNetworkReply* pReply)
{
	m_Pixmap.loadFromData(pReply->readAll());
	pReply->deleteLater();
	if(m_pCover->width() >= m_Pixmap.width() && m_pCover->height() >= m_Pixmap.height())
		m_pCover->setPixmap(m_Pixmap);
	else
		m_pCover->setPixmap(m_Pixmap.scaled(m_pCover->width(), m_pCover->height(), Qt::KeepAspectRatio));
	m_pCover->setMinimumHeight(32);
	m_pCover->setMinimumWidth(32);
}

void CCoverView::resizeEvent(QResizeEvent* e)
{
	QGroupBox::resizeEvent(e);

	if(!m_Pixmap.isNull())
	{
		if(m_pCover->width() >= m_Pixmap.width() && m_pCover->height() >= m_Pixmap.height())
			m_pCover->setPixmap(m_Pixmap);
		else
			m_pCover->setPixmap(m_Pixmap.scaled(m_pCover->width(), m_pCover->height(), Qt::KeepAspectRatio));
	}
	m_pCover->setMinimumHeight(32);
	m_pCover->setMinimumWidth(32);
}

void CCoverView::OnMenuRequested(const QPoint &point)
{
	if(m_ID != 0)
		m_pMenu->popup(QCursor::pos());	
}

class CSetCoverJob: public CInterfaceJob
{
public:
	CSetCoverJob(uint64 ID, const QString& Url)
	{
		m_Request["ID"] = ID;
		//m_Request["Section"] = "Details";

		QVariantMap Properties;
		Properties["CoverUrl"] = Url;
		m_Request["Properties"] = Properties;
	}

	virtual QString			GetCommand()	{return "SetFile";}
	virtual void			HandleResponse(const QVariantMap& Response) {}
};

void CCoverView::OnSetCover()
{
	bool bOk = false;
	QString Url = QInputDialog::getText(this, tr("Cover URL"), tr("Enter the URL to a cover image"), QLineEdit::Normal, m_Url, &bOk);
	if(!bOk)
		return;

	m_Url = Url;

	CSetCoverJob* pSetCoverJob = new CSetCoverJob(m_ID, m_Url);
	theGUI->ScheduleJob(pSetCoverJob);

	ShowCover(m_ID, m_Url);
}