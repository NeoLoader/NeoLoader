#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QVariant>

class CSessionLogWidget: public QWidget
{
    Q_OBJECT
public:
    CSessionLogWidget(QWidget *parent = 0);
    ~CSessionLogWidget();

	void UpdateLog(const QVariant& var);
	qint64 GetLastID() {if(m_LastIDs.isEmpty()) return 0; return m_LastIDs.last();}

protected:
	QTextEdit			*m_pLogText;
	QList<qint64>		m_LastIDs;

    Q_DISABLE_COPY(CSessionLogWidget)
};

