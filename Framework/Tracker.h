#pragma once

#include "./NeoHelper/neohelper_global.h"

#include <QMutex>
#include "Types.h"

class NEOHELPER_EXPORT QTracker
{
public:
	void					TrackMe(const QObject* Object)	{QMutexLocker Locker(&m_Mutex); m_Objects.append(Object);}
	void					LoseMe(const QObject* Object)	{QMutexLocker Locker(&m_Mutex); m_Objects.removeOne(Object);}

	QString					Summary();
	QString					Report();

protected:
	QString					Report(const QObject* Object, int Level);

	static	QString			NameOf(const QObject* Object)	{return Object->metaObject()->className();}
	//static	QString		NameOf(const QObject* Object)	{return Object->objectName();}

	QMutex					m_Mutex;
	QList<const QObject*>	m_Objects;
};

extern NEOHELPER_EXPORT QTracker theTracker;

template <class Q>
class QTracked: public Q
{
public:
	QTracked(Q* parent = NULL) : Q(parent)	{theTracker.TrackMe(this);}
	~QTracked()								{theTracker.LoseMe(this);}
};
