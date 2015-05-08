#pragma once

#include <QGraphicsItem>
#include <QList>

class CRouteEdge;
class GraphWidget;
QT_BEGIN_NAMESPACE
class QGraphicsSceneMouseEvent;
QT_END_NAMESPACE

#define NODE_SIZE 12.0

class CRouteNode : public QGraphicsItem
{
public:
    CRouteNode();
	~CRouteNode();

    void AddEdge(CRouteEdge* Edge);
	QList<CRouteEdge*>& Edges()			{return m_Edges;}

    enum { Type = UserType + 1 };
    int type() const { return Type; }

	void HighLight(bool Set);
	void SetColor(QColor Color);
	void setPos(const QPointF& Pos);

    QRectF boundingRect() const;
    QPainterPath shape() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

protected:
	void Select(bool Set);

    QVariant itemChange(GraphicsItemChange change, const QVariant &value);

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
    
private:
    QList<CRouteEdge*>	m_Edges;
	bool				m_HighLighted;
	bool				m_Selected;
	QColor				m_Color;
	QPointF				m_Return;
};
