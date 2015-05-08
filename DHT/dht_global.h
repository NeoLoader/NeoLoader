#ifndef DHT_GLOBAL_H
#define DHT_GLOBAL_H

#include <QtCore/qglobal.h>

#ifdef DHT_LIB
# define DHT_EXPORT Q_DECL_EXPORT
#else
# define DHT_EXPORT Q_DECL_IMPORT
#endif

#endif // DHT_GLOBAL_H
