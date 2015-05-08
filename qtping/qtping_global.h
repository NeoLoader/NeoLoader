#ifndef QTPING_GLOBAL_H
#define QTPING_GLOBAL_H

#include <QtCore/qglobal.h>

#ifdef QTPING_LIB
# define QTPING_EXPORT Q_DECL_EXPORT
#else
# define QTPING_EXPORT Q_DECL_IMPORT
#endif

#endif // QTPING_GLOBAL_H
