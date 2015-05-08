#pragma once
//#include "GlobalHeader.h"

#include "../FileList/FileList.h"

class CFile;
#ifndef NO_HOSTERS
class CLinkGrabber;
class CContainerDecoder;
#endif

class CFileGrabber: public CFileList
{
	Q_OBJECT

public:
	CFileGrabber(QObject* qObject = NULL);
	~CFileGrabber();

	virtual void				Process(UINT Tick);

	virtual void				AddFile(CFile* pFile);
	virtual uint64				GrabFile(CFile* pFile);
	virtual bool				AddFile(CFile* pFile, uint64 GrabberID);	
	virtual void				AssociateFile(CFile* pFile, uint64 GrabberID);	
	virtual uint64				FindGrabberID(CFile* pFile);

	virtual QStringList			SplitUris(QString sUris);
	virtual uint64				GrabUris(const QStringList& sUris);
	virtual bool				GrabUri(const QString& sUri, uint64 GrabberID = 0);
	virtual CFile*				AddMagnet(QString sUri, QString FileName = "");
	virtual CFile*				AddEd2k(QString sUri, QString FileName = "");
	enum ELinks
	{
		eNone,
		eArchive
	};
	enum EEncoding
	{
		ePlaintext,
		eEncrypted,
		eCompressed,
		eMagnet,
		eed2k,
		eTorrent,
		eMuleCollection
	};
	virtual QString				GetUri(CFile* pFile, ELinks Links, EEncoding Encoding);

	/*virtual uint64				GetCryptoID(uint64 FileID);
	virtual QByteArray			EncryptES(uint64 CryptoID, QStringList LinkList, time_t CreationTime);
	virtual int					DecryptES(CFile* pFile, QString es, time_t CreationTime);
	virtual int					DecryptES(CFile* pFile, uint64 CryptoID, QByteArray CryptoLinks, time_t CreationTime);
	virtual int					KeysPending(uint64 GrabberID);
	*/

	virtual QList<CFile*>		GetFiles(uint64 GrabberID = 0);
	virtual int					CountFiles(uint64 GrabberID = 0);
	virtual QList<uint64>		GetTaskIDs()								{return m_GrabList.keys();}
	virtual QStringList			GetTaskUris(uint64 GrabberID)				{if(m_GrabList.contains(GrabberID)) return m_GrabList.value(GrabberID)->Uris; return QStringList();}

	virtual void				Clear(bool bRemove, uint64 GrabberID = 0);

#ifndef NO_HOSTERS
	virtual CLinkGrabber*		GetLinkGrabber()							{return m_pLinkGrabber;}
	virtual CContainerDecoder*	GetContainerDecoder()						{return m_pContainerDecoder;}
#endif

	virtual void				DownloadPacket(const QString& Url, uint64 GrabebrID);

	static	QStringList			AllQueryItemValue(const QUrl& Url, const QString& key);

	// Load/Store
	void						StoreToFile();
	void						LoadFromFile();

public slots:
	virtual void				OnFinished();

protected:
	//virtual void				ProcessKeyCache();

#ifndef NO_HOSTERS
	CLinkGrabber*				m_pLinkGrabber;
	CContainerDecoder*			m_pContainerDecoder;
#endif

	struct SGrabTask
	{
		SGrabTask(const QStringList& uris) {Uris = uris;}
		QStringList		Uris;
		QSet<uint64>	Files;
	};

	QMap<uint64, SGrabTask*>	m_GrabList;

	/*struct SDecryptEntry
	{
		SDecryptEntry(uint64 ID, const QByteArray& es, time_t ct)
		{
			CryptoID = ID;
			CryptoLinks = es;
			CreationTime = ct;
		}
		uint64		CryptoID;
		QByteArray	CryptoLinks;
		time_t		CreationTime;
	};
	QMap<uint64, SDecryptEntry*>	m_DecryptCache;

	QMap<quint64, QByteArray>	m_KeyCache;	// Keeps crypto keys with thair respective ID's
	QMap<quint64, quint64>		m_IDCache;	// Keeps crypto ID for files, used for generatin links so that for a given file we always take the same ID
	QMap<quint64, quint64>		m_KeyOrders;// Pending Key Requests*/

	QMap<QNetworkReply*, time_t>m_PendingPackets;
};
