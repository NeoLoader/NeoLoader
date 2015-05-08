#ifndef QTPINGER_WIN_H
#define QTPINGER_WIN_H

#include "qtpinger.h"
#include <QThread>
#include <QString>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <wspiapi.h>

#include <windows.h>

/* Note 2: For the most part, you can refer to RFC 791 for detials 
 * on how to fill in values for the IP option information structure. 
 */
typedef struct ip_option_information {
    u_char Ttl;		/* Time To Live (used for traceroute) */
    u_char Tos; 	/* Type Of Service (usually 0) */
    u_char Flags; 	/* IP header flags (usually 0) */
    u_char OptionsSize; /* Size of options data (usually 0, max 40) */
	//u_char FAR *OptionsData;   /* Options data buffer */
#if defined(_WIN64)
	UCHAR * POINTER_32 OptionsData;
#else
	PUCHAR  OptionsData;        // Pointer to options data
#endif // _WIN64
} IPINFO, *PIPINFO, FAR *LPIPINFO;

/* Note 1: The Reply Buffer will have an array of ICMP_ECHO_REPLY
 * structures, followed by options and the data in ICMP echo reply
 * datagram received. You must have room for at least one ICMP
 * echo reply structure, plus 8 bytes for an ICMP header. 
 */
typedef struct icmp_echo_reply {
    u_long Address; 	/* source address */
    u_long Status;	/* IP status value (see below) */
    u_long RTTime;	/* Round Trip Time in milliseconds */
    u_short DataSize; 	/* reply data size */
    u_short Reserved; 	/* */
	//void FAR *Data; 	/* reply data buffer */
#if defined(_WIN64)
    VOID * POINTER_32 Data;
#else
	PVOID   Data;               // Pointer to the reply data
#endif
    struct ip_option_information Options; /* reply options */
} ICMPECHO, *PICMPECHO, FAR *LPICMPECHO;

#pragma pack(1)
typedef struct _IPV6_ADDRESS_EX {
    USHORT sin6_port;
    ULONG  sin6_flowinfo;
    USHORT sin6_addr[8];
    ULONG  sin6_scope_id;
} IPV6_ADDRESS_EX, *PIPV6_ADDRESS_EX;
#pragma pack()

typedef struct icmpv6_echo_reply_lh {
    IPV6_ADDRESS_EX Address;    // Replying address.
    u_long Status;               // Reply IP_STATUS.
    u_long RTTime; // RTT in milliseconds.
    // Reply data follows this structure in memory.
} ICMPV6_ECHO_LH, *PICMPV6_ECHO_LH;

typedef HANDLE WINAPI IcmpCreateFile(VOID); /* INVALID_HANDLE_VALUE on error */
typedef BOOL WINAPI IcmpCloseHandle(HANDLE IcmpHandle); /* FALSE on error */

typedef DWORD WINAPI IcmpSendEcho(
    HANDLE IcmpHandle, 	/* handle returned from IcmpCreateFile() */

    u_long DestAddress, /* destination IP address (in network order) */

    LPVOID RequestData, /* pointer to buffer to send */
    WORD RequestSize,	/* length of data in buffer */
    LPIPINFO RequestOptns,  /* see Note 2 */
    LPVOID ReplyBuffer, /* see Note 1 */
    DWORD ReplySize, 	/* length of reply (must allow at least 1 reply) */
    DWORD Timeout 	/* time in milliseconds to wait for reply */
);

// http://msdn.microsoft.com/en-us/library/windows/desktop/aa366041%28v=vs.85%29.aspx
typedef DWORD Icmp6SendEcho2(
    HANDLE IcmpHandle,

    HANDLE Event,
    FARPROC ApcRoutine,
    PVOID ApcContext,

    struct sockaddr_in6 *SourceAddress,
    struct sockaddr_in6 *DestinationAddress,

    LPVOID RequestData,
    WORD RequestSize,
    LPIPINFO RequestOptions,
    LPVOID ReplyBuffer,
    DWORD ReplySize,
    DWORD Timeout
);

/* IP Flags - 3 bits
 *
 *  bit 0: reserved
 *  bit 1: 0=May Fragment, 1=Don't Fragment
 *  bit 2: 0=Last Fragment, 1=More Fragments
 */
#define IPFLAG_DONT_FRAGMENT 0x02

#define BUFSIZE     8192

QString PIcmpErr(int);

class QtPingerWin: public QThread, public QtPinger
{
	Q_OBJECT

public:
	QtPingerWin(QObject* parent);
	~QtPingerWin();

	virtual QtPingStatus Ping(const QHostAddress& address, int nTTL, int nTimeOut = 3000);

	virtual bool	Start(const QHostAddress& address, int nTTL = 64, int nInterval = 1000);
	virtual bool	IsPinging()		{return bRunning;}
	virtual void	Stop();

	virtual void	run();

signals:
	void			PingResult(QtPingStatus Status);

protected:
	bool bRunning;

	QHostAddress address;
	int nTTL;
	int nInterval;

	int m_RetWSAStartup;
	IcmpCreateFile* lpfnIcmpCreateFile;
	IcmpSendEcho* lpfnIcmpSendEcho;
	IcmpCloseHandle* lpfnIcmpCloseHandle;
    HANDLE hICMP;
	HMODULE hICMP_DLL; 

	IcmpCreateFile* lpfnIcmp6CreateFile;
	Icmp6SendEcho2* lpfnIcmp6SendEcho2;
	IcmpCloseHandle* lpfnIcmp6CloseHandle;
	HANDLE hICMP6;
	HMODULE hIPHLPAPI_DLL; 
};

#endif // QTPINGER_WIN_H