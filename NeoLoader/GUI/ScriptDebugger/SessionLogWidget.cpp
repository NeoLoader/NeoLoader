#include "GlobalHeader.h"

#include "SessionLogWidget.h"
#include <QDateTime>
#include <QScrollBar>

CSessionLogWidget::CSessionLogWidget(QWidget *parent)
	: QWidget(parent, 0)
{
	m_pLogText = new QTextEdit();
    m_pLogText->setReadOnly(true);
//  m_pLogText->setFocusPolicy(Qt::NoFocus);
    m_pLogText->document()->setMaximumBlockCount(255);

    QVBoxLayout* pVbox = new QVBoxLayout(this);
    pVbox->setMargin(0);
    pVbox->setSpacing(0);
    pVbox->addWidget(m_pLogText);
}

CSessionLogWidget::~CSessionLogWidget()
{
}

void CSessionLogWidget::UpdateLog(const QVariant& var)
{
	QVariantList Lines = var.toList();
	if(Lines.isEmpty())
		return;
	for(int i = 0; i < Lines.size(); i ++)
	{
		QVariantMap LogEntry = Lines.at(i).toMap();

		qint64 ID = LogEntry["ID"].toULongLong();
		if(m_LastIDs.contains(ID))
			continue;
		m_LastIDs.append(ID);

		int uFlag = LogEntry["Flag"].toUInt();
		QVariantMap Entry = LogEntry["Line"].toMap();
		QString Line = Entry["Str"].toString();
		foreach(const QString& Arg, Entry["Args"].toStringList())
			Line = Line.arg(Arg);

		QColor Color;
		switch(uFlag & LOG_MASK)
		{
			case LOG_ERROR:		Color = Qt::red;		break;
			case LOG_WARNING:	Color = Qt::darkYellow;	break;
			case LOG_SUCCESS:	Color = Qt::darkGreen;	break;
			case LOG_INFO:		Color = Qt::blue;		break;
			default:			Color = Qt::black;		break;
		}

		m_pLogText->setTextColor(Color);
		m_pLogText->insertPlainText(QDateTime::fromTime_t((time_t)LogEntry["Stamp"].toULongLong()).toLocalTime().time().toString() + ": " + Line + "\n");
		QScrollBar* pBar = m_pLogText->verticalScrollBar();
		pBar->setValue(pBar->maximum());
	}
}
