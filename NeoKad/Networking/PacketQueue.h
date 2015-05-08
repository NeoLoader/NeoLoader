#pragma once

//#include "../Common/Object.h"
#include "../Common/Variant.h"

struct SVarPacket
{
	SVarPacket() : iPriority(0) {}
	SVarPacket(const string& name, const CVariant& data, int priority = 0)
	{
		Name = name;
		Data = data;
		iPriority = priority;
		//uTTL = -1;
	}

	string		Name;
	CVariant	Data;
	int			iPriority;
	UINT		uID;
	//uint64		uTTL;
};

class CPacketQueue
{
public:
	CPacketQueue();

	UINT					Push(const string &Name, const CVariant& Data, int iPriority = 0/*, uint32 uTTL = -1*/);
	bool					IsQueued(UINT uID) const;
	SVarPacket*				Front();
	void					Pop();

	size_t					GetCount() const		{return m_Queue.size();}
	size_t					GetSize() const			{return m_QueueSize;}

protected:
	
	deque<SVarPacket>		m_Queue;
	size_t					m_QueueSize;
	UINT					m_uLastID;
};