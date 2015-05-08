#pragma once

#include "../NeoHelper/neohelper_global.h"

struct NEOHELPER_EXPORT SHttpPost
{
	SHttpPost(QString sName = "", QString sValue = "")
	{
		Name = sName;
		Value = sValue;
		Buffer = NULL;
		Buff = eBuffNone;
	}
	SHttpPost(const SHttpPost* Entry)
	{
		Name = Entry->Name;
		Value = Entry->Value;
		Type = Entry->Type;
		Buffer = NULL;
		ASSERT(Entry->IsBufferClosed()); // we cant copy SHttpPost's that are in a intermediate state
		Buff = eBuffNone;
		if(Buff == eBuffExtern || Buff == eBuffExternClosed)
			Buff = eBuffWaiting;
		else if(Buff == eBuffIntern || Buff == eBuffInternClosed)
		{
			Buff = eBuffIntern;
			Buffer = new QBuffer();
			Buffer->open(QBuffer::ReadWrite);
			Entry->Buffer->seek(0);
			Buffer->write(Entry->Buffer->readAll());
		}
	}
	~SHttpPost()
	{
		if((Buff == eBuffIntern || Buff == eBuffInternClosed) && Buffer)
			delete Buffer;
	}

	QString				Name;
	QString				Value;
	// Multipart Form Submission
	QString				Type;
	QIODevice*			Buffer;
	enum				eState
	{
		eBuffNone = 0,
		eBuffWaiting,
		eBuffExtern,
		eBuffExternClosed,
		eBuffIntern,
		eBuffInternClosed,
	}					Buff;

	void	BufferFile(QByteArray& File)
	{
		ASSERT(Buff == eBuffNone);
		Buff = eBuffIntern;
		Buffer = new QBuffer();
		Buffer->open(QBuffer::ReadWrite);
		Buffer->write(File);
		Buffer->seek(0);
	}
	void	OpenBuffer()			{if(Buff == eBuffExternClosed) Buff = Buffer ? eBuffExtern : eBuffWaiting; else if(Buff == eBuffInternClosed) Buff = eBuffIntern; }
	void	CloseBuffer()			{if(Buff == eBuffExtern) Buff = eBuffExternClosed; else if(Buff == eBuffIntern) Buff = eBuffInternClosed; }
	bool	IsBufferClosed() const	{return Buff == eBuffNone || Buff == eBuffExternClosed || Buff == eBuffInternClosed;}
};

typedef NEOHELPER_EXPORT QList<SHttpPost*> TPostList;

typedef NEOHELPER_EXPORT QMap<QString,QString> TArguments;
TArguments NEOHELPER_EXPORT GetArguments(const QString& Arguments, QChar Separator = L';', QChar Assigner = L'=', QString* First = NULL);
QString	NEOHELPER_EXPORT GetArgument(const TArguments& Arguments, const QString& Name);

QString NEOHELPER_EXPORT Url2FileName(QString Url, bool bDecode = true);
QString NEOHELPER_EXPORT GetFileExt(const QString& FileName);

QDateTime NEOHELPER_EXPORT GetHttpDate(const QString &value);

bool NEOHELPER_EXPORT EscalatePath(QString& Path);

QString NEOHELPER_EXPORT GetTemplate(const QString& File, const QString& Section = "");
QString NEOHELPER_EXPORT FillTemplate(QString Template, const TArguments& Variables);

QString	NEOHELPER_EXPORT GetHttpContentType(QString FileName);
QString NEOHELPER_EXPORT GetHttpErrorString(int Code);
