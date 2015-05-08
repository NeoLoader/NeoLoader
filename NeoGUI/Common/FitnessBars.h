#pragma once

class CFitnessBars: public QWidget
{
	Q_OBJECT
public:
	CFitnessBars(QWidget *parent = 0)
	: QWidget(parent)
	{
		setMaximumHeight(16);
		//setMaximumWidth(40);

		QHBoxLayout* pLayout = new QHBoxLayout();
		pLayout->setMargin(0);
		pLayout->setSpacing(3);
		pLayout->setAlignment(Qt::AlignLeft);

		m_pBar = new QLabel();
		pLayout->addWidget(m_pBar);

		m_pText = new QLabel();
		pLayout->addWidget(m_pText);

		setLayout(pLayout);

		m_iValue = -1;
		SetValue(0, "");
	}

	void SetValue(int iValue, const QString& Text)
	{
		if(m_iValue != iValue)
		{
			m_iValue = iValue;

			int Bars = (iValue) & 0xFF;
			if(Bars < 0)
				Bars = 0;
			else if(Bars > 4)
				Bars = 4;

			int Color = (iValue >> 8) & 0xFF;
			if(Color < 1)
				Color = 1;
			else if(Color > 3)
				Color = 3;

			if(Bars == 0)
				Color = 0;

			m_pBar->setPixmap(QPixmap(QString(":/AvailBars/Bar%1_%2.png").arg(Bars).arg(Color)));
		}
		//QFontMetrics Metric(m_pText->font());
		//m_pText->setMinimumWidth(Metric.width(Text) + 4);
		m_pText->setText(Text);
	}
protected:

	int					m_iValue;
	QLabel*				m_pBar;
	QLabel*				m_pText;
};