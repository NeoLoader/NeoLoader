#include "GlobalHeader.h"
#include "SafeAddress.h"
#include "../Framework/Strings.h"
#include "../../Framework/Exception.h"

#ifndef WIN32
   #include <unistd.h>
   #include <cstdlib>
   #include <cstring>
   #include <netdb.h>
   #include <arpa/inet.h>
   #include <sys/types.h>
   #include <sys/socket.h>
   #include <netinet/in.h>
   #define SOCKET int
   #define SOCKET_ERROR (-1)
   #define closesocket close
#else
   #include <winsock2.h>
   #include <ws2tcpip.h>
   #include <wspiapi.h>
#endif

CSafeAddress::CSafeAddress()
{
	Init();
}

void CSafeAddress::Init()
{
	m_Protocol = eInvalid;
	m_Port = 0;
	
	m_PassKey = 0;
}

CSafeAddress::CSafeAddress(const CSafeAddress& Address)
{
	Init();
	Set(Address);
}

CSafeAddress::~CSafeAddress()
{
}

void CSafeAddress::FromVariant(const CVariant& Variant)
{
	Init();

	if(Variant.Has("IP4"))
	{
		if(Variant["IP4"].GetSize() != 4)
			throw CException(LOG_ERROR | LOG_DEBUG, L"invalid IPv4 address");

		m_eAF = IPv4;
		if(Variant["IP4"].GetType() == CVariant::EBytes)
			memcpy(m_IP, Variant["IP4"].GetData(), 4);
		else
			*((uint32*)m_IP) = ntohl((uint32)Variant["IP4"]);

		if(Variant.Has("UDT"))
		{
			m_Protocol = eUDT_IP4;
			m_Port = Variant["UDT"];
		}
		else if(Variant.Has("UTP"))
		{
			m_Protocol = eUTP_IP4;
			m_Port = Variant["UTP"];
		}
		else if(Variant.Has("TCP"))
		{
			m_Protocol = eTCP_IP4;
			m_Port = Variant["TCP"];
		}
		else
			throw CException(LOG_ERROR | LOG_DEBUG, L"port is missing");
	}
	else if(Variant.Has("IP6"))
	{
		if(Variant["IP6"].GetSize() != 16)
			throw CException(LOG_ERROR | LOG_DEBUG, L"invalid IPv6 address");

		m_eAF = IPv6;
		memcpy(m_IP, Variant["IP6"].GetData(), 16);

		if(Variant.Has("UDT"))
		{
			m_Protocol = eUDT_IP6;
			m_Port = Variant["UDT"];
		}
		else if(Variant.Has("UTP"))
		{
			m_Protocol = eUTP_IP6;
			m_Port = Variant["UTP"];
		}
		else if(Variant.Has("TCP"))
		{
			m_Protocol = eTCP_IP6;
			m_Port = Variant["TCP"];
		}
		else
			throw CException(LOG_ERROR | LOG_DEBUG, L"port is missing");
	}
	else
		throw CException(LOG_ERROR | LOG_DEBUG, L"invalid Variant is not an Address");

	if(Variant.Has("PSK"))
		m_PassKey = Variant["PSK"];
}

CVariant CSafeAddress::ToVariant() const
{
	CVariant Variant;
	switch(m_Protocol)
	{
		case eUDT_IP4:
			//Variant["IP4"] = CVariant(m_IP, 4);
			Variant["IP4"] = (uint32)ntohl(*((uint32*)m_IP));
			Variant["UDT"] = m_Port;
			break;
		case eUTP_IP4:
			//Variant["IP4"] = CVariant(m_IP, 4);
			Variant["IP4"] = (uint32)ntohl(*((uint32*)m_IP));
			Variant["UTP"] = m_Port;
			break;
		//case eUDP_IP4:
		case eTCP_IP4:
			//Variant["IP4"] = CVariant(m_IP, 4);
			Variant["IP4"] = (uint32)ntohl(*((uint32*)m_IP));
			Variant["TCP"] = m_Port;
			break;

		case eUDT_IP6:
			Variant["IP6"] = CVariant(m_IP, 16);
			Variant["UDT"] = m_Port;
			break;
		case eUTP_IP6:
			Variant["IP6"] = CVariant(m_IP, 16);
			Variant["UTP"] = m_Port;
			break;
		//case eUDP_IP6:
		case eTCP_IP6:
			Variant["IP6"] = CVariant(m_IP, 16);
			Variant["TCP"] = m_Port;
			break;

		default:
			ASSERT(0);
	}

	if(m_PassKey)
		Variant["PSK"] = m_PassKey;
	return Variant;
}

CSafeAddress::CSafeAddress(const sockaddr* sa, int sa_len, EProtocol eProtocol)
{
	Init();

	m_Protocol = eProtocol;
#ifdef _DEBUG
	switch(m_Protocol)
	{
		//case eUDP_IP4:
		case eUDT_IP4:
		case eUTP_IP4:
		case eTCP_IP4:
			ASSERT(sa->sa_family == AF_INET);
			break;
		//case eUDP_IP6:
		case eUDT_IP6:
		case eUTP_IP6:
		case eTCP_IP6:
			ASSERT(sa->sa_family == AF_INET6);
			break;
	}
#endif
	CAddress::FromSA(sa, sa_len, &m_Port);
}

void CSafeAddress::ToSA(sockaddr* sa, int *sa_len) const
{
	CAddress::ToSA(sa, sa_len, m_Port);
}

CSafeAddress::CSafeAddress(const wstring& wstr)
{
	Init();

	string str;
	WStrToUtf8(str, wstr);

	string::size_type pos1 = str.find("://");
	string::size_type pos3 = str.find("/", pos1 + 3);
	string::size_type pos2 = str.rfind(":", pos3);
	if(pos2 < pos1 + 3) 
		pos2 = string::npos;
	if(pos1 == string::npos)
		return;

	string Prot = str.substr(0, pos1);
	string Addr = str.substr(pos1+3, ((pos2 == string::npos) ? pos3 : pos2) - (pos1+3));
	string Port = pos2 == string::npos ? "" : str.substr(pos2+1,pos3 - (pos2+1));

	if(Addr.find("[") == 0 && Addr.size() > 2)
	{
		if(Prot == "udt" || Prot == "UDT")
			m_Protocol = eUDT_IP6;
		else if(Prot == "utp" || Prot == "UTP")
			m_Protocol = eUTP_IP6;
		else if(Prot == "tcp" || Prot == "TCP")
			m_Protocol = eTCP_IP6;
		//else if(Prot == "udp" || Prot == "UDP")
		//	m_Protocol = eUDP_IP6;

		Addr.erase(0,1);
		Addr.erase(Addr.size()-1,1);
	}
	else
	{
		if(Prot == "udt" || Prot == "UDT")
			m_Protocol = eUDT_IP4;
		else if(Prot == "utp" || Prot == "UTP")
			m_Protocol = eUTP_IP4;
		else if(Prot == "tcp" || Prot == "TCP")
			m_Protocol = eTCP_IP4;
		//else if(Prot == "udp" || Prot == "UDP")
		//	m_Protocol = eUDP_IP4;
	}

	CAddress::FromString(Addr.c_str());
	m_Port = atoi(Port.c_str());
}

wstring CSafeAddress::ToString() const
{
	stringstream Str;
	switch(m_Protocol)
	{
		//case eUDP_IP4:
		//case eUDP_IP6:
		//	Str << "udp" << "://";
		//	break;
		case eUDT_IP4:
		case eUDT_IP6:
			Str << "udt" << "://";
			break;
		case eUTP_IP4:
		case eUTP_IP6:
			Str << "utp" << "://";
			break;
		case eTCP_IP4:
		case eTCP_IP6:
			Str << "tcp" << "://";
			break;
		default:
			Str << "invalid" << "://";
	}

	switch(m_eAF)
	{
		case IPv6:
		{
			Str << "[";
			Str << CAddress::ToString().c_str();
			Str << "]";
			Str << ":" << m_Port;
			break;
		}
		case IPv4:
		{
			Str << CAddress::ToString().c_str();
			Str << ":" << m_Port;
			break;
		}
	}
	Str << "/";

	wstring wstr;
	Utf8ToWStr(wstr, Str.str());
	return wstr;
}

void CSafeAddress::Set(const CSafeAddress& Address)
{
	m_Protocol = Address.m_Protocol;
	m_eAF = Address.m_eAF;
	memcpy(m_IP, Address.m_IP, Len());
	m_Port = Address.m_Port;

	SetPassKey(Address.m_PassKey); 
}

int CSafeAddress::Compare(const CSafeAddress &R, bool IgnorePort) const
{
	if(m_Protocol < R.m_Protocol)
		return 1;
	if(m_Protocol > R.m_Protocol)
		return -1;
	if(!IgnorePort)
	{
		if(m_Port < R.m_Port)
			return 1;
		if(m_Port > R.m_Port)
			return -1;
	}
	ASSERT(m_eAF == R.m_eAF);
	return memcmp(m_IP, R.m_IP, Len());
}
