#include <qtbencode/bencode.h>
#include <QCoreApplication>

QByteArray encode_example()
{
    /* encoding a string: "s3:foo" */
    qDebug("\"foo\" = %s", Bencoder::encode("foo").buffer().constData());

    /* encoding a large integer: i123456e */
    qDebug("123456 = %s", Bencoder::encode(123456).buffer().constData());

    /* encoding a negative integer: i-123456789123123e */
    qDebug("-123456789123123LL = %s",
           Bencoder::encode(-123456789123123LL).buffer().constData());

    /* encoding a list of integers: li10ei123456789123123ee */
    QList<qlonglong> intlist;
    intlist.append(10);
    intlist.append(123456789123123LL);
    qDebug("[10, 123456789123123] = %s",
           Bencoder::encode(intlist).buffer().constData());

    /* encoding a string map: d1:as3:foo1:bs3:bare */
    QMap<QString, QString> strmap;
    strmap.insert("a", "foo");
    strmap.insert("b", "bar");
    qDebug("{'a': 'foo', 'b': 'bar'} = %s",
           Bencoder::encode(strmap).buffer().constData());

    /* encoding a largish structure
     * Python dictionary:
     * {
     *     'integer list': [10, 123456789123123],
     *     'string map: {'a': 'foo', 'b': 'bar'},
     *     'just a number': 123456789123123
     * }
     */
    BencodedMap d;
    d.set("integer list", intlist);
    d.set("string map", strmap);
    d.set("just a int", 123456789123123LL);
    QByteArray buf = Bencoder::encode(d).buffer();

    /* write it out.. */
    qDebug("\nEncoded structure:");
    qDebug("%s\n", buf.constData());

    return buf;
}

bool decode_example(const QByteArray &buf)
{
    /* Decode the given buffer back to BencodedMap.
     * decode() returns true if the given string was property decoded.
     */
    BencodedMap d;
    if (!Bdecoder::decode(&d, buf))
        return false;

    /* decode the integer list */
    QList<qlonglong> intlist;
    if (!d.get("integer list", &intlist))
        return false;

    qDebug("Decoded integer list:");
    foreach (qlonglong value, intlist)
        qDebug("  %lld", value);

    /* decode the string map */
    QMap<QString, QString> strmap;
    if (!d.get("string map", &strmap))
        return false;

    qDebug("\nDecoded string map:");
    for (QMap<QString, QString>::const_iterator i = strmap.begin();
         i != strmap.end(); ++i) {
        qDebug("  %s = %s", qPrintable(i.key()), qPrintable(i.value()));
    }

    /* Try to decode the integer. This should fail, because it will not fit
       to a 32-bit variable */
    int justint;
    if (!d.get("just a int", &justint))
        qDebug("\nDecoding too a large integer failed, as expected.");

    return true;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QByteArray buf = encode_example();
    if (!decode_example(buf))
        qWarning("WARNING: something went wrong!");

    return 0;
}
