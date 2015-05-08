#include "GlobalHeader.h"
#include "ServiceSummary.h"
#include "FileListWidget.h"
#include "FileListView.h"
#include "../NeoGUI.h"
#include "StatisticsWindow.h"

CServiceSummary::CServiceSummary(QWidget *parent)
:QWidget(parent)
{
	m_pMainLayout = new QGridLayout(this);
	m_pMainLayout->setMargin(0);

	m_AnonGroup = new QGroupBoxEx<QHBoxLayout>(tr("Anonymisation Services"));
	m_AnonGroup->layout()->setMargin(0);
	m_AnonGroup->layout()->setAlignment(Qt::AlignLeft);
	m_pAnonIcon = new QLabel();
	m_pAnonIcon->setPixmap(QPixmap(":/Icons/Shield-Blue"));
	m_AnonGroup->layout()->addWidget(m_pAnonIcon);
	m_pAnonText = new QLabel(tr("VPN"));
	m_AnonGroup->layout()->addWidget(m_pAnonText);
	m_AnonGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
	//m_AnonGroup->setMaximumHeight(50);
	m_pMainLayout->addWidget(m_AnonGroup, 0, 0);

	m_pMainLayout->addWidget(new QWidget(), 0, 1);
	m_pMainLayout->addWidget(new QWidget(), 1, 0, 2, 1);

	setLayout(m_pMainLayout);

	m_TimerId = startTimer(500);
}

CServiceSummary::~CServiceSummary()
{
	killTimer(m_TimerId);
}

void CServiceSummary::timerEvent(QTimerEvent *e)
{
    if (e->timerId() != m_TimerId) 
	{
        QObject::timerEvent(e);
		return;
    }

	const QVariantMap& Data = theGUI->GetInfoData();
	QVariantMap Networks = Data["Networks"].toMap();

	int iNeoShare = Data["NeoShare"].toMap().value("Anonymity").toInt();
	bool bNoP2P = !Networks.contains("BitTorrent") && !Networks.contains("Ed2kMule");
	bool bHasVPN = !Data["Bandwidth"].toMap().value("DefaultNIC").toString().isEmpty();
	bool bHasProxy = !Data["Hosters"].toMap().value("WebProxy").toString().isEmpty();

	int AnonIcon = 0;
	QString AnonStatus = iNeoShare == 0 ? tr("NeoShare has plausible deniability") : tr("NeoShare is always anonymouse");

	if(bHasVPN)
	{
		AnonStatus.append(tr("\r\nVPN is configurated, ed2k and Bittorent Transfers are being anonymized."));
		AnonIcon = 0;
	}
	else if(bNoP2P)
		AnonStatus.append(tr("\r\nNo VPN is configurated."));
	else
	{
		AnonStatus.append(tr("\r\nNo VPN is configurated, ed2k and Bittorent Transfers are not anonymouse!"));
		AnonIcon = 4;
	}

	if(bHasProxy)
	{
		AnonStatus.append(tr("\r\nWeb Proxy is configurated, Hoster Transfers are being anonymized."));
		if(AnonIcon > 2)
			AnonIcon  = 2;
	}
	else if(bHasVPN)
		AnonStatus.append(tr("\r\nNo Web Proxy is configurated, anonymized of Hoster Transfers can not be guarantied."));	
	else
		AnonStatus.append(tr("\r\nNo Web Proxy is configurated, Hoster Transfers are not anonymouse!"));

	switch(AnonIcon)
	{
	case 0: m_pAnonIcon->setPixmap(QPixmap(":/Icons/Shield-Blue")); break;
	case 1: m_pAnonIcon->setPixmap(QPixmap(":/Icons/Shield-Green")); break;
	case 2: m_pAnonIcon->setPixmap(QPixmap(":/Icons/Shield-Yelow")); break;
	case 3: m_pAnonIcon->setPixmap(QPixmap(":/Icons/Shield-Red")); break;
	case 4: m_pAnonIcon->setPixmap(QPixmap(":/Icons/Shield-Magenta")); break;
	}
	m_pAnonText->setText(AnonStatus);
}