#include "bencode.h"
#include "blobkey.h"
#include <cctype>

/*
 * COMPABILITY NOTE
 *
 * Please note that the original decoder in bencode.py accepts integers with
 * leading spaces and zeroes. To enable compability with the original decoder,
 * comment out the following lines.
 */
#define NO_LEADING_SPACES
#define NO_LEADING_ZEROES

void Bencoder::write(const QVariant &data)
{
    switch (data.type()) {
    case QVariant::Map:
        write(data.toMap());
        break;
    case QVariant::List:
        write(data.toList());
        break;
	case QVariant::StringList:
        write(data.toStringList());
        break;
    case QVariant::Int:
    case QVariant::LongLong:
        write(data.toLongLong());
        break;
    case QVariant::UInt:
    case QVariant::ULongLong:
        write(data.toULongLong());
        break;
    case QVariant::String:
        write(data.toString());
        break;
    case QVariant::Bool:
        write(data.toBool());
        break;
    case QVariant::ByteArray:
        write(data.toByteArray());
        break;
    case QVariant::UserType:
        if (data.canConvert<BlobKey>())
            write(BlobKey::fromQVariant(data));
        else
		{
			Q_ASSERT(0);
            qFatal("Bencoder: unable to encode user types");
		}
        break;
	case QVariant::Invalid:
		write("");
		break;
    default:
		//qWarning("Bencoder: invalid type: %s", data.typeName());
		write(data.toString());
        break;
    }
}

void Bencoder::write(int value)
{
    m_buf += 'i' + QByteArray::number(value) + 'e';
}

void Bencoder::write(qlonglong value)
{
    m_buf += 'i' + QByteArray::number(value) + 'e';
}

void Bencoder::write(qulonglong value)
{
    m_buf += 'i' + QByteArray::number(value) + 'e';
}

void Bencoder::write(bool value)
{
	if(m_lazy)
	{
		write((int)value);
		return;
	}

    m_buf += 'b';
    if (value)
        m_buf += '1';
    else
        m_buf += '0';
}

void Bencoder::write(const QString &string)
{
	if(!m_lazy)
		m_buf += 's';

    write(string.toUtf8());
}

void Bencoder::write(const QByteArray &buf)
{
    m_buf += QByteArray::number(buf.size()) + ':';
    m_buf += buf;
}

void Bencoder::write(const BlobKey &key)
{
	Q_ASSERT(!m_lazy);

    m_buf += 'k';
    write(key.toByteArray());
}

void Bencoder::write(const BencodedBuffer &buf)
{
    m_buf += buf.buffer();
}

void Bencoder::write(const char *str)
{
    write(QString(str));
}

BencodedBuffer::BencodedBuffer(const QByteArray &buf, int pos, int len) :
    m_buf(buf), m_pos(pos), m_len(len)
{
}

BencodedBuffer::BencodedBuffer(const QByteArray &buf) :
    m_buf(buf), m_pos(0), m_len(buf.size())
{
}

Bdecoder::Bdecoder(const QByteArray &buf) :
    m_buf(buf), m_pos(0), m_len(buf.size()), m_depth(0)
{
}

Bdecoder::Bdecoder(const BencodedBuffer &buf) :
    m_buf(buf.srcBuffer()), m_pos(buf.srcPos()), m_len(buf.srcPos() + buf.len()),
    m_depth(0)
{
}

void Bdecoder::read(QVariant *data)
{
    if (data == NULL) {
        switch (m_buf[m_pos]) {
        case 'd':
            read(static_cast<QVariantMap *>(NULL));
            break;
        case 'l':
            read(static_cast<QVariantList *>(NULL));
            break;
        case 'i':
            read(static_cast<qlonglong *>(NULL));
            break;
        case 'b':
            read(static_cast<bool *>(NULL));
            break;
        case 's':
            read(static_cast<QString *>(NULL));
            break;
        case 'k':
            read(static_cast<BlobKey *>(NULL));
            break;
        default:
            read(static_cast<QByteArray *>(NULL));
            break;
        }
        return;
    }
    switch (m_buf[m_pos]) {
    case 'd': {
            QVariantMap map;
            read(&map);
            *data = map;
        }
        break;
    case 'l': {
            QVariantList list;
            read(&list);
            *data = list;
        }
        break;
    case 'i': {
            /*qlonglong value;
            read(&value);
            if (value >= INT_MIN && value <= INT_MAX)
                *data = int(value);
            else
                *data = value;*/

			qulonglong value;
			bool bSigned = false;
            read(&value, bSigned);
            if (bSigned && qlonglong(value) >= INT_MIN && qlonglong(value) <= INT_MAX)
                *data = int(value);
            else if (qulonglong(value) <= UINT_MAX)
				*data = quint32(value);
			else
                *data = value;
        }
        break;
    case 'b': {
            bool value;
            read(&value);
            *data = value;
        }
        break;
    case 's': {
            QString string;
            read(&string);
            *data = string;
        }
        break;
    case 'k': {
            BlobKey key;
            read(&key);
            *data = QVariant::fromValue(key);
        }
        break;
    default: {
            QByteArray buf;
            read(&buf);
            *data = buf;
        }
        break;
    }
}

void Bdecoder::read(int *value)
{
    qlonglong ll;
    read(&ll);
    if (ll >= INT_MIN && ll <= INT_MAX)
        *value = ll;
    else {
        setError("integer too large (%lld)", ll);
    }
}

void Bdecoder::read(qlonglong *value)
{
    if (m_pos >= m_len || m_buf[m_pos] != 'i') {
        setError("expected integer, got %c", char(m_buf[m_pos]));
        return;
    }
    m_pos++;
    int end = m_buf.indexOf('e', m_pos);
    if (end <= m_pos || end >= m_len) {
        setError("buffer overrun");
        return;
    }

    if (value) {
        QByteArray s = m_buf.mid(m_pos, end - m_pos);
        if (!validateInt(s))
            return;
        bool ok;
        *value = s.toLongLong(&ok, 10);
        if (!ok) {
            setError("Invalid integer (%s)", s.constData());
            return;
        }
    }
    m_pos = end + 1;
}

void Bdecoder::read(qulonglong *value)
{
    if (m_pos >= m_len || m_buf[m_pos] != 'i') {
        setError("expected integer, got %c", char(m_buf[m_pos]));
        return;
    }
    m_pos++;
    int end = m_buf.indexOf('e', m_pos);
    if (end <= m_pos || end >= m_len) {
        setError("buffer overrun");
        return;
    }

    if (value) {
        QByteArray s = m_buf.mid(m_pos, end - m_pos);
        if (!validateInt(s))
            return;
        bool ok;
        *value = s.toULongLong(&ok, 10);
        if (!ok) {
            setError("Invalid integer (%s)", s.constData());
            return;
        }
    }
    m_pos = end + 1;
}

void Bdecoder::read(qulonglong *value, bool &bSigned)
{
    if (m_pos >= m_len || m_buf[m_pos] != 'i') {
        setError("expected integer, got %c", char(m_buf[m_pos]));
        return;
    }
    m_pos++;
    int end = m_buf.indexOf('e', m_pos);
    if (end <= m_pos || end >= m_len) {
        setError("buffer overrun");
        return;
    }

    if (value) {
        QByteArray s = m_buf.mid(m_pos, end - m_pos);
        if (!validateInt(s))
            return;
        bool ok;
		if(bSigned = (s.left(1) == "-"))
			*value = s.toLongLong(&ok, 10);
		else
			*value = s.toULongLong(&ok, 10);
        if (!ok) {
            setError("Invalid integer (%s)", s.constData());
            return;
        }
    }
    m_pos = end + 1;
}

void Bdecoder::read(bool *value)
{
    if (m_pos >= m_len || m_buf[m_pos] != 'b') {
        setError("expected boolean, got %c", char(m_buf[m_pos]));
        return;
    }
    m_pos++;
    switch (m_buf[m_pos]) {
    case '1':
        if (value)
            *value = true;
        break;
    case '0':
        if (value)
            *value = false;
        break;
    default:
        setError("Invalid boolean");
        return;
    }
    m_pos++;
}

void Bdecoder::read(QString *string)
{
    if (m_pos >= m_len || m_buf[m_pos] != 's') {
        setError("expected string, got %c", char(m_buf[m_pos]));
        return;
    }
    m_pos++;
    if (string) {
        QByteArray buf;
        read(&buf);
        *string = QString::fromUtf8(buf);
    } else
        read(static_cast<QByteArray *>(NULL));
}

void Bdecoder::read(QByteArray *buf)
{
    int end = m_buf.indexOf(':', m_pos);
    if (end <= m_pos || end >= m_len) {
        setError("buffer overrun");
        return;
    }
    QByteArray s = m_buf.mid(m_pos, end - m_pos);
    m_pos = end + 1;

    if (!validateInt(s))
        return;
    bool ok;
    int strlen = s.toUInt(&ok, 10);
    if (!ok) {
        setError("Invalid string length (%s)", s.constData());
        return;
    }

    if (strlen + m_pos > m_len) {
        setError("buffer overrun");
        return;
    }
    if (buf)
        *buf = m_buf.mid(m_pos, strlen);
    m_pos += strlen;
}

void Bdecoder::read(BlobKey *key)
{
    if (m_pos >= m_len || m_buf[m_pos] != 'k') {
        setError("expected BlobKey, got %c", char(m_buf[m_pos]));
        return;
    }
    m_pos++;
    if (key) {
        QByteArray buf;
        read(&buf);
        /* zero-length key is decoded as invalid blob key */
        if (!buf.isEmpty()) {
            *key = BlobKey::fromByteArray(buf);
            if (!key->isValid())
                setError("invalid BlobKey");
        }
    } else
        read(static_cast<QByteArray *>(NULL));
}

void Bdecoder::read(BencodedBuffer *raw)
{
    int m_begin = m_pos;
    read(static_cast<QVariant *>(NULL));
    if (raw)
        *raw = BencodedBuffer(m_buf, m_begin, m_pos - m_begin);
}

void Bdecoder::setError(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    m_error.vsprintf(fmt, ap);
    va_end(ap);
}

bool Bdecoder::validateInt(const QByteArray &s)
{
#ifdef NO_LEADING_SPACES
    if (s.size() >= 1 && isspace(s[0])) {
        setError("Integer with leading spaces: %s", s.constData());
        return false;
    }
#endif
#ifdef NO_LEADING_ZEROES
    if (s.size() >= 2 && (s[0] == '0' || (s[0] == '-' && s[1] == '0'))) {
        setError("Integer with leading zeroes: %s", s.constData());
        return false;
    }
#endif
    return true;
}

