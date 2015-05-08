#pragma once

class CProgressJob;

class CProgressBar: public QLabel
{
	Q_OBJECT

public:
	CProgressBar(uint64 ID = -1, uint64 SubID = 0, const QString& Mode = "", QWidget *parent = 0);
	CProgressBar(uint64 ID, const QString& Groupe, const QString& Hoster, const QString& User, const QString& Mode = "", QWidget *parent = 0);
	~CProgressBar() {}

	void				SetDepth(int iDepth)		{m_Depth = iDepth;}
	void				SetID(uint64 ID);
	void				SetProgress(int Progress)	{m_Progress = Progress;}

	uint64				GetNextUpdate()				{return m_NextUpdate;}
	void				Update();

protected:
	friend class CProgressJob;

	uint64				m_ID;
	QVariantMap			m_Request;

	uint64				m_NextUpdate;
	int					m_Depth;
	int					m_Progress;

	CProgressJob*		m_pProgressJob;
};