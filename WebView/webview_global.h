#ifndef WEBVIEW_GLOBAL_H
#define WEBVIEW_GLOBAL_H

#include <QtCore/qglobal.h>

#ifdef WEBVIEW_LIB
# define WEBVIEW_EXPORT Q_DECL_EXPORT
#else
# define WEBVIEW_EXPORT Q_DECL_IMPORT
#endif

#endif // WEBVIEW_GLOBAL_H
