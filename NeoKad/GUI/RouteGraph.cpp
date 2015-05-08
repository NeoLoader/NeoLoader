#include "GlobalHeader.h"

#include "RouteGraph.h"
#include "RouteEdge.h"
#include "RouteNode.h"



#include <math.h>

/*double SampleDown(const CUInt128& ID)
{
	double Value = 0;
	/for(int i=0; i<ID.GetBitSize(); i++)
	{
		if(ID.GetBit(i))
			Value += pow(2.0, i ? (1.0+i)/2.0 : 0);
	}/
	for(int i=0; i<ID.GetBitSize()/2; i++)
	{
		if(ID.GetBit(i*2) || ID.GetBit((i*2)+1))
			Value += pow(2.0, i);
	}
	return Value;
}*/

CRouteGraph::CRouteGraph(QWidget *parent)
 : QGraphicsView(parent)
{
    QGraphicsScene *scene = new QGraphicsScene(this);
    scene->setItemIndexMethod(QGraphicsScene::NoIndex);
    setScene(scene);
    setCacheMode(CacheBackground);
    setViewportUpdateMode(BoundingRectViewportUpdate);
    setRenderHint(QPainter::Antialiasing);
    setTransformationAnchor(AnchorUnderMouse);
	m_ScaleFactor = 1;
}

void CRouteGraph::Reset()
{
	foreach(CUInt128 ID, m_Nodes.uniqueKeys())
		delete m_Nodes.take(ID);
}

void CRouteGraph::ShowRoute(const QList<SRouteEntry>& Entrys)
{
	CUInt128 MaxDistance;
	uint64 StartTime = ULLONG_MAX;
	uint64 MaxTime = 0;

	// Nodes BEGIN
	QMap<CUInt128, CRouteNode*> Nodes = m_Nodes;
	foreach(const SRouteEntry& Entry, Entrys)
	{
		CRouteNode* Node = Nodes.take(Entry.ID);
		if(!Node)
		{
			ASSERT(!m_Nodes.contains(Entry.ID));
			Node = new CRouteNode();
			m_Nodes.insert(Entry.ID, Node);
			scene()->addItem(Node);
			Node->setScale(m_ScaleFactor);
		}

		Node->SetColor(Entry.Color);
		Node->setToolTip(Entry.Tipp);

		if(Entry.Distance > MaxDistance)
			MaxDistance = Entry.Distance;

		if(Entry.Time > MaxTime)
			MaxTime = Entry.Time;
		if(Entry.Time < StartTime)
			StartTime = Entry.Time;
	}

	foreach(CUInt128 ID, Nodes.uniqueKeys())
	{
		CRouteNode* Node = Nodes[ID];
		m_Nodes.remove(ID);
		delete Node;
	}
	// Nodes END

	// Edges BEGIN
	foreach(const SRouteEntry& Entry, Entrys)
	{
		CRouteNode* Node = m_Nodes.value(Entry.ID);
		ASSERT(Node);

		QMap<CRouteNode*, CRouteEdge*> Edges;
		QMap<CRouteNode*, CRouteEdge*> BackEdges;
		foreach(CRouteEdge* Edge, Node->Edges())
		{
			if(Edge->GetSourceNode() != Node)
				Edges.insert(Edge->GetSourceNode(), Edge);
			if(Edge->GetDestNode() != Node)
			{
				if(Edge->IsBiDir())
					Edges.insert(Edge->GetDestNode(), Edge);
				else
					BackEdges.insert(Edge->GetDestNode(), Edge);
			}
		}

		foreach(const CUInt128 ByID, Entry.ByIDs.uniqueKeys())
		{
			if(CRouteNode* FoundBy = m_Nodes.value(ByID))
			{
				CRouteEdge* Edge = Edges.take(FoundBy);
				if(!Edge)
				{
					Edge = BackEdges.take(FoundBy);
					if(Edge)
						Edge->SetBiDir(true);
					else
					{
						ASSERT(FoundBy != Node);
						Edge = new CRouteEdge(FoundBy, Node);
						scene()->addItem(Edge);
						Edge->setScale(m_ScaleFactor);
					}
				}
				Edge->SetColor(Entry.ByIDs.value(ByID));
			}
		}

		foreach(CRouteEdge* Edge, Edges)
			delete Edge;
		foreach(CRouteEdge* Edge, BackEdges)
			Edge->SetBiDir(false);
	}
	// Edges END

	if(Entrys.isEmpty())
		return;

	double SigDistance = 0;
	int SigDWordIndex = 0;
	for(int i=MaxDistance.GetDWordCount()-1; i >= 0; i--)
	{
		if(MaxDistance.GetDWord(i) != 0)
		{
			SigDWordIndex = i;
			SigDistance = MaxDistance.GetDWord(SigDWordIndex);
			break;
		}
	}
	

	// Positions BEGIN
	qreal Width = width()-32-NODE_SIZE;
	qreal Height = height()-48-NODE_SIZE;
	qreal ScaleX = (MaxTime - StartTime > Width) ? Width / (MaxTime - StartTime) : 1;
	qreal ScaleY = (SigDistance > Height) ? Height / SigDistance : 1;
	//qDebug() << "ScaleX: " << ScaleX << "    ScaleY: " << ScaleY;

	//foreach(CRouteNode* Node, m_Nodes)
	foreach(const SRouteEntry& Entry, Entrys)
	{
		CRouteNode* Node = m_Nodes.value(Entry.ID);
		ASSERT(Node);

		//double Distance = SampleDown(Entry.Distance);
		double Distance = Entry.Distance.GetDWord(SigDWordIndex);
		Node->setPos(QPointF(10 + NODE_SIZE/2 + ScaleX * (Entry.Time - StartTime), -(17 + NODE_SIZE/2 + ScaleY * Distance)));
	}
	// Positions END
}

void CRouteGraph::resizeEvent(QResizeEvent *event)
{
	QGraphicsView::resizeEvent(event);

	//scene()->setSceneRect(-(width()/2)+8, -(height()/2)+8, width()-16, height()-16);
	scene()->setSceneRect(8, -height()+16, width()-16, height()-32);
}

void CRouteGraph::drawBackground(QPainter* Painter, const QRectF&)
{
	QRectF SceneRect = sceneRect();

    Painter->setBrush(Qt::NoBrush);
    //Painter->drawRect(SceneRect);
	Painter->drawLine(SceneRect.left(), SceneRect.bottom(), SceneRect.left(), -SceneRect.height());
	Painter->drawLine(SceneRect.left(), SceneRect.bottom(), SceneRect.right(), SceneRect.bottom());

    QFont Font = Painter->font();
    Font.setPointSize(10);
    Painter->setFont(Font);
    Painter->setPen(Qt::black);
    Painter->drawText(QRectF(SceneRect.right() - 60, SceneRect.bottom(), 60, 20), tr("Time"),QTextOption(Qt::AlignRight));
	Painter->drawText(QRectF(SceneRect.left(), SceneRect.top() - 5, 60, 20), tr("Distance"));
}

void CRouteGraph::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) 
	{
		case Qt::Key_Plus:
			ScaleView(qreal(1.2));
			break;
		case Qt::Key_Minus:
			ScaleView(1 / qreal(1.2));
			break;
		default:
			QGraphicsView::keyPressEvent(event);
    }
}

void CRouteGraph::wheelEvent(QWheelEvent *event)
{
    ScaleView(pow((double)2, -event->delta() / 240.0));
}

void CRouteGraph::ScaleView(qreal ScaleFactor)
{
    qreal Factor = transform().scale(ScaleFactor, ScaleFactor).mapRect(QRectF(0, 0, 1, 1)).width();
    if (Factor < 0.5 || Factor > 100)
        return;
    
	m_ScaleFactor = 1.0/Factor;
	scale(ScaleFactor, ScaleFactor);

	foreach(CRouteNode* Node, m_Nodes)
	{
		Node->setScale(m_ScaleFactor);
		foreach(CRouteEdge* Edge, Node->Edges())
		{
			Edge->Adjust();
			Edge->setScale(m_ScaleFactor);
		}
	}
}
