#include "GlobalHeader.h"
#include "ShareWidget.h"
#include "SettingsView.h"
#include "../NeoGUI.h"

class CFileShareModel: public QFileSystemModel
{
public:
	QStringList checkedPaths()
	{
		QStringList paths;
		foreach(const QPersistentModelIndex& index, checklist)
		{
			QString path = fileInfo(index).absoluteFilePath();
			if(fileInfo(index).isDir() && path.right(1) != "/")
				path.append("/");
			paths.append(path);
		}
		return paths;
	}

	void setCheckedPaths(const QStringList& paths)
	{
		checklist.clear();
		foreach(const QString& path, paths)
		{
			checklist.insert(index(path));
		}
	}

protected:
	QVariant data(const QModelIndex& index, int role) const 
	{
		if (role == Qt::CheckStateRole && index.column() == 0)
			return checklist.contains(index) ? Qt::Checked : Qt::Unchecked;
		return QFileSystemModel::data(index, role);
	}

	Qt::ItemFlags flags(const QModelIndex& index) const 
	{
		Qt::ItemFlags flags = QFileSystemModel::flags(index);
		if(index.column() == 0)
			flags |= Qt::ItemIsUserCheckable;
		return flags;
	}

	bool setData(const QModelIndex& index, const QVariant& value, int role) 
	{
		if (role == Qt::CheckStateRole) 
		{
			if (value == Qt::Checked) 
				checklist.insert(index);
			else 
				checklist.remove(index);
			emit dataChanged(index, index);
			return true;
		}
		return QFileSystemModel::setData(index, value, role);
	}

private:
	QSet<QPersistentModelIndex> checklist;

};

CShareWidget::CShareWidget(QMultiMap<int, SField> Layout, const QString& Title, QWidget *parent)
:CSettingsView(Layout, Title, parent)
{
	//m_pMainLayout = new QVBoxLayout();
	//m_pMainLayout->setMargin(0);

	m_pSplitter = new QSplitter();
	m_pSplitter->setOrientation(Qt::Vertical);

	m_pFsModel = NULL;

	m_pFsTree = new QTreeView();
#ifdef WIN32
	m_pFsTree->setStyle(QStyleFactory::create("windowsxp"));
#endif
	m_pFsTree->setSortingEnabled(true);
	connect(m_pFsTree, SIGNAL(clicked(const QModelIndex&)), this, SLOT(OnItemClicked(const QModelIndex&)));
	
	m_pSharedList = new QListWidget();
	connect(m_pSharedList, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(OnItemDoubleClicked(QListWidgetItem*)));

	m_pSplitter->addWidget(m_pFsTree);
	m_pSplitter->addWidget(m_pSharedList);

	m_pMainLayout->addWidget(m_pSplitter);

	//setLayout(m_pMainLayout);

	m_pSplitter->restoreState(theGUI->Cfg()->GetBlob("Gui/Widget_Share_Spliter"));
}

CShareWidget::~CShareWidget()
{
	theGUI->Cfg()->SetBlob("Gui/Widget_Share_Spliter",m_pSplitter->saveState());
	if(m_pFsModel)
		theGUI->Cfg()->SetBlob("Gui/Widget_Share_Columns",m_pFsTree->header()->saveState());
}

class CShareUpdateJob: public CInterfaceJob
{
public:
	CShareUpdateJob(CShareWidget* pView)
	{
		m_pView = pView;

		QStringList Options;
		Options.append("NeoCore/Content/Shared");
		m_Request["Options"] = Options;
	}

	virtual QString			GetCommand()	{return "GetCore";}
	virtual void			HandleResponse(const QVariantMap& Response) 
	{
		if(m_pView)
		{
			QVariantMap Options = Response["Options"].toMap();
			QStringList Share;
			foreach(const QString& Path, Options["NeoCore/Content/Shared"].toStringList())
			{
				if(QFile::exists(Path))
					Share.append(Path);
			}
			m_pView->m_pSharedList->clear();
			m_pView->m_pSharedList->insertItems(0, Share);
			m_pView->m_pFsModel->setCheckedPaths(Share);
		}
	}
protected:
	QPointer<CShareWidget>	m_pView; // Note: this can be deleted at any time
};

void CShareWidget::UpdateSettings()
{
	m_pFsModel = new CFileShareModel();
	m_pFsModel->setRootPath("");
	m_pFsTree->setModel(m_pFsModel);

	m_pFsTree->header()->restoreState(theGUI->Cfg()->GetBlob("Gui/Widget_Share_Columns"));

	CSettingsView::UpdateSettings();

	CShareUpdateJob* pShareUpdateJob = new CShareUpdateJob(this);
	theGUI->ScheduleJob(pShareUpdateJob);
}

class CShareApplyJob: public CInterfaceJob
{
public:
	CShareApplyJob(const QStringList& Share)
	{
		QVariantMap Options;
		Options["NeoCore/Content/Shared"] = Share;
		m_Request["Options"] = Options;
	}

	virtual QString			GetCommand()	{return "SetCore";}
	virtual void			HandleResponse(const QVariantMap& Response) {}
};

void CShareWidget::ApplySettings()
{
	if(!m_pFsModel)
		return;

	CSettingsView::ApplySettings();

	QStringList Paths = m_pFsModel->checkedPaths();

	CShareApplyJob* pShareApplyJob = new CShareApplyJob(Paths);
	theGUI->ScheduleJob(pShareApplyJob);
}

void CShareWidget::OnItemClicked(const QModelIndex& index)
{
	QStringList Paths = m_pFsModel->checkedPaths();

	m_pSharedList->clear();
	m_pSharedList->insertItems(0, Paths);
}

void CShareWidget::OnItemDoubleClicked(QListWidgetItem* item)
{
	QString Path = item->text();
	QModelIndex index = m_pFsModel->index(Path);
	m_pFsTree->setCurrentIndex(index);
}
