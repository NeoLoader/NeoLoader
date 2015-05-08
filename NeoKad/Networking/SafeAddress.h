#pragma once

#include "../Common/Variant.h"
#include "../../Framework/Address.h"

class CSafeAddress: private CAddress
{
public:

	enum EProtocol 
	{
		eInvalid = 0,
		eUDT_IP4,
		eUDT_IP6,
		eUTP_IP4,
		eUTP_IP6,
		eTCP_IP4,
		eTCP_IP6,
		//eUDP_IP4,
		//eUDP_IP6,
	};

	CSafeAddress();
	CSafeAddress(const CSafeAddress& Address);
	virtual ~CSafeAddress();

	virtual void FromVariant(const CVariant& Variant);
	virtual CVariant ToVariant() const;

	CSafeAddress(const wstring& wstr);
	wstring						ToString() const;

	CSafeAddress(const sockaddr* sa, int sa_len, EProtocol eProtocol);
	void ToSA(sockaddr* sa, int *sa_len) const;

	virtual bool				IsValid()	const			{return GetProtocol() != eInvalid && !IsNull();}

	virtual void				Set(const CSafeAddress& Address);
	virtual CSafeAddress& operator=(const CSafeAddress& Address)	{Set(Address); return *this;}
	virtual int					Compare(const CSafeAddress &R, bool IgnorePort = false) const;
	virtual bool CompareTo(const CSafeAddress &Address) const	{return Compare(Address) == 0;}
	virtual bool operator==(const CSafeAddress &Address) const	{return Compare(Address) == 0;}
	virtual bool operator!=(const CSafeAddress &Address) const	{return Compare(Address) != 0;}

	virtual void				SetPort(uint16 Port)		{m_Port = Port;}
	virtual uint16				GetPort() const				{return m_Port;}

	virtual EProtocol			GetProtocol() const			{return m_Protocol;}

	virtual uint64				GetPassKey() const			{return m_PassKey;}
	virtual void				SetPassKey(uint64 PassKey)	{m_PassKey = PassKey;}

protected:
	virtual void				Init();

	EProtocol					m_Protocol;
	uint16						m_Port;

	uint64						m_PassKey;
};

__inline bool operator<(const CSafeAddress &L, const CSafeAddress &R) {return L.Compare(R) > 0;}

