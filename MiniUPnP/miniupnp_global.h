#ifndef MINIUPNP_GLOBAL_H
#define MINIUPNP_GLOBAL_H

#include <QtCore/qglobal.h>

#ifdef MINIUPNP_LIB
# define MINIUPNP_EXPORT Q_DECL_EXPORT
#else
# define MINIUPNP_EXPORT Q_DECL_IMPORT
#endif

#endif // MINIUPNP_GLOBAL_H
