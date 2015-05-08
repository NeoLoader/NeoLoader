#include "GlobalHeader.h"
#include "Tracker.h"

QTracker theTracker;


QString QTracker::Summary()
{
	QMutexLocker Locker(&m_Mutex); 

	QMap<QString,int> Objects;
	foreach(const QObject* Object, m_Objects)
		Objects[NameOf(Object)]++;

	QString Text;
	foreach(const QString Name, Objects.uniqueKeys())
		Text.append(Name + ": " + QString::number(Objects[Name]) + "\n");
	return Text;
}

QString QTracker::Report()
{
	QMutexLocker Locker(&m_Mutex); 

	QList<const QObject*> Roots;
	foreach(const QObject* Object, m_Objects)
	{
		for(;Object->parent();Object = Object->parent()); // go from the real
		if(!Roots.contains(Object))
			Roots.append(Object);
	}

	QString Text;
	foreach(const QObject* Object, Roots)
		Text.append(Report(Object,1));
	return Text;
}

QString QTracker::Report(const QObject* Object, int Level)
{
	QString Text = QString(Level, ' ') + NameOf(Object) + "\n";

	foreach(const QObject* SubObject, Object->children())
		Text.append(Report(SubObject, Level + 1));
	return Text;
}