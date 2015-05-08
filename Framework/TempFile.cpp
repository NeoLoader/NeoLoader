#include "GlobalHeader.h"
#include "TempFile.h"

CTempFile::CTempFile(const QString& FileName)
{
	m_FileName = FileName;
	m_pDevice = new QBuffer(this);
}

bool CTempFile::open(OpenMode flags)
{
	m_pDevice->open(QIODevice::ReadWrite);
	return QIODevice::open(flags);
}

void CTempFile::close()
{
	QIODevice::close();
	m_pDevice->close();
}
	
void CTempFile::truncate()
{
	reset();
	if(QTemporaryFile* pTemp = qobject_cast<QTemporaryFile*>(m_pDevice))
		pTemp->resize(0);
	else
	{
		delete m_pDevice;
		m_pDevice = new QBuffer(this);
		m_pDevice->open(QIODevice::ReadWrite);
	}
}

qint64 CTempFile::readData(char *data, qint64 maxlen)
{
	ASSERT(m_pDevice->pos() == pos());
	return m_pDevice->read(data, maxlen);
}

qint64 CTempFile::writeData(const char *data, qint64 len)
{
	ASSERT(m_pDevice->pos() == pos());
	// if the temp file becomes to big dump it to disk
	if(m_pDevice->inherits("QBuffer")  && size() + len > MB2B(4))
	{
		QIODevice* pDevice = m_pDevice;
		pDevice->seek(0);
		m_pDevice = new QTemporaryFile(m_FileName, this);
		m_pDevice->open(QIODevice::ReadWrite);
		m_pDevice->write(pDevice->readAll());
		m_pDevice->seek(pos());
		delete pDevice;
	}

	return m_pDevice->write(data, len);
}