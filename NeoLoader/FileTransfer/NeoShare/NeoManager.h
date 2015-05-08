#pragma once

#include "../../../Framework/ObjectEx.h"
#include "NeoKad.h"
#include "../Transfer.h"
class CNeoClient;
class CFile;
class CNeoRoute;
class CNeoSession;
class CFileHash;
struct SNeoEntity;
class CBandwidthLimit;


class CNeoManager: public QObjectEx
{
	Q_OBJECT

public:
	CNeoManager(QObject* qObject = NULL);
	~CNeoManager();

	void							Process();

	void							Process(UINT Tick);

	void							AddConnection(CNeoClient* pClient, bool bPending = false);
    void							RemoveConnection(CNeoClient* pClient);
	const QList<CNeoClient*>&		GetClients() {return m_Clients;}

	void							AddToFile(CFile* pFile, CFileHash* pHash, const SNeoEntity& Neo, EFoundBy FoundBy);
	bool							DispatchClient(CNeoClient* pClient, CFileHash* pHash);

	bool							IsItMe(const SNeoEntity& Neo);
	QByteArray						GetRouteTarget(const QByteArray& EntityID);
	void							RouteBroken(const QByteArray& EntityID);

	CNeoSession*					OpenSession(SNeoEntity& Neo);
	void							CloseSession(const SNeoEntity& Neo, CNeoSession* pSession);

	struct SSubFile
	{
		QString						FileName;
		uint64						FileSize;
		QString						FileDirectory;
		QList<CFileHashPtr>			FileHashes;
	};
	void							InstallMetaData(CFile* pFile, uint64 uFileSize, const QList<SSubFile>& SubFiles);

	void							AddAuxHashes(CFile* pFile, const QList<CFileHashPtr>& FileHashes);

	QVariantMap						WriteHashMap(const QList<CFileHashPtr>& Hashes);
	QList<CFileHashPtr>				ReadHashMap(const QVariantMap& HashMap, uint64 uFileSize);

	QVariant						GetMetadataEntry(CFile* pFile, int Index);

	int								NextMetadataBlock(CFile* pFile, const QByteArray& ID);
	void							AddMetadataBlock(CFile* pFile, int Index, const QVariant& Entry, int EntryCount, const QByteArray& ID);
	void							ResetMetadataBlock(CFile* pFile, int Index, const QByteArray& ID);


	CNeoKad*						GetKad()									{return m_Kademlia;}

	CBandwidthLimit*				GetUpLimit()								{return m_UpLimit;}
	CBandwidthLimit*				GetDownLimit()								{return m_DownLimit;}

	int								GetSessionCount()							{return m_Clients.size();}

	QList<CNeoRoute*>				GetStaticRoutes();
	QList<CNeoRoute*>				GetAllRoutes()								{return m_Routes.values();}

	const QString&					GetVersion() const							{return m_Version;}

	const STransferStats&			GetStats() const							{return m_TransferStats;}

private slots:
	void							OnRouteBroken();
	void							OnConnection(CNeoSession* pSession);

public slots:
	void							OnBytesWritten(qint64 Bytes);
    void 							OnBytesReceived(qint64 Bytes);

protected:
	CNeoRoute*						SetupRoute(const QByteArray& TargetID, CPrivateKey* pEntityKey = NULL, bool bStatic = false);
	void							BreakRoute(const QByteArray& TargetID, const QByteArray& EntityID);

	
	bool							CompareSubFiles(CJoinedPartMap* pParts, const QList<SSubFile>& SubFiles);
	void							SetupSubFiles(CJoinedPartMap* pParts, CFile* pFile, const QList<SSubFile>& SubFiles);
	void							UpdateSubFiles(CJoinedPartMap* pParts, const QList<SSubFile>& SubFiles);

	QList<SSubFile>					TryAssemblyMetadata(CFile* pFile);
	bool							TryInstallMetadata(CFile* pFile);

	CNeoKad*						m_Kademlia;

	QMap<QByteArray, CNeoRoute*>	m_Routes;
	QList<CNeoClient*>				m_Clients;
	QList<CNeoClient*>				m_Pending;

	CBandwidthLimit*				m_UpLimit;
	CBandwidthLimit*				m_DownLimit;

	QString							m_Version;

	STransferStats					m_TransferStats;

	struct SMetadata
	{
		SMetadata() : EntryCount(0) {}
		int EntryCount; 
		QMap<int, QVariantMap>	Entrys;
	};
	struct SMetadataExchange
	{
		SMetadataExchange() : NewData(false) {}
		bool NewData;
		QMap<QByteArray, SMetadata> Metadata;
	};
	QMap<uint64, SMetadataExchange*>m_MetadataExchange;
};