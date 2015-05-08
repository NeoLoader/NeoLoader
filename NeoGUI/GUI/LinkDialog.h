#pragma once
#include "../Common/Dialog.h"

class CMakeLinkJob;

class CLinkDialog : public QDialogEx
{
	Q_OBJECT

public:
	CLinkDialog(QList<uint64> FileIDs, QWidget *parent = 0);

	enum EModes
	{
		eNeo,
		//eCompressed,
		//eEncrypted,
		eMagnet,
		ed2k,
		eTorrent
	};


	static void			CopyLinks(QList<uint64> FileIDs, EModes Mode);

private slots:
	void				OnMakeLinks();
	void				OnEncoding(int) {OnMakeLinks();}
	void				OnAddArc(int)	{OnMakeLinks();}

	void				OnGet();
	void				OnGetTorrent();
	void				OnGetColection();

protected:
	friend class CMakeLinkJob;



	QList<uint64>		m_FileIDs;

	QVBoxLayout*		m_pMainLayout;

	QWidget*			m_pLinkWidget;
	QGridLayout*		m_pLinkLayout;

	QCheckBox*			m_pAddArc;
	QComboBox*			m_pEncoding;
	QToolButton*		m_pGetBtn;
	QMenu*				m_pGetMenu;
	QAction*			m_pGetTorent;
	QAction*			m_pGetColection;
	

	QTextEdit*			m_pLinks;
};