#include "GlobalHeader.h"
#include "KadHeader.h"
#include "LookupHistory.h"
#include "KadNode.h"
#include "Kademlia.h"
#include "KadConfig.h"
#include "KadLookup.h"
#include "RoutingRoot.h"
#include "KadHandler.h"

IMPLEMENT_OBJECT(CLookupHistory, CObject)

CLookupHistory::CLookupHistory(CObject* pParent)
: CObject(pParent) 
{
}

void CLookupHistory::ExpectResponse(const CUInt128& ID)
{
	SLookupNode& Node = m_Nodes[ID];
	Node.AskedResults = GetCurTick();
	Node.AnsweredResults = 0;
	if(Node.FromID == 0)
		Node.FromID = SELF_NODE;
}

void CLookupHistory::RegisterResponse(const CUInt128& ID)
{
	SLookupNode& Node = m_Nodes[ID];
	Node.AnsweredResults = GetCurTick();
	Node.FoundResults += 1;
	if(Node.GraphTime == 0)
		Node.GraphTime = GetCurTick();
}

void CLookupHistory::UnRegisterResponse(const CUInt128& ID)
{
	SLookupNode& Node = m_Nodes[ID];
	if(Node.AskedResults && Node.AnsweredResults != -1)
		Node.FoundResults = 0;
}

void CLookupHistory::AddNodes(const NodeMap& Nodes, const CUInt128& ByID)
{
	for(NodeMap::const_iterator I = Nodes.begin(); I != Nodes.end(); I++)
	{
		LookupNodeMap::iterator J = m_Nodes.find(I->second->GetID());
		if(J == m_Nodes.end())
			J = m_Nodes.insert(LookupNodeMap::value_type(I->second->GetID(), SLookupNode())).first;
		J->second.FoundByIDs.push_back(ByID == 0 ? SELF_NODE : ByID);
	}
}

void CLookupHistory::ExpectNodes(const CUInt128& ID, const CUInt128& ByID)
{
	SLookupNode& Node = m_Nodes[ID];
	Node.AskedCloser = GetCurTick();
	Node.AnsweredCloser = 0;
	Node.FromID = ByID == 0 ? SELF_NODE : ByID;
}

void CLookupHistory::RegisterNodes(const CUInt128& ID, int Count)
{
	SLookupNode& Node = m_Nodes[ID];
	Node.AnsweredCloser = GetCurTick();
	Node.FoundCloser += Count;

	if(Node.GraphTime == 0)
		Node.GraphTime = GetCurTick();
}

void CLookupHistory::RecivedTraceResults(const CUInt128& ID, const CUInt128& ByID)
{
	SLookupNode& Node = m_Nodes[ID];
	if(Node.GraphTime == 0)
		Node.GraphTime = GetCurTick();
	Node.RemoteResults[ByID].Count += 1;
	Node.RemoteResults[ByID].ArivalTime = GetCurTick();

	if(Node.RemoteResults.size() > (size_t)GetParent<CKademlia>()->Cfg()->GetInt("KeepTrace"))
	{
		for(map<CUInt128, SLookupNode::SRemoteResults>::iterator J = Node.RemoteResults.begin(); J != Node.RemoteResults.end(); )
		{
			if(GetCurTick() - J->second.ArivalTime > SEC2MS(GetParent<CKademlia>()->Cfg()->GetInt("TraceTimeOut")))
				J = Node.RemoteResults.erase(J);
			else
				J++;
		}
	}
}

void CLookupHistory::RecivedTraceResults(const CVariant& Trace)
{
	if(Trace.Count() < 1)
		return;

	CUInt128 PrevID = Trace.At(Trace.Count()-1);
	for(int i=Trace.Count()-2; i >= 0; i--)
	{
		CUInt128 CurID = Trace.At(i);
		RecivedTraceResults(CurID, PrevID);
		PrevID = CurID;
	}
}