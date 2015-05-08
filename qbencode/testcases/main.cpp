#include <qtbencode/bencode.h>
#include <QCoreApplication>

template<class T>
static void testDecode(const T &expected, const QByteArray &buf)
{
    T out;
    Bdecoder dec(buf);
    dec.read(&out);
    if (dec.error()) {
        qWarning("WARNING: Unexpected failure: %s",
                 qPrintable(dec.errorString()));
        qWarning("input: %s", buf.constData());
        return;
    }
    if (!dec.atEnd()) {
        qWarning("WARNING: Data left undecoded");
        qWarning("input: %s", buf.constData());
        return;
    }
    if (out != expected) {
        qWarning("WARNING: Result is different from expected");
        qWarning("input: %s", buf.constData());
    }
}

template<class T>
static void testInvalid(const T &foo, const QByteArray &buf)
{
    T out;
    Bdecoder dec(buf);
    dec.read(&out);
    if (!dec.error()) {
        qWarning("WARNING: Unexpected success");
        qWarning("input: %s", buf.constData());
        return;
    }
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    testDecode(1, "i1e");
    testDecode(-1, "i-1e");

    /*
     * Please note that the original decoder in bencode.py accepts integers
     * with leading spaces and zeroes.
     */
    testInvalid(0, "i 1e");
    testInvalid(0, "i-0e");
    testInvalid(0, "i01e");

    testInvalid(0, "123");
    testInvalid(0, "i123123123123123e");
    testInvalid(0, "i-123123123123123e");

    testDecode(QByteArray("foo"), "3:foo");
    testDecode(QByteArray(""), "0:");

    testInvalid(0, "3:foo");
    testInvalid(QByteArray(), "123");
    testInvalid(QByteArray(), "123:");

    testDecode(QString("foo"), "s3:foo");
    testDecode(QString(""), "s0:");

    return 0;
}
