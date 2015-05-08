#pragma once

#include "./NeoHelper/neohelper_global.h"

class NEOHELPER_EXPORT CAddress
{
public: 
	enum EAF
	{
		None = 0,
		IPv4,
		IPv6
	};

	CAddress(EAF eAF = None);
#ifdef USING_QT
	CAddress(const QString& Src);
#endif
	CAddress(const byte* IP);
	CAddress(uint32 IP); // must be in host order, same as with Qt

	bool			FromString(const std::string& Str);
	std::string		ToString() const;
#ifdef USING_QT
	EAF				FromArray(const QString& Array);
	QString			ToQString(bool bForUrl = false) const;
#endif
	uint32			ToIPv4() const; // must be in host order, same as with Qt

	bool			IsNull() const;

	bool			Convert(EAF eAF);
	bool			IsMappedIPv4() const;

	void			FromSA(const sockaddr* sa, int sa_len, uint16* pPort = NULL) ;
	void			ToSA(sockaddr* sa, int *sa_len, uint16 uPort = 0) const;

	bool operator < (const CAddress &Other) const	{if(m_eAF != Other.m_eAF) return m_eAF < Other.m_eAF; 
														return memcmp(m_IP, Other.m_IP, Len()) < 0;}
	bool operator == (const CAddress &Other) const	{return (m_eAF == Other.m_eAF) && memcmp(m_IP, Other.m_IP, Len()) == 0;}
	bool operator != (const CAddress &Other) const	{return !(*this == Other);}

	size_t			Len() const;
	int				AF() const;
	__inline EAF	Type() const					{return m_eAF;}
	__inline const unsigned char*Data() const		{return m_IP;}
	__inline size_t	Size() const					{switch(m_eAF){case IPv4: return 4; case IPv6: return 16; default: return 0; }}

protected:
	byte			m_IP[16];
	EAF				m_eAF;
};

NEOHELPER_EXPORT bool IsLanIPv4(uint32 IPv4);

NEOHELPER_EXPORT char* _inet_ntop(int af, const void *src, char *dst, int size);
NEOHELPER_EXPORT int _inet_pton(int af, const char *src, void *dst);

__inline uint32 _ntohl(uint32 IP)
{
	uint32 PI;
	((byte*)&PI)[0] = ((byte*)&IP)[3];
	((byte*)&PI)[1] = ((byte*)&IP)[2];
	((byte*)&PI)[2] = ((byte*)&IP)[1];
	((byte*)&PI)[3] = ((byte*)&IP)[0];
	return PI;
}

__inline uint16 _ntohs(uint16 PT)
{
	uint16 TP;
	((byte*)&TP)[0] = ((byte*)&PT)[1];
	((byte*)&TP)[1] = ((byte*)&PT)[0];
	return TP;
}