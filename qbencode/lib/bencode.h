#ifndef _QT_BENCODE_H
#define _QT_BENCODE_H

#include <QVariant>
#include <QByteArray>
#include <QStringList>

#ifdef QBENCODE_LIB
# define QBENCODE_EXPORT Q_DECL_EXPORT
#else
# define QBENCODE_EXPORT Q_DECL_IMPORT
#endif

class BlobKey;

class QBENCODE_EXPORT BencodedBuffer {
public:
    BencodedBuffer(const QByteArray &buf, int pos, int len);
    explicit BencodedBuffer(const QByteArray &buf = QByteArray());

    int srcPos() const { return m_pos; }
    int len() const { return m_len; }
    QByteArray srcBuffer() const { return m_buf; }

    QByteArray buffer() const { return m_buf.mid(m_pos, m_len); }

private:
    QByteArray m_buf;
    int m_pos, m_len;
};

typedef QList<BencodedBuffer> BencodedList;

class QBENCODE_EXPORT Bencoder {
public:
    Bencoder(bool lazy = true) {m_lazy = lazy;}

    BencodedBuffer buffer() const { return BencodedBuffer(m_buf); }

    template<class T>
    void write(const QMap<QString, T> &map)
    {
        m_buf += 'd';
        for (typename QMap<QString, T>::const_iterator i = map.begin();
             i != map.end(); ++i) {
            write(i.key().toUtf8());
            write(i.value());
        }
        m_buf += 'e';
    }

    template<class T>
    void write(const QList<T> &list)
    {
        m_buf += 'l';
        foreach (const T &item, list)
            write(item);
        m_buf += 'e';
    }

    template<class T1, class T2>
    void write(const QPair<T1, T2> &pair)
    {
        m_buf += 'l';
        write(pair.first);
        write(pair.second);
        m_buf += 'e';
    }

    void write(const QVariant &data);
    void write(int value);
    void write(qlonglong value);
    void write(qulonglong value);
    void write(bool value);
    void write(const QString &string);
    void write(const QByteArray &buf);
    void write(const BlobKey &key);
    void write(const BencodedBuffer &buf);
    void write(const char *str);

    template<class T>
    void write(const T *obj)
    {
        obj->write(this);
    }

    void setError();
    template<class T>
    static BencodedBuffer encode(const T &val, bool lazy = true)
    {
        Bencoder bencoder(lazy);
        bencoder.write(val);
        return bencoder.buffer();
    }

private:
    QByteArray m_buf;
	bool m_lazy;

    void write(void *) {}
};

class QBENCODE_EXPORT Bdecoder {
public:
    static const int MAX_DEPTH = 64;

    Bdecoder(const QByteArray &buf);
    Bdecoder(const BencodedBuffer &buf);

    bool error() const { return !m_error.isEmpty(); }
    QString errorString() const { return m_error; }
    int pos() const { return m_pos; }
    bool atEnd() const { return m_pos == m_len; }

    void read(QVariant *data);
    void read(int *value);
    void read(qlonglong *value);
    void read(qulonglong *value);
	void read(qulonglong *value, bool &bSigned);
    void read(bool *value);
    void read(QString *string);
    void read(QByteArray *buf);
    void read(BlobKey *key);
    void read(BencodedBuffer *raw);

    template<class T>
    void read(QMap<QString, T> *map)
    {
        if (map)
            map->clear();
        if (m_pos >= m_len || m_buf[m_pos] != 'd') {
            setError("expected dict, got %c", char(m_buf[m_pos]));
            return;
        }
        if (m_depth >= MAX_DEPTH) {
            setError("Max recursion depth exceeded");
            return;
        }
        Q_ASSERT(m_depth >= 0);
        m_depth++;
        m_pos++;
        while (m_buf[m_pos] != 'e' && !error()) {
            if (map) {
                QByteArray key;
                read(&key);
                T val;
                read(&val);
                map->insert(QString::fromUtf8(key), val);
            } else {
                read(static_cast<QByteArray*>(NULL));
                read(static_cast<T*>(NULL));
            }
        }
        m_pos++;
        m_depth--;
    }

    template<class T>
    void read(QList<T> *list)
    {
        if (list)
            list->clear();
        if (m_pos >= m_len || m_buf[m_pos] != 'l') {
            setError("expected list, got %c", char(m_buf[m_pos]));
            return;
        }
        if (m_depth >= MAX_DEPTH) {
            setError("Max recursion depth exceeded");
            return;
        }
        Q_ASSERT(m_depth >= 0);
        m_depth++;
        m_pos++;
        while (m_buf[m_pos] != 'e' && !error()) {
            if (list) {
                T item;
                read(&item);
                list->append(item);
            } else
                read(static_cast<T*>(NULL));
        }
        m_pos++;
        m_depth--;
    }

    template<class T1, class T2>
    void read(QPair<T1, T2> *pair)
    {
        if (m_pos >= m_len || m_buf[m_pos] != 'l') {
            setError("expected list, got %c", char(m_buf[m_pos]));
            return;
        }
        if (m_depth >= MAX_DEPTH) {
            setError("Max recursion depth exceeded");
            return;
        }
        Q_ASSERT(m_depth >= 0);
        m_depth++;
        m_pos++;
        if (pair) {
            read(&pair->first);
            read(&pair->second);
        } else {
            read(static_cast<T1*>(NULL));
            read(static_cast<T2*>(NULL));
        }
        if (m_pos >= m_len || m_buf[m_pos] != 'e') {
            setError("Invalid pair");
            return;
        }
        m_pos++;
        m_depth--;
    }

    template<class T>
    void read(T **obj)
    {
        *obj = new T;
        (*obj)->read(this);
    }

    void setError(const char *fmt, ...);

    template<class T>
    static bool decode(T *value, const QByteArray &buf)
    {
        Bdecoder decoder(buf);
        decoder.read(value);
        return !decoder.error() && decoder.atEnd();
    }

    template<class T>
    static bool decode(T *value, const BencodedBuffer &buf)
    {
        Bdecoder decoder(buf);
        decoder.read(value);
        return !decoder.error() && decoder.atEnd();
    }

private:
    QByteArray m_buf;
    int m_pos;
    int m_len;
    int m_depth;
    QString m_error;

    bool validateInt(const QByteArray &s);
};

class BencodedMap: public QMap<QString, BencodedBuffer> {
public:
    BencodedMap() {}
	BencodedMap(const QMap<QString, BencodedBuffer> &from) :
    QMap<QString, BencodedBuffer>(from){}

    template<class T>
    void set(const QString &key, const T &val)
    {
        insert(key, Bencoder::encode(val));
    }
    template<class T>
    bool get(const QString &key, T *val) const
    {
        return Bdecoder::decode(val, value(key));
    }

	QVariant get(const QString &key)
	{
		QVariant data;
		get(key, &data);
		return data;
	}
};

Q_DECLARE_METATYPE(BencodedMap)

#endif
