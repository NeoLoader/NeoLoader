#include "GlobalHeader.h"
#include "SearchWindow.h"
#include "FileListView.h"
#include "FileListWidget.h"
#include "ServicesWidget.h"
#include "../NeoGUI.h"
#include "../Common/TreeWidgetEx.h"
#include "./OnlineSearch/OnlineCompleter.h"
#include "./OnlineSearch/OnlineFileList.h"
#include <QStyledItemDelegate>
#include "../Common/Common.h"

class QStyledItemDelegateEx : public QStyledItemDelegate
{
public:
	QStyledItemDelegateEx(QObject* parent = 0) : QStyledItemDelegate(parent) {}

	QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
	{
		QStringList Options = index.data(Qt::UserRole).toStringList();
		if (index.column() == 1 && !Options.isEmpty()) 
		{
			QComboBox *cb = new QComboBox(parent);
			cb->addItems(Options);
			cb->setFrame(false);
			return cb;
		}
		return QStyledItemDelegate::createEditor(parent, option, index);
	}
};

CSearchWindow::CSearchWindow(QWidget *parent)
:QWidget(parent)
{
	m_pSearchSyncJob = NULL;

	m_pMainLayout = new QVBoxLayout(this);
	m_pMainLayout->setMargin(1);

	m_pSearchWidget = new QWidget();
	m_pSearchLayout = new QGridLayout();
	m_pSearchLayout->setMargin(3);

	m_pExpression = new QLineEdit();
	m_pExpression->setMinimumWidth(300);
	connect(m_pExpression, SIGNAL(returnPressed()), this, SLOT(OnStart()));

//#ifndef _DEBUG
//	COnlineCompleter* pOnlineCompleter = new COnlineCompleter();
//	m_pExpression->setCompleter(pOnlineCompleter);
//	connect(m_pExpression, SIGNAL(textEdited(const QString&)), pOnlineCompleter, SLOT(OnUpdate(const QString&)));
//#endif

	m_pNetwork = new QComboBox();
	m_pNetwork->addItem(tr("Smart Search"), "SmartAgent");
	m_pNetwork->addItem(tr("Neo Kad"), "NeoKad");
	m_pNetwork->addItem(tr("Mule Kad"), "MuleKad");
	m_pNetwork->addItem(tr("Ed2k Servers"), "Ed2kServer");
	m_pNetwork->addItem(tr("Online"), "WebSearch");
	connect(m_pNetwork, SIGNAL(activated(int)), this, SLOT(OnNetwork(int)));

	m_pType = new QComboBox();
	m_pType->addItem(tr("All Downloads"), "");
	m_pType->addItem(tr("Neo Downloads"), "neo");
	m_pType->addItem(tr("Torrent Downloads"), "btih");
	m_pType->addItem(tr("Hoster Downloads"), "arch");
	m_pType->addItem(tr("eMule/ed2k Downloads"), "ed2k");
	m_pType->addItem(tr("P2P Downloads"), "neo|btih|ed2k");

	m_pSite = new QComboBox();
	m_pSite->addItem(tr("downloadstube.net"));
	QTimer::singleShot(100, this, SLOT(LoadFinders()));

	m_pAux = new QStackedWidget();
	m_pAux->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	m_pAux->addWidget(new QLabel(""));
	m_pAux->addWidget(m_pType);
	m_pAux->addWidget(m_pSite);

	m_pStart = new QPushButton(tr("Start"));
	connect(m_pStart, SIGNAL(pressed()), this, SLOT(OnStart()));

	m_pGoToNew = new QCheckBox(tr("Goto New"));
	connect(m_pGoToNew, SIGNAL(clicked(bool)), this, SLOT(OnGoToNew(bool)));

	m_pCriteria = new QTableWidget(0,3);
	connect(m_pCriteria, SIGNAL(cellChanged(int, int)), this, SLOT(OnCriteriaChanged(int, int)));
	m_pCriteria->horizontalHeader()->hide();
	m_pCriteria->verticalHeader()->hide();
	m_pCriteria->setItemDelegate(new QStyledItemDelegateEx(this));

	m_pCriteria->setRowCount(1);
	m_pCriteria->setColumnCount(3);

	m_pNetwork->setCurrentIndex(m_pNetwork->findData(theGUI->Cfg()->GetString("Gui/Widget_SearchMode")));
	OnNetwork(m_pNetwork->currentIndex());
	m_pType->setCurrentIndex(m_pType->findData(theGUI->Cfg()->GetString("Gui/Widget_SearchType")));

	QLabel* pLabel = new QLabel(tr("Search Expression:"));
	pLabel->setMaximumHeight(10);
	m_pSearchLayout->addWidget(pLabel, 0, 0);
	m_pSearchLayout->addWidget(m_pExpression, 1, 0, 1, 3);
	m_pSearchLayout->addWidget(m_pNetwork, 2, 0);
	m_pSearchLayout->addWidget(m_pAux, 2, 1);
	m_pSearchLayout->addWidget(m_pStart, 1, 3);
	m_pSearchLayout->addWidget(m_pGoToNew, 2, 3);
	m_pSearchLayout->addWidget(m_pCriteria, 0, 4, 3, 1);

	m_pSearchWidget->setLayout(m_pSearchLayout);	

	m_pMainLayout->addWidget(m_pSearchWidget);

	m_pSearchTree = new QTreeWidgetEx();
	m_pSearchTree->setHeaderLabels(tr("Expression|Status|Found Files").split("|"));
	//m_pSearchTree->setContextMenuPolicy(Qt::CustomContextMenu);
	//connect(m_pSearchTree, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenuRequested(const QPoint &)));
	//connect(m_pSearchTree, SIGNAL(itemClicked(QTreeWidgetItem*, int)), this, SLOT(OnItemClicked(QTreeWidgetItem*, int)));
	connect(m_pSearchTree, SIGNAL(itemSelectionChanged()), this, SLOT(OnSelectionChanged()));
	//connect(m_pSearchTree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(OnItemDoubleClicked(QTreeWidgetItem*, int)));
	m_pMainLayout->addWidget(m_pSearchTree);

	m_pSearchTree->header()->restoreState(theGUI->Cfg()->GetBlob("Gui/Widget_Searches_Columns"));

	setLayout(m_pMainLayout);

	m_TimerId = startTimer(500);
}

CSearchWindow::~CSearchWindow()
{
	theGUI->Cfg()->SetSetting("Gui/Widget_SearchMode", m_pNetwork->itemData(m_pNetwork->currentIndex()));
	theGUI->Cfg()->SetSetting("Gui/Widget_SearchType", m_pType->itemData(m_pType->currentIndex()));
	theGUI->Cfg()->SetSetting("Gui/Widget_SearchSite", m_pSite->currentText());

	theGUI->Cfg()->SetBlob("Gui/Widget_Searches_Columns",m_pSearchTree->header()->saveState());

	killTimer(m_TimerId);
}

void CSearchWindow::LoadFinders()
{
	QStringList Finders = g_Services->GetFinders();
	if(Finders.isEmpty())
		QTimer::singleShot(100, this, SLOT(LoadFinders()));
	else
	{
		foreach(const QString& Finder, Finders)
			m_pSite->addItem(Finder);

		m_pSite->setCurrentText(theGUI->Cfg()->GetString("Gui/Widget_SearchSite"));
	}
}

void CSearchWindow::OnNetwork(int Index)
{
	m_pCombo = new QComboBox();
	m_pCombo->setMinimumWidth(150);
	connect(m_pCombo, SIGNAL(activated(int)), this, SLOT(OnCriteria(int)));
	QString SearchNet = m_pNetwork->itemData(m_pNetwork->currentIndex()).toString();
	m_pCombo->addItem("...");
	if(SearchNet == "NeoKad" || SearchNet == "SmartAgent")
	{
		m_pAux->setCurrentWidget(m_pType);
		
		m_pCombo->addItem(tr("Category"),"Category");
		m_pCombo->addItem(tr("File Type"),"FileType");
		m_pCombo->addItem(tr("Min Size"),"MinSize");
		m_pCombo->addItem(tr("Max Size"),"MaxSize");
		m_pCombo->addItem(tr("Min Results"),"MinResults");
		m_pCombo->addItem(tr("Custom"),"");
	}
	else if(SearchNet == "MuleKad" || SearchNet == "Ed2kServer")
	{
		m_pAux->setCurrentIndex(0);
		
		m_pCombo->addItem(tr("File Ext"),"FileExt");
		m_pCombo->addItem(tr("Min Size"),"MinSize");
		m_pCombo->addItem(tr("Max Size"),"MaxSize");
		m_pCombo->addItem(tr("Availability"),"Availability");
		if(SearchNet == "Ed2kServer")
			m_pCombo->addItem(tr("Min Results"),"MinResults");
		
	}
	else if(SearchNet == "WebSearch")
	{
		m_pAux->setCurrentWidget(m_pSite);

		m_pCombo->addItem(tr("Min Results"),"MinResults");
		m_pCombo->addItem(tr("Custom"),"");
	}

	int LastRow = m_pCriteria->rowCount() - 1;
	m_pCriteria->setCellWidget(LastRow, 0, m_pCombo);
	m_pCriteria->setCellWidget(LastRow, 2, new QLabel(""));
	//m_pCriteria->setItem(LastRow, 1, new QTableWidgetItem("..."));

#if QT_VERSION < 0x050000
	m_pCriteria->horizontalHeader()->setResizeMode(1, QHeaderView::Stretch);
#else
	m_pCriteria->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
#endif
	m_pCriteria->horizontalHeader()->resizeSection(0, m_pCombo->width()+1);
	m_pCriteria->verticalHeader()->resizeSection(0, m_pCombo->height()+1);
	m_pCriteria->horizontalHeader()->resizeSection(2, 16+1);
}

void CSearchWindow::OnCriteria(int Index)
{
	QString Text = m_pCombo->currentText();
	if(Text == "...")
		return;
	QVariant Data = m_pCombo->itemData(m_pCombo->currentIndex(), Qt::UserRole);

	for(int i=0; i < m_pCriteria->rowCount() - 1; i++)
	{
		if(m_pCriteria->item(i, 0)->data(Qt::UserRole) == Data)
			return; // we already have this one in list;
	}

	int NextRow = m_pCriteria->rowCount();
	m_pCriteria->setRowCount(NextRow + 1);
	m_pCriteria->verticalHeader()->resizeSection(NextRow, m_pCombo->height()+1);

	m_pCriteria->setCellWidget(NextRow-1, 0, NULL); // this deletes m_pCombo !!
	QTableWidgetItem* pItem = new QTableWidgetItem(Text);
	pItem->setData(Qt::UserRole, Data);
	if(!Data.toString().isEmpty())
		pItem->setFlags(pItem->flags() & ~Qt::ItemIsEditable);
	m_pCriteria->setItem(NextRow-1, 0, pItem);

	QTableWidgetItem* pValue = new QTableWidgetItem();
	pValue->setData(Qt::UserRole, GetOptions(Data.toString()));
	m_pCriteria->setItem(NextRow-1, 1, pValue);

	QPushButton* pButton = new QPushButton(tr("X"));
	connect(pButton, SIGNAL(clicked(bool)), this, SLOT(OnRemoveCriteria()));
	m_pCriteria->setCellWidget(NextRow-1, 2, pButton);
	
	OnNetwork(m_pNetwork->currentIndex());
}

QStringList CSearchWindow::GetOptions(const QString& Name)
{
	QStringList Options;
	if(Name == "Category")
	{
		Options.append("");
		Options.append("Series");
		Options.append("Movies");
		Options.append("Video");
		Options.append("Audio");
		Options.append("Pictures");
		Options.append("Documents");
		Options.append("Books");
		Options.append("Music");
		Options.append("Software");
		Options.append("Games");
		Options.append("Adult");
		//Options.append("Other");
	}
	return Options;
}

void CSearchWindow::OnCriteriaChanged(int row, int column)
{
	if(column == 0)
	{
		if(m_pCriteria->item(row, column)->text().isEmpty())
			m_pCriteria->removeRow(row);
	}
}

void CSearchWindow::OnRemoveCriteria()
{
	for(int i=0; i < m_pCriteria->rowCount(); i++)
	{
		if(m_pCriteria->cellWidget(i, 2) == sender())
			m_pCriteria->removeRow(i);
	}
}

class CSearchSyncJob: public CInterfaceJob
{
public:
	CSearchSyncJob(CSearchWindow* pView)
	{
		m_pView = pView;
	}

	virtual QString			GetCommand()	{return "SearchList";}
	virtual void			HandleResponse(const QVariantMap& Response)
	{
		if(m_pView)
			m_pView->SyncSearches(Response);
	}
	virtual void			Finish(bool bOK)
	{
		if(m_pView)
			m_pView->m_pSearchSyncJob = NULL;
		CInterfaceJob::Finish(bOK);
	}

protected:
	QPointer<CSearchWindow>	m_pView; // Note: this can be deleted at any time
};

void CSearchWindow::timerEvent(QTimerEvent *e)
{
    if (e->timerId() != m_TimerId) 
	{
        QObject::timerEvent(e);
		return;
    }

	m_pGoToNew->setChecked(theGUI->Cfg()->GetBool("Gui/GoToNew"));

	if(m_pSearchSyncJob == NULL)
	{
		m_pSearchSyncJob = new CSearchSyncJob(this);
		theGUI->ScheduleJob(m_pSearchSyncJob);
	}
}

void CSearchWindow::OnGoToNew(bool Checked)
{
	theGUI->Cfg()->SetSetting("Gui/GoToNew", Checked);
}

class CStopSearchJob: public CInterfaceJob
{
public:
	CStopSearchJob(uint64 ID)
	{
		m_Request["ID"] = ID;
	}

	virtual QString			GetCommand()	{return "StopSearch";}
	virtual void			HandleResponse(const QVariantMap& Response) {}
};

void CSearchWindow::StopSearch(uint64 ID)
{
	CStopSearchJob* pStopSearchJob = new CStopSearchJob(ID);
	theGUI->ScheduleJob(pStopSearchJob);
}

class CStartSearchJob: public CInterfaceJob
{
public:
	CStartSearchJob(const QString& SearchNet, const QString& Expression, const QVariantMap& Criteria = QVariantMap())
	{
		m_Request["Log"] = true;
		m_Request["SearchNet"] = SearchNet;
		m_Request["Expression"] = Expression;
		m_Request["Criteria"] = Criteria;
	}

	CStartSearchJob(uint64 ID)
	{
		m_Request["Log"] = true;
		m_Request["ID"] = ID;
	}

	virtual QString			GetCommand()	{return "StartSearch";}
	virtual void			HandleResponse(const QVariantMap& Response) {}
};

void CSearchWindow::StartSearch(const QString& SearchNet, const QString& Expression, const QVariantMap& Criteria)
{
	CStartSearchJob* pStartSearchJob = new CStartSearchJob(SearchNet, Expression, Criteria);
	theGUI->ScheduleJob(pStartSearchJob);
}

void CSearchWindow::UpdateSearch(uint64 ID)
{
	CStartSearchJob* pStartSearchJob = new CStartSearchJob(ID);
	theGUI->ScheduleJob(pStartSearchJob);
}

void CSearchWindow::OnStart()
{
	QVariantMap Criteria;
	for(int i=0; i < m_pCriteria->rowCount() - 1; i++)
	{
		QString Name = m_pCriteria->item(i, 0)->data(Qt::UserRole).toString();
		if(Name.isEmpty())
			Name = m_pCriteria->item(i, 0)->text();
		QString Value = m_pCriteria->item(i, 1)->text();
		if(Name == "MaxSize" || Name == "MinSize")
		{
			if(Value.contains("kb", Qt::CaseInsensitive))
				Value = QString::number((int) Value.mid(0, Value.length()-2).trimmed().toDouble() * 1024);
			else if(Value.contains("mb", Qt::CaseInsensitive))
				Value = QString::number((int) Value.mid(0, Value.length()-2).trimmed().toDouble() * 1024*1024);
			else if(Value.contains("gb", Qt::CaseInsensitive))
				Value = QString::number((int) Value.mid(0, Value.length()-2).trimmed().toDouble() * 1024*1024*1024);
		}
		Criteria[Name] = Value;
	}

	QString SearchNet = m_pNetwork->itemData(m_pNetwork->currentIndex()).toString();

	if(SearchNet == "NeoKad" || SearchNet == "SmartAgent")
		Criteria["Type"] = m_pType->itemData(m_pType->currentIndex());
	else if(SearchNet == "WebSearch")
		Criteria["Site"] = m_pSite->currentText();

	StartNewSearch(SearchNet, m_pExpression->text(), Criteria);
}

void CSearchWindow::RestoreSearch(const QString& SearchNet, const QString& Expression, const QVariantMap& Criteria)
{
	m_pExpression->setText(Expression);

	while(m_pCriteria->rowCount() > 1)
		m_pCriteria->removeRow(0);

	int Index = m_pNetwork->findData(SearchNet);
	if(Index != -1)
		m_pNetwork->setCurrentIndex(Index);

	if(SearchNet == "NeoKad" || SearchNet == "SmartAgent")
	{
		int SubIndex = m_pType->findData(Criteria["Type"].toString());
		if(SubIndex != -1)
			m_pType->setCurrentIndex(SubIndex);
	}
	else if(SearchNet == "WebSearch")
		m_pSite->setCurrentText(Criteria["Site"].toString());

	int Counter = 0;
	foreach(const QString& Text, Criteria.uniqueKeys())
	{
		m_pCriteria->insertRow(Counter);
		m_pCriteria->verticalHeader()->resizeSection(Counter, m_pCombo->height()+1);
		
		int Index = m_pCombo->findData(Text);
		QString Data;
		QString Name;
		if(Index == -1)
			Name = Text;
		else
		{
			Data = Text;
			Name = m_pCombo->itemText(Index);
		}

		QString Value = Criteria[Text].toString();
		if(Text == "MaxSize" || Text == "MinSize")
			Value = FormatSize(Value.toULongLong());

		QTableWidgetItem* pItem = new QTableWidgetItem(Name);
		pItem->setData(Qt::UserRole, Data);
		if(!Data.isEmpty())
			pItem->setFlags(pItem->flags() & ~Qt::ItemIsEditable);
		m_pCriteria->setItem(Counter, 0, pItem);

		QTableWidgetItem* pValue = new QTableWidgetItem(Value);
		pValue->setData(Qt::UserRole, GetOptions(Name));
		m_pCriteria->setItem(Counter, 1, pValue);

		QPushButton* pButton = new QPushButton(tr("X"));
		connect(pButton, SIGNAL(clicked(bool)), this, SLOT(OnRemoveCriteria()));
		m_pCriteria->setCellWidget(Counter, 2, pButton);
		Counter++;
	}
}

void CSearchWindow::SyncSearches(const QVariantMap& Response)
{
	QMap<uint64, SSearch*> OldSearches = m_Searches;

	QList<QTreeWidgetItem*> NewItems;
	foreach (const QVariant vSearch, Response["Searches"].toList())
	{
		QVariantMap Search = vSearch.toMap();
		uint64 SearchID = Search["ID"].toULongLong();

		QString Expression = Search["Expression"].toString();
#ifdef _DEBUG
		if(Expression.isEmpty())
			Expression = "...";
#endif
		if(Expression.isEmpty())
			continue;

		SSearch* pSearch = OldSearches.take(SearchID);
		if(!pSearch)
		{
			pSearch = new SSearch();
			pSearch->Expression = Expression;
			pSearch->SearchNet = Search["SearchNet"].toString();
			pSearch->Criteria = Search["Criteria"].toMap();
			m_Searches.insert(SearchID, pSearch);

			pSearch->pItem = new QTreeWidgetItem();
			pSearch->pItem->setData(eExpression, Qt::UserRole, SearchID);
			NewItems.append(pSearch->pItem);
		}

		pSearch->pItem->setText(eExpression, Expression);
		pSearch->pItem->setText(eStatus, Search["Status"].toString());
		pSearch->pItem->setText(eFoundFiles, Search["Count"].toString());
	}
	m_pSearchTree->addTopLevelItems(NewItems);

	foreach(SSearch* pSearch, OldSearches)
	{
		m_Searches.remove(OldSearches.key(pSearch));
		delete pSearch->pItem;
		delete pSearch;
	}
}

void CSearchWindow::StartNewSearch(const QString& SearchNet, const QString& Expression, const QVariantMap& Criteria)
{
	if(SearchNet == "WebSearch" && m_pSite->currentIndex() == 0)
	{
		emit OnLine(Expression);
		return;
	}

	StartSearch(SearchNet, Expression, Criteria);
}

void CSearchWindow::OnItemClicked(QTreeWidgetItem* pItem, int Column)
{
	uint64 SearchID = pItem->data(eExpression, Qt::UserRole).toULongLong();
	if(SSearch* pSearch = m_Searches.value(SearchID))
		RestoreSearch(pSearch->SearchNet, pSearch->Expression, pSearch->Criteria);
}

void CSearchWindow::OnSelectionChanged()
{
	if(QTreeWidgetItem* pItem = m_pSearchTree->currentItem())
		OnItemClicked(pItem, 0);
}

//void CSearchWindow::OnItemDoubleClicked(QTreeWidgetItem* pItem, int Column)
//{
//
//}

//void CSearchWindow::OnMenuRequested(const QPoint &point)
//{
//	m_pMenu->popup(QCursor::pos());	
//}