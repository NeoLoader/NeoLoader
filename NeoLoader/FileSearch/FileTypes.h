#pragma once

bool IsExecutableExt(const QString& Ext);
bool IsArchiveExt(const QString& Ext);
bool IsDocumentExt(const QString& Ext);
bool IsPictureExt(const QString& Ext);
bool IsVideoExt(const QString& Ext);
bool IsAudioExt(const QString& Ext);

enum EFileTypes
{
	eUnknownExt = 0,
	eVideoExt,
	eAudioExt,
	ePictureExt,
	eDocumentExt,
	eArchiveExt,
	eProgramExt
};

EFileTypes GetFileTypeByExt(const QString& Ext);