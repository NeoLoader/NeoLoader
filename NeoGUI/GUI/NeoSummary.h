#pragma once
#include <QVBoxLayout>
#include "../Common/GroupBoxEx.h"

class CFileListWidget;
class QPieChart;

class CNeoSummary: public QWidget
{
	Q_OBJECT

public:
	CNeoSummary(QWidget *parent = 0);
	~CNeoSummary();

private slots:


protected:
	friend class CStartTopSearchJob;

	virtual void		timerEvent(QTimerEvent *e);

	int					m_TimerId;

	QGridLayout*		m_pMainLayout;

	QGroupBoxEx<QVBoxLayout>* m_pUploadGroup;
	QPieChart*			m_pUploadVolume;
	QGroupBoxEx<QVBoxLayout>* m_pDownloadGroup;
	QPieChart*			m_pDownloadVolume;

	QGroupBoxEx<QVBoxLayout>* m_pTopGroup;
	CFileListWidget*	m_pTopFiles;

	QGroupBoxEx<QHBoxLayout>* m_pFWGroup;
	QLabel*				m_pFWIcon;
	QLabel*				m_pFWText;

	uint64				m_SearchID;
	uint64				m_LastSearch;
};