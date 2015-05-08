#pragma once

#define _CRT_SECURE_NO_WARNINGS

// std includes
#include <string>
#include <sstream>
#include <deque>
#include <list>
#include <vector>
#include <map>
#include <set>

using namespace std;

// Qt includes
#include <QtCore>
#if QT_VERSION < 0x050000
#include <QtGui>
#else
#include <QWidget>
#include <QLabel>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QAction>
#include <QFormLayout>
#include <QTextEdit>
#include <QTreeWidget>
#include <QTabWidget>
#include <QGroupBox>
#include <QToolBar>
#include <QSplitter>
#include <QStackedWidget>
#include <QComboBox>
#include <QTableWidget>
#include <QSpinBox>
#include <QStackedLayout>
#include <QDialog>
#include <QLineEdit>
#include <QMainWindow>
#include <QDesktopServices>
#include <QToolButton>
#include <QApplication>
#include <QMenu>
#include <QInputDialog>
#include <QHeaderView>
#include <QVariant>
#include <QDesktopWidget>
#include <QStyleFactory>
#include <QMessageBox>
#include <QTextBrowser>
#include <QAbstractItemModel>
#include <QScrollArea>
#include <QScrollBar>
#include <QFileDialog>
#include <QCompleter>
#include <QDateTimeEdit>
#include <QListWidgetItem>
#include <QFileSystemModel>
#include <QWizard>
#include <QClipboard>
#include <QFileIconProvider>
#endif

// other includes
#include "../Framework/Types.h"
#include "../Framework/DebugHelpers.h"
#include "../Framework/Functions.h"

#define ARRSIZE(x)	(sizeof(x)/sizeof(x[0]))

#ifndef Max
#define Max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef Min
#define Min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

#define NO_HOSTERS