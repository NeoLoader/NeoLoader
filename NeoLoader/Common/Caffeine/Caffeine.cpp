#include "GlobalHeader.h"
#ifdef WIN32
#include "Caffeine_win.cpp"
#elif __APPLE__
#include "Caffeine_mac.cpp"
#else
#include "Caffeine_linux.cpp"
#endif
