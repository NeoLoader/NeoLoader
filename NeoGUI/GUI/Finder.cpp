#include "GlobalHeader.h"
#include "Finder.h"
#include "../NeoGUI.h"


CFinder::CFinder(QWidget *parent)
:QWidget(parent)
{
	m_pSearchLayout = new QHBoxLayout();
	m_pSearchLayout->setMargin(0);
	m_pSearchLayout->setSpacing(6);
	m_pSearchLayout->setAlignment(Qt::AlignLeft);

	m_pSearch = new QLineEdit();
	m_pSearch->setMinimumWidth(150);
	m_pSearch->setMaximumWidth(350);
	m_pSearchLayout->addWidget(m_pSearch);
	QObject::connect(m_pSearch, SIGNAL(textChanged(QString)), this, SLOT(OnUpdate()));
    //QObject::connect(m_pSearch, SIGNAL(returnPressed()), this, SLOT(_q_next()));

	m_pCaseSensitive = new QCheckBox(tr("Case Insensitive"));
	m_pSearchLayout->addWidget(m_pCaseSensitive);
	connect(m_pCaseSensitive, SIGNAL(stateChanged(int)), this, SLOT(OnUpdate()));

	m_pRegExp = new QCheckBox(tr("RegExp"));
	m_pSearchLayout->addWidget(m_pRegExp);
	connect(m_pRegExp, SIGNAL(stateChanged(int)), this, SLOT(OnUpdate()));

	QToolButton* pClose = new QToolButton(this);
    pClose->setIcon(QIcon(":/Icons/CloseSearch"));
    pClose->setAutoRaise(true);
    pClose->setText(tr("Close"));
    m_pSearchLayout->addWidget(pClose);
	QObject::connect(pClose, SIGNAL(clicked()), this, SLOT(Close()));

	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pSearchLayout->addWidget(pSpacer);

	setLayout(m_pSearchLayout);

	setMaximumHeight(30);

	hide();
}

CFinder::~CFinder()
{
}

void CFinder::Open()
{
	show();
	m_pSearch->setFocus(Qt::OtherFocusReason);
	OnUpdate();
}

void CFinder::OnUpdate()
{
	emit SetFilter(QRegExp(m_pSearch->text(), m_pCaseSensitive->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive, m_pRegExp->isChecked() ? QRegExp::RegExp : QRegExp::Wildcard));
}

void CFinder::Close()
{
	emit SetFilter(QRegExp());
	hide();
}