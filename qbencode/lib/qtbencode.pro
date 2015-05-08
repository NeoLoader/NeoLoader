unix: isEmpty(PREFIX) error(Prefix is not set. Please run ./configure instead!)

TEMPLATE = lib
TARGET = qtbencode
DEPENDPATH += .
INCLUDEPATH += ../daemon/
QT += network

win32: CONFIG += dll

unix: {
    exists($$PREFIX/lib64) INSTALL_PATH = $$PREFIX/lib64
    isEmpty(INSTALL_PATH) INSTALL_PATH = $$PREFIX/lib

    target.path = $$INSTALL_PATH
    headers.path = $$PREFIX/include/qtbencode
}
headers.files = bencode.h blobkey.h
INSTALLS += target headers

HEADERS += bencode.h blobkey.h
SOURCES += bencode.cpp blobkey.cpp
