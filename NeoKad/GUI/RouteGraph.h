#pragma once

#include <QGraphicsView>
#include <QKeyEvent>
#include <QWheelEvent>
#include "../Kad/UIntX.h"

class CRouteNode;

struct SRouteEntry
{	
	CUInt128			ID;
	QString				Tipp;
	QColor				Color;
	QMap<CUInt128, QColor> ByIDs;
	uint64				Time;
	CUInt128			Distance;
};

class CRouteGraph : public QGraphicsView
{
    Q_OBJECT

public:
    CRouteGraph(QWidget *parent = 0);

	void Reset();
	void ShowRoute(const QList<SRouteEntry>& Entrys);

	void ScaleView(qreal ScaleFactor);
	qreal GetScale()		{return m_ScaleFactor;}

protected:
    void keyPressEvent(QKeyEvent *event);
	void resizeEvent(QResizeEvent *event);
    void wheelEvent(QWheelEvent *event);
    void drawBackground(QPainter *painter, const QRectF &rect);

private:
	QMap<CUInt128, CRouteNode*>	m_Nodes;
	qreal						m_ScaleFactor;
};
