#pragma once

class CSettingsView;
class CShareWidget;
class CPropertiesView;
struct SField;

class CSettingsWindow: public QMainWindow
{
	Q_OBJECT

public:
	CSettingsWindow(QWidget *parent = 0);
	~CSettingsWindow();

	void				Open();

private slots:
	void				OnItemClicked(QTreeWidgetItem* pItem, int Column);
	void				OnSelectionChanged();
	void				OnClicked(QAbstractButton* pButton);

protected:
	void				LoadAll();
	void				SaveAll();

	void				SetupInterface(QMultiMap<int, SField>&);
	void 				SetupGeneral(QMultiMap<int, SField>&);
	void 				SetupNetwork(QMultiMap<int, SField>&);
	void 				SetupShared(QMultiMap<int, SField>&);
	void 				SetupHosters(QMultiMap<int, SField>&);
	void 				SetupNeoShare(QMultiMap<int, SField>&);
	void 				SetupBitTorrent(QMultiMap<int, SField>&);
	void 				SetupEd2kMule(QMultiMap<int, SField>&);
	void 				SetupAdvanced(QMultiMap<int, SField>&);

	QWidget*			m_pMainWidget;
	QGridLayout*		m_pMainLayout;

	QTreeWidget*		m_pGroupeTree;

	QWidget*			m_pSettingsWidget;
	QStackedLayout*		m_pSettingsLayout;

	CSettingsView*		m_pInterface;
	CSettingsView*		m_pGeneral;
	CSettingsView*		m_pNetwork;
	CShareWidget*		m_pShare;
	CSettingsView*		m_pHoster;
	CSettingsView*		m_pNeoShare;
	CSettingsView*		m_pBitTorrent;
	CSettingsView*		m_pEd2kMule;
	CSettingsView*		m_pAdcanced;
	CPropertiesView*	m_pProperties;

	QDialogButtonBox*	m_pButtons;
};

#ifdef WIN32
#include "../Common/ShellSetup.h"

extern CShellSetup g_ShellSetup;

class CShellIntegrateBox: public QCheckBox
{
	Q_OBJECT

public:
	CShellIntegrateBox(const QString& Text);

private slots:
	void OnChanged();

	void OnClicked(bool Checked);

protected:
	QString		m_OpenPath;

	bool		m_bEd2k;
	bool		m_bMagnet;
	bool		m_bTorrent;
	bool		m_bHoster;
};

#endif