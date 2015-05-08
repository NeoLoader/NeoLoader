#pragma once

#include <QtNetwork>

class COnlineCompleter: public QCompleter
{
	Q_OBJECT

public:
	COnlineCompleter(QWidget *parent = 0);

public slots:
	void			OnUpdate(const QString& Text);

private slots:
	void			OnFinished(QNetworkReply*);

protected:
	QNetworkAccessManager*	m_pNet;
	QStringListModel* m_pModel;
	QStringList		m_List;	
	int				m_LastLength;
	int				m_LastCount;
};