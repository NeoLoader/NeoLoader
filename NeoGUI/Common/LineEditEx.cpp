#include "GlobalHeader.h"
#include "LineEditEx.h"
#include "QApplication"

QLineEditEx::QLineEditEx(QWidget *parent)
: QLineEdit(parent)
{
    m_Index = -1;
	m_Limit = 100;

	m_TabIndex = -1;

    connect(this, SIGNAL(returnPressed()), this, SLOT(addHistory()));
}

void QLineEditEx::addHistory()
{
    addHistory(text().trimmed());
}

void QLineEditEx::pressReturn()
{
	emit returnPressed();
}

void QLineEditEx::addHistory(const QString &text)
{
    if (m_History.indexOf(text) == 0 || text.isEmpty())
		return;

    m_History.prepend(text);
    m_Index = -1;

    if (m_History.size() > m_Limit) 
        m_History.removeAt(m_Limit);
}

QStringList QLineEditEx::history()
{
    return m_History;
}

void QLineEditEx::clearHistory()
{
    m_History.clear();
    m_Index = -1;
}

void QLineEditEx::keyPressEvent(QKeyEvent *e)
{
    if(e->key() == Qt::Key_Up)	
	{
		loadHistory(true,m_History,m_Index);
		return;
	}
    else if (e->key() == Qt::Key_Down)
	{
		loadHistory(false,m_History,m_Index);
		return;
	}
	else if(e->key() == Qt::Key_Left)
	{
		if(m_TabIndex != -1)
		{
			suggest(false);
			return;
		}
	}
	else if(e->key() == Qt::Key_Right)
	{
		if(cursorPosition() == text().length() || m_TabIndex != -1)
		{
			suggest(true);
			return;
		}
	}
	else
	{
		m_TabBase.clear();
		m_TabIndex = -1;
	}
	QLineEdit::keyPressEvent(e);
}

void QLineEditEx::loadHistory(bool up, QStringList& History, int &Index)
{
	int count = History.size();
    if (count == 0)
		return;

    if (up)
		Index++;
    else 
		Index--;

    if (Index >= count)
        Index = count-1;
    else if (Index < 0)
        Index=0;

    setText(History.at(Index));
}

void QLineEditEx::suggest(bool up)
{
	if(m_TabBase.isEmpty())
		m_TabBase = text();
	if(m_TabBase.isEmpty())
		return;

	int Length = m_TabBase.length();

	QStringList Suggestions;
	foreach(QString Line, m_History)
	{
		if(Line.left(Length).compare(m_TabBase, Qt::CaseInsensitive) == 0)
		{
			int Pos = Line.indexOf(" ", Length);
			if(Pos != -1)
				Line.truncate(Pos);
			if(!Suggestions.contains(Line))
				Suggestions.append(Line);
		}
	}

	loadHistory(up,Suggestions,m_TabIndex);
}