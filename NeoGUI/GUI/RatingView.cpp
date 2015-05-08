#include "GlobalHeader.h"
#include "RatingView.h"
#include "../NeoGUI.h"
#include "../Common/TreeWidgetEx.h"

CRatingView::CRatingView(QWidget *parent)
:QWidget(parent)
{
	m_ID = 0;
	m_pRatingSyncJob = NULL;

	m_pMainLayout = new QVBoxLayout();

	m_pRatingWidget = new QWidget();
	m_pRatingLayout = new QFormLayout();
	m_pRatingLayout->setMargin(0);

	m_pRatings = new QTreeWidgetEx();
	//m_pRatings->setHeaderLabels(tr("Rating|Description|FileName").split("|"));
	m_pRatings->setHeaderLabels(tr("Key|Value").split("|"));
	//m_pRatings->setContextMenuPolicy(Qt::CustomContextMenu);
	//connect(m_pRatings, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenuRequested(const QPoint &)));
	m_pRatingLayout->setWidget(0, QFormLayout::SpanningRole, m_pRatings);
	
	m_pFindRating = new QPushButton(tr("Find Rating"));
	connect(m_pFindRating, SIGNAL(pressed()), this, SIGNAL(FindRating()));
	m_pRatingLayout->setWidget(1, QFormLayout::LabelRole, m_pFindRating);

	m_pClearRating = new QPushButton(tr("Clear Rating"));
	connect(m_pClearRating, SIGNAL(pressed()), this, SIGNAL(ClearRating()));
	m_pRatingLayout->setWidget(2, QFormLayout::LabelRole, m_pClearRating);

	m_pRatingWidget->setLayout(m_pRatingLayout);

	m_pMainLayout->addWidget(m_pRatingWidget);

	setLayout(m_pMainLayout);

	m_pRatings->header()->restoreState(theGUI->Cfg()->GetBlob("Gui/Widget_Ratings_Columns"));

	m_TimerId = startTimer(500);
}

CRatingView::~CRatingView()
{
	theGUI->Cfg()->SetBlob("Gui/Widget_Ratings_Columns",m_pRatings->header()->saveState());

	killTimer(m_TimerId);
}

void CRatingView::ShowRating(uint64 ID)
{
	m_ID = ID;
	m_pRatings->clear();
	if(m_ID != 0)
		ShowRating();
}

class CRatingSyncJob: public CInterfaceJob
{
public:
	CRatingSyncJob(CRatingView* pView, uint64 ID)
	{
		m_pView = pView;
		m_Request["ID"] = ID;
		//m_Request["Section"] = "Rating";
	}

	virtual QString			GetCommand()	{return "GetFile";}
	virtual void			HandleResponse(const QVariantMap& Response) 
	{
		if(m_pView)
			m_pView->SyncRatings(Response);
	}
	virtual void			Finish(bool bOK)
	{
		if(m_pView)
			m_pView->m_pRatingSyncJob = NULL;
		CInterfaceJob::Finish(bOK);
	}

protected:
	QPointer<CRatingView>	m_pView; // Note: this can be deleted at any time
};

void CRatingView::ShowRating()
{
	if(m_pRatingSyncJob == NULL)
	{
		m_pRatingSyncJob = new CRatingSyncJob(this, m_ID);
		theGUI->ScheduleJob(m_pRatingSyncJob);
	}
}

void CRatingView::SyncRatings(const QVariantMap& Response)
{
	QMap<uint64, QTreeWidgetItem*> OldRatings;
	for(int i = 0; i < m_pRatings->topLevelItemCount(); ++i) 
	{
		QTreeWidgetItem* pItem = m_pRatings->topLevelItem(i);
		//QString ID = pItem->data(eRating, Qt::UserRole).toString();
		uint64 ID = pItem->data(eName, Qt::UserRole).toULongLong();
		ASSERT(!OldRatings.contains(ID));
		OldRatings.insert(ID,pItem);
	}

	foreach (const QVariant vRating, Response["Ratings"].toList())
	{
		QVariantMap Rating = vRating.toMap();
		uint64 ID = Rating["ID"].toULongLong();

		QTreeWidgetItem* pItem = OldRatings.take(ID);
		if(!pItem)
		{
			pItem = new QTreeWidgetItem();
			//pItem->setData(eRating, Qt::UserRole, ID);
			pItem->setData(eName, Qt::UserRole, ID);
			pItem->setText(eName, Rating["SourceID"].toString());
			m_pRatings->addTopLevelItem(pItem);
		}

		QMap<QString, QTreeWidgetItem*> OldItems;
		for(int i = 0; i < pItem->childCount(); ++i) 
		{
			QTreeWidgetItem* pSubItem = pItem->child(i);
			QString ID = pSubItem->text(eName);
			ASSERT(!OldItems.contains(ID));
			OldItems.insert(ID,pSubItem);
		}

		foreach(const QString& Key, Rating.uniqueKeys())
		{
			QTreeWidgetItem* pSubItem = OldItems.take(Key);
			if(!pSubItem)
			{
				pSubItem = new QTreeWidgetItem();
				pSubItem->setText(eName, Key);
				pItem->addChild(pSubItem);
			}

			pSubItem->setText(eValue, Rating[Key].toString());
		}

		foreach(QTreeWidgetItem* pSubItem, OldItems)
			delete pSubItem;

		/*QString RatingStr;
		switch(Rating["Rating"].toInt())
		{
		case 0:	RatingStr = tr("Not Rated");break;
		case 1:	RatingStr = tr("Fake");		break;
		case 2:	RatingStr = tr("Poor");		break;
		case 3:	RatingStr = tr("Fair");		break;
		case 4:	RatingStr = tr("Good");		break;
		case 5:	RatingStr = tr("Excellent");break;
		}

		pItem->setText(eRating, RatingStr);
		pItem->setText(eDescription, Rating["Description"].toString());
		pItem->setText(eFileName, Rating["FileName"].toString());*/
	}

	foreach(QTreeWidgetItem* pItem, OldRatings)
		delete pItem;
}

void CRatingView::timerEvent(QTimerEvent *e)
{
    if (e->timerId() != m_TimerId) 
	{
        QObject::timerEvent(e);
		return;
    }

	//if(m_ID != 0)
	//	ShowRating();
}

//void CRatingView::OnMenuRequested(const QPoint &point)
//{
//	m_pMenu->popup(QCursor::pos());	
//}
