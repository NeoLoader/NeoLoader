#pragma once

class CProxyEdit;

class CHosterView: public QWidget
{
	Q_OBJECT

public:
	CHosterView(QWidget *parent = 0);

	void				SetMode(UINT Mode);

	void				ShowHoster(const QString& Hoster);

public slots:
	void				SaveHoster();

protected:
	friend class CHosterSyncJob;
	friend class CHosterApplyJob;

	QString				m_Hoster;

	QVBoxLayout*		m_pMainLayout;

	QGroupBox*			m_pHosterWidget;
	QFormLayout*		m_pHosterLayout;

	QLineEdit*			m_pHostName;
	QCheckBox*			m_pUploadTo;
	CProxyEdit*			m_pProxy;

	bool				m_bLockDown;
};
