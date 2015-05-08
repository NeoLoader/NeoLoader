#pragma once

class CSummarySyncJob;

class CFileSummary: public QWidget
{
	Q_OBJECT

public:
	CFileSummary(UINT Mode, QWidget *parent = 0);

	void				ShowSummary(uint64 ID);

	void				ChangeMode(UINT Mode)		{m_Mode = Mode;}

	static int			GetFitness(const QVariantMap& File);

private slots:
	void				OnAnchorClicked(const QUrl& Url);

protected:
	friend class CSummarySyncJob;

	void				AddEntry(const QString& Text, const QString& Icon = "", int Size = 0);

	uint64				m_ID;
	UINT				m_Mode;

	QHBoxLayout*		m_pMainLayout;
	QScrollArea*		m_pScrollArea;

	QWidget*			m_pBoxWidget;
	QVBoxLayout*		m_pBoxLayout;

	QList<QWidget*>		m_Widgets;
};

class QResizeWidget: public QWidget
{
	Q_OBJECT

public:
	QResizeWidget() {}

private slots:
	void ResizeWidgets()
	{
		foreach(QObject* pObj, this->children())
		{
			if(QTextBrowser* pEdit = qobject_cast<QTextBrowser*>(pObj))
			{
				if(int h = pEdit->document()->size().height())
				{
					pEdit->setMinimumHeight(h);
					pEdit->setMaximumHeight(h);
				}
			}
		}
	}

protected:
	void resizeEvent(QResizeEvent* e)
	{
		QWidget::resizeEvent(e);
		ResizeWidgets();
	}
};