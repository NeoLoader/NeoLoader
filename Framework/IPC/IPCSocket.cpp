#include "GlobalHeader.h"
#include "IPCSocket.h"
#include "../../Framework/Cryptography/SymmetricKey.h"
#include "../../Framework/Cryptography/KeyExchange.h"
#include "../../Framework/Cryptography/HashFunction.h"
#if 0 //QT_VERSION >= 0x050000
#define QT_JSON
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#else
#include "../../qjson/src/parser.h"
#include "../../qjson/src/serializer.h"
#endif
#include "../Xml.h"
#include "../../Framework/Scope.h"
#include "../../Framework/Exception.h"
#include "../../qbencode/lib/bencode.h"

int _QVariant_Type = qRegisterMetaType<QVariant>("QVariant");

CIPCSocket::CIPCSocket(QLocalSocket* pLocal, bool bIncomming)
{
	Init();
	m_pLocal = pLocal;

	connect(m_pLocal, SIGNAL(connected()), this, SLOT(OnConnected()));
	connect(m_pLocal, SIGNAL(disconnected()), this, SLOT(OnDisconnected()));
	connect(m_pLocal, SIGNAL(readyRead()), this, SLOT(OnReadyRead()));
	//connect(m_pLocal, SIGNAL(error(QLocalSocket::LocalSocketError)), this, SLOT(OnError(QLocalSocket::LocalSocketError)));

	m_bConnected = bIncomming;
	if(!bIncomming)
		QTimer::singleShot(SEC2MS(1),this,SLOT(OnTimer()));
}

CIPCSocket::CIPCSocket(QTcpSocket* pRemote, bool bIncomming)
{
	Init();
	m_pRemote = pRemote;
	m_Encrypt = 1;
	
	connect(m_pRemote, SIGNAL(connected()), this, SLOT(OnConnected()));
	connect(m_pRemote, SIGNAL(disconnected()), this, SLOT(OnDisconnected()));
	connect(m_pRemote, SIGNAL(readyRead()), this, SLOT(OnReadyRead()));
	//connect(m_pRemote, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(OnError(QAbstractSocket::SocketError)));

	m_bConnected = false;
	if(!bIncomming)
		QTimer::singleShot(SEC2MS(10),this,SLOT(OnTimer()));
}

void CIPCSocket::Init()
{
	m_pLocal = NULL;
	m_pRemote = NULL;
	m_Encoding = eBinary;
	m_CryptoKey = NULL;
	m_Exchange = NULL;
	m_Encrypt = 0;
	m_Counter = 0;
	m_pResult = NULL;
}

CIPCSocket::~CIPCSocket()
{
	CloseSocket();
	delete m_Exchange;
}

void CIPCSocket::CloseSocket()
{
	m_bConnected = false;

	QLocalSocket* pLocal = m_pLocal;
	m_pLocal = NULL;
	if(pLocal)
		pLocal->deleteLater();

	QTcpSocket* pRemote = m_pRemote;
	m_pRemote = NULL;
	if(pRemote)
		pRemote->deleteLater();
}

void CIPCSocket::OnConnected()
{
	if(m_pLocal)
	{
		m_bConnected = true;
		emit Connected();
	}
	if(m_pRemote)
	{
		if(m_Encrypt == 0)
			SendLoginReq();
		else
		{
			m_Exchange = new CKeyExchange(CAbstractKey::eECDH | CAbstractKey::eECP, "secp521r1");
			CScoped<CAbstractKey> pKey(m_Exchange->InitialsieKeyExchange());

			QVariantMap Data;
			Data["Key"] = pKey->ToByteArray();
			if(m_Encrypt == 2)
			{
				m_Exchange->SetIV(new CAbstractKey(KEY_128BIT,true));
				Data["IV"] = m_Exchange->GetIV()->ToByteArray();
			}
			else
				m_Exchange->SetIV(new CAbstractKey((byte*)CHashFunction::Hash(QByteArray((char*)pKey->GetKey(), (int)pKey->GetSize()), CAbstractKey::eSHA256).data(), KEY_256BIT));
			Send("Secure", "Request", Data, m_Encoding, 0);
		}
	}
}

void CIPCSocket::Disconnect(const QString& Error)
{
	if(!Error.isEmpty())
	{
		Send("Error", "", Error, m_Encoding, 0);
		if(QIODevice* pDev = Dev())
			pDev->waitForBytesWritten(100);
	}

	if(m_pLocal)
		m_pLocal->disconnectFromServer();
	if(m_pRemote)
		m_pRemote->disconnectFromHost();
}

void CIPCSocket::OnDisconnected()
{
	CloseSocket();
	emit Disconnected();
}

/*void CIPCSocket::OnError(QAbstractSocket::SocketError socketError)
{
	LogLine(LOG_ERROR, tr("Remote Connection Error %1").arg(socketError));
	OnDisconnected();
}

void CIPCSocket::OnError(QLocalSocket::LocalSocketError socketError)
{
	LogLine(LOG_ERROR, tr("Local Connection Error %1").arg(socketError));
	OnDisconnected();
}*/

void CIPCSocket::OnTimer()
{
	if(!m_bConnected)
	{
		CloseSocket();
		emit Disconnected();
	}
}

sint64 CIPCSocket::SendRequest(const QString& Name, const QVariant& Data)
{
	ASSERT(m_pResult == NULL);
	if(!m_bConnected)
		return 0;
	
	Send("Request", Name, Data, m_Encoding, ++m_Counter);
	return m_Counter;
}

bool CIPCSocket::SendRequest(const QString& Name, const QVariant& Data, QVariant& Result, int TimeOut)
{
	if(!SendRequest(Name, Data))
		return false;

	m_pResult = &Result;
	while(m_pResult)
	{
		if(!IsConnected()
		|| (m_pLocal && !m_pLocal->waitForReadyRead(TimeOut))
		|| (m_pRemote && !m_pRemote->waitForReadyRead(TimeOut)))
		{
			m_pResult = NULL;
			return false;
		}
	}
	return true;
}

bool CIPCSocket::SendResponse(const QString& Name, const QVariant& Data, sint64 Number)
{
	if(!m_bConnected)
		return false;

	Send("Response", Name, Data, m_Encoding, Number);
	return true;
}

void CIPCSocket::Send(const QString& Type, const QString& Name, const QVariant& Data, EEncoding Encoding, sint64 Number)
{
	if(QIODevice* pDev = Dev())
	{
		QByteArray Out = Variant2String(Data, Encoding);

		QByteArray Header;
		Header.append(Type + (Name.isEmpty() ? "" : (" " + Name)) + "\r\n");
		Header.append("Length: " + QByteArray::number(Out.length()) + "\r\n");
		if(Encoding == eJson)
			Header.append("Encoding: json\r\n");
		else if(Encoding == eXML)
			Header.append("Encoding: xml\r\n");
		else if(Encoding == eBencode)
			Header.append("Encoding: bencode\r\n");
		else if(Encoding == eBinary)
			Header.append("Encoding: binary\r\n");
		else {
			ASSERT(0);
		}
		Header.append("Number: " + QByteArray::number(Number) + "\r\n");

		Header.append("\r\n");
		if(m_CryptoKey)
		{
			m_CryptoKey->Encrypt(&Header);
			m_CryptoKey->Encrypt(&Out);
		}
		pDev->write(Header);
		pDev->write(Out);
	}
}

void CIPCSocket::OnReadyRead()
{
	if(QIODevice* pDev = Dev())
	{
		/*QByteArray In = pDev->readAll();
		if(m_CryptoKey)
			m_CryptoKey->Decrypt(&In);
		m_ReadBuffer.append(In);*/
		if(qint64 uToGo = pDev->bytesAvailable())
		{
			ASSERT(uToGo != -1);
			size_t uSize = m_ReadBuffer.GetSize();
			m_ReadBuffer.SetSize(uSize + uToGo, true);
			byte* pData = m_ReadBuffer.GetData(uSize, uToGo);
			qint64 uRead = pDev->read((char*)pData, uToGo);
			if(uRead == -1)
				uRead = 0;
			if(m_CryptoKey)
				m_CryptoKey->Decrypt(pData, pData, uRead);
			if(uRead != uToGo)
				m_ReadBuffer.SetSize(uSize + uRead, true);
		}
	}

	for(;;)
	{
		QByteArray ReadBuffer = QByteArray::fromRawData((char*)m_ReadBuffer.GetBuffer(), (int)m_ReadBuffer.GetSize());

		int End = ReadBuffer.indexOf("\r\n\r\n");
		if(End == -1) // header not yet complee
			break;

		QList<QByteArray> Header = QByteArray::fromRawData((char*)m_ReadBuffer.GetBuffer(), End).split('\n');
		QByteArray FirstLine = Header.takeFirst();
		QList<QByteArray> TypeName = FirstLine.split(' ');
		QByteArray Type = TypeName[0].trimmed();
		QByteArray Name = TypeName.size() > 1 ? TypeName[1].trimmed() : "";
		int ContentLength = 0;
		EEncoding& Encoding = m_Encoding;
		sint64 Number = 0;
		foreach(const QByteArray &Line, Header)
		{
			int Sep = Line.indexOf(":");
			QByteArray Key = Line.left(Sep);
			QByteArray Value = Line.mid(Sep+1).trimmed();
			if(Key == "Length")
				ContentLength = Value.toUInt();
			if(Key == "Encoding")
			{
				if(Value == "json")
					Encoding = eJson;
				else if(Value == "xml")
					Encoding = eXML;
				else if(Value == "bencode")
					Encoding = eBencode;
				else if(Value == "binary")
					Encoding = eBinary;
				else {
					ASSERT(0);
				}
			}
			if(Key == "Number")
				Number = Value.toLongLong();
		}

		if(ReadBuffer.size() < End+4 + ContentLength) // data not yet complete
			break;

		if(Encoding == eUnknown)
		{
			Disconnect("UnknownEncoding");
			return; // disconnected
		}

		QVariant Data = String2Variant(QByteArray::fromRawData((char*)m_ReadBuffer.GetBuffer() + End+4, ContentLength), Encoding);
		Receive(Type, Name, Data, Encoding, Number);

		m_ReadBuffer.ShiftData(End+4 + ContentLength);
		//ReadBuffer.remove(0,End+4 + ContentLength);
	}
}

void CIPCSocket::Receive(const QString& Type, const QString& Name, const QVariant& Data, EEncoding Encoding, sint64 Number)
{
	if(Type == "Secure")
	{
		if(Name == "Error")
		{
			LogLine(LOG_ERROR, tr("Secure Failed, Disconnecting"));
			Disconnect();
		}
		else if(ProcessEncryption(Data.toMap()))
		{
			if(Name == "Response")
				SendLoginReq();
		}
		else 
		{
			if(Name == "Response")
				Disconnect();
			else
				Send("Secure", "Error", QVariant(), m_Encoding, 0);
		}
	}
	else if(Type == "Login")
	{
		if(Name == "Error")
		{
			LogLine(LOG_ERROR, tr("Login Failed, Disconnecting"));
			Disconnect();
		}
		else if(Name == "Request")
			ProcessLoginReq(Data.toMap());
		else 
			ProcessLoginRes(Data.toMap());
	}
	else if(!m_bConnected) // client logged in?
	{
		LogLine(LOG_ERROR, tr("Recived Request form not logged in client").arg(QString(Name)));
		Disconnect("Unauthorized");
	}
	else
	{
		if(Type == "Request")
			emit Request(Name, Data, Number);
		else if(Type == "Response")
		{
			if(m_pResult && Number == m_Counter)
			{
				*m_pResult = Data;
				m_pResult = NULL;
			}
			else
				emit Response(Name, Data, Number);
		}
		else if(Type == "Event")
			emit Event(Name, Data, Number);
	}
}

sint64 CIPCSocket::SendEvent(const QString& Name, const QVariant& Data)
{
	if(!m_bConnected)
		return 0;

	Send("Event", Name, Data, m_Encoding, ++m_Counter);
	return m_Counter;
}

bool CIPCSocket::ProcessEncryption(const QVariantMap& Data)
{
	CScoped<CAbstractKey> pPubKey(new CAbstractKey());
	pPubKey->SetKey(Data["Key"].toByteArray());

	QByteArray IV = Data["IV"].toByteArray();
	CScoped<CAbstractKey> pIV;
	if(!IV.isEmpty())
		pIV = new CAbstractKey((byte*)IV.data(), IV.size(), KEY_256BIT);
	else
		pIV = new CAbstractKey((byte*)CHashFunction::Hash(QByteArray((char*)pPubKey->GetKey(), (int)pPubKey->GetSize()), CAbstractKey::eSHA256).data(), KEY_256BIT);

	if(!m_Exchange)
	{
		m_Exchange = new CKeyExchange(CAbstractKey::eECDH | CAbstractKey::eECP, "secp521r1");
		CScoped<CAbstractKey> pKey(m_Exchange->InitialsieKeyExchange());

		QVariantMap Data;
		Data["Key"] = pKey->ToByteArray();
		if(!IV.isEmpty())
		{
			m_Exchange->SetIV(new CAbstractKey(KEY_128BIT,true));
			Data["IV"] = m_Exchange->GetIV()->ToByteArray();
		}
		else
			m_Exchange->SetIV(new CAbstractKey((byte*)CHashFunction::Hash(QByteArray((char*)pKey->GetKey(), (int)pKey->GetSize()), CAbstractKey::eSHA256).data(), KEY_256BIT));
		Send("Secure", "Response", Data, m_Encoding, 0);
	}

	bool bRet;
	if(CAbstractKey* CryptoKey = m_Exchange->FinaliseKeyExchange(pPubKey))
	{
		//LogLine(LOG_SUCCESS, tr("Secure Key: ") + QByteArray((char*)CryptoKey->GetKey(), CryptoKey->GetSize()).toBase64().replace("=",""));
		bRet = true;

		m_CryptoKey = new CSymmetricKey(CAbstractKey::eAES | CAbstractKey::eCFB | CAbstractKey::eSHA256); // AES in CFB (Cipher feedback) Mode, and eSHA256 for key size adjustement
		m_CryptoKey->SetKey(CryptoKey->GetKey(), CryptoKey->GetSize());
		m_CryptoKey->SetupEncryption(m_Exchange->GetIV()->GetKey(), m_Exchange->GetIV()->GetSize());
		m_CryptoKey->SetupDecryption(pIV->GetKey(), pIV->GetSize());
		delete CryptoKey;
	}
	else
	{
		LogLine(LOG_ERROR, tr("Secure Failed"));
		bRet = false;
	}

	delete m_Exchange;
	m_Exchange = NULL;

	return bRet;
}

void CIPCSocket::SendLoginReq()
{
	QVariantMap Data;
	Data["User"] = m_User;
	Data["Password"] = m_Password;
	Send("Login", "Request", Data, m_Encoding, 0);
}

void CIPCSocket::ProcessLoginReq(const QVariantMap& Data)
{
	m_LoginToken.clear();
	if(CIPCServer* pServer = GetServer())
		m_LoginToken = pServer->CheckLogin(Data["User"].toString(), Data["Password"].toString());
	else
		LogLine(LOG_ERROR, tr("Client instance recived a login attempt"));

	if(!m_LoginToken.isEmpty())
	{
		m_bConnected = true;
		QVariantMap Data;
		Data["Token"] = m_LoginToken;
		Send("Login", "Response", Data, m_Encoding, 0);
	}
	else
	{
		LogLine(LOG_ERROR,tr("Login Attempt Failed!!!"));
		Send("Login", "Error", QVariant(), m_Encoding, 0);
	}
}

void CIPCSocket::ProcessLoginRes(const QVariantMap& Data)
{
	m_LoginToken = Data["Token"].toString();
	m_bConnected = true;
	emit Connected();
}

QByteArray CIPCSocket::Variant2String(const QVariant& Variant, EEncoding Encoding)
{
	switch(Encoding)
	{
		case eJson:
		{
#ifdef QT_JSON
			QJsonDocument doc(QJsonValue::fromVariant(Variant).toObject());
			return doc.toJson();
#else
			QJson::Serializer json;
			QByteArray strJson = json.serialize(Variant);
			if(strJson.isEmpty())
			{
				ASSERT(0);
				return "Error";
			}
			return strJson;
#endif
		}
		case eXML:
		{
			return CXml::Serialize(Variant).toUtf8();
		}
		case eBencode:
		{
			QByteArray strBencode = Bencoder::encode(Variant, false).buffer();
			return strBencode;
		}
		case eBinary:
		{
			QByteArray strBin;
			QDataStream Stream(&strBin, QIODevice::ReadWrite);
			Stream << Variant;
			return strBin;
		}
		default: 
		{
			ASSERT(0);
			return "Error";
		}
	}
}

QVariant CIPCSocket::String2Variant(const QByteArray& String, EEncoding& Encoding)
{
	if(Encoding == eUnknown)
	{
		if(String.left(1) == "{" || String.left(1) == "[")
			Encoding = eJson;
		else if(String.left(1) == "<")
			Encoding = eXML;
		else
			Encoding = eBencode;
	}

	switch(Encoding)
	{
		case eJson:
		{
#ifdef QT_JSON
			QJsonDocument doc = QJsonDocument::fromJson(String);
			return doc.toVariant();
#else
			QJson::Parser json;
			bool ok;
			QVariant Variant = json.parse (String, &ok);
			return Variant;
#endif
		}
		case eXML:
		{
			return CXml::Parse(QString::fromUtf8(String));
		}
		case eBencode:
		{
			Bdecoder Decoder(String);
			QVariantMap Variant;
			Decoder.read(&Variant);
			if (Decoder.error())
			{
				ASSERT(0);
				return QVariant();
			}
			return Variant;
		}
		case eBinary:
		{
			QVariant Variant;
			QDataStream Stream((QByteArray*)&String, QIODevice::ReadWrite);
			Stream >> Variant;
			return Variant;
		}
		default: 
		{
			ASSERT(0);
			return QVariant();
		}
	}
}
