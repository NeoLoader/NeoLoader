#pragma once

#include "../../../Framework/ObjectEx.h"
#include "../../FileList/Hashing/FileHash.h"

class CPrivateKey;
class CFile;

class CMuleCollection: public QObjectEx
{
	Q_OBJECT

public:
	CMuleCollection(QObject* qObject = NULL);
	
	bool					LoadFromFile(const QString& FilePath);
	bool					LoadFromData(const QByteArray& Data, const QString& FileName = "");
	bool					SaveToFile(const QString& FilePath, CPrivateKey* pPrivKey = NULL);
	QByteArray				SaveToData(CPrivateKey* pPrivKey = NULL);
	bool					SaveToSimpleFile(const QString& FilePath);

	void					Populate(CFile* pFile);
	bool 					Import(CFile* pFile);

	const QString&			GetCollectionName() const				{return m_CollectionName;}
	void					SetCollectionName(const QString& Name)	{m_CollectionName = Name;}

	bool					IsSigned() const						{return !m_CreatorKey.isEmpty();}

	struct SFileInfo
	{
		SFileInfo() : FileSize(0) {}
		QString						FileName;
		qint64						FileSize;
		QByteArray					HashEd2k;
		QByteArray					HashAICH;

		QVariantMap					Properties;
	};

	void					AddFile(const SFileInfo& File);
	const QList<SFileInfo>&	GetFiles() const						{return m_Files;}

	void					SetProperty(const QString& Name, const QVariant& Value)		{if(Value.isValid()) m_Properties.insert(Name, Value); else m_Properties.remove(Name);}
	QVariant				GetProperty(const QString& Name)							{return m_Properties.value(Name);}
	bool					HasProperty(const QString& Name)							{return m_Properties.contains(Name);}
	//void					SetProperty(const QString& Name, const QVariant& Value)		{setProperty(Name.toLatin1(), Value);}
	//QVariant				GetProperty(const QString& Name)							{return property(Name.toLatin1());}
	QList<QString>			GetAllProperties()											{return m_Properties.uniqueKeys();}

protected:

	QString					m_CollectionName;
	QVariantMap				m_Properties;

	QList<SFileInfo>		m_Files;
	uint64					m_TotalLength;

	QByteArray				m_CreatorKey;
};