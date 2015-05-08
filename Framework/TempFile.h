#pragma once

#include <QTemporaryFile>
#include <QBuffer>

#include "./NeoHelper/neohelper_global.h"

class NEOHELPER_EXPORT CTempFile : public QIODevice
{
	Q_OBJECT
public:
	CTempFile(const QString& FileName = "");
	~CTempFile(){}

	virtual void truncate();
	virtual bool open(OpenMode flags);
	virtual void close();
	virtual qint64 size() const				{return m_pDevice->size();}
	virtual bool seek(qint64 pos)			{return QIODevice::seek(pos) && m_pDevice->seek(pos);}
	virtual bool isSequential() const		{return false;}
	virtual bool atEnd() const				{return pos() >= size();}

protected:
	virtual qint64	readData(char *data, qint64 maxlen);
    virtual qint64	writeData(const char *data, qint64 len);

	QString			m_FileName;
	QIODevice*		m_pDevice;
};
