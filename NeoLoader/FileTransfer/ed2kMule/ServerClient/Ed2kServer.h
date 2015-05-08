#pragma once
//#include "GlobalHeader.h"

#include "../../../../Framework/ObjectEx.h"
#include "../../../../Framework/Address.h"
#include "../../../../Framework/Buffer.h"
#include "../MuleTags.h"
#include "../../Transfer.h"

class CServerClient;
class CFile;

class CEd2kServer : public QObjectEx
{
    Q_OBJECT

public:
	CEd2kServer(QObject* qObject = 0);

	void					Process(UINT Tick);

	void					SetAddress(const CAddress& IP, uint16 uPort)	{m_Address.SetIP(IP); m_Port = uPort;}
	const SAddressCombo&	GetAddress()									{return m_Address;}
	CAddress				GetIP()											{return m_Address.GetIP();}
	uint16					GetPort()										{return m_Port;}
	QString					GetUrl()										{return QString("ed2k://|server|%1|%2|/").arg(GetAddress().GetIP().ToQString()).arg(GetPort());}

	void					SetCryptoPort(uint16 CryptoPort)				{m_CryptoPort = CryptoPort;}
	uint16					GetCryptoPort()									{return m_CryptoPort ? m_CryptoPort : m_Port;}

	uint16					GetUDPPort()									{return m_UDPKey ? m_UDPPort : m_Port + 4;}
	void					SetUDPPort(uint16 UDPPort, uint32 UDPKey)		{m_UDPPort = UDPPort; m_UDPKey = UDPKey;}
	uint32					GetUDPKey()										{return m_UDPKey;}

	void					SetChallenge(uint32 Challenge)					{m_Challenge = Challenge;}
	uint32					GetChallenge()									{return m_Challenge;}

	void					SetFlagsTCP(uint32 uFlags)						{m_FlagsTCP.Bits = uFlags;}
	void					SetFlagsUDP(uint32 uFlags)						{m_FlagsUDP.Bits = uFlags;}


	CServerClient*			GetClient()										{return m_pClient;}

	bool					SupportsCompression()							{return m_FlagsTCP.Fields.uCompression;}

	bool					SupportsObfuscationTCP()						{return m_FlagsTCP.Fields.uTcpObfuscation || m_FlagsUDP.Fields.uTcpObfuscation;}
	bool					SupportsObfuscationUDP()						{return m_FlagsUDP.Fields.uUdpObfuscation;}
	bool					SupportsNatTraversal()							{return m_FlagsTCP.Fields.uNatTraversal || m_FlagsUDP.Fields.uNatTraversal;}

	bool					SupportsExtGetSources()							{return m_FlagsUDP.Fields.uExtGetSources;}
	bool					SupportsExtGetSources2()						{return m_FlagsUDP.Fields.uExtGetSources2;}
	bool					SupportsExtGetFiles()							{return m_FlagsUDP.Fields.uExtGetFiles;}
	bool					SupportsLargeFiles()							{return m_FlagsUDP.Fields.uLargeFiles || m_FlagsTCP.Fields.uLargeFiles;}
	//bool					SupportsUnicode()								{return m_FlagsUDP.Fields.uUnicode || m_FlagsTCP.Fields.uUnicode;}
	bool					SupportsNewTags()								{return m_FlagsUDP.Fields.uNewTags || m_FlagsTCP.Fields.uNewTags;}

	void					SetLimits(uint32 SoftLimit, uint32 HardLimit)	{m_SoftLimit = SoftLimit; m_HardLimit = HardLimit;}
	uint32					GetSoftLimit()									{return m_SoftLimit;}
	uint32					GetHardLimit()									{return m_HardLimit;}

	void					SetFileCount(uint32 FileCount)					{m_FileCount = FileCount;}
	uint32					GetFileCount()									{return m_FileCount;}

	void					SetUserCount(uint32 UserCount, uint32 LowIDCount) {m_UserCount = UserCount; m_LowIDCount = LowIDCount;}
	void					SetUserCount(uint32 UserCount)					{m_UserCount = UserCount;}
	uint32					GetUserCount()									{return m_UserCount;}
	uint32					GetLowIDCount()									{return m_UserCount;}

	void					SetUserLimit(uint32 UserLimit)					{m_UserLimit = UserLimit;}
	uint32					GetUserLimit()									{return m_UserLimit;}

	uint64					GetLastConnectAttempt()							{return m_LastConnectAttempt;}
	int						GetConnectionFails()							{return m_ConnectFailCount;}
	uint64					GetLastDisconnected()							{return m_LastDisconnected;}

	QString					GetStatusStr();

	uint64					GetLastPingAnswer()								{return m_LastPingAnswer;}
	void					SetLastPingAnswer()								{m_LastPingAttempt = 0; m_PingAttemptCount = 0; m_LastPingAnswer = GetCurTick();}
	uint64					GetLastPingAttempt()							{return m_LastPingAttempt;}
	void					SetLastPingAttempt()							{m_PingAttemptCount ++; m_LastPingAttempt = GetCurTick();}
	int						GetPingAttempts()								{return m_PingAttemptCount;}

	void					SetStatic(bool Static)							{m_bStatic = Static;}
	bool					IsStatic()										{return m_bStatic;}
	void					UpdateTimeOut();
	bool					HasTimmedOut()									{return m_uTimeOut < GetCurTick();}

	const QString&			GetName()										{return m_Name;}
	const void				SetName(const QString& Name)					{m_Name = Name;}
	const QString&			GetVersion()									{return m_Version;}
	const void				SetVersion(const QString& Version)				{m_Version = Version;}
	const QString&			GetDescription()								{return m_Description;}
	const void				SetDescription(const QString& Description)		{m_Description = Description;}

	void					AddSources(CBuffer& Packet, bool bWithObfu);
	CFile*					ReadFile(CBuffer& Packet);
	void					WriteFile(CFile* pFile, CBuffer& Packet);


	struct SFile
	{
		SFile() : bPulished(0), uNextReask(0) {}
		bool	bPulished;
		uint64	uNextReask;
	};

	QList<CFile*>			GetFiles()										{return m_Files.keys();}
	void					AddFile(CFile* pFile)							{m_Files.insert(pFile, SFile());}
	SFile*					GetFile(CFile* pFile)							{return m_Files.contains(pFile) ? &m_Files[pFile] : NULL;}
	void					RemoveFile(CFile* pFile)						{m_Files.remove(pFile);}
	void					RemoveAllFiles()								{ m_Files.clear(); }

	uint32					GetNextRequestFrame()							{return m_NextRequestFrame;}

public slots:
	bool					Connect();
	void					Disconnect();

private slots:
	void					OnConnected();
	void					OnDisconnected();

protected:
	SAddressCombo			m_Address;
	uint16					m_Port;
	uint16					m_CryptoPort;
	uint16					m_UDPPort;
	uint32					m_UDPKey;

	uint32					m_Challenge;
	USvrFlagsTCP			m_FlagsTCP;
	USvrFlagsUDP			m_FlagsUDP;

	uint32					m_SoftLimit;
	uint32					m_HardLimit;
	uint32					m_FileCount;

	uint32					m_UserLimit;
	uint32					m_LowIDCount;
	uint32					m_UserCount;

	CServerClient*			m_pClient;

	uint64					m_LastConnectAttempt;
	int						m_ConnectFailCount;
	uint64					m_LastDisconnected;

	uint64					m_LastPingAnswer;
	uint64					m_LastPingAttempt;
	int						m_PingAttemptCount;

	bool					m_bStatic;
	uint64					m_uTimeOut;

	QString					m_Name;
	QString					m_Version;
	QString					m_Description;

	// Note: This list is _NOT_ guaranteed to hold valid pointers !!!
	QMap<CFile*, SFile>		m_Files;
	uint32					m_NextRequestFrame;
};
