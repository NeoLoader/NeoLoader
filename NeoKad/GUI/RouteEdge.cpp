#include "GlobalHeader.h"

#include <QPainter>

#include "RouteEdge.h"
#include "RouteNode.h"

#include <math.h>
#ifndef M_PI
#define M_PI       3.14159265358979323846264338327950288419717
#endif

CRouteEdge::CRouteEdge(CRouteNode* SourceNode, CRouteNode* DestNode)
{
    setAcceptedMouseButtons(0);
    m_SourceNode = SourceNode;
    m_DestNode = DestNode;
	ASSERT(m_SourceNode != m_DestNode);
    m_SourceNode->AddEdge(this);
    m_DestNode->AddEdge(this);
    Adjust();
	m_HighLighted = false;
	m_Color = Qt::gray;
	m_BiDir = false;
}

CRouteEdge::~CRouteEdge()
{
	m_DestNode->Edges().removeAll(this);
	m_SourceNode->Edges().removeAll(this);
}

void CRouteEdge::Adjust()
{
    if(!m_SourceNode || !m_DestNode)
		return;

    QLineF Line(mapFromItem(m_SourceNode, 0, 0), mapFromItem(m_DestNode, 0, 0));
    qreal Length = Line.length();

    prepareGeometryChange();

    if (Length <= qreal(NODE_SIZE)) 
		m_SourcePoint = m_DestPoint = Line.p1();
	else
	{
        QPointF EdgeOffset((Line.dx() * NODE_SIZE/2) / Length, (Line.dy() * NODE_SIZE/2) / Length);
        m_SourcePoint = Line.p1() + EdgeOffset;
        m_DestPoint = Line.p2() - EdgeOffset;
    }
}

QRectF CRouteEdge::boundingRect() const
{
    if(!m_SourceNode || !m_DestNode)
		return QRectF();

    qreal Extra = ARROW_SIZE / 2.0;
    return QRectF(m_SourcePoint, QSizeF(m_DestPoint.x() - m_SourcePoint.x(), m_DestPoint.y() - m_SourcePoint.y())).normalized().adjusted(-Extra, -Extra, Extra, Extra);
}

void CRouteEdge::HighLight(bool Set)
{
	m_HighLighted = Set; 
	update();
}

void CRouteEdge::SetColor(QColor Color)
{
	m_Color = Color;
	update();
}

void CRouteEdge::SetBiDir(bool Set)
{
	m_BiDir = Set;
	update();
}

void CRouteEdge::paint(QPainter* Painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    if(!m_SourceNode || !m_DestNode)
		return;

    QLineF Line(m_SourcePoint, m_DestPoint);
    if (qFuzzyCompare(Line.length(), qreal(0.)))
        return;

    // Draw the line itself
	if(m_HighLighted)
	{
		Painter->setPen(QPen(m_Color, 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
		Painter->setBrush(m_Color);
	}
	else
	{
		Painter->setPen(QPen(m_Color.dark(), 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
		Painter->setBrush(m_Color.dark());
	}
    Painter->drawLine(Line);

    // Draw the arrows
    double Angle = ::acos(Line.dx() / Line.length());
    if (Line.dy() >= 0)
        Angle = M_PI + M_PI - Angle;

	if(m_BiDir)
	{
		QPointF SourceArrowP1 = m_SourcePoint + QPointF(sin(Angle + M_PI / 3) * ARROW_SIZE, cos(Angle + M_PI / 3) * M_PI);
		QPointF SourceArrowP2 = m_SourcePoint + QPointF(sin(Angle + 2*M_PI / 3) * ARROW_SIZE, cos(Angle + 2*M_PI / 3) * M_PI);
	}
    QPointF DestArrowP1 = m_DestPoint + QPointF(sin(Angle - M_PI / 3) * ARROW_SIZE, cos(Angle - M_PI / 3) * ARROW_SIZE);
    QPointF DestArrowP2 = m_DestPoint + QPointF(sin(Angle - 2*M_PI / 3) * ARROW_SIZE, cos(Angle - 2*M_PI / 3) * ARROW_SIZE);

    //Painter->drawPolygon(QPolygonF() << Line.p1() << SourceArrowP1 << SourceArrowP2);
    Painter->drawPolygon(QPolygonF() << Line.p2() << DestArrowP1 << DestArrowP2);
}
