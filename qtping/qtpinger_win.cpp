#include "qtpinger_win.h"

#define IP_STATUS_BASE 11000
#define IP_SUCCESS 0
#define IP_BUF_TOO_SMALL (IP_STATUS_BASE + 1)
#define IP_DEST_NET_UNREACHABLE (IP_STATUS_BASE + 2)
#define IP_DEST_HOST_UNREACHABLE (IP_STATUS_BASE + 3)
#define IP_DEST_PROT_UNREACHABLE (IP_STATUS_BASE + 4)
#define IP_DEST_PORT_UNREACHABLE (IP_STATUS_BASE + 5)
#define IP_NO_RESOURCES (IP_STATUS_BASE + 6)
#define IP_BAD_OPTION (IP_STATUS_BASE + 7)
#define IP_HW_ERROR (IP_STATUS_BASE + 8)
#define IP_PACKET_TOO_BIG (IP_STATUS_BASE + 9)
#define IP_REQ_TIMED_OUT (IP_STATUS_BASE + 10)
#define IP_BAD_REQ (IP_STATUS_BASE + 11)
#define IP_BAD_ROUTE (IP_STATUS_BASE + 12)
#define IP_TTL_EXPIRED_TRANSIT (IP_STATUS_BASE + 13)
#define IP_TTL_EXPIRED_REASSEM (IP_STATUS_BASE + 14)
#define IP_PARAM_PROBLEM (IP_STATUS_BASE + 15)
#define IP_SOURCE_QUENCH (IP_STATUS_BASE + 16)
#define IP_OPTION_TOO_BIG (IP_STATUS_BASE + 17)
#define IP_BAD_DESTINATION (IP_STATUS_BASE + 18)
#define IP_ADDR_DELETED (IP_STATUS_BASE + 19)
#define IP_SPEC_MTU_CHANGE (IP_STATUS_BASE + 20)
#define IP_MTU_CHANGE (IP_STATUS_BASE + 21)
#define IP_UNLOAD (IP_STATUS_BASE + 22)
#define IP_GENERAL_FAILURE (IP_STATUS_BASE + 50)
#define MAX_IP_STATUS IP_GENERAL_FAILURE
#define IP_PENDING (IP_STATUS_BASE + 255)

#if defined(WIN32)
QtPinger* newQtPinger(QObject* parent)
{
	return new QtPingerWin(parent);
}
#endif

QtPingerWin::QtPingerWin(QObject* parent) 
: QThread(parent)
, lpfnIcmpCreateFile(0), lpfnIcmp6CreateFile(0), lpfnIcmpSendEcho(0), lpfnIcmp6SendEcho2(0), lpfnIcmpCloseHandle(0), hICMP(INVALID_HANDLE_VALUE), hICMP6(INVALID_HANDLE_VALUE), hICMP_DLL(0)
{
	bRunning = false;

	nTTL = 0;
	nInterval = 0;

	if(parent)
		connect(this, SIGNAL(PingResult(QtPingStatus)), parent, SIGNAL(PingResult(QtPingStatus)));

	WSADATA wsaData;
	m_RetWSAStartup = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if(m_RetWSAStartup != 0)
	{
		qDebug() << QString("WSAStartup() failed, err: %1").arg(m_RetWSAStartup);
		return;
	}

	// IPv4
	hICMP_DLL = LoadLibrary(L"ICMP.DLL");
	if (hICMP_DLL == 0) 
	{
		qDebug() << QString("LoadLibrary() failed: Unable to locate ICMP.DLL!");
		return;
	}

	lpfnIcmpCreateFile  = (IcmpCreateFile*)GetProcAddress(hICMP_DLL,"IcmpCreateFile");
	lpfnIcmpSendEcho    = (IcmpSendEcho*)GetProcAddress(hICMP_DLL,"IcmpSendEcho");
	lpfnIcmpCloseHandle = (IcmpCloseHandle*)GetProcAddress(hICMP_DLL,"IcmpCloseHandle");

	if ((!lpfnIcmpCreateFile) || (!lpfnIcmpSendEcho) || (!lpfnIcmpCloseHandle)) 
	{
		qDebug() << QString("GetProcAddr() failed for at least one function.");
		return;
	}

	hICMP = (HANDLE) lpfnIcmpCreateFile();
	if (hICMP == INVALID_HANDLE_VALUE) 
	{
		qDebug() << QString("IcmpCreateFile() failed, err: %1").arg(PIcmpErr(GetLastError()));
		return;
	}
	//

	// IPv6
	hIPHLPAPI_DLL = LoadLibrary(L"IPHLPAPI.DLL");
	if (hIPHLPAPI_DLL == 0) 
	{
		qDebug() << QString("LoadLibrary() failed: Unable to locate IPHLPAPI.DLL!");
		return;
	}

	lpfnIcmp6CreateFile = (IcmpCreateFile*)GetProcAddress(hIPHLPAPI_DLL,"Icmp6CreateFile");
	lpfnIcmp6SendEcho2  = (Icmp6SendEcho2*)GetProcAddress(hIPHLPAPI_DLL,"Icmp6SendEcho2"); 
	lpfnIcmp6CloseHandle = (IcmpCloseHandle*)GetProcAddress(hIPHLPAPI_DLL,"IcmpCloseHandle");

	if ((!lpfnIcmp6CreateFile) || (!lpfnIcmp6SendEcho2) || (!lpfnIcmp6CloseHandle)) 
	{
		qDebug() << QString("GetProcAddr() failed for at least one function.");
		return;
	}

	hICMP6 = (HANDLE) lpfnIcmp6CreateFile();
	if (hICMP6 == INVALID_HANDLE_VALUE) 
	{
		qDebug() << QString("Icmp6CreateFile() failed, err: %1").arg(PIcmpErr(GetLastError()));
		return;
	}
	//
}

QtPingerWin::~QtPingerWin()
{
	if(bRunning)
		Stop();

	// IPv4
	if(hICMP)
		lpfnIcmpCloseHandle(hICMP);
	if(hICMP_DLL)
		FreeLibrary(hICMP_DLL);
	//

	// IPv6
	if(hICMP6)
		lpfnIcmp6CloseHandle(hICMP6);
	if(hIPHLPAPI_DLL)
		FreeLibrary(hIPHLPAPI_DLL);
	//

	if(m_RetWSAStartup != 0)
		WSACleanup();
}

QtPingStatus QtPingerWin::Ping(const QHostAddress& address, int nTTL, int nTimeOut)
{
	QtPingStatus returnValue;

	//char achReqData[BUFSIZE];
	//for (int j=0, i=32; j<nDataLen; j++, i++) 
	//{
	//	if (i >= 126) 
	//		i = 32;
	//	achReqData[j] = i;
	//}
	//int test = max(sizeof(ICMPV6_ECHO_LH),sizeof(ICMPECHO)); // 36
	char achRepData[BUFSIZE + 128];

	IPINFO stIPInfo, *lpstIPInfo;
	lpstIPInfo = &stIPInfo;
    stIPInfo.Ttl      = nTTL;
    /* Set IP Type of Service field value: 
     *  bits
     * 0-2: Precedence bits = 000
     *   3: Delay bit       = 0
     *   4: Throughput bit  = 1 (to maximize throughput)
     *   5: Reliability bit = 0
     *   6: Cost bit        = 0
     *   7: <reserved>      = 0
     *
     * See RFCs 1340, and 1349 for appropriate settings
     */
    stIPInfo.Tos      = 0; //nTOS;
    stIPInfo.Flags    = 0; //fDontFrag ? IPFLAG_DONT_FRAGMENT : 0;
	stIPInfo.OptionsSize = 0;
	stIPInfo.OptionsData = NULL;

	//DWORD dwReplyCount = lpfnIcmpSendEcho(hICMP, stDestAddr.s_addr, achReqData, nDataLen, lpstIPInfo, achRepData, sizeof(achRepData), nTimeOut);
	DWORD dwReplyCount = -1;
	if(address.protocol() == QAbstractSocket::IPv4Protocol && hICMP != INVALID_HANDLE_VALUE)
		dwReplyCount = lpfnIcmpSendEcho(hICMP, ntohl(address.toIPv4Address()), 0, 0, lpstIPInfo, achRepData, sizeof(achRepData), nTimeOut);
	else if(address.protocol() == QAbstractSocket::IPv6Protocol && hICMP6 != INVALID_HANDLE_VALUE)
	{
		struct sockaddr_in6 s6;
		memset(&s6, 0, sizeof(s6));
		s6.sin6_family = AF_INET6;
		s6.sin6_port = 0;
		s6.sin6_addr = in6addr_any;

		struct sockaddr_in6 a6;
		memset(&a6, 0, sizeof(a6));
		a6.sin6_family = AF_INET6;
		a6.sin6_port = 0;
		Q_IPV6ADDR tmp = address.toIPv6Address();
		memcpy(&a6.sin6_addr.s6_addr, &tmp, sizeof(tmp));

		dwReplyCount = lpfnIcmp6SendEcho2(hICMP6, NULL, NULL, NULL, &s6, &a6, 0, 0, lpstIPInfo, achRepData, sizeof(achRepData), nTimeOut);
	}
	else
		return returnValue;

	if (dwReplyCount != 0) 
	{
		quint32 status = 0;
		if(address.protocol() == QAbstractSocket::IPv4Protocol)
		{
			ICMPECHO* IcmpReply = (ICMPECHO*)achRepData;

			status = IcmpReply->Status;
			returnValue.delay = IcmpReply->RTTime;
			returnValue.address = QHostAddress(ntohl(IcmpReply->Address));
			returnValue.ttl = (status != IP_SUCCESS) ? nTTL : IcmpReply->Options.Ttl;
		}
		else if(address.protocol() == QAbstractSocket::IPv6Protocol)
		{
			ICMPV6_ECHO_LH* IcmpReply = (ICMPV6_ECHO_LH*)achRepData;

			status = IcmpReply->Status;
			returnValue.delay = IcmpReply->RTTime;
			returnValue.address = QHostAddress((quint8*)IcmpReply->Address.sin6_addr);
			returnValue.ttl = nTTL;
		}

		if (status == IP_SUCCESS) 
			returnValue.success = true;
		else
		{
			returnValue.success = false;
			returnValue.error = PIcmpErr(status);
		}
	} 
	else 
	{
		returnValue.success = false;
		returnValue.error = PIcmpErr(GetLastError());
	}

	return returnValue;
}

/*---------------------------------------------------------
 * IcmpSendEcho() Error Strings
 * 
 * The values in the status word returned in the ICMP Echo 
 *  Reply buffer after calling IcmpSendEcho() all have a
 *  base value of 11000 (IP_STATUS_BASE).  At times,
 *  when IcmpSendEcho() fails outright, GetLastError() will 
 *  subsequently return these error values also.
 *
 * Two Errors value defined in ms_icmp.h are missing from 
 *  this string table (just to simplify use of the table):
 *    "IP_GENERAL_FAILURE (11050)"
 *    "IP_PENDING (11255)"
 */
#define MAX_ICMP_ERR_STRING  IP_STATUS_BASE + 22
char *aszSendEchoErr[] = {
    "IP_STATUS_BASE (11000)",
    "IP_BUF_TOO_SMALL (11001)",
    "IP_DEST_NET_UNREACHABLE (11002)",
    "IP_DEST_HOST_UNREACHABLE (11003)",
    "IP_DEST_PROT_UNREACHABLE (11004)",
    "IP_DEST_PORT_UNREACHABLE (11005)",
    "IP_NO_RESOURCES (11006)",
    "IP_BAD_OPTION (11007)",
    "IP_HW_ERROR (11008)",
    "IP_PACKET_TOO_BIG (11009)",
    "IP_REQ_TIMED_OUT (11010)",
    "IP_BAD_REQ (11011)",
    "IP_BAD_ROUTE (11012)",
    "IP_TTL_EXPIRED_TRANSIT (11013)",
    "IP_TTL_EXPIRED_REASSEM (11014)",
    "IP_PARAM_PROBLEM (11015)",
    "IP_SOURCE_QUENCH (11016)",
    "IP_OPTION_TOO_BIG (11017)",
    "IP_BAD_DESTINATION (11018)",
    "IP_ADDR_DELETED (11019)",
    "IP_SPEC_MTU_CHANGE (11020)",
    "IP_MTU_CHANGE (11021)",
    "IP_UNLOAD (11022)"
};

QString PIcmpErr(int nICMPErr)
{
    int  nErrIndex = nICMPErr - IP_STATUS_BASE;
    if ((nICMPErr > MAX_ICMP_ERR_STRING) || (nICMPErr < IP_STATUS_BASE+1)) 
	{
		// Error value is out of range, display normally
		LPVOID lpMsgBuf;
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, nICMPErr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &lpMsgBuf, 0, NULL);
		if(lpMsgBuf)
		{
			QString strError = QString("(%1) &2").arg(nICMPErr).arg((char*)lpMsgBuf);
			LocalFree (lpMsgBuf); // Free the buffer
			return strError;
		}
		return QString("(%1)").arg(nICMPErr);
    }
	else // Display ICMP Error String
		return aszSendEchoErr[nErrIndex];
}


//////////////////////////////////////////////////////////////////////////////////////////////////
//

bool QtPingerWin::Start(const QHostAddress& address, int nTTL, int nInterval)
{
	if(bRunning)
	{
		Q_ASSERT(0);
		return false;
	}

	this->address = address;
	this->nTTL = nTTL;
	this->nInterval = nInterval;

	bRunning = true;
	start();
	return true;
}

int _QtPingStatus_Type = qRegisterMetaType<QtPingStatus>("QtPingStatus");

void QtPingerWin::run()
{
	while(bRunning)
	{
		QtPingStatus retValue = Ping(this->address, this->nTTL);
		emit PingResult(retValue);

		for(int i=0; i < nInterval && bRunning; i+= 10)
			msleep(10);
	}
}

void QtPingerWin::Stop()
{
	bRunning = false;
	wait();
}
