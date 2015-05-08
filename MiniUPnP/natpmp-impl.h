#pragma once

#include "states-impl.h"
struct natpmp;

natpmp * natpmpInit();
void natpmpClose(natpmp *);
int natpmpPulse(natpmp *, int port, int proto, char isEnabled, int* public_port = 0);