#pragma once

#include "../../../Framework/Address.h"
#include "../../../Framework/ObjectEx.h"
#include "../../../Framework/Cryptography/AsymmetricKey.h"

class CAbstractSearch;
class CFile;
class CFileHash;
#include <QSettings>
class CKadPublisher;

class CNeoKad: public QObjectEx
{
	Q_OBJECT

public:
	CNeoKad(QObject* qObject = NULL);
	~CNeoKad();

	void							Process(UINT Tick);

	bool							IsEnabled() const;
	enum EStatus
	{
		eDisconnected,
		eConnecting,
		eConnected
	};
	bool							IsConnected() const							{return m_KadStatus == eConnected;}
	bool							IsDisconnected() const						{return m_KadStatus == eDisconnected;}

	int								GetUpRate()									{return m_UpRate;}
	int								GetDownRate()								{return m_DownRate;}

	void							StartKad();
	void							StopKad();

	void							StartSearch(CAbstractSearch* pSearch);
	void							SyncSearch(CAbstractSearch* pSearch);
	void							StopSearch(CAbstractSearch* pSearch);

	void							ResetPub(CFile* pFile);
	bool							FindSources(CFile* pFile);
	bool							FindIndex(CFile* pFile);
	bool							FindRating(CFile* pFile);
	bool							IsFindingRating(CFile* pFile);

	enum EKadOp
	{
		eLookup,
		ePublish,
		eRoute
	};
	QVariantMap						GetLookupCfg(EKadOp Op = eLookup);
	QByteArray						GetCodeID(const QString& Script);
	QByteArray						GetStoreKey()								{return m_PrivateKey.ToByteArray();}

	uint16							GetPort()			{return m_Port;}
	const QByteArray&				GetNodeID() const	{return m_KadID;}

	QString							GetStatus(CFile* pFile = NULL, uint64* pNext = NULL) const;

	bool							IsFirewalled(CAddress::EAF eAF, bool bNAT = false) const;
	CAddress						GetAddress(CAddress::EAF eAF) const;

	CKadPublisher*					GetPublisher()		{return m_KadPublisher;}

protected:
	void							AddFileToSearch(QVariantMap File, CAbstractSearch* pSearch);
	void							SyncFiles();

	void							SyncLog();
	void							SyncScripts(const QVariantList ScriptList);

	uint64							m_uLastLog;

	EStatus							m_KadStatus;
	QByteArray						m_KadID;
	uint16							m_Port;
	CAddress						m_Address;
	int								m_FWStatus;
	CAddress						m_AddressV6;
	int								m_FWStatusV6;
	int								m_UpRate;
	int								m_DownRate;

	QSettings						m_Scripts;
	CPrivateKey						m_PrivateKey;

	QSet<CAbstractSearch*>			m_RunningSearches;

	struct SFile
	{
		SFile()
		{
			NextSourceSearch = 0;
			NextHashSearch = 0;
			NextIndexSearch = 0;
			NextAliasSearch = 0;
		}

		uint64			NextSourceSearch;
		uint64			NextHashSearch;
		uint64			NextIndexSearch;
		uint64			NextAliasSearch;

		uint64			FileID;
	};
	QMap<uint64, SFile*>			m_Files;

	CKadPublisher*					m_KadPublisher;
};

extern QRegExp g_kwrdSpliter;