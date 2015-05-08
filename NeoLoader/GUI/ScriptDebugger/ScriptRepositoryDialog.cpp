#include "GlobalHeader.h"
#include "ScriptRepositoryDialog.h"

CScriptRepositoryDialog::CScriptRepositoryDialog(QWidget *parent)
	: QDialog(parent)
{
    m_pView = new QTreeWidget();
	m_pView->setHeaderLabels(QString("File Name|Script Name|Script Version").split("|"));
	connect(m_pView, SIGNAL(itemClicked(QTreeWidgetItem*, int)), this, SLOT(OnItemClicked(QTreeWidgetItem*, int)));
	connect(m_pView, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(OnItemDoubleClicked(QTreeWidgetItem*, int)));

	m_pButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
	QObject::connect(m_pButtonBox, SIGNAL(accepted()), this, SLOT(accept()));
	QObject::connect(m_pButtonBox, SIGNAL(rejected()), this, SLOT(reject()));

	m_pEdit = new QLineEdit();

	QWidget* pWidget = new QWidget();
    QFormLayout* pFbox = new QFormLayout(pWidget);
	pFbox->setWidget(0, QFormLayout::SpanningRole, m_pEdit);
	pFbox->setWidget(1, QFormLayout::FieldRole, m_pButtonBox);

	QVBoxLayout* pVbox = new QVBoxLayout(this);
    pVbox->setMargin(0);
    pVbox->setSpacing(0);
    pVbox->addWidget(m_pView);
	pVbox->addWidget(pWidget);
}

CScriptRepositoryDialog::~CScriptRepositoryDialog()
{
}

QString CScriptRepositoryDialog::SelectScript(const QString& Action, const QString& DefaultName)
{
	m_pButtonBox->button(QDialogButtonBox::Ok)->setText(Action);
	m_pEdit->setText(DefaultName);
	if(!exec())
		return QString();
	QString FileName = m_pEdit->text();
	if(FileName.isEmpty())
		return QString();
	if(FileName.right(3) != ".js")
		FileName.append(".js");
	return FileName;
}

void CScriptRepositoryDialog::OnItemClicked(QTreeWidgetItem* pItem, int Column)
{
	m_pEdit->setText(pItem->text(0));
}

void CScriptRepositoryDialog::OnItemDoubleClicked(QTreeWidgetItem* pItem, int Column)
{
	accept();
}

void CScriptRepositoryDialog::SyncScripts(const QVariant& Scripts)
{
	QMap<QString, QTreeWidgetItem*> OldScripts;
	for(int i = 0; i < m_pView->topLevelItemCount(); ++i) 
	{
		QTreeWidgetItem* pItem = m_pView->topLevelItem(i);
		QString FileName = pItem->data(0, Qt::UserRole).toString();
		Q_ASSERT(!OldScripts.contains(FileName));
		OldScripts.insert(FileName,pItem);
	}

	foreach(const QVariant& var, Scripts.toList()) 
	{
		QVariantMap Script = var.toMap();

		QString FileName = Script["FileName"].toString();
		QTreeWidgetItem* pItem = OldScripts.take(FileName);
		if(!pItem)
		{
			pItem = new QTreeWidgetItem();
			pItem->setData(0, Qt::UserRole, FileName);
			m_pView->addTopLevelItem(pItem);
		}
		pItem->setText(0, Script["FileName"].toString());
		pItem->setText(1, Script["Name"].toString());
		pItem->setText(2, Script["Version"].toString());
		//pItem->setData(1, Qt::UserRole, Script);
	}

	foreach(QTreeWidgetItem* pItem, OldScripts)
		delete pItem;
}