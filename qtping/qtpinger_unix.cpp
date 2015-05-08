#include "qtpinger_unix.h"

#include <QTimer>

#if !defined(WIN32)// || defined(_DEBUG)
QtPinger* newQtPinger(QObject* parent)
{
	return new QtPingerUnix(parent);
}
#endif

QtPingerUnix::QtPingerUnix(QObject* parent)
: QObject(parent)
{
	nTTL = 0;
	nInterval = 0;

	if(parent)
		connect(this, SIGNAL(PingResult(QtPingStatus)), parent, SIGNAL(PingResult(QtPingStatus)));

	connect(&m_Ping, SIGNAL(readyReadStandardOutput()), this, SLOT(OnOutput()));
	connect(&m_Ping, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(OnFinished(int, QProcess::ExitStatus)));

	bReadHead = false;
	bIPv6 = false;
}

QtPingerUnix::~QtPingerUnix()
{
}

#ifdef WIN32
#define CMD_CNT "-n"
#define CMD_TTL "-i"
#define CMD_INF "-t"
#else
#define CMD_CNT "-c"
#define CMD_TTL "-t"
#define CMD_INT "-i"
#endif
#define CMD_TIO "-w"

#define REG_IP(x) QString(x ? "[0-9a-z]{4}:[0-9a-z:]*:[0-9a-z]{4}" : "[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}")

#ifdef WIN32
#define	REG_HDR	".*" + REG_IP(bIPv6) + ".+" // Ping wird ausgeführt für ping.inode.at [195.58.160.103] mit 32 Bytes Daten:
#else
#define	REG_HDR	".*\\(" + REG_IP(bIPv6) + "\\).+"	// PING ping.inode.at (195.58.160.103) 56(84) bytes of data.
#endif

QtPingStatus ParseResult(const QString& Line, bool bIPv6)
{
	QtPingStatus retValue;
	if(QRegExp(".*" + REG_IP(bIPv6) + "\\:.+").exactMatch(Line))
	{
		int Pos1 = Line.indexOf(QRegExp("" + REG_IP(bIPv6) + ""));
		int Pos2 = Line.indexOf(": ", Pos1);
		QString IP = Line.mid(Pos1, Pos2-Pos1);

		retValue.address = QHostAddress(IP);

#ifdef WIN32
		int Pos3 = Line.indexOf(QRegExp("[=<][0-9]+ms"), Pos2);
#else
		int Pos3 = Line.indexOf(QRegExp("[=][0-9\\.]+ ms"), Pos2);
#endif
		if(Pos3 == -1)
		{
			retValue.success = false;

			QString Err = Line.mid(Pos2).trimmed();
			retValue.error = Err;
		}
		else
		{
			retValue.success = true;

			int Pos4 = Line.indexOf("ms", Pos3);
			QString ms = Line.mid(Pos3+1, Pos4-(Pos3+1));
#ifdef WIN32
			retValue.delay = ms.toInt();
#else
			retValue.delay = ms.toDouble();
#endif

			int Pos5 = Line.indexOf("ttl=", Pos2, Qt::CaseInsensitive);
			if(Pos5 != -1)
			{
				int Pos6 = Line.indexOf(" ", Pos5);
				if(Pos6 == -1)
					Pos6 = Line.length();
				QString ttl = Line.mid(Pos5+4, Pos6-(Pos5+4));
				retValue.ttl = ttl.toInt(); 
			}
		}
	}
	else
	{
		retValue.success = false;
		retValue.error = Line;
	}
	return retValue;
}

QtPingStatus QtPingerUnix::Ping(const QHostAddress& address, int nTTL, int nTimeOut)
{
	bool bIPv6 = address.protocol() == QAbstractSocket::IPv6Protocol;

	QProcess Ping;
	QStringList Arguments;
	Arguments.append(address.toString());
	Arguments.append(CMD_CNT);
	Arguments.append("1");
	Arguments.append(CMD_TTL);
	Arguments.append(QString::number(nTTL));
	Arguments.append(CMD_TIO);
#ifdef WIN32
	Arguments.append(QString::number(nTimeOut)); // integer in ms
	Ping.start("ping.exe", Arguments);
#else
	Arguments.append(QString::number(double(nTimeOut)/1000.0)); // float in sec
	Ping.start(bIPv6 ? "ping6" : "ping", Arguments);
#endif
	Ping.waitForFinished();

	// Skip Header
	while(Ping.canReadLine())
	{
		QString Line = Ping.readLine().trimmed();
		if(QRegExp(REG_HDR).exactMatch(Line))
			break;
	}

	QString Line = Ping.readLine().trimmed();
	return ParseResult(Line, bIPv6);
}

bool QtPingerUnix::Start(const QHostAddress& address, int nTTL, int nInterval)
{
	if(m_Ping.state() != QProcess::NotRunning)
	{
		Q_ASSERT(0);
		return false;
	}

	this->address = address;
	this->nTTL = nTTL;
    this->nInterval = nInterval;

	Start();

	return m_Ping.waitForStarted(3000);
}

void QtPingerUnix::Start()
{
    bReadHead = true;
	bIPv6 = address.protocol() == QAbstractSocket::IPv6Protocol;

	QStringList Arguments;
	Arguments.append(address.toString());
	Arguments.append(CMD_TTL);
	Arguments.append(QString::number(nTTL));
#ifdef WIN32
	Arguments.append(CMD_INF);
	
	m_Ping.start("ping.exe", Arguments);
#else
	Arguments.append(CMD_INT);
    Arguments.append(QString::number(double(nInterval)/1000.0));

	m_Ping.start(bIPv6 ? "ping6" : "ping", Arguments);
#endif
}

void QtPingerUnix::OnOutput()
{
	while(m_Ping.canReadLine())
	{
		QString Line = m_Ping.readLine().trimmed();
		if(bReadHead)
		{
			if(QRegExp(REG_HDR).exactMatch(Line))
				bReadHead = false;
			continue;
		}

		QtPingStatus retValue = ParseResult(Line, bIPv6);
		emit PingResult(retValue);
	}
}

void QtPingerUnix::Stop()
{
	m_Ping.terminate();
	m_Ping.waitForFinished(3000);
}

void QtPingerUnix::OnFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
	QTimer::singleShot(100, this, SLOT(Start())); // if the pinger fails restart it
}
