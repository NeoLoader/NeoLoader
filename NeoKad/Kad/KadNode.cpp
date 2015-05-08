#include "GlobalHeader.h"
#include "KadHeader.h"
#include "KadNode.h"
#include "Kademlia.h"
#include "KadHandler.h"
#include "KadConfig.h"
#include "FirewallHandler.h"
#include "../../Framework/Cryptography/HashFunction.h"

CTrustedKey::CTrustedKey(byte* pKey, size_t uSize, bool bAuthenticated, UINT eAlgorithm)
 : CAbstractKey(pKey, uSize)
{
	if(eAlgorithm == CAbstractKey::eUndefined)
		eAlgorithm = CHashFunction::eSHA256;
	SetAlgorithm(eAlgorithm);

	m_bAuthenticated = bAuthenticated;

	CHashFunction Hash(eAlgorithm);
	Hash.Add(pKey, uSize);
	Hash.Finish();
	Hash.Fold((byte*)&m_FingerPrint, sizeof(uint64));
}

IMPLEMENT_OBJECT(CKadNode, CObject)

CKadNode::CKadNode(CObject* pParent)
: CObject(pParent)
{
	m_Protocol = 0;

	m_uFirstSeen = m_uLastSeen = GetTime();
	m_uLastHello = 0;
	m_iFailed = 0;

	m_UpLimit = new CBandwidthLimit(this);
	m_DownLimit = new CBandwidthLimit(this);
}

void CKadNode::Load(const CVariant& Node, bool bFull)
{
	m_Protocol = Node["PROT"];
	m_ID.SetValue(CUInt128(Node["NID"]));

	CVariant Addr = Node["ADDR"];
	for(uint32 i=0; i < Addr.Count(); i++)
	{
		CNodeAddress Address;
		if(bFull)
			Address.FromExtVariant(Addr.At(i));
		else
			Address.FromVariant(Addr.At(i));
		InsertAddress(Address);
	}

	if(bFull)
	{
		m_Version = Node["VER"].To<string>();
		m_uFirstSeen = Node["FSEN"];
		m_uLastSeen = Node["LSEN"];

		if(IsFading())
		{
			for(AddressMap::iterator I = m_AddressMap.begin(); I != m_AddressMap.end(); I++)
				I->second.SetClass(NODE_DEFAULT_CLASS);
			m_uLastSeen = GetTime();
		}
	}
}

CVariant CKadNode::Store(bool bFull) const
{
	CVariant Node;
	Node["PROT"] = m_Protocol;
	Node["NID"] = m_ID;

	CVariant Addr;
	for(AddressMap::const_iterator I = m_AddressMap.begin(); I != m_AddressMap.end(); I++)
	{
		if(bFull)
			Addr.Append(I->second.ToExtVariant());
		else
			Addr.Append(I->second.ToVariant());
	}
	Node["ADDR"] = Addr;

	Node["VER"] = m_Version;
	if(bFull)
	{
		Node["FSEN"] = m_uFirstSeen;
		Node["LSEN"] = m_uLastSeen;
	}
	return Node;
}

CPublicKey* CKadNode::SetIDKey(const CVariant& Info)
{
	const CVariant& KeyValue = Info["PK"];
	CHolder<CPublicKey> pKey = new CPublicKey();	
	if(pKey->SetKey(KeyValue.GetData(), KeyValue.GetSize()))
	{
		UINT eHashFunkt = Info.Has("HK") ? CAbstractKey::Str2Algorithm(Info["HK"]) & CAbstractKey::eHashFunkt : CAbstractKey::eUndefined;
		if(m_ID.SetKey(pKey, eHashFunkt))
			return m_ID.GetKey();
		else
			LogLine(LOG_ERROR, L"Recived wrong Key from Node");
	}
	else
		LogLine(LOG_ERROR, L"Recived Invalid Key from Node");
	return NULL;
}

void CKadNode::Merge(CKadNode* pNode)
{
	for(AddressMap::iterator I = pNode->m_AddressMap.begin(); I != pNode->m_AddressMap.end(); I++)
		UpdateAddress(I->second);

	if(pNode->m_uFirstSeen < m_uFirstSeen)
		m_uFirstSeen = pNode->m_uFirstSeen;
}

void CKadNode::UpdateAddress(const CNodeAddress& Address)
{
	if(Address.GetProtocol() == CSafeAddress::eInvalid)
	{
		LogLine(LOG_ERROR, L"Atempted to add an invalid address to a kad node!");
		return;
	}

	m_uLastSeen = GetTime();

	// Note: The map layout is important:
	// For each protocol we have 0 or 1 verifyed addresses and 0 or 1 unverifyed addresses
	// The most recent addresses are located at the top
	// For the most purposes we howeever selelct always the verifyed address
	// If a new verifyed address is added all older addresses are removed
	// if a new unverifyed address is added older unverifyed addresses are removed
	// those we keep never more than 2 adresses for each protocol
	
	bool bAdd = true;
	for(AddressMap::iterator I = m_AddressMap.find(Address.GetProtocol()); I != m_AddressMap.end() && I->first == Address.GetProtocol();)
	{
		bool bAux = false;
		if(Address.Compare(I->second) == 0 || (bAux = (Address.Compare(I->second, true) == 0))) // Note: this comparation compares only addresses, not additional informations
		{
			if(Address.IsVerifyed())
			{
				I->second.SetVerifyed();
				I->second.SetAssistent(Address.GetAssistent()); // only verifyed (signed) adresses can set or clear the Assistent
			}

			if(Address.GetFirstSeen() < I->second.GetFirstSeen())
				I->second.SetFirstSeen(Address.GetFirstSeen());
			if(Address.GetLastSeen() > I->second.GetLastSeen())
				I->second.SetLastSeen(Address.GetLastSeen());
			if(Address.GetClass() < I->second.GetClass())
				I->second.SetClass(Address.GetClass());

			if(bAux)
			{
				// the Aux port is the port on which we see the node communicating but not its primary port
				// when we try to talk to the node we always go for the primary port
				if(Address.IsVerifyed())
				{
					I->second.SetAuxPort(I->second.GetPort());
					I->second.SetPort(Address.GetPort());
				}
				else
					I->second.SetAuxPort(Address.GetPort());
			}

			bAdd = false;
			break;
		}
		else if(!I->second.IsVerifyed()) // we dont keep unverifyed address es
			I = m_AddressMap.erase(I); 
		else
			I++;
	}
	
	// insert new address at the top
	AddressMap::iterator I = m_AddressMap.find(Address.GetProtocol());
	if(bAdd)
	{
		I = m_AddressMap.insert(I, AddressMap::value_type(Address.GetProtocol(), Address));

		if(I->second.GetPassKey() == 0) // node may use a custom passkey
			I->second.SetPassKey(GetPassKey());
		else { // Note: custom PCK's are actually allowed, but nut used yet those an custom apsskey ist most likelly a bug
			ASSERT(I->second.GetPassKey() == GetPassKey());
		}
	}
		
	if(Address.IsVerifyed()) // if the address is a verifyed one, remove all other with same protocol
	{
		for(AddressMap::iterator J = ++I; J != m_AddressMap.end() && J->first == Address.GetProtocol();)
			J = m_AddressMap.erase(J);
	}
}

void CKadNode::InsertAddress(const CNodeAddress& Address)
{
	AddressMap::iterator I = m_AddressMap.insert(AddressMap::value_type(Address.GetProtocol(), Address));

	if(I->second.GetPassKey() == 0) // node may use a custom passkey
		I->second.SetPassKey(GetPassKey());
	else { // Note: custom PCK's are actually allowed, but not used yet those an custom apsskey ist most likelly a bug
		ASSERT(I->second.GetPassKey() == GetPassKey());
	}
}

void CKadNode::RemoveAddress(const CNodeAddress& Address)
{
	for(AddressMap::iterator I = m_AddressMap.find(Address.GetProtocol()); I != m_AddressMap.end() && I->first == Address.GetProtocol(); I++)
	{
		if(I->second.CompareTo(Address))
		{
			m_AddressMap.erase(I);
			return;
		}
	}
}

CNodeAddress CKadNode::GetAddress(CSafeAddress::EProtocol Protocol) const
{
	CNodeAddress Address;
	AddressMap::const_iterator J = m_AddressMap.find(Protocol);
	if(J != m_AddressMap.end())
		Address = J->second;
	return Address;
}

bool CKadNode::CheckAddress(const CNodeAddress& Address)
{
	for(AddressMap::iterator I = m_AddressMap.find(Address.GetProtocol()); I != m_AddressMap.end() && I->first == Address.GetProtocol(); I++)
	{
		if(I->second.CompareTo(Address))
			return I->second.IsVerifyed();
	}
	return false;
}

void CKadNode::UpdateClass(bool bOk, const CNodeAddress& Address)
{
	if(bOk) // a node that fails once is not used untill the next scheduled ahoy succeeds
		m_iFailed = 0;

	for(AddressMap::iterator I = m_AddressMap.find(Address.GetProtocol()); I != m_AddressMap.end() && I->first == Address.GetProtocol(); I++)
	{
		if(I->second.CompareTo(Address))
		{
			if(bOk)
			{
				m_uLastSeen = GetTime();
				I->second.SetLastSeen(m_uLastSeen);

				if(I->second.GetClass() > NODE_ZERO_CLASS)
				{
					time_t uAge = GetTime() - I->second.GetFirstSeen();
					if(uAge <= GetParent<CKademlia>()->Cfg()->GetInt64("Age2ndClass"))
						I->second.SetClass(NODE_2ND_CLASS);
					else if(uAge <= GetParent<CKademlia>()->Cfg()->GetInt64("Age1stClass"))
						I->second.SetClass(NODE_1ST_CLASS);
					else // best
						I->second.SetClass(NODE_ZERO_CLASS);
				}
			}
			else
			{
				I->second.IncrClass();
				if(I->second.GetClass() >= NODE_OFFLINE_CLASS)
					m_AddressMap.erase(I);
			}
			break;
		}
	}
}

int CKadNode::GetClass() const
{
	int iClass = NODE_OFFLINE_CLASS;
	for(AddressMap::const_iterator I = m_AddressMap.begin(); I != m_AddressMap.end(); I++) // Loop through each protocol separatly
	{
		if(I->second.IsBlocked())
			continue; // this one is not usable

		// K-ToDo-Now:
		//if(!GetParent<CKademlia>()->Fwh()->GetAddress(I->first))
		//	continue; // we dont support this protocol

		if(I->second.GetClass() < iClass)
			iClass = I->second.GetClass();
	}
	return iClass;
}

bool CKadNode::IsNeeded() const
{
	return m_uLastSeen + GetParent<CKademlia>()->Cfg()->GetInt64("RetentionTime") > GetTime();
}

time_t CKadNode::GetTimeToKeep() const
{
	switch(GetClass())
	{
		case NODE_ZERO_CLASS:		return GetParent<CKademlia>()->Cfg()->GetInt64("KeepZeroClass");
		case NODE_1ST_CLASS:		return GetParent<CKademlia>()->Cfg()->GetInt64("Keep1ndClass");
		case NODE_2ND_CLASS:		return GetParent<CKademlia>()->Cfg()->GetInt64("Keep2ndClass");
		case NODE_DEFAULT_CLASS:	return GetParent<CKademlia>()->Cfg()->GetInt64("KeepDefaultClass");
		default:					return 0;
	}
}

bool CKadNode::IsFading(bool bHalf) const
{
	time_t Keep = GetTimeToKeep();
	if(bHalf)
		Keep /= 2;
	if(m_uLastSeen + Keep < GetTime())
		return true;
	return false;
}

uint64 CKadNode::GetPassKey() const
{
	uint64 PassKey = 0;
	CAbstractKey::Fold(m_ID.GetData(), m_ID.GetSize(),(byte*)&PassKey, sizeof(PassKey));
	return PassKey;
}
