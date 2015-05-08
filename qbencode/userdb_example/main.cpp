#include <qtbencode/bencode.h>
#include <QCoreApplication>
#include <QFile>

class User {
public:
    User() {}
    User(const QString &name, int age) :
        m_name(name), m_age(age)
    {
    }

    void show() const
    {
        qDebug("%s, %d", qPrintable(m_name), m_age);
    }

    void read(Bdecoder *decoder);
    void write(Bencoder *encoder) const;

private:
    QString m_name;
    int m_age;
};

/* User database format in Python:
 * [
 *     {'name': 'user 1', 'age': 27},
 *     {'name': 'user 2', 'age': 21},
 *     ...
 * ]
 */

void User::read(Bdecoder *decoder)
{
    BencodedMap d;
    decoder->read(&d);
    if (decoder->error())
        return;

    if (!d.get("name", &m_name) ||
        !d.get("age", &m_age)) {
        decoder->setError("Invalid user");
        return;
    }

    if (m_name.isEmpty()) {
        decoder->setError("empty name");
        return;
    }
    if (m_age < 0 || m_age >= 100) {
        decoder->setError("invalid age: %s", m_age);
        return;
    }
}

void User::write(Bencoder *encoder) const
{
    BencodedMap d;
    d.set("name", m_name);
    d.set("age", m_age);
    encoder->write(d);
}

/*
 * Commands:
 *     add [name] [age] - Add user 
 *     list - List users
 */

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QFile f("userdb.dat");

    QList<User *> users;

    /* load user database */
    if (f.open(QIODevice::ReadOnly)) {
        QByteArray buf = f.readAll();
        f.close();

        if (!Bdecoder::decode(&users, buf)) {
            qWarning("WARNING: user database is corrupted");
            return 1;
        }
    }

    bool save = false;

    QString cmd;
    if (argc >= 2)
        cmd = argv[1];

    if (cmd == "add" && argc >= 4) {
        User *user = new User(argv[2], QString(argv[3]).toInt());
        users.append(user);
        save = true;

    } else if (cmd == "list") {
        foreach (const User *user, users)
            user->show();

    } else
        qWarning("Unknown command: %s", qPrintable(cmd));

    /* save users */
    if (save && f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(Bencoder::encode(users).buffer());
        f.close();
    }
    qDeleteAll(users);

    return 0;
}
