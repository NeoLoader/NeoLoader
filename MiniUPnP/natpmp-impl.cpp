#include "natpmp-impl.h"

#include <QDebug>
#include <stdlib.h>
#ifdef WIN32
#define snprintf _snprintf
#else
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#define closesocket close
#endif

#define ENABLE_STRNATPMPERR
extern "C" {
	#include "libnatpmp/natpmp.h"
}

#define LIFETIME_SECS 3600
#define COMMAND_WAIT_SECS 8

typedef enum
{
    NATPMP_IDLE,
    NATPMP_ERR,
    NATPMP_DISCOVER,
    NATPMP_RECV_PUB,
    NATPMP_SEND_MAP,
    NATPMP_RECV_MAP,
    NATPMP_SEND_UNMAP,
    NATPMP_RECV_UNMAP
}
natpmp_state;

struct natpmp
{
    char           has_discovered;
    char           is_mapped;

    int           public_port;
    int           private_port;

    time_t            renew_time;
    time_t            command_time;
    natpmp_state   state;
    natpmp_t       nat_pmp;
};

void logVal(const char * func, int ret)
{
    if( ret == NATPMP_TRYAGAIN )
        return;

    //if( ret >= 0 )
	//	qDebug() << QString("%1 succeeded (%2)").arg(QString(func)).arg(ret);
    //else
	//	qDebug() << QString("%1 failed. Natpmp returned %2").arg(QString(func)).arg(ret);
}

natpmp* natpmpInit()
{
    natpmp* nat = (natpmp*)malloc(sizeof(natpmp));
	memset(nat, 0, sizeof(natpmp));
    nat->state = NATPMP_DISCOVER;
    nat->public_port = 0;
    nat->private_port = 0;
    nat->nat_pmp.s = -1; /* socket */
    return nat;
}

void natpmpClose(natpmp* nat)
{
	if(nat->nat_pmp.s >= 0)
		closesocket(nat->nat_pmp.s);
	free(nat);
}

int canSendCommand( const struct natpmp * nat )
{
    return time(NULL) >= nat->command_time;
}

void setCommandTime( struct natpmp * nat )
{
    nat->command_time = time(NULL) + COMMAND_WAIT_SECS;
}

int natpmpPulse( struct natpmp * nat, int private_port, int proto, char isEnabled, int* public_port )
{
    if(isEnabled && (nat->state == NATPMP_DISCOVER))
    {
        int val = initnatpmp(&nat->nat_pmp);
        logVal("initnatpmp", val);
        val = sendpublicaddressrequest(&nat->nat_pmp);
        logVal("sendpublicaddressrequest", val);
        nat->state = val < 0 ? NATPMP_ERR : NATPMP_RECV_PUB;
        nat->has_discovered = 1;
        setCommandTime(nat);
    }

    if(nat->state == NATPMP_RECV_PUB && canSendCommand(nat))
    {
        natpmpresp_t response;
        const int val = readnatpmpresponseorretry(&nat->nat_pmp, &response);
        logVal("readnatpmpresponseorretry", val);
        if(val >= 0)
        {
			//qDebug() << QString("Found public address \"%1\"").arg(QString(inet_ntoa(response.pnu.publicaddress.addr)));
            nat->state = NATPMP_IDLE;
        }
        else if(val != NATPMP_TRYAGAIN)
            nat->state = NATPMP_ERR;
    }

    if((nat->state == NATPMP_IDLE) || (nat->state == NATPMP_ERR))
    {
        if(nat->is_mapped && (!isEnabled || nat->private_port != private_port))
            nat->state = NATPMP_SEND_UNMAP;
    }

    if(nat->state == NATPMP_SEND_UNMAP && canSendCommand(nat))
    {
        const int val = sendnewportmappingrequest(&nat->nat_pmp, proto, nat->private_port, nat->public_port, 0);
        logVal("sendnewportmappingrequest", val);
        nat->state = val < 0 ? NATPMP_ERR : NATPMP_RECV_UNMAP;
        setCommandTime(nat);
    }

    if(nat->state == NATPMP_RECV_UNMAP)
    {
        natpmpresp_t resp;
        const int val = readnatpmpresponseorretry(&nat->nat_pmp, &resp);
        logVal("readnatpmpresponseorretry", val);
        if(val >= 0)
        {
            const int private_port = resp.pnu.newportmapping.privateport;

			//qDebug() << QString("no longer forwarding port %1").arg(private_port);

            if(nat->private_port == private_port)
            {
                nat->private_port = 0;
                nat->public_port = 0;
                nat->state = NATPMP_IDLE;
                nat->is_mapped = 0;
            }
        }
        else if(val != NATPMP_TRYAGAIN)
            nat->state = NATPMP_ERR;
    }

    if(nat->state == NATPMP_IDLE)
    {
        if(isEnabled && !nat->is_mapped && nat->has_discovered)
            nat->state = NATPMP_SEND_MAP;

        else if(nat->is_mapped && time(NULL) >= nat->renew_time)
            nat->state = NATPMP_SEND_MAP;
    }

    if(nat->state == NATPMP_SEND_MAP && canSendCommand(nat))
    {
        const int val = sendnewportmappingrequest(&nat->nat_pmp, proto, private_port, private_port, LIFETIME_SECS);
        logVal("sendnewportmappingrequest", val);
        nat->state = val < 0 ? NATPMP_ERR : NATPMP_RECV_MAP;
        setCommandTime(nat);
    }

    if( nat->state == NATPMP_RECV_MAP )
    {
        natpmpresp_t resp;
        const int    val = readnatpmpresponseorretry(&nat->nat_pmp, &resp);
        logVal("readnatpmpresponseorretry", val);
        if(val >= 0)
        {
            nat->state = NATPMP_IDLE;
            nat->is_mapped = 1;
            nat->renew_time = time(NULL) + (resp.pnu.newportmapping.lifetime / 2);
            nat->private_port = resp.pnu.newportmapping.privateport;
            nat->public_port = resp.pnu.newportmapping.mappedpublicport;
			//qDebug() << QString("Port %1 forwarded successfully").arg(nat->private_port);
        }
        else if(val != NATPMP_TRYAGAIN)
            nat->state = NATPMP_ERR;
    }

    switch(nat->state)
    {
	case NATPMP_IDLE: if(public_port) *public_port = nat->public_port;
							return nat->is_mapped ? PORT_MAPPED : PORT_UNMAPPED;
	case NATPMP_DISCOVER:	return PORT_UNMAPPED;
	case NATPMP_RECV_PUB:
	case NATPMP_SEND_MAP:
	case NATPMP_RECV_MAP:	return PORT_MAPPING;
	case NATPMP_SEND_UNMAP:
	case NATPMP_RECV_UNMAP:	return PORT_UNMAPPING;
    default:				return PORT_ERROR;
    }
}

