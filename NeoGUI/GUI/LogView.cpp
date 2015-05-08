#include "GlobalHeader.h"
#include "LogView.h"
#include "../NeoGUI.h"
#include "../Common/LineEditEx.h"
#include "../../Framework/IPC/IPCSocket.h"

#define MAX_LOG_BLOCKS 1000

CLogView::CLogView(QWidget *parent)
:QWidget(parent)
{
	m_pLogSyncJob = NULL;
	m_uLastLog = 0;
	m_bFilter = true;

	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(3);

	m_LogEdits[0] = new QTextEdit();
	m_LogEdits[0]->setReadOnly(true);
	m_LogEdits[0]->document()->setMaximumBlockCount(MAX_LOG_BLOCKS);

	m_pMainLayout->addWidget(m_LogEdits[0]);

	setLayout(m_pMainLayout);

	m_TimerId = 0;
}

CLogView::~CLogView()
{
	if(m_TimerId)
		killTimer(m_TimerId);
}

void CLogView::ShowFile(uint64 ID)
{
	Suspend(ID == 0);
	
	if(m_Request.value("File") == ID)
		return;

	ClearLog();
	m_Request.clear();
	m_Request["File"] = ID;
}

void CLogView::ShowTransfer(uint64 ID, uint64 SubID)
{
	Suspend(ID == 0);
	
	m_bFilter = false;

	if(m_Request.value("File") == ID && m_Request.value("Transfer") == SubID)
		return;

	ClearLog();
	m_Request.clear();
	m_Request["File"] = ID;
	m_Request["Transfer"] = SubID;
}

#ifdef CRAWLER
void CLogView::ShowSite(const QString& SiteName)
{
	Suspend(SiteName.isEmpty());

	if(m_Request.value("SiteName") == SiteName)
		return;

	ClearLog();
	m_Request.clear();
	m_Request["SiteName"] = SiteName;
}
#endif

class CLogSyncJob: public CInterfaceJob
{
public:
	CLogSyncJob(CLogView* pView, const QVariantMap& Request)
	{
		m_pView = pView;
		m_Request = Request;
		m_Request["LastID"] = pView->m_uLastLog;
	}

	virtual QString			GetCommand()	{return "GetLog";}
	virtual void			HandleResponse(const QVariantMap& Response) 
	{
		if(m_pView)
		{
			QVariantList Lines = Response["Lines"].toList();
			if(Lines.isEmpty())
				return;

			QVariantMap LogEntry = Lines.at(0).toMap();
			int Flags = LogEntry["Flag"].toUInt();
			QString Pack = QDateTime::fromTime_t((time_t)LogEntry["Stamp"].toULongLong()).toLocalTime().time().toString() + ": " + CLogMsg(LogEntry["Line"]).Print();
			for(int i = 1; i < Lines.size(); i ++)
			{
				LogEntry = Lines.at(i).toMap();
				if(Flags != LogEntry["Flag"].toUInt())
				{
					m_pView->AddLog(Flags, Pack);
					Pack.clear();
					Flags = LogEntry["Flag"].toUInt();
				}
				if(!Pack.isEmpty())
					Pack += "\n";
				Pack += QDateTime::fromTime_t((time_t)LogEntry["Stamp"].toULongLong()).toLocalTime().time().toString() + ": " + CLogMsg(LogEntry["Line"]).Print();
			}
			m_pView->AddLog(Flags, Pack);
			m_pView->m_uLastLog = LogEntry["ID"].toULongLong();
		}
	}
	virtual void			Finish(bool bOK)
	{
		if(m_pView)
			m_pView->m_pLogSyncJob = NULL;
		CInterfaceJob::Finish(bOK);
	}

protected:
	QPointer<CLogView>	m_pView; // Note: this can be deleted at any time
};

void CLogView::timerEvent(QTimerEvent *e)
{
    if (e->timerId() != m_TimerId) 
	{
        QObject::timerEvent(e);
		return;
    }

	if(m_pLogSyncJob == NULL)
	{
		m_pLogSyncJob = new CLogSyncJob(this, m_Request);
		theGUI->ScheduleJob(m_pLogSyncJob);
	}	
}

void CLogView::Suspend(bool bSet)
{
	if(bSet)
	{
		if(m_TimerId != 0)
		{
			killTimer(m_TimerId);
			m_TimerId = 0;
		}
	}
	else
	{
		if(m_TimerId == 0)
			m_TimerId = startTimer(500);
	}
}

void CLogView::ClearLog()
{
	m_uLastLog = 0;
	foreach(QTextEdit* pLogEdit, m_LogEdits)
		pLogEdit->clear();
}

void CLogView::AddLog(uint32 uFlag, const QString &Entry)
{
	AppendLog(m_LogEdits[0], uFlag, Entry);
}

void CLogView::AppendLog(QTextEdit* pLogEdit, uint32 uFlag, QString Entry)
{
	QString Color;
	switch(uFlag & LOG_MASK)
	{
		case LOG_ERROR:		Color = "#ff0000";	break;
		case LOG_WARNING:	Color = "#808000";	break;
		case LOG_SUCCESS:	Color = "#008000";	break;
		case LOG_INFO:		Color = "#0000ff";	break;
		default:			Color = "#000000";	break;
	}

	// filter debug client messages from log display its to many data
	//if(m_bFilter && !((uFlag & LOG_MOD_MASK) == 0) || (uFlag & LOG_MOD_MASK) == LOG_MOD('w') && uFlag & LOG_DEBUG)
	//	return;

	QScrollBar* pBar = pLogEdit->verticalScrollBar();
	bool bScroll = pBar->value() == pBar->maximum();
	pLogEdit->append(QString("<FONT color='%1'>%2</FONT>").arg(Color).arg(Entry.replace("\n", "<br>")));
	if(bScroll)
		pBar->setValue(pBar->maximum());
}

CLogViewEx::CLogViewEx(QWidget *parent)
: CLogView(parent) 
{
	m_pLogTabs = new QTabWidget();

	m_LogEdits[1] = new QTextEdit();
	m_LogEdits[1]->setReadOnly(true);
	m_LogEdits[1]->document()->setMaximumBlockCount(MAX_LOG_BLOCKS);
	m_pLogTabs->addTab(m_LogEdits[1], tr("NeoLoader"));

	m_pLogTabs->addTab(m_LogEdits[0], tr("NeoCore"));

	m_LogEdits[LOG_MOD('s')] = new QTextEdit();
	m_LogEdits[LOG_MOD('s')]->setReadOnly(true);
	m_LogEdits[LOG_MOD('s')]->document()->setMaximumBlockCount(MAX_LOG_BLOCKS);
	m_pLogTabs->addTab(m_LogEdits[LOG_MOD('s')], tr("NeoShare"));

	m_LogEdits[LOG_XMOD('n')] = new QTextEdit();
	m_LogEdits[LOG_XMOD('n')]->setReadOnly(true);
	m_LogEdits[LOG_XMOD('n')]->document()->setMaximumBlockCount(MAX_LOG_BLOCKS);
	m_pLogTabs->addTab(m_LogEdits[LOG_XMOD('n')], tr("NeoKad"));

	m_LogEdits[LOG_MOD('t')] = new QTextEdit();
	m_LogEdits[LOG_MOD('t')]->setReadOnly(true);
	m_LogEdits[LOG_MOD('t')]->document()->setMaximumBlockCount(MAX_LOG_BLOCKS);
	m_pLogTabs->addTab(m_LogEdits[LOG_MOD('t')], tr("BitTorrent"));

	/*m_LogEdits[LOG_XMOD('d')] = new QTextEdit();
	m_LogEdits[LOG_XMOD('d')]->setReadOnly(true);
	m_LogEdits[LOG_XMOD('d')]->document()->setMaximumBlockCount(MAX_LOG_BLOCKS);
	m_pLogTabs->addTab(m_LogEdits[LOG_XMOD('d')], tr("MainlineDHT"));*/

	m_LogEdits[LOG_MOD('m')] = new QTextEdit();
	m_LogEdits[LOG_MOD('m')]->setReadOnly(true);
	m_LogEdits[LOG_MOD('m')]->document()->setMaximumBlockCount(MAX_LOG_BLOCKS);
	m_pLogTabs->addTab(m_LogEdits[LOG_MOD('m')], tr("Ed2kMule"));

	m_LogEdits[LOG_XMOD('k')] = new QTextEdit();
	m_LogEdits[LOG_XMOD('k')]->setReadOnly(true);
	m_LogEdits[LOG_XMOD('k')]->document()->setMaximumBlockCount(MAX_LOG_BLOCKS);
	m_pLogTabs->addTab(m_LogEdits[LOG_XMOD('k')], tr("MuleKad"));


	/*m_pConsoleWidget = new QWidget();

	m_pConsoleText = new QTextEdit(m_pConsoleWidget);
	m_pConsoleText->setReadOnly(true);
	m_pConsoleText->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard | Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
	m_pConsoleText->setUndoRedoEnabled(false);
#ifdef WIN32
	m_pConsoleText->setCurrentFont(QFont("Lucida Console"));
	m_pConsoleText->setFontPointSize(10);
	m_pConsoleText->setLineWrapMode(QTextEdit::NoWrap);
#endif
	m_pConsoleLine = new QLineEditEx(m_pConsoleWidget);
	connect(m_pConsoleLine, SIGNAL(returnPressed()), this, SLOT(OnCommand()));

	m_pConsoleLayout = new QVBoxLayout();
	m_pConsoleLayout->setContentsMargins(0, 0, 0, 0);
	m_pConsoleLayout->addWidget(m_pConsoleText);
	m_pConsoleLayout->addWidget(m_pConsoleLine);
	m_pConsoleWidget->setLayout(m_pConsoleLayout);

	m_pLogTabs->addTab(m_pConsoleWidget, tr("Console"));*/

	m_pMainLayout->addWidget(m_pLogTabs);
}

void CLogViewEx::AddLog(uint32 uFlag, const QString &Entry)
{
	UINT uModule = uFlag & LOG_MOD_MASK;
	QTextEdit* pLogEdit = m_LogEdits.value(uModule);
	if(!pLogEdit)
		pLogEdit = m_LogEdits[0];
	AppendLog(pLogEdit, uFlag, Entry);

	if(uModule == 0 && (uFlag & LOG_DEBUG) == 0)
		AppendLog(m_LogEdits[1], uFlag, Entry);
}

/*class CConsoleJob: public CInterfaceJob
{
public:
	CConsoleJob(CLogViewEx* pView, const QVariantMap& Request)
	{
		m_pView = pView;
		m_Request = Request;
	}

	virtual QString			GetCommand()	{return "Console";}
	virtual void			HandleResponse(const QVariantMap& Response) 
	{
		if(m_pView)
		{
			if(Response["Parameters"].canConvert(QVariant::String))
				m_pView->m_pConsoleText->setPlainText(Response["Parameters"].toString());
			else
			{
				CIPCSocket::EEncoding Encoding = (CIPCSocket::EEncoding)Response["Encoding"].toInt();
				m_pView->m_pConsoleText->setPlainText(CIPCSocket::Variant2String(Response["Parameters"], Encoding, false));
			}
		}
	}

protected:
	QPointer<CLogViewEx>	m_pView; // Note: this can be deleted at any time
};

void CLogViewEx::OnCommand()
{
	StrPair Pair = Split2(m_pConsoleLine->text()," ");
	m_pConsoleLine->setText("");
	if(Pair.first.isEmpty())
		return;

	QVariantMap Request;
	Request["Command"] = Pair.first;
	CIPCSocket::EEncoding Encoding = CIPCSocket::eText;
	QVariant Data = CIPCSocket::String2Variant(Pair.second.toLatin1(), Encoding);
	if(Encoding == CIPCSocket::eText)
		Request["Parameters"] = Pair.second;
	else
	{
		Request["Encoding"] = Encoding;
		Request["Parameters"] = Data;
	}

	CConsoleJob* pConsoleJob = new CConsoleJob(this, Request);
	theGUI->ScheduleJob(pConsoleJob);
}
*/