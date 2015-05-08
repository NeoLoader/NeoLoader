#pragma once
#include "../Common/Dialog.h"

class CHosterView;
class CAccountView;
class CHostersSyncJob;
class CPropertiesView;

class CServicesWidget: public QWidget
{
	Q_OBJECT

public:
	enum EMode
	{
		eUnknown,
		eHosters,
		eSolvers,
		eFinders
	};

	CServicesWidget(QWidget *parent = 0);
	~CServicesWidget();

	void				SetMode(EMode Mode);

	QStringList			GetBestHosters();
	QStringList			GetKnownHosters();
	QStringList			GetFinders();
	QStringList			GetHosters(bool bAll = false);
	QMap<QString, QStringList>	GetAccounts();

	static void			AddAccount(const QString& ServiceName, const QString& UserName, const QString& Password);

	static void			BuyAccount(const QString& ServiceName = "");

public slots:
	void				OnRemoveAccount();
	void				OnCheckAccount();
	void				OnAddAccount();

private slots:
	void				OnMenuRequested(const QPoint &point);
	void				OnItemClicked(QTreeWidgetItem* pItem, int Column);
	void				OnSelectionChanged();

protected:
	friend class CHostersSyncJob;
	void				SyncHosters();
	void				SyncHosters(const QVariantMap& Response);
	void				UpdateTree();

	virtual void		timerEvent(QTimerEvent *e);

	int					m_TimerId;
	
	CHostersSyncJob*	m_pHostersSyncJob;

	struct SHoster
	{
		QTreeWidgetItem* pItem;
	};

	QMap<QString, SHoster*>	m_Hosters;

	enum EColumns
	{
		eName = 0,
		eStatus,
		eAPIs
	};

	QMultiMap<EMode, QVariantMap>	m_Services;

	EMode				m_Mode;

	QVBoxLayout*		m_pMainLayout;
	QSplitter*			m_pSplitter;

	QWidget*			m_pFilterWidget;
	QGridLayout*		m_pFilterLayout;

	QWidget*			m_pHostsWidget;
	QVBoxLayout*		m_pHostsLayout;

	QTreeWidget*		m_pHosterTree;
	QLineEdit*			m_pHostFilter;
	QCheckBox*			m_pAccountsOnly;

	QTabWidget*			m_pHosterTabs;

	QWidget*			m_pHostWidget;
	QStackedLayout*		m_pHostLayout;

	QWidget*			m_pSubWidget;
	QVBoxLayout*		m_pSubLayout;

	CHosterView*		m_HosterView;
	CAccountView*		m_AccountView;

	CPropertiesView*	m_pProperties;

	QMenu*				m_pMenu;
	QAction*			m_pAddAccount;
	QAction*			m_pRemoveAccount;
	QAction*			m_pCheckAccount;
};

extern CServicesWidget* g_Services;

//////////////////////////////////////////////////////////////////////////////
//

class CNewAccount : public QDialogEx
{
	Q_OBJECT

public:
	CNewAccount(const QString& Hoster, QWidget *pMainWindow);

	struct SHosterAcc
	{
		QString Hoster;
		QString Username;
		QString Password;
	};

	static SHosterAcc GetAccount(const QString& Hoster = "", QWidget *pMainWindow = NULL)
	{
		SHosterAcc Account;
		CNewAccount NewAccount("", pMainWindow);
		if(!NewAccount.exec())
			return Account;

		Account.Hoster = NewAccount.GetHoster();
		Account.Username = NewAccount.m_pUserName->text();
		Account.Password = NewAccount.m_pPassword->text();
		return Account;
	}

	static bool	AddAccount(const QString& Hoster = "", QWidget *pMainWindow = NULL)
	{
		CNewAccount NewAccount(Hoster, pMainWindow);
		if(!NewAccount.exec())
			return false;

		CServicesWidget::AddAccount(NewAccount.GetHoster(), NewAccount.m_pUserName->text(), NewAccount.m_pPassword->text());
		return true;
	}

private slots:
	void		OnBuyAccount()
	{
		CServicesWidget::BuyAccount(GetHoster());
	}

protected:

	QString				GetHoster()		{return m_pHosters ? m_pHosters->currentText() : m_Hoster;}

	QString				m_Hoster;
	QComboBox*			m_pHosters;
	QPushButton*		m_pBuyAccount;
	QLineEdit*			m_pUserName;
	QLineEdit*			m_pPassword;
	QDialogButtonBox*	m_pButtonBox;
	QFormLayout*		m_pAccountLayout;
};