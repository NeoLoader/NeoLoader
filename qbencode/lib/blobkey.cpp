#include "blobkey.h"
#include <QCryptographicHash>
#include <QtEndian>

BlobKey::BlobKey(const BlobKey &from) :
    m_valid(from.m_valid)
{
    memcpy(m_sha1, from.m_sha1, SHA1_LENGTH);
}

QByteArray BlobKey::toByteArray() const
{
    if (!m_valid)
        return QByteArray();
    return QByteArray(reinterpret_cast<const char *>(m_sha1), SHA1_LENGTH);
}

QString BlobKey::toString() const
{
    return toByteArray().toHex();
}

bool BlobKey::operator == (const BlobKey &b) const
{
    return m_valid && b.m_valid && memcmp(m_sha1, b.m_sha1, SHA1_LENGTH) == 0;
}

bool BlobKey::operator != (const BlobKey &b) const
{
    return m_valid && b.m_valid && memcmp(m_sha1, b.m_sha1, SHA1_LENGTH) != 0;
}

bool BlobKey::operator < (const BlobKey &b) const
{
    Q_ASSERT(m_valid && b.m_valid);
    return memcmp(m_sha1, b.m_sha1, SHA1_LENGTH) < 0;
}

quint8 BlobKey::operator[] (size_t i) const
{
    Q_ASSERT(i < SHA1_LENGTH);
    return m_sha1[i];
}

BlobKey BlobKey::fromByteArray(const QByteArray &arr)
{
    BlobKey key;
    if (arr.size() == SHA1_LENGTH) {
        key.m_valid = true;
        memcpy(key.m_sha1, arr, SHA1_LENGTH);
    }
    return key;
}

BlobKey BlobKey::fromString(const QString &str)
{
    return fromByteArray(QByteArray::fromHex(str.toLatin1()));
}

BlobKey BlobKey::fromQVariant(const QVariant &var)
{
    return var.value<BlobKey>();
}

BlobKey BlobKey::hashBlob(const QByteArray &buf)
{
    QByteArray arr = QCryptographicHash::hash(buf, QCryptographicHash::Sha1);
    return fromByteArray(arr);
}
