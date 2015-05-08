#include "GlobalHeader.h"
#include "HosterUploader.h"
#include "ServicesWidget.h"
#include "../NeoGUI.h"

CHosterUploader::CHosterUploader(bool bSelectAction, QWidget *parent)
	: QDialogEx(parent)
{
	setWindowTitle(tr("Hoster Upload"));

	m_pMainLayout = new QGridLayout(this);

	m_pUploadTree = new QTreeWidget();
	m_pUploadTree->setHeaderLabels(QString("Hosters").split("|"));
	m_pMainLayout->addWidget(m_pUploadTree, 0, 0, 1, 2);

	if(bSelectAction)
	{
		m_pUploadMode = new QComboBox();
		m_pUploadMode->addItem(tr("Multi Part"), "UploadParts");
		m_pUploadMode->addItem(tr("Split Archive"), "UploadArchive");
		m_pUploadMode->addItem(tr("Single File"), "UploadSolid");
		m_pMainLayout->addWidget(m_pUploadMode, 1, 0);
	}
	else
		m_pUploadMode = NULL;

	m_pButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
	QObject::connect(m_pButtonBox, SIGNAL(accepted()), this, SLOT(accept()));
	QObject::connect(m_pButtonBox, SIGNAL(rejected()), this, SLOT(reject()));
	m_pMainLayout->addWidget(m_pButtonBox, 1, 1);

	restoreGeometry(theGUI->Cfg()->GetBlob("Gui/Widget_Upload_Dialog"));

	QStringList Hosters = theGUI->Cfg()->GetStringList("Gui/Widget_Upload_Hosters");

	const QMap<QString, QStringList>& Accounts = g_Services->GetAccounts();
	for(QMap<QString, QStringList>::const_iterator I = Accounts.begin(); I != Accounts.end(); I++)
	{
		QString Hoster = I.key();

		QTreeWidgetItem* pItem = new QTreeWidgetItem();
		pItem->setText(0, Hoster);
		m_pUploadTree->addTopLevelItem(pItem);

		QStringList Accounts = I.value();
		foreach(const QString& Account, Accounts)
		{
			if(Account.isEmpty())
				pItem->setCheckState(0, Hosters.contains(Hoster) ? Qt::Checked : Qt::Unchecked);
			else
			{
				QTreeWidgetItem* pSubItem = new QTreeWidgetItem();
				pSubItem->setText(0, Account);
				pSubItem->setCheckState(0, Hosters.contains(Account + "@" + Hoster) ? Qt::Checked : Qt::Unchecked);
				pItem->addChild(pSubItem);
			}
		}

		pItem->setExpanded(true);
	}
}

CHosterUploader::~CHosterUploader()
{
	theGUI->Cfg()->SetBlob("Gui/Widget_Captcha_Dialog",saveGeometry());
}

QStringList CHosterUploader::GetHosters()
{
	QStringList Hosters;

	for(int i=0; i < m_pUploadTree->topLevelItemCount(); i++)
	{
		QTreeWidgetItem* pItem = m_pUploadTree->topLevelItem(i);
		if(pItem->checkState(0) == Qt::Checked)
			Hosters.append("@" + pItem->text(0));

		for(int i=0; i < pItem->childCount(); i++)
		{
			QTreeWidgetItem* pSubItem = pItem->child(i);
			if(pSubItem->checkState(0) == Qt::Checked)
				Hosters.append(pSubItem->text(0) + "@" + pItem->text(0));
		}
	}

	theGUI->Cfg()->SetSetting("Gui/Widget_Upload_Hosters", Hosters);
	return Hosters;
}