#pragma once

class CCaptchaPullJob;

class CCaptchaDialog: public QMainWindow
{
	Q_OBJECT
public:
#if QT_VERSION < 0x050000
	CCaptchaDialog(QWidget *parent = 0, Qt::WFlags flags = 0);
#else
	CCaptchaDialog(QWidget *parent = 0, Qt::WindowFlags flags = 0);
#endif
    ~CCaptchaDialog();

private slots:
	void				OnEdit(const QString& text);
	void				OnOK();
	void				OnCancel();
	void				OnResize();

protected:
	void				Submit();

	friend class CCaptchaPullJob;

	void				FetchNextCaptcha();

	virtual void		timerEvent(QTimerEvent *e);

	int					m_TimerId;
	int					m_CountDown;
	
	CCaptchaPullJob*	m_pCaptchaPullJob;
	QVariantList		m_Captchas;

	QLabel*				m_pCaptcha;
	QLineEdit*			m_pSolution;
	QDialogButtonBox*	m_pButtonBox;
	QWidget*			m_pMainWidget;
	QFormLayout*		m_pMainLayout;
};