#pragma once

class CAccountView: public QWidget
{
	Q_OBJECT

public:
	CAccountView(QWidget *parent = 0);

	void				SetMode(UINT Mode);

	void				ShowAccount(const QString& Hoster, const QString& Account);

public slots:
	void				SaveAccount();

protected:
	friend class CAccountSyncJob;
	friend class CAccountSaveJob;

	QString				m_Hoster;
	QString				m_Account;

	QVBoxLayout*		m_pMainLayout;

	QGroupBox*			m_pAccountWidget;
	QFormLayout*		m_pAccountLayout;

	QLineEdit*			m_pUserName;
	QLineEdit*			m_pPassword;
	QCheckBox*			m_pEnabled;
	QCheckBox*			m_pFree;
	QCheckBox*			m_pUploadTo;

	bool				m_bLockDown;
};