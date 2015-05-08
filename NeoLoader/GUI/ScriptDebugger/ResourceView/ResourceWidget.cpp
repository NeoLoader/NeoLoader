#include "GlobalHeader.h"
#include "ResourceWidget.h"

CResourceWidget::CResourceWidget(QWidget *parent)
 : QWidget(parent, 0)
{
    m_pView = new QTreeWidget();
	m_pView->setHeaderLabels(QString("Url|Staus|Type").split("|"));
	connect(m_pView, SIGNAL(itemClicked(QTreeWidgetItem*, int)), this, SLOT(OnItemClicked(QTreeWidgetItem*, int)));
	connect(m_pView, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(OnItemDoubleClicked(QTreeWidgetItem*, int)));

    QVBoxLayout* pVbox = new QVBoxLayout(this);
    pVbox->setMargin(0);
    pVbox->addWidget(m_pView);

	m_pMenu = NULL;
}

CResourceWidget::~CResourceWidget()
{
}

void CResourceWidget::SetContextMenu(QMenu* pMenu)
{
	m_pMenu = pMenu;
	if(m_pMenu)
	{
		Q_ASSERT(m_pMenu->parent() == this);
		m_pView->setContextMenuPolicy(Qt::CustomContextMenu);
		connect(m_pView, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenuRequested(const QPoint &)));
	} 
	else 
	{
		m_pView->setContextMenuPolicy(Qt::NoContextMenu);
		disconnect(m_pView, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenuRequested(const QPoint &)));
		delete m_pMenu;
	}
}

qint64 CResourceWidget::CurrentResourceID()
{
	QTreeWidgetItem* item = m_pView->currentItem();
	if(item)
		return (qint64)item;
	return -1;
}

QVariantMap CResourceWidget::GetResource(qint64 ID)
{
	return m_Resources.value(ID);
}

void CResourceWidget::SyncResources(const QVariant& Resources)
{
	m_Resources.clear(); // all will be refilled

	QMap<QString, QTreeWidgetItem*> OldResources;
	for(int i = 0; i < m_pView->topLevelItemCount(); ++i) 
	{
		QTreeWidgetItem* pItem = m_pView->topLevelItem(i);
		QString Handle = pItem->data(0, Qt::UserRole).toString();
		Q_ASSERT(!OldResources.contains(Handle));
		OldResources.insert(Handle,pItem);
	}

	foreach(const QVariant& var, Resources.toList()) 
	{
		QVariantMap Resource = var.toMap();

		QString Handle = Resource["Handle"].toString();
		QTreeWidgetItem* pItem = OldResources.take(Handle);
		if(!pItem)
		{
			pItem = new QTreeWidgetItem();
			pItem->setData(0, Qt::UserRole, Handle);
			m_pView->addTopLevelItem(pItem);
			pItem->setExpanded(true);
		}
		m_Resources[(qint64)pItem] = Resource;
		UpdateItem(pItem, Resource);
	}

	foreach(QTreeWidgetItem* pItem, OldResources)
		delete pItem;
}

void CResourceWidget::UpdateItem(QTreeWidgetItem* CurItem, const QVariantMap& CurResource)
{
	CurItem->setText(0, CurResource["Url"].toString());
	CurItem->setText(1, CurResource["Status"].toString());
	CurItem->setText(2, CurResource["ContentType"].toString());

	QMap<QString, QTreeWidgetItem*> OldResources;
	for(int i = 0; i < CurItem->childCount(); ++i)
	{
		QTreeWidgetItem* pItem = CurItem->child(i);
		QString Handle = pItem->data(0, Qt::UserRole).toString();
		Q_ASSERT(!OldResources.contains(Handle));
		OldResources.insert(Handle,pItem);
	}	

	QVariant Resources = CurResource["Resources"];
	foreach(const QVariant& var, Resources.toList()) {
		QVariantMap Resource = var.toMap();

		QString Handle = Resource["Handle"].toString();
		QTreeWidgetItem* pItem = OldResources.take(Handle);
		if(!pItem)
		{
			pItem = new QTreeWidgetItem();
			pItem->setData(0, Qt::UserRole, Handle);
			CurItem->addChild(pItem);
			pItem->setExpanded(true);
		}
		m_Resources[(qint64)pItem] = Resource;
		UpdateItem(pItem, Resource);
	}

	foreach(QTreeWidgetItem* pItem, OldResources)
		delete pItem;
}

void CResourceWidget::OnMenuRequested(const QPoint &point)
{
	m_pMenu->popup(QCursor::pos());	
}

void CResourceWidget::OnItemClicked(QTreeWidgetItem* pItem, int Column)
{
	emit ResourceSelected((qint64)pItem);
}

void CResourceWidget::OnItemDoubleClicked(QTreeWidgetItem* pItem, int Column)
{
	emit ResourceActivated((qint64)pItem);
}
