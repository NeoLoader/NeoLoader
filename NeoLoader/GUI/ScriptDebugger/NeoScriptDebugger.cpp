#include "GlobalHeader.h"
#include "NeoScriptDebugger.h"
#include "../../../NeoScriptTools/JSDebugging/JSScriptDebuggerFrontend.h"
#include "../NeoLoader.h"
#include "../../Interface/CoreClient.h"
#include "../../Interface/CoreServer.h"
#include "./ResourceView/ResourceWidget.h"
#include "./ResourceView/ResourceView.h"
#include "SessionLogWidget.h"
#include "ScriptRepositoryDialog.h"

#include <QApplication>
#include <QDockWidget>
#include <QMainWindow>
#include <QMenuBar>
#include <QStatusBar>
#include <QBoxLayout>
#include <QApplication>
#include <QToolBar>
#include <QClipboard>
#include <QComboBox>
#include <QCheckBox>

#if QT_VERSION < 0x050000
CNeoScriptDebugger::CNeoScriptDebugger(QWidget *parent, Qt::WFlags flags)
#else
CNeoScriptDebugger::CNeoScriptDebugger(QWidget *parent, Qt::WindowFlags flags)
#endif
: CJSScriptDebugger(parent, flags),
	m_ResourceWidget(0), m_SessionLogWidget(0), m_ScriptRepositoryDialog(0),
	m_NewAction(0), m_LoadAction(0), m_SaveAction(0), m_SaveAsAction(0), m_DeleteAction(0), m_ClearAction(0),
	m_DefaultAction(0), m_HtmlCodeAction(0), m_RawTraceAction(0), m_DomTreeAction(0), m_CopyUrlAction(0), m_HoldAction(0), m_SetInterceptorAction(0), m_ClearInterceptorAction(0)
{
	m_TimerId = -1;

	m_WebViewAvailable = CWebKitView::Init();
}

CNeoScriptDebugger::~CNeoScriptDebugger()
{
	killTimer(m_TimerId);
}

void CNeoScriptDebugger::setup()
{
	// Setup MenuBar
	menuBar()->addMenu(createFileMenu(this));

	CJSScriptDebugger::setup();

	// Setup Dock
    QDockWidget *resourcesDock = new QDockWidget(this);
	resourcesDock->setObjectName(QLatin1String("qtscriptdebugger_webResourcesDockWidget"));
    resourcesDock->setWindowTitle(tr("Resources"));
    resourcesDock->setWidget(widget(-1));
    addDockWidget(Qt::LeftDockWidgetArea, resourcesDock);

    QDockWidget *logDock = new QDockWidget(this);
	logDock->setObjectName(QLatin1String("qtscriptdebugger_webSessionLogDockWidget"));
    logDock->setWindowTitle(tr("Session Log"));
    logDock->setWidget(widget(-2));
    addDockWidget(Qt::BottomDockWidgetArea, logDock);

	QDockWidget *consoleDock = qobject_cast<QDockWidget*>(widget(ConsoleWidget)->parent());
	tabifyDockWidget(consoleDock, logDock);

	m_ResourceWidget->SetContextMenu(createResourceMenu(m_ResourceWidget));

	m_ScriptRepositoryDialog = new CScriptRepositoryDialog(this);
	m_ScriptRepositoryDialog->resize(320,480);

	m_TimerId = startTimer(75); // pull events 
}

void CNeoScriptDebugger::attachTo(CJSScriptDebuggerFrontend *frontend)
{
	CJSScriptDebugger::attachTo(frontend);

	connect(frontend, SIGNAL(processCustom(const QVariant&)), this, SLOT(ProcessCustom(const QVariant&)));
}

CJSScriptDebuggerFrontend* CNeoScriptDebugger::frontend()
{
	return (CJSScriptDebuggerFrontend*)m_frontend;
}

void CNeoScriptDebugger::timerEvent(QTimerEvent *e)
{
    if (e->timerId() != m_TimerId) 
	{
		QObject::timerEvent(e);
		return;
	}

	if(m_ResourceWidget)
	{
		QVariantMap Request;
		Request["Action"] = "ListResources";
		frontend()->sendCustom(Request);
	}

	if(QScriptDebuggerCustomViewInterface* pCustom = currentCustom())
	{
		if(!m_HoldAction->isChecked())
		{
			QVariantMap Request;
			Request["Action"] = "GetResource";
			Request["Handle"] = m_ResourceWidget->GetResource(pCustom->customID())["Handle"];
			if(pCustom->inherits("CRawTraceView"))
				Request["Type"] = "RawTrace";
			frontend()->sendCustom(Request);
		}
	}

	if(m_ScriptRepositoryDialog->isVisible())
	{
		QVariantMap Request;
		Request["Action"] = "ListAllScripts";
		frontend()->sendCustom(Request);
	}

	QVariantMap Request;
	Request["Action"] = "GetStatus";
	if(m_SessionLogWidget)
		Request["LastID"] = m_SessionLogWidget->GetLastID();
	frontend()->sendCustom(Request);
}

void CNeoScriptDebugger::ProcessCustom(const QVariant& var)
{
	QVariantMap Response = var.toMap();
	if(Response["Action"] == "GetStatus")
	{
		bool bIdle = !Response["Evaluating"].toBool();

		m_NewAction->setEnabled(bIdle || isInteractive());
		m_LoadAction->setEnabled(bIdle || isInteractive());
		m_SaveAction->setEnabled(bIdle);
		if(bIdle)
			action(ContinueAction)->setEnabled(true);

		QString Message = Response["Status"].toString();

		if(Response.contains("UpRate") || Response.contains("DownRate"))
			Message += QString(" (Dl:%1kb Ul:%2kb)").arg(Response["DownRate"].toInt()/1024.0).arg(Response["UpRate"].toInt()/1024.0);

		statusBar()->showMessage(Message);
		setWindowTitle(tr("Neo Script Debugger - ") + Response["Url"].toString());

		if(m_SessionLogWidget)
			m_SessionLogWidget->UpdateLog(Response["Log"]);
	}
	else if(Response["Action"] == "ListResources")
	{
		m_ResourceWidget->SyncResources(Response["Resources"]);
	}
	else if(Response["Action"] == "GetResource")
	{
		if(QScriptDebuggerCustomViewInterface* pCustom = currentCustom())
		{
			if(m_ResourceWidget->GetResource(pCustom->customID())["Handle"] == Response["Handle"])
			{
				if(pCustom->inherits("CDomTreeView"))
				{
					QVariantMap Resource = Response["Resource"].toMap();
					if(Resource["TextData"] != pCustom->text())
					{
						QVariantMap Request;
						Request["Action"] = "GetResource";
						Request["Handle"] = Response["Handle"];
						Request["Type"] = "DomTree";
						frontend()->sendCustom(Request);
					}
				}
				
				pCustom->setData(Response["Resource"]);
			}
		}
	}
	else if(Response["Action"] == "ListAllScripts")
	{
		m_ScriptRepositoryDialog->SyncScripts(Response["Scripts"]);
	}
}

void CNeoScriptDebugger::SaveScript(const QString& FileName, const QString& Script)
{
	QVariantMap Request;
	Request["Action"] = "SaveScript";
	Request["FileName"] = FileName;
	Request["Script"] = Script;
	frontend()->sendCustom(Request);
}

void CNeoScriptDebugger::LoadScript(const QString& FileName)
{
	QVariantMap Request;
	Request["Action"] = "LoadScript";
	Request["FileName"] = FileName;
	frontend()->sendCustom(Request);
}

void CNeoScriptDebugger::DeleteScript(const QString& FileName)
{
	QVariantMap Request;
	Request["Action"] = "DeleteScript";
	Request["FileName"] = FileName;
	frontend()->sendCustom(Request);
}

void CNeoScriptDebugger::SetInterceptor(bool bSet)
{
	qint64 ID = m_ResourceWidget->CurrentResourceID();
	QString Url = m_ResourceWidget->GetResource(ID)["Url"].toString();
	QVariantMap Request;
	Request["Action"] = "SetInterceptor";
	Request["Url"] = Url;
	if(bSet)
	{
		QScriptDebuggerCustomViewInterface* view = currentCustom();
		if(!view)
			return;
		Request["TextData"] = view->text();
	}
	frontend()->sendCustom(Request);
}

void CNeoScriptDebugger::ShowResource(qint64 ID)
{
	QString Url = m_ResourceWidget->GetResource(ID)["Url"].toString();
	QVariantMap Resource = m_ResourceWidget->GetResource(ID);
	if(Resource["Type"] == "Binary")
	{
		if(Resource["ContentType"].toString().contains("image", Qt::CaseInsensitive))
			setCurrentCustom(ID, Url, CImageView::factory);
	}
	else if(m_WebViewAvailable && Resource["ContentType"].toString().contains("html", Qt::CaseInsensitive))
		setCurrentCustom(ID, Url, CWebKitView::factory);
	else
		setCurrentCustom(ID, Url, CHtmlCodeView::factory);
}

void CNeoScriptDebugger::UpdateResource(qint64 ID)
{
	QVariantMap Resource = m_ResourceWidget->GetResource(ID);

	m_DefaultAction->setEnabled(Resource["Type"] != "Binary" || Resource["ContentType"].toString().contains("image", Qt::CaseInsensitive));
	m_HtmlCodeAction->setEnabled(Resource["Type"] != "Binary");
	m_DomTreeAction->setEnabled(Resource["Type"] == "Page");

	m_ClearInterceptorAction->setEnabled(Resource["Status"] == "Intercepted");
}

void CNeoScriptDebugger::OnNew()
{
	m_ScriptRepositoryDialog->setWindowTitle(tr("Save New Script..."));
	QString FileName = m_ScriptRepositoryDialog->SelectScript(tr("Save"));
	if(FileName != "")
	{
		QString Default = 
		"/*\r\n"
		"*	Name: %1\r\n"
		"*	Version: 1\r\n"
		"*\r\n"
		"*/\r\n"
		"\r\n"
		"	debugger;\r\n";

		SaveScript(FileName, Default.arg(FileName.left(FileName.length() - 3)));
		LoadScript(FileName);
	}
}

void CNeoScriptDebugger::OnLoad()
{
	m_ScriptRepositoryDialog->setWindowTitle(tr("Load Script..."));
	QString FileName = m_ScriptRepositoryDialog->SelectScript(tr("Load"));
	if(FileName != "")
		LoadScript(FileName);
}

void CNeoScriptDebugger::OnSave()
{
	QString FileName = currentName();
	if(FileName != "")
		SaveScript(FileName, currentText(true));
	else
		OnSaveAs();
}

void CNeoScriptDebugger::OnSaveAs()
{
	m_ScriptRepositoryDialog->setWindowTitle(tr("Save Script As..."));
	QString FileName = m_ScriptRepositoryDialog->SelectScript(tr("Save"), currentName());
	if(FileName != "")
	{
		SaveScript(FileName, currentText(true));
		renameCurrent(FileName);
	}
}

void CNeoScriptDebugger::OnDelete()
{
	m_ScriptRepositoryDialog->setWindowTitle(tr("Delete Script..."));
	QString FileName = m_ScriptRepositoryDialog->SelectScript(tr("Delete"), currentName());
	if(FileName != "")
		DeleteScript(FileName);
}

void CNeoScriptDebugger::OnClear()
{
	QVariantMap Request;
	Request["Action"] = "ResetSession";
	frontend()->sendCustom(Request);

	if(isInteractive())
		action(ContinueAction)->trigger();
}

void CNeoScriptDebugger::OnDefault()
{
	ShowResource(m_ResourceWidget->CurrentResourceID());
}

void CNeoScriptDebugger::OnHtmlCode()
{
	qint64 ID = m_ResourceWidget->CurrentResourceID();
	QString Url = m_ResourceWidget->GetResource(ID)["Url"].toString();
	setCurrentCustom(ID, Url, CHtmlCodeView::factory);
}

void CNeoScriptDebugger::OnRawTrace()
{
	qint64 ID = m_ResourceWidget->CurrentResourceID();
	QString Url = m_ResourceWidget->GetResource(ID)["Url"].toString();
	setCurrentCustom(ID, Url, CRawTraceView::factory);
}

void CNeoScriptDebugger::OnDomTree()
{
	qint64 ID = m_ResourceWidget->CurrentResourceID();
	QString Url = m_ResourceWidget->GetResource(ID)["Url"].toString();
	setCurrentCustom(ID, Url, CDomTreeView::factory);
}

void CNeoScriptDebugger::OnCopyUrl()
{
	qint64 ID = m_ResourceWidget->CurrentResourceID();
	QString Url = m_ResourceWidget->GetResource(ID)["Url"].toString();
	QApplication::clipboard()->setText(Url);
}

void CNeoScriptDebugger::OnSetInterceptor()
{
	SetInterceptor(true);
}

void CNeoScriptDebugger::OnClearInterceptor()
{
	SetInterceptor(false);
}

QAction *CNeoScriptDebugger::action(int action) const
{
	return CJSScriptDebugger::action(action);
}

QWidget *CNeoScriptDebugger::widget(int widget) const
{
	CNeoScriptDebugger *that = const_cast<CNeoScriptDebugger*>(this);

	switch (widget) {
	case -1: {
			that->m_ResourceWidget = new CResourceWidget();
			connect(m_ResourceWidget, SIGNAL(ResourceActivated(qint64)), this, SLOT(ShowResource(qint64)));
			connect(m_ResourceWidget, SIGNAL(ResourceSelected(qint64)), this, SLOT(UpdateResource(qint64)));
		}
		return m_ResourceWidget;
	case -2: {
			that->m_SessionLogWidget = new CSessionLogWidget();
		}
		return m_SessionLogWidget;
	default:
		return CJSScriptDebugger::widget(widget);
	}
}


QToolBar *CNeoScriptDebugger::createStandardToolBar(QWidget *parent)
{
	QToolBar* pToolBar = CJSScriptDebugger::createStandardToolBar(parent);
	int i = 0;
	pToolBar->insertAction(pToolBar->actions().at(i++), m_NewAction);
	pToolBar->insertAction(pToolBar->actions().at(i++), m_LoadAction);
	pToolBar->insertAction(pToolBar->actions().at(i++), m_SaveAction);
	pToolBar->insertSeparator(pToolBar->actions().at(i++));
	pToolBar->insertAction(pToolBar->actions().at(i++), m_ClearAction);
	pToolBar->insertSeparator(pToolBar->actions().at(i++));
    return pToolBar;
}

QMenu *CNeoScriptDebugger::createStandardMenu(QWidget *parent)
{
    return CJSScriptDebugger::createStandardMenu(parent);
}

QMenu *CNeoScriptDebugger::createFileMenu(QWidget *parent)
{
	QMenu *fileMenu = new QMenu(tr("File"), parent);

	QIcon newIcon;
	newIcon.addPixmap(pixmap(QString::fromLatin1("filenew.png")), QIcon::Normal);
	newIcon.addPixmap(pixmap(QString::fromLatin1("filenew.png"), true), QIcon::Disabled);
	m_NewAction = new QAction(newIcon, CNeoScriptDebugger::tr("New"), parent);
	m_NewAction->setShortcut(QString("Ctrl+N"));
	connect(m_NewAction, SIGNAL(triggered()), this, SLOT(OnNew()));
	fileMenu->addAction(m_NewAction);

	QIcon loadIcon;
	loadIcon.addPixmap(pixmap(QString::fromLatin1("fileopen.png")), QIcon::Normal);
	loadIcon.addPixmap(pixmap(QString::fromLatin1("fileopen.png"), true), QIcon::Disabled);
	m_LoadAction = new QAction(loadIcon, CNeoScriptDebugger::tr("Load"), parent);
	m_LoadAction->setShortcut(QString("Ctrl+L"));
	connect(m_LoadAction, SIGNAL(triggered()), this, SLOT(OnLoad()));
	fileMenu->addAction(m_LoadAction);

	QIcon saveIcon;
	saveIcon.addPixmap(pixmap(QString::fromLatin1("filesave.png")), QIcon::Normal);
	saveIcon.addPixmap(pixmap(QString::fromLatin1("filesave.png"), true), QIcon::Disabled);
	m_SaveAction = new QAction(saveIcon, CNeoScriptDebugger::tr("Save"), parent);
	m_SaveAction->setShortcut(QString("Ctrl+S"));
	connect(m_SaveAction, SIGNAL(triggered()), this, SLOT(OnSave()));
	fileMenu->addAction(m_SaveAction);

	m_SaveAsAction = new QAction(CNeoScriptDebugger::tr("SaveAs"), parent);
	m_SaveAsAction->setShortcut(QString("Ctrl+Shift+S"));
	connect(m_SaveAsAction, SIGNAL(triggered()), this, SLOT(OnSaveAs()));
	fileMenu->addAction(m_SaveAsAction);

	QIcon deleteIcon;
	deleteIcon.addPixmap(pixmap(QString::fromLatin1("delete.png"), false, false), QIcon::Normal);
	m_DeleteAction = new QAction(deleteIcon, CNeoScriptDebugger::tr("Delete"), parent);
	connect(m_DeleteAction, SIGNAL(triggered()), this, SLOT(OnDelete()));
	fileMenu->addAction(m_DeleteAction);

	fileMenu->addSeparator();

	QIcon clearIcon;
	clearIcon.addPixmap(pixmap(QString::fromLatin1("reload.png"), false, false), QIcon::Normal);
	clearIcon.addPixmap(pixmap(QString::fromLatin1("reload.png"), true, false), QIcon::Disabled);
	m_ClearAction = new QAction(clearIcon, CNeoScriptDebugger::tr("Clear"), parent);
	m_ClearAction->setShortcut(QString("Ctrl+R"));
	connect(m_ClearAction, SIGNAL(triggered()), this, SLOT(OnClear()));
	fileMenu->addAction(m_ClearAction);

	return fileMenu;
}

QMenu *CNeoScriptDebugger::createResourceMenu(QWidget *parent)
{
	QMenu *resourceMenu = new QMenu(parent);

	m_DefaultAction = new QAction(CNeoScriptDebugger::tr("View Resource"), parent);
	connect(m_DefaultAction, SIGNAL(triggered()), this, SLOT(OnDefault()));
	resourceMenu->addAction(m_DefaultAction);

	m_HtmlCodeAction = new QAction(CNeoScriptDebugger::tr("View Html Code"), parent);
	connect(m_HtmlCodeAction, SIGNAL(triggered()), this, SLOT(OnHtmlCode()));
	resourceMenu->addAction(m_HtmlCodeAction);

	m_RawTraceAction = new QAction(CNeoScriptDebugger::tr("View Request Trace"), parent);
	connect(m_RawTraceAction, SIGNAL(triggered()), this, SLOT(OnRawTrace()));
	resourceMenu->addAction(m_RawTraceAction);

	m_DomTreeAction = new QAction(CNeoScriptDebugger::tr("View Dom Tree"), parent);
	connect(m_DomTreeAction, SIGNAL(triggered()), this, SLOT(OnDomTree()));
	resourceMenu->addAction(m_DomTreeAction);

	resourceMenu->addSeparator();

	m_CopyUrlAction = new QAction(CNeoScriptDebugger::tr("Copy Url"), parent);
	connect(m_CopyUrlAction, SIGNAL(triggered()), this, SLOT(OnCopyUrl()));
	resourceMenu->addAction(m_CopyUrlAction);

	m_HoldAction = new QAction(CNeoScriptDebugger::tr("Hold Auto Refresh"), parent);
	m_HoldAction->setCheckable(true);
	resourceMenu->addAction(m_HoldAction);

	resourceMenu->addSeparator();

	m_SetInterceptorAction = new QAction(CNeoScriptDebugger::tr("Set Interceptor"), parent);
	connect(m_SetInterceptorAction, SIGNAL(triggered()), this, SLOT(OnSetInterceptor()));
	resourceMenu->addAction(m_SetInterceptorAction);

	m_ClearInterceptorAction = new QAction(CNeoScriptDebugger::tr("Clear Interceptor"), parent);
	connect(m_ClearInterceptorAction, SIGNAL(triggered()), this, SLOT(OnClearInterceptor()));
	resourceMenu->addAction(m_ClearInterceptorAction);

	return resourceMenu;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

class CNewTask: public QDialog
{
	//Q_OBJECT

public:
	CNewTask(QWidget *pMainWindow)
	: QDialog(pMainWindow)
	{
		setWindowTitle(CNeoScriptDebugging::tr("New Debug Task"));

		m_pEntry = new QComboBox(this);
		m_pEntry->setEditable(true);

        m_pUrl = new QComboBox(this);
		m_pUrl->setEditable(true);

		m_pFileName = new CPathCombo(false, this);

		m_pAuto = new QCheckBox(CNeoScriptDebugging::tr("Auto Start"), this);

		m_pButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
		QObject::connect(m_pButtonBox, SIGNAL(accepted()), this, SLOT(accept()));
		QObject::connect(m_pButtonBox, SIGNAL(rejected()), this, SLOT(reject()));

		m_pMainLayout = new QFormLayout(this);
		m_pMainLayout->setWidget(0, QFormLayout::LabelRole, m_pEntry);
		m_pMainLayout->setWidget(0, QFormLayout::FieldRole, m_pUrl);
		m_pMainLayout->setWidget(1, QFormLayout::LabelRole, new QLabel(CNeoScriptDebugging::tr("FileName")));
		m_pMainLayout->setWidget(1, QFormLayout::FieldRole, m_pFileName);
		m_pMainLayout->setWidget(2, QFormLayout::LabelRole, m_pAuto);
		m_pMainLayout->setWidget(2, QFormLayout::FieldRole, m_pButtonBox);
	}

	static bool	GetTask(QWidget *pMainWindow, QString& Entry, const QStringList& Entries, QString& Url, const QStringList& Urls, bool& AutoStart, QString& FileName, const QStringList& FileNames)
	{
		CNewTask NewDebug(pMainWindow);
		NewDebug.m_pEntry->addItems(Entries);
		NewDebug.m_pUrl->addItems(Urls);
		NewDebug.m_pFileName->AddItems(FileNames);
		if(!NewDebug.exec())
			return false;

		Entry = NewDebug.m_pEntry->currentText();
		Url = NewDebug.m_pUrl->currentText();
		AutoStart = NewDebug.m_pAuto->checkState() == Qt::Checked; //Qt::PartiallyChecked
		FileName = NewDebug.m_pFileName->GetText();
		return true;
	}

private:
	QComboBox*			m_pEntry;
	QCheckBox*			m_pAuto;
	QComboBox*			m_pUrl;
	CPathCombo*			m_pFileName;
	QDialogButtonBox*	m_pButtonBox;
	QFormLayout*		m_pMainLayout;

};

QMap<uint64, CNeoScriptDebugging*> CNeoScriptDebugging::m_Debuggers;

CNeoScriptDebugging::CNeoScriptDebugging(uint64 ID)
{
	m_ID = ID;
	m_Debuggers.insert(ID, this);

	CJSScriptDebuggerFrontend* pDebuggerFrontend = new CJSScriptDebuggerFrontend();

	QObject::connect(this, SIGNAL(SendResponse(QVariant)), pDebuggerFrontend, SLOT(processResponse(QVariant)), Qt::QueuedConnection);
	QObject::connect(pDebuggerFrontend, SIGNAL(sendRequest(QVariant)), this, SLOT(ProcessRequest(QVariant)), Qt::QueuedConnection);

	m_pScriptDebugger = new CNeoScriptDebugger();


	QByteArray Geometry = theLoader->Cfg()->GetBlob("Debugger/Wnd_Geometry");
	if (!Geometry.isEmpty())
        m_pScriptDebugger->restoreGeometry(Geometry);

	m_pScriptDebugger->attachTo(pDebuggerFrontend);

    QByteArray State = theLoader->Cfg()->GetBlob("Debugger/Wnd_State");
    if (!State.isEmpty())
        m_pScriptDebugger->restoreState(State);
	
	QObject::connect(m_pScriptDebugger, SIGNAL(detach()), this, SLOT(Detach()), Qt::QueuedConnection);
}

CNeoScriptDebugging::~CNeoScriptDebugging()
{
	theLoader->Cfg()->SetBlob("Debugger/Wnd_Geometry", m_pScriptDebugger->saveGeometry());
	theLoader->Cfg()->SetBlob("Debugger/Wnd_State", m_pScriptDebugger->saveState());

	delete m_pScriptDebugger;
	m_Debuggers.remove(m_ID);
}

void CNeoScriptDebugging::OpenNew()
{
	QString Entry;
	QStringList Entries = theLoader->Cfg()->GetStringList("Debugger/Last_Entries");
	QString Url;
	QStringList Urls = theLoader->Cfg()->GetStringList("Debugger/Last_Urls");
	bool AutoStart;
	QString FileName;
	QStringList FileNames = theLoader->Cfg()->GetStringList("Debugger/Last_Files");
	if(!CNewTask::GetTask(NULL, Entry, Entries,Url, Urls, AutoStart, FileName, FileNames))
		return;

	if(!Url.contains(":"))
		Url.prepend("http://");
	Urls.removeAll(Url);
	Urls.prepend(Url);
	while(Urls.size() > 10)
		Urls.removeLast();
	theLoader->Cfg()->SetSetting("Debugger/Last_Urls", Urls);

	FileNames.removeAll(FileName);
	FileNames.prepend(FileName);
	while(FileNames.size() > 10)
		FileNames.removeLast();
	theLoader->Cfg()->SetSetting("Debugger/Last_Files", FileNames);

	Entries.removeAll(Entry);
	Entries.prepend(Entry);
	while(Entries.size() > 10)
		Entries.removeLast();
	theLoader->Cfg()->SetSetting("Debugger/Last_Entries", Entries);

	QMap<QString, QVariant> Parameters;
	Parameters["Url"] = Url.trimmed();
	Parameters["Entry"] = AutoStart ? Entry : "";
	Parameters["FileName"] = FileName;
	theLoader->Itf()->SendRequest("DbgAddTask", Parameters);
}

void CNeoScriptDebugging::CloseAll()
{
	foreach(CNeoScriptDebugging* pDebugger, m_Debuggers)
		pDebugger->m_pScriptDebugger->close();
}

void CNeoScriptDebugging::Detach()
{
	QMap<QString, QVariant> Parameters;
	Parameters["ID"] = m_ID;
	theLoader->Itf()->SendRequest("DbgRemoveTask", Parameters);

	deleteLater();
}

void CNeoScriptDebugging::Dispatch(const QVariantMap& Response)
{
	uint64 ID = Response["ID"].toULongLong();
	if(CNeoScriptDebugging* Proxy = m_Debuggers.value(ID))
		emit Proxy->SendResponse(Response["Data"]);
}

void CNeoScriptDebugging::ProcessRequest(const QVariant& var)
{
	QVariantMap Request;
	Request["ID"] = m_ID;
	Request["Data"] = var;
	theLoader->Itf()->SendRequest("DbgCommand", Request);
}
