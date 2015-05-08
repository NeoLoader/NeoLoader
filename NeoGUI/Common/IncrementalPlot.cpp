#include "GlobalHeader.h"
#include "IncrementalPlot.h"
#include "../NeoGUI.h"
#include "../Common/Common.h"

#include "../../qwt/src/qwt_plot.h"
#include "../../qwt/src/qwt_plot_curve.h"
#include "../../qwt/src/qwt_plot_grid.h"
#include "../../qwt/src/qwt_plot_panner.h"
#include "../../qwt/src/qwt_plot_magnifier.h"
#include "../../qwt/src/qwt_plot_layout.h"
#include "../../qwt/src/qwt_scale_draw.h"
#include "../../qwt/src/qwt_legend.h"

class CDateScale : public QwtScaleDraw
{
public:
	QwtText label(double value) const { 
		return QDateTime::fromTime_t(value).time().toString(Qt::ISODate);  
	}
};

class CRateScale : public QwtScaleDraw
{
public:
  QwtText label(double value) const {
	  return FormatSize(value);
  }
};

class QwtLegendEx: public QwtLegend
{
public:
	QwtLegendEx() {}

    virtual void updateLegend( const QVariant &itemInfo, const QList<QwtLegendData> &data )
	{
		QList<QwtLegendData> temp = data;
		for(int i=0; i < temp.size(); i++)
		{
			if(temp[i].title().isEmpty())
				temp.removeAt(i--);
		}
		QwtLegend::updateLegend(itemInfo, temp);
	}
};

CIncrementalPlot::CIncrementalPlot(const QColor& Back, const QColor& Front, const QColor& Grid, const QString& yAxis, EUnits eUnits, QWidget *parent)
:QWidget(parent)
{
	m_bReplotPending = false;
	m_iLimit = HR2S(1);

	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);

	m_pChart = new QwtPlot();

	m_pMainLayout->addWidget(m_pChart);

	setLayout(m_pMainLayout);

	m_pChart->setStyleSheet(QString("color: rgb(%1, %2, %3); background-color: rgb(%4, %5, %6);")
		.arg(Front.red()).arg(Front.green()).arg(Front.blue())
		.arg(Back.red()).arg(Back.green()).arg(Back.blue()));

	QFont smallerFont(QApplication::font());
	m_pChart->setAxisScaleDraw(QwtPlot::xBottom, new CDateScale);
	switch(eUnits)
	{
	case eBytes: m_pChart->setAxisScaleDraw(QwtPlot::yLeft, new CRateScale);
	}
	m_pChart->setAxisFont(QwtPlot::yLeft,smallerFont);
	m_pChart->setAxisFont(QwtPlot::xBottom,smallerFont);

	m_pChart->plotLayout()->setAlignCanvasToScales(true);

	QwtLegendEx* pLegend = new QwtLegendEx;
	pLegend->setStyleSheet(QString("color: rgb(%1, %2, %3);")
		.arg(Front.red()).arg(Front.green()).arg(Front.blue()));
	m_pChart->insertLegend(pLegend, QwtPlot::BottomLegend);

	QwtPlotGrid *pGrid = new QwtPlotGrid;
	pGrid->setMajorPen(QPen(Grid,0,Qt::DotLine));
	pGrid->attach(m_pChart);

	QwtPlotPanner *pPanner = new QwtPlotPanner(m_pChart->canvas());
	pPanner->setAxisEnabled(QwtPlot::yRight,false);
	pPanner->setAxisEnabled(QwtPlot::xTop,false);
	pPanner->setMouseButton(Qt::MidButton);
	
	m_pChart->setAxisAutoScale(QwtPlot::yLeft);
	m_pChart->setAxisAutoScale(QwtPlot::xBottom);
	m_pChart->setAxisTitle(QwtPlot::yLeft,yAxis);
	//m_pChart->setAxisTitle(QwtPlot::xBottom,tr("Time"));
}

CIncrementalPlot::~CIncrementalPlot()
{
	foreach(const SCurve& Curve, m_Curves)
	{
		free(Curve.xData);
		free(Curve.yData);
	}
}

void CIncrementalPlot::AddPlot(const QString& Name, const QColor& Color, Qt::PenStyle Style, const QString& Title)
{
	SCurve& Curve = m_Curves[Name];
	ASSERT(Curve.pPlot == NULL);

	Curve.pPlot = new QwtPlotCurve(Title);
	Curve.pPlot->setPen(QPen(QBrush(Color), 1, Style));

	Curve.uSize = 1;
	Curve.xData = (double*)malloc(sizeof(double)*Curve.uSize);
	Curve.yData = (double*)malloc(sizeof(double)*Curve.uSize);
	Curve.xData[Curve.uSize-1] = GetTime();
	Curve.yData[Curve.uSize-1] = 0;

	Curve.pPlot->setRawSamples(Curve.xData,Curve.yData,Curve.uSize);
    Curve.pPlot->attach(m_pChart);

    /*zoomer = new ChartDateZoomer(qpChart->canvas());
    zoomer->setRubberBandPen(QPen(Qt::red,2,Qt::DotLine));
    zoomer->setTrackerPen(QPen(Qt::red));
    zoomer->setMousePattern(QwtEventPattern::MouseSelect2,Qt::RightButton,Qt::ControlModifier);
    zoomer->setMousePattern(QwtEventPattern::MouseSelect3,Qt::RightButton);*/
}

void CIncrementalPlot::AddPlotPoint(const QString& Name, double Value)
{
	if(!m_Curves.contains(Name))
		return;

	SCurve& Curve = m_Curves[Name];
	ASSERT(Curve.pPlot != NULL);

	if(m_iLimit > Curve.uSize)
	{
		Curve.uSize++;
		Curve.xData = (double*)realloc(Curve.xData, sizeof(double)*Curve.uSize);
		Curve.yData = (double*)realloc(Curve.yData, sizeof(double)*Curve.uSize);
	}
	else
	{
		memmove(Curve.xData, Curve.xData + 1, sizeof(double)*(Curve.uSize - 1));
		memmove(Curve.yData, Curve.yData + 1, sizeof(double)*(Curve.uSize - 1));
	}
	Curve.xData[Curve.uSize-1] = GetTime();
	Curve.yData[Curve.uSize-1] = Value;

	Curve.pPlot->setRawSamples(Curve.xData,Curve.yData,Curve.uSize);

	if(!m_bReplotPending)
	{
		m_bReplotPending = true;
		QTimer::singleShot(100,this,SLOT(Replot()));
	}
}

void CIncrementalPlot::Replot()
{
	m_pChart->replot();
	m_bReplotPending = false;
}

void CIncrementalPlot::Reset()
{
	foreach(const SCurve& curve, m_Curves)
	{
		SCurve& Curve = *(SCurve*)&curve;
		ASSERT(Curve.pPlot != NULL);

		Curve.uSize = 1;
		Curve.xData = (double*)malloc(sizeof(double)*Curve.uSize);
		Curve.yData = (double*)malloc(sizeof(double)*Curve.uSize);
		Curve.xData[Curve.uSize-1] = GetTime();
		Curve.yData[Curve.uSize-1] = 0;

		Curve.pPlot->setRawSamples(Curve.xData,Curve.yData,Curve.uSize);
	}

	m_pChart->replot();
}