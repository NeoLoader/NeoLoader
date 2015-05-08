#include "upnp-impl.h"

#include <QDebug>
#include <stdlib.h>
#ifdef WIN32
#define snprintf _snprintf
#endif

extern "C" {
	#include "miniupnp/miniupnpc.h"
	#include "miniupnp/upnpcommands.h"
}

typedef enum
{
    UPNP_IDLE,
    UPNP_ERR,
    UPNP_DISCOVER,
    UPNP_MAP,
    UPNP_UNMAP
}
upnp_state;

struct upnp
{
    char            hasDiscovered;
    struct UPNPUrls    urls;
    struct IGDdatas    data;
    int                port;
    char               lanaddr[16];
    unsigned int       isMapped;
    upnp_state      state;
};

upnp* upnpInit()
{
    upnp* handle = (upnp*)malloc(sizeof(upnp));
	memset(handle, 0, sizeof(upnp));
    handle->state = UPNP_DISCOVER;
    handle->port = -1;
    return handle;
}

void upnpClose(upnp* handle)
{
    Q_ASSERT( !handle->isMapped );
    Q_ASSERT( handle->state == UPNP_IDLE || handle->state == UPNP_ERR || handle->state == UPNP_DISCOVER );
    if(handle->hasDiscovered)
        FreeUPNPUrls(&handle->urls);
    free(handle);
}

enum
{
  UPNP_IGD_NONE = 0,
  UPNP_IGD_VALID_CONNECTED = 1,
  UPNP_IGD_VALID_NOT_CONNECTED = 2,
  UPNP_IGD_INVALID = 3
};

int upnpPulse(upnp * handle, int port, int proto, char isEnabled, int doPortCheck, const char* name)
{
    if(isEnabled && (handle->state == UPNP_DISCOVER))
    {
        struct UPNPDev * devlist;
        devlist = upnpDiscover(200, NULL, NULL, 0);
        if(UPNP_GetValidIGD(devlist, &handle->urls, &handle->data,
                             handle->lanaddr, sizeof(handle->lanaddr )) == UPNP_IGD_VALID_CONNECTED)
        {
			//qDebug() << QString("Found Internet Gateway Device \"%1\"").arg(QString(handle->urls.controlURL));
			//qDebug() << QString("Local Address is \"%1\"").arg(QString(handle->lanaddr));

            handle->state = UPNP_IDLE;
            handle->hasDiscovered = 1;
        }
        else
        {
            handle->state = UPNP_ERR;
			//qDebug() << QString("UPNP_GetValidIGD failed");
        }
        freeUPNPDevlist(devlist);
    }

    if(handle->state == UPNP_IDLE)
    {
        if(handle->isMapped && (!isEnabled || handle->port != port))
            handle->state = UPNP_UNMAP;
    }

	char* protoStr = "";
	switch(proto)
	{
	case PORT_TCP: protoStr = "TCP"; break;
	case PORT_UDP: protoStr = "UDP"; break;
	}

    if(isEnabled && handle->isMapped && doPortCheck)
    {
        char portStr[8];
        char intPort[8];
        char intClient[16];

        snprintf(portStr, sizeof(portStr), "%d", handle->port);
        if(UPNP_GetSpecificPortMappingEntry( handle->urls.controlURL, handle->data.first.servicetype, portStr, protoStr, intClient, intPort ) != UPNPCOMMAND_SUCCESS)
        {
			//qDebug() << QString("Port %1 isn't forwarded").arg(handle->port);
            handle->isMapped = 0;
        }
    }

    if( handle->state == UPNP_UNMAP )
    {
        char portStr[16];
        snprintf(portStr, sizeof(portStr), "%d", handle->port);
        UPNP_DeletePortMapping(handle->urls.controlURL, handle->data.first.servicetype, portStr, protoStr, NULL);

		//qDebug() << QString("Stopping port forwarding through \"%1\", service \"%1\"").arg(QString(handle->urls.controlURL)).arg(QString(handle->data.first.servicetype));

        handle->isMapped = 0;
        handle->state = UPNP_IDLE;
        handle->port = -1;
    }

    if(handle->state == UPNP_IDLE)
    {
        if(isEnabled && !handle->isMapped)
            handle->state = UPNP_MAP;
    }

    if(handle->state == UPNP_MAP)
    {
        int  err = -1;

        if(!handle->urls.controlURL || !handle->data.first.servicetype)
            handle->isMapped = 0;
        else
        {
            char portStr[16];
            char desc[64];
            snprintf(portStr, sizeof(portStr), "%d", port);
            snprintf(desc, sizeof(desc), "%s at %d", name, port);

            err = UPNP_AddPortMapping(handle->urls.controlURL, handle->data.first.servicetype, portStr, portStr, handle->lanaddr, desc, protoStr, NULL);
			//if(err)
			//	qDebug() << QString("%1 Port forwarding failed with error %2").arg(QString(protoStr)).arg(err);
            
            handle->isMapped = !err;
        }
		//qDebug() << QString("Port forwarding through \"%1\", service \"%2\". (local address: %3:%4)")
		//		.arg(QString(handle->urls.controlURL)).arg(QString(handle->data.first.servicetype)).arg(QString(handle->lanaddr)).arg(port);

        if(handle->isMapped)
        {
			//qDebug() << QString("Port forwarding successful!");
            handle->port = port;
            handle->state = UPNP_IDLE;
        }
        else
        {
			//qDebug() << QString("If your router supports UPnP, please make sure UPnP is enabled!");
            handle->port = -1;
            handle->state = UPNP_ERR;
        }
    }

    switch( handle->state )
    {
    case UPNP_DISCOVER:	return PORT_UNMAPPED;
	case UPNP_MAP:		return PORT_MAPPING;
	case UPNP_UNMAP:	return PORT_UNMAPPING;
	case UPNP_IDLE:		return handle->isMapped ? PORT_MAPPED : PORT_UNMAPPED;
	default:			return PORT_ERROR;
    }
}