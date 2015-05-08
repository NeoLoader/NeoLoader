#ifndef NEOGUI_GLOBAL_H
#define NEOGUI_GLOBAL_H

#include <QtCore/qglobal.h>

#ifdef NEOGUI_LIB
# define NEOGUI_EXPORT Q_DECL_EXPORT
#else
# define NEOGUI_EXPORT Q_DECL_IMPORT
#endif

#endif // NEOGUI_GLOBAL_H
