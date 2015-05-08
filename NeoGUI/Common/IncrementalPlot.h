#pragma once

class QwtPlot;
class QwtPlotCurve;

class CIncrementalPlot : public QWidget
{
  Q_OBJECT

public:
	enum EUnits{
		eAU,
		eBytes
	};

	CIncrementalPlot(const QColor& Back = Qt::white, const QColor& Front = Qt::black, const QColor& Grid = Qt::gray, const QString& yAxis = "", EUnits eUnits = eAU, QWidget *parent = 0);
	~CIncrementalPlot();

	void				AddPlot(const QString& Name, const QColor& Color, Qt::PenStyle Style, const QString& Title = "");
	void				AddPlotPoint(const QString& Name, double Value);

	void				Reset();

	void				SetLimit(int iLimit)	{m_iLimit = iLimit;}

public slots:
	void				Replot();

protected:
	QVBoxLayout*		m_pMainLayout;

	QwtPlot*			m_pChart;
	struct SCurve
	{
		SCurve() : pPlot(0), uSize(0), xData(0), yData(0) {}
		QwtPlotCurve*	pPlot;
		size_t			uSize;
		double*			xData;
		double*			yData;
	};
	QMap<QString, SCurve> m_Curves;
	bool				m_bReplotPending;
	int					m_iLimit;
};
