#pragma once

#include "../Networking/SmartSocket.h"
#include "../Networking/SocketSession.h"

class CNodeAddress: public CSafeAddress
{
public:
	CNodeAddress()							{NodeInit();}
	CNodeAddress(const CNodeAddress& Address, bool bVerifyed = false)
	 : CSafeAddress(Address)				
	{
		NodeInit(Address.IsVerifyed() || bVerifyed); 
		if(CSafeAddress* pAssistent = Address.GetAssistent()) 
			SetAssistent(pAssistent);
	}
	CNodeAddress(const CSafeAddress& Address, bool bVerifyed = false)
	 : CSafeAddress(Address)				{NodeInit(bVerifyed);}
	virtual ~CNodeAddress()					{delete m_pAssistent;}

	virtual bool operator==(const CNodeAddress &Address) const		{return CompareTo(Address);}
	virtual CNodeAddress& operator=(const CNodeAddress& Address)	
	{
		Set(Address); 
		if(Address.IsVerifyed())
			SetVerifyed();
		if(CSafeAddress* pAssistent = Address.GetAssistent()) 
			SetAssistent(pAssistent); 
		return *this;
	}

	virtual void			FromVariant(const CVariant& Variant)
	{
		CSafeAddress::FromVariant(Variant);
		if(Variant.Has("REL"))
		{
			delete m_pAssistent;
			m_pAssistent = new CSafeAddress();
			if(Variant["REL"].IsValid())
				m_pAssistent->FromVariant(Variant["REL"]);
		}	
	}
	virtual CVariant		ToVariant(bool bWithAssistent = true) const
	{
		CVariant Variant = CSafeAddress::ToVariant();
		if(m_pAssistent)
		{
			if(m_pAssistent->IsValid())
				Variant["REL"] = m_pAssistent->ToVariant();
			else
				Variant["REL"] = CVariant();
		}
		return Variant;
	}

	virtual void			FromExtVariant(const CVariant& Variant)
	{
		FromVariant(Variant);
		m_bVerifyed = Variant["CHKD"];
		m_uFirstSeen = Variant["FSEN"];
		m_uLastSeen = Variant["LSEN"];
		m_iClass = Variant["CLAS"];
	}
	virtual CVariant		ToExtVariant(bool bWithAssistent = true) const
	{
		CVariant Variant = ToVariant();
		Variant["CHKD"] = m_bVerifyed;
		Variant["FSEN"] = m_uFirstSeen;
		Variant["LSEN"] = m_uLastSeen;
		Variant["CLAS"] = m_iClass;
		return Variant;
	}

	virtual void			SetVerifyed()					{m_bVerifyed = true;}
	virtual bool			IsVerifyed() const				{return m_bVerifyed;}
	virtual void			SetClass(int iClass)			{m_iClass = iClass;}
	virtual void			IncrClass()						{m_iClass++;}
	virtual int				GetClass() const				{return m_iClass;}

	virtual void			SetLastSeen(time_t uLastSeen)	{m_uLastSeen = uLastSeen;}
	virtual time_t			GetLastSeen() const				{return m_uLastSeen;}
	virtual void			SetFirstSeen(time_t uFirstSeen)	{m_uFirstSeen = uFirstSeen;}
	virtual time_t			GetFirstSeen() const			{return m_uFirstSeen;}

	virtual bool			IsBlocked() const				{return m_Port == 0 || (m_pAssistent && !m_pAssistent->IsValid());}

	virtual CSafeAddress*	GetAssistent() const			{return m_pAssistent;}
	virtual void			SetAssistent(const CSafeAddress* pAssistent)
	{
		delete m_pAssistent;
		if(pAssistent)
		{
			m_pAssistent = new CSafeAddress();
			*m_pAssistent = *pAssistent;
		}
		else 
			m_pAssistent = NULL;
	}

	virtual void				SetAuxPort(uint16 AuxPort)	{m_AuxPort = AuxPort;}
	virtual uint16				GetAuxPort() const			{return m_AuxPort;}

protected:
	virtual void			NodeInit(bool bVerifyed = false)
	{
		m_bVerifyed = bVerifyed;
		m_uFirstSeen = m_uLastSeen = GetTime();
		m_iClass = NODE_DEFAULT_CLASS;

		m_AuxPort = 0;
		m_pAssistent = NULL;
	}

	bool					m_bVerifyed;
	int						m_iClass;
	time_t					m_uLastSeen;
	time_t					m_uFirstSeen;

	uint16					m_AuxPort;
	CSafeAddress*			m_pAssistent;
};