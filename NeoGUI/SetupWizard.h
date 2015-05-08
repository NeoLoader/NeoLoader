#pragma once

#include "../Framework/IPC/JobManager.h"
class CPathEdit;
class CKbpsEdit;
class CProxyEdit;
class CAnonSlider;

class CInterfaceJobEx: public CInterfaceJob
{
	Q_OBJECT
public:

private slots:
	virtual void Retry() = 0;
};

class CSetupWizard: public QWizard
{
    Q_OBJECT
public:

    enum 
	{ 
		Page_Intro, 
		Page_Networks, 
		Page_Accounts,
		Page_Security,
		Page_Advanced,
		Page_Conclusion 
	};

    CSetupWizard(QWidget *parent = 0);

	static void	 ShowWizard();

private slots:
    void showHelp();
};

////////////////////////////////////////////////////////////////////////////////////////////////////////
// CIntroPage

class CIntroPage: public QWizardPage
{
    Q_OBJECT
public:
    CIntroPage(QWidget *parent = 0);

    int nextId() const;

private:
    QLabel* m_pTopLabel;
	QLabel* m_pMainLabel;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////
// CNetworksPage

class CNetworksPage: public QWizardPage
{
    Q_OBJECT
public:
    CNetworksPage(QWidget *parent = 0);

    int nextId() const;

private slots:
	void			OnIncoming(QString Path);
	void			OnEd2k(bool bChecked);

private:
	CPathEdit*		m_pIncoming;
	QLineEdit*		m_pTemp;

	QCheckBox*		m_pBitTorrent;
	QCheckBox*		m_pEd2kMule;
	QCheckBox*		m_pAutoEd2k;
	QCheckBox*		m_pNeoShare;
	QCheckBox*		m_pAddHosters;
	QCheckBox*		m_pRegister;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAccountsPage

class CAccountsPage: public QWizardPage
{
    Q_OBJECT
public:
    CAccountsPage(QWidget *parent = 0);

    int nextId() const;
	void initializePage();

private slots:
	void			OnAddAccount();

private:
	//QComboBox*		m_pHosters;
	QPushButton*	m_pAddAccount;
	QTreeWidget*	m_pAccounts;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSecurityPage

class CSecurityPage: public QWizardPage
{
    Q_OBJECT
public:
    CSecurityPage(QWidget *parent = 0);

    int nextId() const;
	void initializePage();

private:
	CAnonSlider*	m_pSlider;
	//QWidget*		m_pNoTracking;
	QComboBox*		m_pNIC;
	CProxyEdit*		m_pProxy;
	QComboBox*		m_pEncryption;
	QCheckBox*		m_pStaticHash;
	//QComboBox*		m_pIpFilter;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAdvancedPage

class CAdvancedPage: public QWizardPage
{
    Q_OBJECT
public:
    CAdvancedPage(QWidget *parent = 0);

    int nextId() const;
	void initializePage();

private slots:
	void			OnAdvanced(bool bChecked);

private:
	QFormLayout*	m_pLayout;

	//QCheckBox*		m_pAdvanced;
	CKbpsEdit*		m_pDownRate;
	CKbpsEdit*		m_pUpRate;

	QLineEdit*		m_pMaxCon;
	QLineEdit*		m_pMaxNew;

	QCheckBox*		m_pUPnP;
	QLineEdit*		m_pNeoPort;
	QLineEdit*		m_pTorrentPort;
	QLineEdit*		m_pMuleTCP;
	QLineEdit*		m_pMuleUDP;
};


////////////////////////////////////////////////////////////////////////////////////////////////////////
// CConclusionPage

class CConclusionPage: public QWizardPage
{
    Q_OBJECT
public:
    CConclusionPage(QWidget *parent = 0);

    int nextId() const;

private:
    QLabel*			m_pBottomLabel;
	QLabel*			m_pMainLabel;
};
