#pragma once

#include "./NeoHelper/neohelper_global.h"

#include <QSettings>
#include <QVariant>
#include <QMutex>
#include "Types.h"
#include <QStringList>

class CSettings;

class NEOHELPER_EXPORT CSettings: public QSettings
{
	Q_OBJECT

public:
	struct SStrRef
	{
		SStrRef(const char* pRef)
		 : Ref(pRef) {}

		bool operator < (const SStrRef& Other) const {return strcmp(Ref, Other.Ref) < 0;}
		bool operator == (const SStrRef& Other) const {return strcmp(Ref, Other.Ref) == 0;}

		operator QString() const {return QString(Ref);}

		const char* Ref;
	};

	struct SSetting
	{
		SSetting(){
			MinValue = 0;MaxValue = 0;
		}

		SSetting(const QVariant& value){
			Value = value;
			MinValue = 0; MaxValue = 0;
		}

		SSetting(qint64 value, qint64 minvalue = LLONG_MIN, qint64 maxvalue = LLONG_MAX) {
			Value = value;
			MinValue = minvalue; MaxValue = maxvalue;
		}

		SSetting(const QString& value, const QStringList& values) {
			Value = value;
			MinValue = 0; MaxValue = 0;
			Values = values;
		}

		bool Check(const QVariant& value) const
		{
			if(MinValue != MaxValue)
			{
				if(!value.canConvert(QVariant::LongLong))
					return false;
				qint64 Test = value.toLongLong();
				if(Test < MinValue || MinValue > MaxValue)
					return false;
			}
			else if(!Values.isEmpty())
			{
				if(!value.canConvert(QVariant::String))
					return false;
				if(!Values.contains(value.toString()))
					return false;
			}
			return true;
		}

		virtual bool IsBlob() const {return false;}

		QVariant Value;
		qint64 MinValue;
		qint64 MaxValue;
		QStringList Values;
	};

	struct SBlobSetting: SSetting
	{
		SBlobSetting(const QByteArray& value){
			Value = value;
			MinValue = 0;MaxValue = 0;
		}
		virtual bool IsBlob() const {return true;}
	};

	static void			InitSettingsEnvironment(const QString& Orga, const QString& Name, const QString& Domain);

	CSettings(const QString& Name, QMap<QString, SSetting> DefaultValues, QObject* qObject = NULL);

	static QString		GetAppDir()									{return m_sAppDir;}
	static QString		GetSettingsDir()							{return m_sConfigDir;} 
	static bool			IsPortable()								{return m_bPortable;}

	bool				SetSetting(const QString& key, const QVariant& value);
	QVariant			GetSetting(const QString& key);

	void				SetBlob(const QString& key, const QByteArray& value);
	QByteArray			GetBlob(const QString& key);

#define IMPL_CFG_CACHE_GET(x,y,z) \
	x					Get##y(const SStrRef& key) \
	{ \
		QMutexLocker Locker(&m_Mutex); \
		QMap<SStrRef, x>::Iterator I =  m_Cache_##x.find(key); \
		if(I != m_Cache_##x.end()) \
			return I.value(); \
		Locker.unlock(); \
		x val = GetSetting(key).to##z(); \
		Locker.relock(); \
		m_Cache_##x.insert(key, val); \
		return val; \
	}
	IMPL_CFG_CACHE_GET(bool, Bool, Bool);
	IMPL_CFG_CACHE_GET(qint32, Int, Int);
	IMPL_CFG_CACHE_GET(quint32, UInt, UInt);
	IMPL_CFG_CACHE_GET(quint64, UInt64, ULongLong);
#undef IMPL_CFG_CACHE_GET

	const QString		GetString(const QString& key)				{return GetSetting(key).toString();}
	const QStringList	GetStringList(const QString& key)			{return GetSetting(key).toStringList();}

	const QStringList 	ListSettings()								{QMutexLocker Locker(&m_Mutex); return QSettings::allKeys();}
	const QStringList 	ListGroupes()								{QMutexLocker Locker(&m_Mutex); return QSettings::childGroups();}
	const QStringList 	ListKeys(const QString& Root);

protected:
	QMutex				m_Mutex;
	QString				m_sName;
	QMap<QString, SSetting> m_DefaultValues;

#define IMPL_CFG_CACHE(x) \
	QMap<SStrRef, x>		m_Cache_##x;
	IMPL_CFG_CACHE(bool)
	IMPL_CFG_CACHE(qint32)
	IMPL_CFG_CACHE(quint32)
	IMPL_CFG_CACHE(quint64)
#undef IMPL_CFG_CACHE

	static void			InitInstalled();

	static QString		m_sAppDir;
	static QString		m_sConfigDir;
	static bool			m_bPortable;
};
