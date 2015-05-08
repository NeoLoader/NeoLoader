#pragma once

#include "KadNode.h"
class CKadLookup;

struct SLookupNode
{
	SLookupNode()
	{
		GraphTime = GetCurTick();

		AskedCloser = 0;
		AnsweredCloser = 0;
		FoundCloser = 0;

		AskedResults = 0;
		AnsweredResults = 0;
		FoundResults = 0;
	}
	uint64				GraphTime;		// first found
	list<CUInt128>		FoundByIDs;		// found by

	struct SRemoteResults
	{
		SRemoteResults() {Count = 0; ArivalTime = 0;}
		int		Count;
		uint64	ArivalTime;
	};

	map<CUInt128, SRemoteResults>	RemoteResults;

	CUInt128			FromID;

	uint64				AskedCloser;
	uint64				AnsweredCloser;
	int					FoundCloser;

	uint64				AskedResults;
	uint64				AnsweredResults;
	int					FoundResults;
};

#define SELF_NODE	CUInt128(true)	// 0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF
#define NONE_NODE	CUInt128()		// 0x00000000000000000000000000000000

typedef map<CUInt128, SLookupNode>	LookupNodeMap;

class CLookupHistory: public CObject
{
public:
	DECLARE_OBJECT(CKadLookup)

	CLookupHistory(CObject* pParent = NULL);

	const LookupNodeMap& GetNodes()						{return m_Nodes;}

	void				ExpectResponse(const CUInt128& ID);
	void				RegisterResponse(const CUInt128& ID);
	void				UnRegisterResponse(const CUInt128& ID);
	void				AddNodes(const NodeMap& Nodes, const CUInt128& ByID);
	void				ExpectNodes(const CUInt128& ID, const CUInt128& ByID);
	void				RegisterNodes(const CUInt128& ID, int Count);
	void				RecivedTraceResults(const CUInt128& ID, const CUInt128& ByID);

	void				RecivedTraceResults(const CVariant& Trace);

protected:

	LookupNodeMap		m_Nodes;
};