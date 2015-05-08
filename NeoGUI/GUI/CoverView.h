#pragma once

#include <QtNetwork>

class CCoverView: public QGroupBox
{
	Q_OBJECT

public:
	CCoverView(QWidget *parent = 0);
	~CCoverView() {}

	void				ShowCover(uint64 ID, const QString& Url);

public slots:
	void				OnMenuRequested(const QPoint &point);

	void				OnSetCover();

private slots:
	void				OnFinished(QNetworkReply*);

protected:
	void				resizeEvent(QResizeEvent* e);

	uint64				m_ID;
	QString				m_Url;

	QVBoxLayout*		m_pMainLayout;
	QLabel*				m_pCover;

	QMenu*				m_pMenu;
	QAction*			m_pSetCover;

	QNetworkAccessManager*	m_pNet;
	QPixmap				m_Pixmap;
};