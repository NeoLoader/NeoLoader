#pragma once

#include<QObject>

class CCaffeinePrivate;

class CCaffeine: QObject
{
	Q_OBJECT

public:
	CCaffeine(QObject* parent = 0);
	~CCaffeine();

	void			Start();
	bool			IsRunning();
	void			Stop();

private:
	friend class CCaffeinePrivate;
    Q_DISABLE_COPY(CCaffeine)
    QScopedPointer<CCaffeinePrivate> d;
};