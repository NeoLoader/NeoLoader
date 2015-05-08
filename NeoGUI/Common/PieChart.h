#pragma once

#include "nightcharts.h"
#include <QPainter>
#include <QPaintEvent>

class QPieChart: public QWidget
{
	Q_OBJECT

public:
	QPieChart(QWidget* parent = 0) : QWidget(parent){
		m_pPieChart = NULL;
	}
	~QPieChart() {
		delete m_pPieChart;
	}

	void Reset()
	{
		delete m_pPieChart;

		m_pPieChart = new Nightcharts;
		m_pPieChart->setShadows(true);
		m_pPieChart->setType(Nightcharts::Dpie);//{Histogramm,Pie,Dpie};
		m_pPieChart->setLegendType(Nightcharts::Horizontal);//{Round,Vertical}
	}

	void AddPiece(const QString& Name, const QColor& Color,float Percentage)
	{
		if(!m_pPieChart)
			Reset();
		m_pPieChart->addPiece(Name, Color, Percentage);
	}

protected:
	void paintEvent(QPaintEvent* e)
	{
		if(m_pPieChart)
		{
			QPainter p(this);
			int h = (m_pPieChart->piece()+1)*(p.fontMetrics().height()+10);
			m_pPieChart->setCords(0, 0, width(), height() - h);
			m_pPieChart->draw(&p);
			m_pPieChart->drawLegend(&p);
		}
		else
			QWidget::paintEvent(e);
	}

	Nightcharts* m_pPieChart;
};