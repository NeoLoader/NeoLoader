#include "GlobalHeader.h"
#include "CaptchaDialog.h"
#include "../NeoGUI.h"

#if QT_VERSION < 0x050000
CCaptchaDialog::CCaptchaDialog(QWidget *parent, Qt::WFlags flags)
#else
CCaptchaDialog::CCaptchaDialog(QWidget *parent, Qt::WindowFlags flags)
#endif
  : QMainWindow(parent, flags)
{
	m_pCaptchaPullJob = NULL;

	setWindowTitle(tr("Captcha Input"));

	m_pMainWidget = new QWidget();
	m_pMainLayout = new QFormLayout();

	m_pCaptcha = new QLabel();
	m_pMainLayout->setWidget(0, QFormLayout::SpanningRole, m_pCaptcha);

	m_pSolution = new QLineEdit();
	m_pSolution->setMaximumWidth(100);
	connect(m_pSolution, SIGNAL(returnPressed()), this, SLOT(OnOK()));
	connect(m_pSolution, SIGNAL(textEdited(const QString&)), this, SLOT(OnEdit(const QString&)));
	m_pMainLayout->setWidget(1, QFormLayout::LabelRole, new QLabel(tr("Solution:")));
	m_pMainLayout->setWidget(1, QFormLayout::FieldRole, m_pSolution);

	m_pButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
	QObject::connect(m_pButtonBox, SIGNAL(accepted()), this, SLOT(OnOK()));
	QObject::connect(m_pButtonBox, SIGNAL(rejected()), this, SLOT(OnCancel()));
	m_pMainLayout->setWidget(2, QFormLayout::SpanningRole, m_pButtonBox);
	m_pMainWidget->setLayout(m_pMainLayout);
	setCentralWidget(m_pMainWidget);

	restoreGeometry(theGUI->Cfg()->GetBlob("Gui/Widget_Captcha_Dialog"));

	m_TimerId = startTimer(500);
	m_CountDown = 0;
}

CCaptchaDialog::~CCaptchaDialog()
{
	killTimer(m_TimerId);
}

class CCaptchaPullJob: public CInterfaceJob
{
public:
	CCaptchaPullJob(CCaptchaDialog* pView)
	{
		m_pView = pView;
		m_Request["Mode"] = "Full";
	}

	virtual QString			GetCommand()	{return "GetCaptchas";}
	virtual void			HandleResponse(const QVariantMap& Response)
	{
		if(m_pView)
		{
			m_pView->m_Captchas = Response["Captchas"].toList();
			m_pView->FetchNextCaptcha();
		}
	}
	virtual void			Finish(bool bOK)
	{
		if(m_pView)
			m_pView->m_pCaptchaPullJob = NULL;
		CInterfaceJob::Finish(bOK);
	}

protected:
	QPointer<CCaptchaDialog>m_pView; // Note: this can be deleted at any time
};

void CCaptchaDialog::timerEvent(QTimerEvent *e)
{
    if (e->timerId() != m_TimerId) 
	{
        QMainWindow::timerEvent(e);
		return;
    }

	if(m_Captchas.isEmpty() && m_pCaptchaPullJob == NULL)
	{
		m_pCaptchaPullJob = new CCaptchaPullJob(this);
		theGUI->ScheduleJob(m_pCaptchaPullJob);
	}
	else if(!m_Captchas.isEmpty() && isHidden())
		FetchNextCaptcha();

	if(m_CountDown != 0)
	{
		m_CountDown--;
		m_pButtonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel (%1)").arg(m_CountDown/2));
		if(m_CountDown == 0)
			OnCancel();
	}
}

void CCaptchaDialog::OnEdit(const QString& text)
{
	m_pButtonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
	m_CountDown = 0;
}

void CCaptchaDialog::FetchNextCaptcha()
{
	if(m_Captchas.isEmpty())
		return;

	QVariantMap Captcha = m_Captchas.first().toMap();

	QPixmap PixMap;
	PixMap.loadFromData(Captcha["Data"].toByteArray());
	m_pCaptcha->setPixmap(PixMap);

	setWindowTitle(tr("Captcha Input: %1").arg(Captcha["Info"].toString()));

	show();

	m_CountDown = 2 * theGUI->Cfg()->GetInt("Gui/CaptchaTimeOut");
	QTimer::singleShot(10,this,SLOT(OnResize()));
}

void CCaptchaDialog::OnResize()
{
	resize(m_pMainWidget->minimumWidth(), m_pMainWidget->minimumHeight());
}

class CCaptchaPushJob: public CInterfaceJob
{
public:
	CCaptchaPushJob(const QVariantMap& Captcha, const QString& Solution)
	{
		m_Request["ID"] = Captcha["ID"];
		if(!Solution.isEmpty())
			m_Request["Solution"] = Solution;
	}

	virtual QString			GetCommand()	{return "SetSolution";}
	virtual void			HandleResponse(const QVariantMap& Response)	{}
};

void CCaptchaDialog::OnOK()
{
	if(m_pSolution->text().isEmpty())
		m_pSolution->setText("???");

	Submit();
}

void CCaptchaDialog::OnCancel()
{
	m_pSolution->clear();

	Submit();
}

void CCaptchaDialog::Submit()
{
	m_pCaptcha->clear();

	if(m_Captchas.isEmpty())
		return;

	CCaptchaPushJob* pCaptchaPushJob = new CCaptchaPushJob(m_Captchas.takeFirst().toMap(), m_pSolution->text());
	theGUI->ScheduleJob(pCaptchaPushJob);

	m_pSolution->clear();
	theGUI->Cfg()->SetBlob("Gui/Widget_Captcha_Dialog",saveGeometry());

	if(m_Captchas.isEmpty())
		hide();
	else
		FetchNextCaptcha();
}