#pragma once
#include "../Common/GroupBoxEx.h"

class CFileListWidget;

class CServiceSummary: public QWidget
{
	Q_OBJECT

public:
	CServiceSummary(QWidget *parent = 0);
	~CServiceSummary();

private slots:


protected:
	virtual void		timerEvent(QTimerEvent *e);

	int					m_TimerId;

	QGridLayout*		m_pMainLayout;

	QGroupBoxEx<QHBoxLayout>* m_AnonGroup;
	QLabel*				m_pAnonIcon;
	QLabel*				m_pAnonText;
};