#ifndef QTFTP_GLOBAL_H
#define QTFTP_GLOBAL_H

#include <QtCore/qglobal.h>

#ifdef QTFTP_LIB
# define QTFTP_EXPORT Q_DECL_EXPORT
#else
# define QTFTP_EXPORT Q_DECL_IMPORT
#endif

#endif // QTFTP_GLOBAL_H
