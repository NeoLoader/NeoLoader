#include "GlobalHeader.h"
#include "PacketQueue.h"

CPacketQueue::CPacketQueue()
{
	m_QueueSize = 0;
	m_uLastID = 0;
}

UINT CPacketQueue::Push(const string &Name, const CVariant& Data, int iPriority/*, uint32 uTTL*/)
{
	SVarPacket Packet(Name, Data, iPriority);
	Packet.Data.Freeze();
	ASSERT(Packet.Data.IsFrozen());
	Packet.uID = ++m_uLastID;
	/*if(uTTL != -1)
		Packet.uTTL = GetCurTick() + uTTL;*/

	m_QueueSize += Packet.Data.GetSize() + Packet.Name.size();

	if(iPriority)
	{
		//deque<SVarPacket>::iterator I = find_if(m_Queue.begin(), m_Queue.end(), ...);
		deque<SVarPacket>::iterator first = m_Queue.begin();
		deque<SVarPacket>::iterator last = m_Queue.end();
		for (; first != last; ++first) 
		{
			if (first->iPriority < iPriority) // find the first packet that has a lower priority than ours and insert it befoure it
			{ 
				last = first;
				break;
			}
		}
		m_Queue.insert(last, Packet);
	}
	else
		m_Queue.push_back(Packet);

	return m_uLastID;
}

bool CPacketQueue::IsQueued(UINT uID) const
{
	deque<SVarPacket>::const_iterator first = m_Queue.begin();
	deque<SVarPacket>::const_iterator last = m_Queue.end();
	for (; first != last; ++first) 
	{
		if (first->uID == uID)
		{
			/*if(pPacket->uTTL != -1 && pPacket->uTTL < GetCurTick())
				return false;*/
			return true;
		}
	}
	return false;
}

SVarPacket* CPacketQueue::Front()
{
	while(!m_Queue.empty())
	{
		SVarPacket* pPacket = &m_Queue.front();
		/*if(pPacket->uTTL != -1 && pPacket->uTTL < GetCurTick()) // is this packet past its expiration date?
		{
			Pop();
			continue;
		}*/
		return pPacket;
	}
	return NULL;
}

void CPacketQueue::Pop()
{
	if(!m_Queue.empty())
	{
		SVarPacket &Packet = m_Queue.front();
		ASSERT(m_QueueSize >= Packet.Data.GetSize() + Packet.Name.size());
		m_QueueSize -= Packet.Data.GetSize() + Packet.Name.size();
		m_Queue.pop_front();
	}
}