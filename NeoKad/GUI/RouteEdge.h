#pragma once

#include <QGraphicsItem>

class CRouteNode;

#define ARROW_SIZE 6.0

class CRouteEdge : public QGraphicsItem
{
public:
    CRouteEdge(CRouteNode* SourceNode, CRouteNode* DestNode);
	~CRouteEdge();

	CRouteNode* GetSourceNode() const	{return m_SourceNode;}
	CRouteNode*	GetDestNode() const		{return m_DestNode;}

    void Adjust();

	void HighLight(bool Set);
	void SetColor(QColor Color);
	void SetBiDir(bool Set);
	bool IsBiDir()						{return m_BiDir;}

    enum { Type = UserType + 2 };
    int type() const { return Type; }
    
protected:
    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
    
private:
    CRouteNode*			m_SourceNode;
	CRouteNode*			m_DestNode;

    QPointF				m_SourcePoint;
    QPointF				m_DestPoint;
	bool				m_HighLighted;
	QColor				m_Color;
	bool				m_BiDir;
};

