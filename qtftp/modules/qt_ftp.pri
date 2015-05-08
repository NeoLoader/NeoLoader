QT.ftp.VERSION = 5.0.0
QT.ftp.MAJOR_VERSION = 5
QT.ftp.MINOR_VERSION = 0
QT.ftp.PATCH_VERSION = 0

QT.ftp.name = QtFtp
QT.ftp.bins = $$QT_MODULE_BIN_BASE
QT.ftp.includes = $$QT_MODULE_INCLUDE_BASE $$QT_MODULE_INCLUDE_BASE/QtFtp
QT.ftp.private_includes = $$QT_MODULE_INCLUDE_BASE/QtFtp/$$QT.ftp.VERSION
QT.ftp.sources = $$QT_MODULE_BASE/src/qftp
QT.ftp.libs = $$QT_MODULE_LIB_BASE
QT.ftp.plugins = $$QT_MODULE_PLUGIN_BASE
QT.ftp.imports = $$QT_MODULE_IMPORT_BASE
QT.ftp.depends = core network
