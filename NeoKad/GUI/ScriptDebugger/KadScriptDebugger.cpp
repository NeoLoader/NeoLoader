#include "GlobalHeader.h"
#include "KadScriptDebugger.h"
#include "../../../Framework/Settings.h"
#include "../../../Framework/OtherFunctions.h"
#include "KadScriptWidget.h"

#include "../../Kad/KadHeader.h"
#include "../../Kad/KadID.h"

#include "../NeoScriptTools/V8Debugging/V8ScriptDebuggerFrontend.h"
#include "KadScriptDebuggerFrontend.h"

#include "../../NeoKad.h"

CKadScriptDebugger::CKadScriptDebugger(const QString& Pipe)
 : m_KadScriptWidget(0)
 , m_NewAction(0), m_LoadAction(0), m_SaveAction(0), m_SaveAsAction(0)
 , m_TerminateAction(0)
 
{
	CSettings::InitSettingsEnvironment(APP_ORGANISATION, APP_NAME, APP_DOMAIN);
	QMap<QString, CSettings::SSetting> Settings;

	Settings.insert("Debugger/Wnd_Geometry", CSettings::SSetting(""));
	Settings.insert("Debugger/Wnd_State", CSettings::SSetting(""));
	Settings.insert("Debugger/Wnd_Tree", CSettings::SSetting(""));

	m_Settings = new CSettings("NeoKad", Settings, this);

	m_Manager = new CJobManager(this);

	//if(Pipe.left(1) == ":")
	//{
	//	CV8ScriptDebuggerFrontend* pDebuggerFrontend = new CV8ScriptDebuggerFrontend();
	//	attachTo(pDebuggerFrontend);
	//	pDebuggerFrontend->connectTo(Pipe.mid(1).toUInt());
	//}
	//else
	{
		CKadScriptDebuggerFrontend* pDebuggerFrontend = new CKadScriptDebuggerFrontend();
		attachTo(pDebuggerFrontend);
		pDebuggerFrontend->connectTo(Pipe);
	}

	m_TimerId = startTimer(500);
}

CKadScriptDebugger::~CKadScriptDebugger()
{
	killTimer(m_TimerId);

	if(m_KadScriptWidget)
	{
		m_Settings->SetBlob("Debugger/Wnd_Geometry", saveGeometry());
		m_Settings->SetBlob("Debugger/Wnd_State", saveState());
		m_Settings->SetBlob("Debugger/Wnd_Tree", m_KadScriptWidget->saveState());
	}
}

void CKadScriptDebugger::timerEvent(QTimerEvent *e)
{
    if (e->timerId() != m_TimerId) 
	{
        CJSScriptDebugger::timerEvent(e);
		return;
    }

	// Note: this must be executed in sync, so that pending pensing commands wont stall this command
	QVariantMap Response = frontend()->executeCommand("GetEngineState", QVariantMap());

	bool bExecuting = false;
	QString Message;
	if(Response.isEmpty())
		Message = "Disconnected ...";
	else if(Response["Executing"].toBool())
	{
		bExecuting = true;
		if(Response["Debugging"].toBool())
			Message = "Debugging";
		else
			Message = "Executing";
	}
	else
	{
		CJSScriptDebugger::action(InterruptAction)->setEnabled(false);
		ASSERT(!Response["Debugging"].toBool());
		Message = "Idle";
	}

	m_NewAction->setDisabled(bExecuting);
	m_LoadAction->setDisabled(bExecuting);
	m_SaveAction->setDisabled(bExecuting);
	m_SaveAsAction->setDisabled(bExecuting);

	m_TerminateAction->setEnabled(bExecuting);

	m_KadScriptWidget->DissableToolbar(bExecuting);

	statusBar()->showMessage(Message);
}

void CKadScriptDebugger::OnNew()
{
	QString Name = QInputDialog::getText(this, tr("Script Name"),tr("Enter name for new kad script"));
	if(Name.isEmpty())
		return;

	string Source;
	Source += "/*\r\n";
	Source += "\tName: " + Name.toStdString() + "\r\n";
	Source += "\tVersion: 0.0.0.1\r\n";
	Source += "*/\r\n";
	Source += "\r\n";
	Source += "function Debug()\r\n";
	Source += "{\r\n";
	Source += "	debug.log('hello world');\r\n";
	Source += "	debugger;\r\n";
	Source += "}\r\n";

	CScoped<CPrivateKey> pPrivKey = new CPrivateKey(CPrivateKey::eECP);
	pPrivKey->GenerateKey("brainpoolP512r1");

	CScoped<CPublicKey> pPubKey = pPrivKey->PublicKey();

	CVariant CodeID((byte*)NULL, KEY_128BIT);
	CKadID::MakeID(pPubKey, CodeID.GetData(), CodeID.GetSize());

	CKadActionJob* pKadActionJob = new CKadActionJob("InstallScript");
	pKadActionJob->Set("CodeID", CodeID.ToQVariant());
	pKadActionJob->Set("Source", QString::fromStdString(Source));
	pKadActionJob->Set("DeveloperKey", CVariant(pPrivKey->GetKey(), pPrivKey->GetSize()).ToQVariant());
	
	ScheduleJob(pKadActionJob);
}

void CKadScriptDebugger::OnLoad()
{
	QString ScriptPath = QFileDialog::getOpenFileName(this, tr("Select Script"));
	if(ScriptPath.isEmpty())
		return;

	StrPair PathFile = Split2(ScriptPath, "/", true);
	StrPair FileEx = Split2(PathFile.second, ".", true);

	string Source = ReadFileAsString(PathFile.first + "/" + FileEx.first + ".js").toStdString();

	CScoped<CPrivateKey> pPrivKey = new CPrivateKey(CPrivateKey::eECP);
	QFile KeyFile(PathFile.first + "/" + FileEx.first + ".der");
	if(!KeyFile.exists())
	{
		pPrivKey->GenerateKey("brainpoolP512r1");
		KeyFile.open(QFile::WriteOnly);
		KeyFile.write(pPrivKey->ToByteArray());
	}
	else
	{
		KeyFile.open(QFile::ReadOnly);
		pPrivKey->SetKey(KeyFile.readAll());
	}
	KeyFile.close();

	CScoped<CPublicKey> pPubKey = pPrivKey->PublicKey();

	/*CVariant Authentication;
	Authentication["PK"] = CVariant(pPubKey->GetKey(), pPubKey->GetSize());
	CBuffer SrcBuff((byte*)Source.data(), Source.size(), true);
	CBuffer Signature;
	pPrivKey->Sign(&SrcBuff,&Signature);
	Authentication["SIG"] = Signature;
	Authentication.Freeze();*/

	CVariant CodeID((byte*)NULL, KEY_128BIT);
	CKadID::MakeID(pPubKey, CodeID.GetData(), CodeID.GetSize());

	CKadActionJob* pKadActionJob = new CKadActionJob("InstallScript");
	pKadActionJob->Set("CodeID", CodeID.ToQVariant());
	pKadActionJob->Set("Source", QString::fromStdString(Source));
	//pKadActionJob->Set("Authentication", Authentication.ToQVariant());
	pKadActionJob->Set("DeveloperKey", CVariant(pPrivKey->GetKey(), pPrivKey->GetSize()).ToQVariant());
	
	ScheduleJob(pKadActionJob);
}

void CKadScriptDebugger::OnSave()
{
	QString Source = currentText(true);
	if(Source.isEmpty())
		return;

	StrPair IDName = Split2(currentName(), "/");

	CKadActionJob* pKadActionJob = new CKadActionJob("UpdateScript");
	pKadActionJob->Set("CodeID", QByteArray::fromHex(IDName.first.toLatin1()));
	pKadActionJob->Set("Source", Source);
	
	ScheduleJob(pKadActionJob);
}

class CGetDevkeyJob: public CInterfaceJob
{
public:
	CGetDevkeyJob(const QString& ScriptPath)
	{
		m_ScriptPath = ScriptPath;
	}

	void					SetCodeID(const QVariant& CodeID) {m_Request["CodeID"] = CodeID;}
	
	virtual QString			GetCommand()	{return "GetDeveloperKey";}
	virtual void			HandleResponse(const QVariantMap& Response)
	{
		QFile KeyFile(m_ScriptPath + ".der");
		KeyFile.open(QFile::WriteOnly);
		KeyFile.write(Response["DeveloperKey"].toByteArray());
	}

protected:
	QString	m_ScriptPath;
};

void CKadScriptDebugger::OnSaveAs()
{
	QString ScriptPath = QFileDialog::getSaveFileName(this, tr("Select File"));
	if(ScriptPath.isEmpty())
		return;

	if(ScriptPath.right(3).compare(".js", Qt::CaseInsensitive) == 0)
		ScriptPath.truncate(ScriptPath.length() - 3);

	QString Source = currentText(true);
	if(Source.isEmpty())
		return;

	WriteStringToFile(ScriptPath + ".js", Source);

	StrPair IDName = Split2(currentName(), "/");

	CGetDevkeyJob* pKadActionJob = new CGetDevkeyJob(ScriptPath);
	pKadActionJob->SetCodeID(QByteArray::fromHex(IDName.first.toLatin1()));

	ScheduleJob(pKadActionJob);
}

void CKadScriptDebugger::OnTerminate()
{
	frontend()->terminate();
}

void CKadScriptDebugger::attachTo(CKadScriptDebuggerFrontend *frontend)
{
	QByteArray Geometry = m_Settings->GetBlob("Debugger/Wnd_Geometry");
	if (!Geometry.isEmpty())
		restoreGeometry(Geometry);
	
	CJSScriptDebugger::attachTo(frontend);

	QByteArray State = m_Settings->GetBlob("Debugger/Wnd_State");
	if (!State.isEmpty())
		restoreState(State);
	QByteArray Tree = m_Settings->GetBlob("Debugger/Wnd_Tree");
	if (!Tree.isEmpty())
		m_KadScriptWidget->restoreState(Tree);

	connect(m_Manager, SIGNAL(SendRequest(QString, QVariantMap)), frontend, SLOT(SendRequest(QString, QVariantMap)));
	connect(frontend, SIGNAL(DispatchResponse(QString, QVariantMap)), m_Manager, SLOT(DispatchResponse(QString, QVariantMap)));
	m_Manager->Suspend(false);
}

CKadScriptDebuggerFrontend* CKadScriptDebugger::frontend()
{
	return (CKadScriptDebuggerFrontend*)m_frontend;
}

void CKadScriptDebugger::ScheduleJob(CInterfaceJob* pJob)
{
	m_Manager->ScheduleJob(pJob);
}

void CKadScriptDebugger::UnScheduleJob(CInterfaceJob* pJob)
{
	m_Manager->UnScheduleJob(pJob);
}

void CKadScriptDebugger::setup()
{
	menuBar()->addMenu(createFileMenu(this));

	CJSScriptDebugger::setup();

	QDockWidget *kadDock = new QDockWidget(this);
	kadDock->setObjectName(QLatin1String("qtscriptdebugger_kadScriptsDockWidget"));
    kadDock->setWindowTitle(tr("Kad Script"));
    kadDock->setWidget(widget(-1));
    addDockWidget(Qt::LeftDockWidgetArea, kadDock);

	QDockWidget *scriptsDock = qobject_cast<QDockWidget*>(widget(ScriptsWidget)->parent());
	tabifyDockWidget(scriptsDock, kadDock);
}

QToolBar *CKadScriptDebugger::createStandardToolBar(QWidget *parent)
{
	QToolBar* pToolBar = CJSScriptDebugger::createStandardToolBar(parent);
	
	QAction* pBefoure = pToolBar->actions().first();

	pToolBar->insertAction(pBefoure, m_NewAction);
	pToolBar->insertAction(pBefoure, m_LoadAction);
	pToolBar->insertAction(pBefoure, m_SaveAction);
	pToolBar->insertAction(pBefoure, m_SaveAsAction);
	pToolBar->insertSeparator(pBefoure);

	pToolBar->insertAction(pBefoure, m_TerminateAction);
	pToolBar->insertSeparator(pBefoure);

    return pToolBar;
}

QMenu *CKadScriptDebugger::createFileMenu(QWidget *parent)
{
	QMenu *fileMenu = new QMenu(tr("File"), parent);

	QIcon newIcon;
	newIcon.addPixmap(pixmap(QString::fromLatin1("filenew.png")), QIcon::Normal);
	newIcon.addPixmap(pixmap(QString::fromLatin1("filenew.png"), true), QIcon::Disabled);
	m_NewAction = new QAction(newIcon, CKadScriptDebugger::tr("New Script"), parent);
	m_NewAction->setShortcut(QString("Ctrl+N"));
	connect(m_NewAction, SIGNAL(triggered()), this, SLOT(OnNew()));
	fileMenu->addAction(m_NewAction);

	QIcon loadIcon;
	loadIcon.addPixmap(pixmap(QString::fromLatin1("fileopen.png")), QIcon::Normal);
	loadIcon.addPixmap(pixmap(QString::fromLatin1("fileopen.png"), true), QIcon::Disabled);
	m_LoadAction = new QAction(loadIcon, CKadScriptDebugger::tr("Install and Load"), parent);
	m_LoadAction->setShortcut(QString("Ctrl+L"));
	connect(m_LoadAction, SIGNAL(triggered()), this, SLOT(OnLoad()));
	fileMenu->addAction(m_LoadAction);

	QIcon saveIcon;
	saveIcon.addPixmap(pixmap(QString::fromLatin1("filesave.png")), QIcon::Normal);
	saveIcon.addPixmap(pixmap(QString::fromLatin1("filesave.png"), true), QIcon::Disabled);
	m_SaveAction = new QAction(saveIcon, CKadScriptDebugger::tr("Save and Reload"), parent);
	m_SaveAction->setShortcut(QString("Ctrl+S"));
	connect(m_SaveAction, SIGNAL(triggered()), this, SLOT(OnSave()));
	fileMenu->addAction(m_SaveAction);

	QIcon saveAsIcon;
	saveAsIcon.addPixmap(pixmap(QString::fromLatin1("exportpdf.png")), QIcon::Normal);
	saveAsIcon.addPixmap(pixmap(QString::fromLatin1("exportpdf.png"), true), QIcon::Disabled);
	m_SaveAsAction = new QAction(saveAsIcon, CKadScriptDebugger::tr("Export to File"), parent);
	m_SaveAsAction->setShortcut(QString("Ctrl+Shift+S"));
	connect(m_SaveAsAction, SIGNAL(triggered()), this, SLOT(OnSaveAs()));
	fileMenu->addAction(m_SaveAsAction);

	return fileMenu;
}

QMenu *CKadScriptDebugger::createStandardMenu(QWidget *parent)
{
    QMenu* pMenu = CJSScriptDebugger::createStandardMenu(parent);

	QAction* pBefoure = pMenu->actions().first();

	QIcon Stop;
	Stop.addPixmap(pixmap(QString::fromLatin1(":/Stop")), QIcon::Normal);
	Stop.addPixmap(pixmap(QString::fromLatin1(":/Stop"), true), QIcon::Disabled);
	m_TerminateAction = new QAction(Stop, CKadScriptDebugger::tr("Terminate"), parent);
	m_TerminateAction->setShortcut(QString("Ctrl+T"));
	connect(m_TerminateAction, SIGNAL(triggered()), this, SLOT(OnTerminate()));
	pMenu->insertAction(pBefoure, m_TerminateAction);
	pMenu->insertSeparator(pBefoure);

	return pMenu;
}

QAction *CKadScriptDebugger::action(int action) const
{
	return CJSScriptDebugger::action(action);
}

QWidget *CKadScriptDebugger::widget(int widget) const
{
	CKadScriptDebugger *that = const_cast<CKadScriptDebugger*>(this);

	switch (widget) {
	case -1: {
			that->m_KadScriptWidget = new CKadScriptWidget(that);
		}
		return m_KadScriptWidget;
	default:
		return CJSScriptDebugger::widget(widget);
	}
}
