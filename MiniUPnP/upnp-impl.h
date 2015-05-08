#pragma once

#include "states-impl.h"
struct upnp;

upnp* upnpInit();
void upnpClose(upnp*);
int upnpPulse(upnp*, int port, int proto, char isEnabled, int doPortCheck, const char* name);