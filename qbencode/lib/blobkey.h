#ifndef _QT_BENCODE_BLOBKEY_H
#define _QT_BENCODE_BLOBKEY_H

#include <QByteArray>
#include <QVariant>

class BlobKey
{
public:
    BlobKey() :
        m_valid(false)
    {
    }
    BlobKey(const BlobKey &from);

    static const int SHA1_LENGTH = 20;

    QByteArray toByteArray() const;
    QString toString() const;

    bool operator == (const BlobKey &b) const;
    bool operator != (const BlobKey &b) const;
    bool operator < (const BlobKey &b) const;
    uchar operator[] (size_t i) const;

    bool isValid() const { return m_valid; }

    static BlobKey fromByteArray(const QByteArray &arr);
    static BlobKey fromString(const QString &str);
    static BlobKey fromQVariant(const QVariant &var);
    static BlobKey hashBlob(const QByteArray &buf);

private:
    bool m_valid;
    uchar m_sha1[SHA1_LENGTH];
};

Q_DECLARE_METATYPE(BlobKey)

#endif
