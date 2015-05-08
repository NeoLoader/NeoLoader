#include "GlobalHeader.h"
#include "KadHeader.h"
#include "FirewallHandler.h"
#include "Kademlia.h"
#include "KadConfig.h"
#include "KadHandler.h"
#include "KadNode.h"
#include "RoutingFork.h"
#include "RoutingRoot.h"
#include "PayloadIndex.h"

IMPLEMENT_OBJECT(CFirewallHandler, CObject)

CFirewallHandler::CFirewallHandler(CSmartSocket* pSocket, CObject* pParent)
: CObject(pParent), CSmartSocketInterface(pSocket, "FW")
{
}

void CFirewallHandler::Process(UINT Tick)
{
	list<CSafeAddress::EProtocol> Protocols = GetProtocols();
	for(list<CSafeAddress::EProtocol>::iterator I = Protocols.begin(); I != Protocols.end();I++)
	{
		SWatchDog &WatchDog = m_WatchDogs[*I];

		if(WatchDog.NextCheck < GetCurTick())
		{
			if(WatchDog.TestStage == SWatchDog::eNone)
			{
				LogLine(LOG_DEBUG, L"checking address");
				WatchDog.TestStage = SWatchDog::eDetectAddr;
			}

			int Running = 0;
			int Done = 0;
			for(map<CSafeAddress, uint64>::iterator J = WatchDog.Pending.begin(); J != WatchDog.Pending.end(); J++)
			{
				if(J->second == -1)
					Done++;
				else if(J->second > GetCurTick())
					Running++;
			}

			if(WatchDog.TestStage == SWatchDog::eDetectAddr)
			{
				int Fails = 0; // we need some simple statistical criteria for no more nodes to be asked
				while(Max(Fails,Running + Done) < GetParent<CKademlia>()->Cfg()->GetInt("AddressChecks"))
				{
					CKadNode* pNode = GetParent<CKademlia>()->Root()->GetRandomNode(*I);
					if(!pNode)
						break;
					
					CSafeAddress Address = pNode->GetAddress(*I);
					ASSERT(Address.IsValid());
					if(WatchDog.Pending.find(Address) != WatchDog.Pending.end())
					{
						Fails++;
						continue;
					}

					WatchDog.Pending.insert(pair<CSafeAddress, uint64>(Address, GetCurTick() + SEC2MS(GetParent<CKademlia>()->Cfg()->GetInt("AddressTimeout"))));

					CPointer<CComChannel> pChannel = Connect(Address, true);
					SendCheckReq(pChannel);
					Running++;
					Fails = 0;
				}

				if(Running == 0) // if there are no more checks running go to next step
				{
					WatchDog.Pending.clear();
					if(WatchDog.Results.empty())
					{
						WatchDog.NextCheck = GetCurTick() + SEC2MS(GetParent<CKademlia>()->Cfg()->GetInt("AddressInterval"));
						WatchDog.TestStage = SWatchDog::eNone;
						continue; // we have no results, retry shoetly
					}

					int TotalCount = 0;
					CSafeAddress BestAddress;
					int BestCount = 0;
					int PortCount = 0;
					for(map<CSafeAddress, SWatchDog::SResult>::iterator J = WatchDog.Results.begin(); J != WatchDog.Results.end(); J++)
					{
						if(J->second.Count > BestCount)
						{
							TotalCount += J->second.Count;
							BestCount = J->second.Count;
							BestAddress = J->first;
							PortCount = 0;
							for(map<uint16, int>::iterator K = J->second.Ports.begin(); K != J->second.Ports.end(); K++)
							{
								if(K->second > PortCount)
								{
									PortCount = K->second;
									BestAddress.SetPort(K->first);
								}
							}
						}
					}
					ASSERT(BestAddress.IsValid());
					WatchDog.Results.clear();

					if(BestCount * 100 < TotalCount * GetParent<CKademlia>()->Cfg()->GetInt("AddressRatio"))
						continue; // we have a conflict, retry imminetly
					WatchDog.CurAddress = BestAddress;
					
					if(BestAddress.GetPort() != GetParent<CKademlia>()->GetPort(*I) // if the majority returned the local port trust it
					 && PortCount * 100 < BestCount * GetParent<CKademlia>()->Cfg()->GetInt("AddressRatio"))
						BestAddress.SetPort(0); // port is NATed and not Traversable

					WatchDog.TestStage = SWatchDog::eCheckPort;
					WatchDog.CurAddress = BestAddress;

					LogLine(LOG_DEBUG, L"Address %s Confirmed", WatchDog.CurAddress.ToString().c_str());
				}
			}
			else if(WatchDog.TestStage == SWatchDog::eCheckPort || WatchDog.TestStage == SWatchDog::eCheckAuxPort)
			{
				int Fails = 0; // we need some simple statistical criteria for no more nodes to be asked
				while(Max(Fails,Running + Done) < GetParent<CKademlia>()->Cfg()->GetInt("AddressChecks"))
				{
					CKadNode* pNode = GetParent<CKademlia>()->Root()->GetRandomNode(*I);
					if(!pNode)
						break;
					
					CSafeAddress Address = pNode->GetAddress(*I);
					ASSERT(Address.IsValid());
					if(WatchDog.Pending.find(Address) != WatchDog.Pending.end())
					{
						Fails++;
						continue;
					}

					WatchDog.Pending.insert(pair<CSafeAddress, uint64>(Address, GetCurTick() + SEC2MS(GetParent<CKademlia>()->Cfg()->GetInt("AddressTimeout"))));
					CSafeAddress TestAddress = WatchDog.CurAddress;
					if(WatchDog.TestStage == SWatchDog::eCheckPort) // first we test the port we expect to hav been forwarded
						TestAddress.SetPort(GetParent<CKademlia>()->GetPort(*I));
					//else if(WatchDog.TestStage == SWatchDog::eCheckAuxPort)

					CPointer<CComChannel> pChannel = Connect(Address, true);
					SendTestReq(pChannel, TestAddress);
					Running++;
					Fails = 0;
				}

				if(Running == 0) // if there are no more checks running go to next step
				{
					WatchDog.Pending.clear();
					
					int Tests = 0;
					int Successes = 0;
					for(map<CSafeAddress, pair<bool,bool> >::iterator J = WatchDog.Reports.begin(); J != WatchDog.Reports.end(); J++)
					{
						if(!J->second.first)
							continue; // invalid result
						Tests++;
						if(J->second.second)
							Successes++;
					}
					WatchDog.Reports.clear();

					if(Tests * GetParent<CKademlia>()->Cfg()->GetInt("AddressConRatio") <= Successes * 100)
					{
						if(WatchDog.TestStage == SWatchDog::eCheckPort)
						{
							uint16 uPort = GetParent<CKademlia>()->GetPort(*I);
							if(WatchDog.CurAddress.GetPort() == uPort)
							{
								LogLine(LOG_SUCCESS | LOG_DEBUG, L"Adress check found Open %s", WatchDog.CurAddress.ToString().c_str());
								m_AddrPool[*I] = CMyAddress(WatchDog.CurAddress);
							}
							else
							{
								uint16 uAuxPort = WatchDog.CurAddress.GetPort();
								WatchDog.CurAddress.SetPort(uPort); // make sure we will advertige the primary port
								LogLine(LOG_SUCCESS | LOG_DEBUG, L"Adress check found Open Forwarded %s", WatchDog.CurAddress.ToString().c_str());
								m_AddrPool[*I] = CMyAddress(WatchDog.CurAddress);
								m_AddrPool[*I].SetAuxPort(uAuxPort); // make sure we know the port is forwarded and the otehr side first sees the wrong port
							}
						}
						else
						{
							ASSERT(WatchDog.TestStage == SWatchDog::eCheckAuxPort);
							LogLine(LOG_INFO | LOG_DEBUG, L"Adress check found Open NATed %s", WatchDog.CurAddress.ToString().c_str());
							m_AddrPool[*I] = CMyAddress(WatchDog.CurAddress);
						}
					}
					else if(WatchDog.TestStage == SWatchDog::eCheckPort && WatchDog.CurAddress.GetPort() != 0 && WatchDog.CurAddress.GetPort() != GetParent<CKademlia>()->GetPort(*I))
					{
						WatchDog.TestStage = SWatchDog::eCheckAuxPort;
						continue;
					}
					else 
					{
						if(WatchDog.CurAddress.GetPort() != 0)
						{
							LogLine(LOG_WARNING | LOG_DEBUG, L"Adress check found NATed %s", WatchDog.CurAddress.ToString().c_str());
							m_AddrPool[*I] = CMyAddress(WatchDog.CurAddress, eFWNATed);
						}
						else
						{
							WatchDog.CurAddress.SetPort(0); // there is no usable port for incomming communication
							LogLine(LOG_ERROR | LOG_DEBUG, L"Adress check found Closed %s", WatchDog.CurAddress.ToString().c_str());
							m_AddrPool[*I] = CMyAddress(WatchDog.CurAddress, eFWClosed);
						}
					}

					WatchDog.NextCheck = GetCurTick() + SEC2MS(GetParent<CKademlia>()->Cfg()->GetInt("AddressInterval"));
					WatchDog.TestStage = SWatchDog::eNone; // shedule next test
				}
			}
		}
	}

	// check if we need NAt assistance and if yes if we have an assistent, request one if needed
	for(TMyAddressMap::iterator I = m_AddrPool.begin(); I != m_AddrPool.end();I++)
	{
		if(I->second.NeedsAssistance() && GetCurTick() - I->second.IsAssistentPending() > SEC2MS(GetParent<CKademlia>()->Cfg()->GetInt("AddressTimeout")))
		{
			if(CKadNode* pNode = GetParent<CKademlia>()->Root()->GetRandomNode(I->second.GetProtocol()))
			{
				CSafeAddress Address = pNode->GetAddress(I->second.GetProtocol());
				ASSERT(Address.IsValid());

				CPointer<CComChannel> pChannel = Connect(Address);
				SendAssistanceReq(pChannel);
				I->second.AddPendingAssistent(pChannel);
			}
		}
	}

	// clean up broken relay tunnels, forget all that dont have a valid conneciton anymore
	for(map<CSafeAddress, pair<CPointer<CComChannel>, uint64> >::iterator I = m_PersistentTunnels.begin(); I != m_PersistentTunnels.end(); )
	{
		if(!I->second.first->IsConnected())
		{
			LogLine(LOG_NOTE, L"Relay tunnel to %s, broke after %.2f seconds", I->first.ToString().c_str(), (GetCurTick() - I->second.second) / 1000.0);
			TMyAddressMap::iterator J =  m_AddrPool.find(I->first.GetProtocol());
			if(J != m_AddrPool.end() && J->second.GetAssistentChannel())
			{
				if(J->second.GetAssistentChannel() == I->second.first)
					J->second.ClearAssistent();
			}
			I = m_PersistentTunnels.erase(I);
		}
		else
			I++;
	}

	// clean up pending address tests, forget all that dont have a valid conneciton anymore
	for(map<CSafeAddress, CPointer<CComChannel> >::iterator I = m_PendingTestRelays.begin(); I != m_PendingTestRelays.end(); )
	{
		if(!I->second) // the pointer is week and we leave it to the smart socket to timeout it
			I = m_PendingTestRelays.erase(I);
		else
			I++;
	}
}

/////////////////////

void CFirewallHandler::SendCheckReq(CComChannel* pChannel)
{
	CVariant CheckReq(CVariant::EMap); // Note: dont sent invalid variants, init as map
	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugFW"))
		LogLine(LOG_DEBUG, L"Sending 'Address request' to %s", pChannel->GetAddress().ToString().c_str());
	pChannel->QueuePacket(FW_CHECK_RQ, CheckReq);
}

void CFirewallHandler::HandleCheckReq(const CVariant& CheckReq, CComChannel* pChannel)
{
	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugFW"))
		LogLine(LOG_DEBUG, L"Recived 'Address request' from %s", pChannel->GetAddress().ToString().c_str());

	CVariant CheckRes;
	CheckRes["ADDR"] = pChannel->GetAddress().ToVariant(); // return seen address
	
	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugFW"))
		LogLine(LOG_DEBUG, L"Sending 'Address Response' to %s", pChannel->GetAddress().ToString().c_str());
	pChannel->QueuePacket(FW_CHECK_RS, CheckRes);
}

void CFirewallHandler::HandleCheckRes(const CVariant& CheckRes, CComChannel* pChannel)
{
	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugFW"))
		LogLine(LOG_DEBUG, L"Recived 'Address response' from %s", pChannel->GetAddress().ToString().c_str());

	SWatchDog &WatchDog = m_WatchDogs[pChannel->GetAddress().GetProtocol()];
	map<CSafeAddress, uint64>::iterator I = WatchDog.Pending.find(pChannel->GetAddress());
	if(I == WatchDog.Pending.end() || I->second == -1)
		throw CException(LOG_WARNING | LOG_DEBUG, L"Unsilicitated Address response from %s", pChannel->GetAddress().ToString().c_str());

	I->second = -1;

	CSafeAddress UsedAddress;
	UsedAddress.FromVariant(CheckRes["ADDR"]);
	CSafeAddress BaseAddress = UsedAddress;
	BaseAddress.SetPort(0);

	SWatchDog::SResult &Result = WatchDog.Results[BaseAddress];
	Result.Count++;
	Result.Ports[UsedAddress.GetPort()]++;
}

/////////////////////

void CFirewallHandler::SendTestReq(CComChannel* pChannel, const CSafeAddress& TestAddress)
{
	CVariant TestReq(CVariant::EMap); // Note: dont sent invalid variants, init as map
	TestReq["ADDR"] = TestAddress.ToVariant();

	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugFW"))
		LogLine(LOG_DEBUG, L"Sending 'Address test request' to %s", pChannel->GetAddress().ToString().c_str());
	pChannel->QueuePacket(FW_TEST_RQ, TestReq);
}

void CFirewallHandler::HandleTestReq(const CVariant& TestReq, CComChannel* pChannel)
{
	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugFW"))
		LogLine(LOG_DEBUG, L"Recived 'Address test relay Request' from %s", pChannel->GetAddress().ToString().c_str());

	CKadNode* pNode = GetParent<CKademlia>()->Root()->GetRandomNode(pChannel->GetAddress().GetProtocol());
	if(!pNode) // should not happen but in case
	{
		SendTestAck(pChannel, CSafeAddress(), "NoHlp");
		return;
	}

	CSafeAddress NodeAddress = pNode->GetAddress(pChannel->GetAddress().GetProtocol());
	ASSERT(NodeAddress.IsValid());
	if(pChannel->GetAddress().Compare(NodeAddress, true) == 0) // node cant test its own ports, nor should two nodes on the same IP
	{
		SendTestAck(pChannel, CSafeAddress(), "NoHlp");
		return;
	}

	CSafeAddress TestAddress;
	TestAddress.FromVariant(TestReq["ADDR"]);

	CVariant RelTestReq;
	RelTestReq["ADDR"] = TestAddress.ToVariant();

	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugFW"))
		LogLine(LOG_DEBUG, L"Sending 'relayed Address test request' to %s", NodeAddress.ToString().c_str());

	m_PendingTestRelays[TestAddress] = CPointer<CComChannel>(pChannel, true);
	pChannel->QueuePacket(FW_TEST_REL_RQ, RelTestReq);
}

void CFirewallHandler::HandleTestRelReq(const CVariant& RelTestReq, CComChannel* pChannel)
{
	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugFW"))
		LogLine(LOG_DEBUG, L"Recived 'relayed Address test Request' from %s", pChannel->GetAddress().ToString().c_str());

	CSafeAddress NodeAddress;
	NodeAddress.FromVariant(RelTestReq["ADDR"]);

	bool bNack = GetChannels(NodeAddress).size() > 0; // we can not test if we are already connected

	CVariant RelTestAck;
	RelTestAck["ADDR"] = RelTestReq["ADDR"];
	if(bNack)
		RelTestAck["ERR"] = "HasCon";

	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugFW"))
		LogLine(LOG_DEBUG, L"Sending 'relayed Address test ack' to %s", pChannel->GetAddress().ToString().c_str());
	pChannel->QueuePacket(FW_TEST_REL_ACK, RelTestAck);

	if(!bNack)
		SendTestRes(pChannel);
}

void CFirewallHandler::HandleTestRelAck(const CVariant& RelTestAck, CComChannel* pChannel)
{
	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugFW"))
		LogLine(LOG_DEBUG, L"Recived 'relayed Address test ack' from %s", pChannel->GetAddress().ToString().c_str());

	CSafeAddress TestAddress;
	TestAddress.FromVariant(RelTestAck["ADDR"]);

	map<CSafeAddress, CPointer<CComChannel> >::iterator I = m_PendingTestRelays.find(TestAddress);
	if(I == m_PendingTestRelays.end() || !I->second) // pointer is week
		return;

	SendTestAck(I->second, pChannel->GetAddress(), RelTestAck.Get("ERR"));
	m_PendingTestRelays.erase(I);
}

void CFirewallHandler::SendTestAck(CComChannel* pChannel, const CSafeAddress& HlprAddress, const CVariant& Err)
{
	CVariant TestAck;
	TestAck["ADDR"] = pChannel->GetAddress().ToVariant();
	if(HlprAddress.IsValid())
		TestAck["HLPR"] = HlprAddress.ToVariant();
	if(Err.IsValid())
		TestAck["ERR"] = Err;

	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugFW"))
		LogLine(LOG_DEBUG, L"Sending 'Address test ack' to %s", pChannel->GetAddress().ToString().c_str());
	pChannel->QueuePacket(FW_TEST_ACK, TestAck);
}

void CFirewallHandler::HandleTestAck(const CVariant& TestAck, CComChannel* pChannel)
{
	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugFW"))
		LogLine(LOG_DEBUG, L"Recived 'Address test ack' from %s", pChannel->GetAddress().ToString().c_str());

	SWatchDog &WatchDog = m_WatchDogs[pChannel->GetAddress().GetProtocol()];
	map<CSafeAddress, uint64>::iterator I = WatchDog.Pending.find(pChannel->GetAddress());
	if(I == WatchDog.Pending.end() || I->second == -1)
		throw CException(LOG_WARNING | LOG_DEBUG, L"Unsilicitated Address test response from %s", pChannel->GetAddress().ToString().c_str());

	if(TestAck.Has("ERR"))
	{
		I->second = 0; // count as failed
		return;
	}

	I->second = -1;

	// K-ToDo: check if thats right
	//CSafeAddress TestAddress;
	//TestAddress.FromVariant(TestAck["ADDR"]);

	CSafeAddress HelperAddress;
	HelperAddress.FromVariant(TestAck["HLPR"]);

	WatchDog.Reports[HelperAddress].first = true;

	WatchDog.NextCheck = GetCurTick() + SEC2MS(3); // force a short wair for the responce to arive, this path is shorter but is longer but it still may be faster
}

void CFirewallHandler::SendTestRes(CComChannel* pChannel)
{
	CVariant TestRes;
	TestRes["ADDR"] = pChannel->GetAddress().ToVariant();

	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugFW"))
		LogLine(LOG_DEBUG, L"Sending 'Address test result' to %s", pChannel->GetAddress().ToString().c_str());
	pChannel->QueuePacket(FW_TEST_RES, TestRes);
}

void CFirewallHandler::HandleTestRes(const CVariant& TestRes, CComChannel* pChannel)
{
	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugFW"))
		LogLine(LOG_DEBUG, L"Recived 'Address test result' from %s", pChannel->GetAddress().ToString().c_str());

	SWatchDog &WatchDog = m_WatchDogs[pChannel->GetAddress().GetProtocol()];

	// K-ToDo: check if thats right
	//CSafeAddress TestAddress;
	//TestAddress.FromVariant(TestAck["ADDR"]);

	// Note: if first is not set second is ignorred, packets may arive in any order
	WatchDog.Reports[pChannel->GetAddress()].second = true;
}

/////////////////////

void CFirewallHandler::SendAssistanceReq(CComChannel* pChannel)
{
	CVariant AssistanceReq(CVariant::EMap); // Note: dont sent invalid variants, init as map
	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugFW"))
		LogLine(LOG_DEBUG, L"Sending 'Assistance request' to %s", pChannel->GetAddress().ToString().c_str());
	pChannel->QueuePacket(FW_REQUEST_ASSISTANCE, AssistanceReq);
}

void CFirewallHandler::HandleAssistanceReq(const CVariant& AssistanceReq, CComChannel* pChannel)
{
	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugFW"))
		LogLine(LOG_DEBUG, L"Recived 'Assistance request' from %s", pChannel->GetAddress().ToString().c_str());

	CVariant AssistanceRes;
	if(m_PersistentTunnels.size() >= (size_t)GetParent<CKademlia>()->Cfg()->GetInt("MaxTunnels"))
		AssistanceRes["ERR"] = "Denided";
	else
	{
		pair<CPointer<CComChannel>, uint64> &PersistentTunnel = m_PersistentTunnels[pChannel->GetAddress()];
		if(PersistentTunnel.first == NULL)
		{
			PersistentTunnel.first = pChannel;
			PersistentTunnel.second = GetCurTick();
		}
		AssistanceRes["AGE"] = GetCurTick() - PersistentTunnel.second;
	}

	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugFW"))
		LogLine(LOG_DEBUG, L"Sending 'Assistance response' to %s", pChannel->GetAddress().ToString().c_str());
	pChannel->QueuePacket(FW_ASSISTANCE_RESPONSE, AssistanceRes);
}

void CFirewallHandler::HandleAssistanceRes(const CVariant& AssistanceRes, CComChannel* pChannel)
{
	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugFW"))
		LogLine(LOG_DEBUG, L"Recived 'Assistance response' from %s", pChannel->GetAddress().ToString().c_str());

	TMyAddressMap::iterator I =  m_AddrPool.find(pChannel->GetAddress().GetProtocol());
	if(I != m_AddrPool.end() && I->second.IsAssistentPending(pChannel))
	{
		if(AssistanceRes.Has("ERR"))
			I->second.ClearAssistent();
		else
		{
			I->second.ConfirmAssistent();

			pair<CPointer<CComChannel>, uint64> &PersistentTunnel = m_PersistentTunnels[pChannel->GetAddress()];
			if(PersistentTunnel.first == NULL)
			{
				PersistentTunnel.first = pChannel;
				PersistentTunnel.second = GetCurTick();
			}
		}
	}
}

/////////////////////

void CFirewallHandler::SendTunnel(const CNodeAddress& Address, bool bCallBack)
{
	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugFW"))
		LogLine(LOG_DEBUG, L"Sending 'Tunel request' to %s", Address.ToString().c_str());

	CVariant RdvReq;
	RdvReq["ADDR"] = Address.ToVariant();
	RdvReq["MODE"] = bCallBack ? "CLBK" : "RDVZ";
	RelayPacket(FW_TUNNEL_REQUEST, RdvReq, Address);
}

void CFirewallHandler::HandleTunnel(const CVariant& RdvReq, CComChannel* pChannel)
{
	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugFW"))
		LogLine(LOG_DEBUG, L"Resived 'Tunel request' from %s", pChannel->GetAddress().ToString().c_str());

	if(RdvReq["MODE"].To<string>() == "RDVZ")
		Rendevouz(pChannel->GetAddress());
	else if(RdvReq["MODE"].To<string>() == "CLBK")
		CallBack(pChannel->GetAddress(), true);
	else
		throw CException(LOG_WARNING, L"Unsupported Session Tunneling Mode %s", RdvReq["MODE"].To<wstring>().c_str());
}

////////////////////////////////////////////////////////////////////////
// 

bool CFirewallHandler::RelayPacket(const string& Name, const CVariant& Packet, const CNodeAddress& Address)
{
	CSafeAddress* pAddress = Address.GetAssistent();
	if(!pAddress)
		return false;

	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugFW"))
		LogLine(LOG_DEBUG, L"Sending Relayed 'Packet' to %s over %s", Address.ToString().c_str(), pAddress->ToString().c_str());

	CVariant Relay;
	CSafeAddress NodeAddress = Address;
	Relay["ADDR"] = NodeAddress.ToVariant();
	Relay["NAME"] = Name;
	Relay["DATA"] = Packet;

	CPointer<CComChannel> pChannel = Connect(*pAddress, true);
	pChannel->QueuePacket(FW_RELAY_REQUEST, Relay); // send relayed packets directly over UDP
	return true;
}

void CFirewallHandler::HandleRelay(const CVariant& Packet, CComChannel* pChannel)
{
	CSafeAddress TargetAddress;
	TargetAddress.FromVariant(Packet["ADDR"]);
	
	map<CSafeAddress, pair<CPointer<CComChannel>, uint64> >::iterator I = m_PersistentTunnels.find(TargetAddress);
	if(I == m_PersistentTunnels.end())
		return; // K-ToDo-Now: send error

	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugFW"))
		LogLine(LOG_DEBUG, L"Relaying 'Packet' from %s to %s", pChannel->GetAddress().ToString().c_str(), TargetAddress.ToString().c_str());

	CVariant Relay;
	Relay["ADDR"] = pChannel->GetAddress().ToVariant();
	Relay["NAME"] = Packet["NAME"];
	Relay["DATA"] = Packet["DATA"];
	I->second.first->QueuePacket(FW_RELAYED_PACKET, Relay);
}

void CFirewallHandler::HandleRelayed(const CVariant& Relay, CComChannel* pChannel)
{
	CSafeAddress SourceAddress;
	SourceAddress.FromVariant(Relay["ADDR"]);

	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugFW"))
		LogLine(LOG_DEBUG, L"Recived Relayed 'Packet' from %s over %s", SourceAddress.ToString().c_str(), pChannel->GetAddress().ToString().c_str());

	CPointer<CComChannel> pAuxChannel = Connect(SourceAddress, true);
	ProcessPacket(Relay["NAME"], Relay["DATA"], pAuxChannel);
}

////////////////////////////////////////////////////////////////////////
// 

bool CFirewallHandler::ProcessPacket(const string& Name, const CVariant& Packet, CComChannel* pChannel)
{
	try
	{
		if(Name.compare(FW_CHECK_RQ) == 0)					HandleCheckReq(Packet, pChannel);
		else if(Name.compare(FW_CHECK_RS) == 0)				HandleCheckRes(Packet, pChannel);

		else if(Name.compare(FW_TEST_RQ) == 0)				HandleTestReq(Packet, pChannel);
		else if(Name.compare(FW_TEST_REL_RQ) == 0)			HandleTestRelReq(Packet, pChannel);
		else if(Name.compare(FW_TEST_REL_ACK) == 0)			HandleTestRelAck(Packet, pChannel);
		else if(Name.compare(FW_TEST_ACK) == 0)				HandleTestAck(Packet, pChannel);
		else if(Name.compare(FW_TEST_RES) == 0)				HandleTestRes(Packet, pChannel);
		
		else if(Name.compare(FW_REQUEST_ASSISTANCE) == 0)	HandleAssistanceReq(Packet, pChannel);
		else if(Name.compare(FW_ASSISTANCE_RESPONSE) == 0)	HandleAssistanceRes(Packet, pChannel);

		else if(Name.compare(FW_TUNNEL_REQUEST) == 0)		HandleTunnel(Packet, pChannel);

		else if(Name.compare(FW_RELAY_REQUEST) == 0)		HandleRelay(Packet, pChannel);
		else if(Name.compare(FW_RELAYED_PACKET) == 0)		HandleRelayed(Packet, pChannel);

		else 
			throw CException(LOG_WARNING, L"Unsupported Firewall Packet Recived %S", Name.c_str());
	}
	catch(const CException& Exception)
	{
		LogLine(Exception.GetFlag(), L"Packet \'%S\' Error: %s", Name.c_str(), Exception.GetLine().c_str());
		return false;
	}
	return true;
}

////////////////////////////////////////////////////////////////////////
// 

const CMyAddress* CFirewallHandler::GetAddress(CSafeAddress::EProtocol Protocol) const
{
	TMyAddressMap::const_iterator I =  m_AddrPool.find(Protocol);
	if(I != m_AddrPool.end())
		return &I->second;
	return NULL;
}

EFWStatus CFirewallHandler::GetFWStatus(CSafeAddress::EProtocol Protocol) const
{
	TMyAddressMap::const_iterator I =  m_AddrPool.find(Protocol);
	if(I != m_AddrPool.end())
		return I->second.GetStatus();
	return eFWClosed;
}
