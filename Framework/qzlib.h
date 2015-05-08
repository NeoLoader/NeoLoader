#pragma once
#include "../zlib/zlib.h"

#include "./NeoHelper/neohelper_global.h"

QByteArray NEOHELPER_EXPORT Pack(const QByteArray& Data);
QByteArray NEOHELPER_EXPORT Unpack(const QByteArray& Data);

bool NEOHELPER_EXPORT gzip_arr(QByteArray& in);
bool NEOHELPER_EXPORT IsgZiped(const QByteArray& zipped);
QByteArray NEOHELPER_EXPORT ungzip_arr(z_stream* &zS, QByteArray& zipped, bool bGZip = true, int iRecursion = 0);

void NEOHELPER_EXPORT clear_z(z_stream* &zS);
