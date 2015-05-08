// found here: http://wirastokarim.wordpress.com/2010/05/28/history-di-qlineedit-bag-2-selesai/
// no idea what language it is and what does it say in the blog post, but the code is C++ and it works
//

#ifndef QLINEEDITEX_H
#define QLINEEDITEX_H

#include <QLineEdit>
#include <QKeyEvent>

class QLineEditEx : public QLineEdit
{
    Q_OBJECT

public:
    QLineEditEx(QWidget *parent=0);

    void			addHistory(const QString &text);
    QStringList		history();
    void			clearHistory();
    void			setHistoryLimit(int limit) { m_Limit = limit; }
    int				historyLimit() { return m_Limit; }
	QStringList&	saveState() {return m_History;}
	void			restoreState(QStringList History) {m_History = History;}

public slots:
    void			addHistory();
	void			pressReturn();

protected:
    void			keyPressEvent(QKeyEvent *e);
	void			loadHistory(bool up, QStringList& History, int &Index);
	void			suggest(bool up);

    QStringList		m_History;
    int				m_Index;
    int				m_Limit;

	QString			m_TabBase;
	int				m_TabIndex;
};

#endif // QLINEEDITEX_H