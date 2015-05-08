#include "GlobalHeader.h"

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QStyleOption>

#include "RouteEdge.h"
#include "RouteNode.h"
#include "RouteGraph.h"


CRouteNode::CRouteNode()
{
    setFlag(ItemIsMovable);
    setFlag(ItemSendsGeometryChanges);
    setCacheMode(DeviceCoordinateCache);
    setZValue(-1);
	m_HighLighted = false;
	m_Selected = false;
	m_Color = Qt::yellow;
}

CRouteNode::~CRouteNode()
{
	foreach(CRouteEdge* Edge, m_Edges)
		delete Edge;
}

void CRouteNode::AddEdge(CRouteEdge* Edge)
{
    m_Edges.append(Edge);
    Edge->Adjust();
}

QRectF CRouteNode::boundingRect() const
{
	qreal Extra = NODE_SIZE + 4;
    return QRectF(-Extra/2, -Extra/2, Extra, Extra);
}

QPainterPath CRouteNode::shape() const
{
    QPainterPath path;
    path.addEllipse(-NODE_SIZE/2, -NODE_SIZE/2, NODE_SIZE, NODE_SIZE);
    return path;
}

void CRouteNode::HighLight(bool Set)
{
	m_HighLighted = Set;
	update();
}

void CRouteNode::SetColor(QColor Color)
{
	m_Color = Color;
	update();
}

void CRouteNode::setPos(const QPointF& Pos)
{
	m_Return = Pos;
	if(!m_Selected)
		QGraphicsItem::setPos(Pos);
}

void CRouteNode::paint(QPainter *Painter, const QStyleOptionGraphicsItem* Option, QWidget *)
{
    QRadialGradient Gradient(-3, -3, NODE_SIZE/2);
    if (Option->state & QStyle::State_Sunken)
	{
        Gradient.setCenter(3, 3);
        Gradient.setFocalPoint(3, 3);
		Gradient.setColorAt(0, m_Color.dark().light());
		Gradient.setColorAt(1, m_Color.light());
    }
	else if (m_HighLighted) 
	{
		Gradient.setColorAt(0, m_Color);
		Gradient.setColorAt(1, m_Color.dark().light());
	}
	else 
	{
        Gradient.setColorAt(0, m_Color);
        Gradient.setColorAt(1, m_Color.dark());
    }
    Painter->setBrush(Gradient);

    Painter->setPen(QPen(Qt::black, 0));
    Painter->drawEllipse(-NODE_SIZE/2, -NODE_SIZE/2, NODE_SIZE, NODE_SIZE);
}

QVariant CRouteNode::itemChange(GraphicsItemChange change, const QVariant &value)
{
    switch (change) 
	{
		case ItemPositionHasChanged:
			foreach (CRouteEdge* Edge, m_Edges)
				Edge->Adjust();
			break;
		default:
			break;
    };
    return QGraphicsItem::itemChange(change, value);
}

void CRouteNode::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
	Select(true);
    QGraphicsItem::mousePressEvent(event);
}

void CRouteNode::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
	Select(false);
	setPos(m_Return);
    QGraphicsItem::mouseReleaseEvent(event);
}

void CRouteNode::Select(bool Set)
{
	m_Selected = Set;
    HighLight(Set);
	foreach(CRouteEdge* Edge, m_Edges)
	{
		Edge->HighLight(Set);
		if(Edge->GetSourceNode() == this)
			Edge->GetDestNode()->HighLight(Set);
		else
			Edge->GetSourceNode()->HighLight(Set);
	}
}