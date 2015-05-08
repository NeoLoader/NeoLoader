#pragma once

#include <QVBoxLayout>
#include <QToolBar>
#include <QAction>
#include <QLineEdit>
#include <QTabWidget>
#include <QTreeWidgetItem>
#include <QSplitter>
#include <QLabel>
#include <QFormLayout>
#include <QTextEdit>
#include <QDialogButtonBox>
#include <QInputDialog>


#include "../Framework/ObjectEx.h"
#include "Common/Pointer.h"
#include "Common/Variant.h"

class CKadLookup;

class CLookupWidget: public QWidget
{
	Q_OBJECT
public:
	CLookupWidget(QWidget *parent = 0);

	map<CVariant, CPointer<CKadLookup> >&	GetLookups()	{return m_Lookups;}
	void				SetCurID(QVariant CurID)			{m_CurID = CurID;}

private slots:
	void				OnStartLookup();
	void				OnStopLookup();
	void				OnMoreLookup();
	void				OnClearLookup();
	void				OnLoadLookup(QTreeWidgetItem* pItem, int column);
	void				OnSendPacket();

protected:
	
	map<CVariant, CPointer<CKadLookup> >	m_Lookups;
	QVariant			m_CurID;

	QVBoxLayout			*m_pLookupLayout;
	QToolBar			*m_pLookupBar;
	QAction				*m_pLookupStop;
	QLineEdit			*m_pLookupID;
	QAction				*m_pLookupStart;
	QAction				*m_pLookupMore;
	QAction				*m_pLookupClear;
	/*QWidget				*m_pLookupAuxWidget;
	QFormLayout			*m_pLookupAuxLayout;*/
	QLineEdit			*m_pLookupJumps;
	QLineEdit			*m_pLookupHops;
	QLineEdit			*m_pLookupSpread;
	QTabWidget			*m_pLookupTab;
};

class CPaylaodWidget: public QWidget
{
	Q_OBJECT

public:
	CPaylaodWidget(QWidget *pParent = NULL)
	: QWidget(pParent)
	{
		m_pSplitter = new QSplitter(Qt::Vertical);
		
		m_pValue = new QLineEdit();

		m_pAuxWidget = new QWidget();
		m_pAuxLayout = new QFormLayout(m_pAuxWidget);

		m_pLabel = new QLabel();
		m_pAuxLayout->setWidget(0, QFormLayout::LabelRole, m_pLabel);
		m_pAuxLayout->setWidget(0, QFormLayout::FieldRole, m_pValue);
		m_pSplitter->addWidget(m_pAuxWidget);

		m_pRequest = new QTextEdit();
		m_pSplitter->addWidget(m_pRequest);
		m_pResponse = new QTextEdit();
		m_pSplitter->addWidget(m_pResponse);


		m_pMainLayout = new QVBoxLayout(this);
		m_pMainLayout->setContentsMargins(0, 0, 0, 0);
		m_pMainLayout->addWidget(m_pSplitter);
	}


	QVBoxLayout			*m_pMainLayout;
	QSplitter			*m_pSplitter;

	QLabel				*m_pLabel;
	QLineEdit			*m_pValue;
	QWidget				*m_pAuxWidget;
	QFormLayout			*m_pAuxLayout;

	QTextEdit			*m_pRequest;
	QTextEdit			*m_pResponse;
};

class CRouteWidget: public QWidget
{
	Q_OBJECT

public:
	CRouteWidget(QWidget *pParent = NULL)
	: QWidget(pParent)
	{
		m_pMainLayout = new QVBoxLayout(this);
		m_pMainLayout->setContentsMargins(0, 0, 0, 0);
		//m_pSplitter = new QSplitter(Qt::Vertical);
		//m_pMainLayout->addWidget(m_pSplitter);

		m_pButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok, Qt::Horizontal, this);

		m_pTargetID = new QLineEdit();
		m_pTargetID->setMinimumSize(250, QWIDGETSIZE_MAX);

		m_pEntityID = new QLineEdit();
		m_pEntityID->setMinimumSize(250, QWIDGETSIZE_MAX);
		m_pEntityID->setReadOnly(true);

		m_pReceiverID = new QLineEdit();
		m_pReceiverID->setMinimumSize(250, QWIDGETSIZE_MAX);
		m_pReceiverID->setMaxLength(32);

		//m_pAsSession = new QCheckBox(tr("As Controlled Session"));
		//m_pAsSession->setTristate();

		m_pData = new QTextEdit();
		//m_pSplitter->addWidget(m_pData);
		m_pMainLayout->addWidget(m_pData);

		m_pAuxWidget = new QWidget();
		m_pAuxLayout = new QFormLayout(m_pAuxWidget);
		m_pAuxLayout->setWidget(0, QFormLayout::LabelRole, new QLabel(tr("TargetID")));
		m_pAuxLayout->setWidget(0, QFormLayout::FieldRole, m_pTargetID);
		m_pAuxLayout->setWidget(1, QFormLayout::LabelRole, new QLabel(tr("my EntityID")));
		m_pAuxLayout->setWidget(1, QFormLayout::FieldRole, m_pEntityID);
		m_pAuxLayout->setWidget(2, QFormLayout::LabelRole, new QLabel(tr("ReciverID")));
		m_pAuxLayout->setWidget(2, QFormLayout::FieldRole, m_pReceiverID);
		//m_pAuxLayout->setWidget(3, QFormLayout::LabelRole, m_pAsSession);
		m_pAuxLayout->setWidget(3, QFormLayout::FieldRole, m_pButtonBox);
		//m_pSplitter->addWidget(m_pAuxWidget);
		m_pMainLayout->addWidget(m_pAuxWidget);

	}

	QVBoxLayout			*m_pMainLayout;
	//QSplitter			*m_pSplitter;

	QWidget				*m_pAuxWidget;
	QFormLayout			*m_pAuxLayout;

	QTextEdit			*m_pData;
	QLineEdit			*m_pTargetID;
	QLineEdit			*m_pReceiverID;
	QLineEdit			*m_pEntityID;
	//QCheckBox			*m_pAsSession;
	QDialogButtonBox*	m_pButtonBox;
};
