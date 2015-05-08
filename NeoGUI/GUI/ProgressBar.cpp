#include "GlobalHeader.h"
#include "ProgressBar.h"
#include "../NeoGUI.h"
#include <QPainter>

CProgressBar::CProgressBar(uint64 ID, uint64 SubID, const QString& Mode, QWidget *parent)
:QLabel(parent)
{
	setMaximumHeight(16);

	m_pProgressJob = 0;

	m_ID = ID;
	m_Request["ID"] = ID;
	if(SubID)
		m_Request["SubID"] = SubID;
	m_Request["Mode"] = Mode;
	
	m_NextUpdate = 0;
	m_Depth = 5;
	m_Progress = -1;
}

CProgressBar::CProgressBar(uint64 ID, const QString& Groupe, const QString& Hoster, const QString& User, const QString& Mode, QWidget *parent)
{
	setMaximumHeight(16);

	m_pProgressJob = 0;

	m_ID = ID;
	m_Request["ID"] = ID;
	m_Request["PartClass"] = Groupe;
	m_Request["HostName"] = Hoster;
	m_Request["UserName"] = User;
	
	m_NextUpdate = 0;
	m_Depth = 5;
	m_Progress = -1;
}

void CProgressBar::SetID(uint64 ID) 
{
	m_ID = ID;
	m_Request["ID"] = ID;

	Update();
}

class CProgressJob: public CInterfaceJob
{
public:
	CProgressJob(CProgressBar* pView, const QVariantMap& Request, int Depth)
	{
		m_pView = pView;
		m_Request = Request;
		m_Request["Width"] = pView->width();
		int iHeight = pView->height() - 2;
		if(iHeight > 32)
			iHeight = 32; // enforce a limit just in case
		m_Request["Height"] = iHeight;
		m_Request["Depth"] = Depth;

	}

	virtual QString			GetCommand()	{return "GetProgress";}
	virtual void			HandleResponse(const QVariantMap& Response)
	{
		if(m_pView)
		{
			QPixmap Pixmap;
			Pixmap.loadFromData(Response["Data"].toByteArray());
			if(m_pView->m_Progress != -1)
			{
				QPainter Painter(&Pixmap);
				Painter.setPen(Qt::white);
				QFont Font = Painter.font();
				//Font.setBold(true);
				Font.setPointSize(10);
				Painter.setFont(Font);
				Painter.drawText(Pixmap.rect(), Qt::AlignCenter, tr("%1 %").arg(m_pView->m_Progress));
			}
			m_pView->setPixmap(Pixmap);
			m_pView->m_NextUpdate = GetCurTick() + SEC2MS(Max(Min(Response["RenderTime"].toULongLong()/2, 10),1));
		}
	}
	virtual void			Finish(bool bOK)
	{
		if(m_pView)
			m_pView->m_pProgressJob = NULL;
		CInterfaceJob::Finish(bOK);
	}

protected:
	QPointer<CProgressBar> m_pView; // Note: this can be deleted at any time
};

void CProgressBar::Update()
{
	m_NextUpdate = GetCurTick() + SEC2MS(10);
	if(m_pProgressJob != NULL)
		return; // still pending
	
	if(m_ID != -1)
	{
		m_pProgressJob = new CProgressJob(this, m_Request, m_Depth);
		theGUI->ScheduleJob(m_pProgressJob);
	}
}