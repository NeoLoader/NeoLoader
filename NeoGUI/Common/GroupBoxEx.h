#pragma once

#include <QGroupBox>

template <class T>
class QGroupBoxEx: public QGroupBox
{
public:
	QGroupBoxEx(const QString& Name, QWidget* parent = 0) : QGroupBox(Name, parent) 
	{
		m_pLayout = new T();
		setLayout(m_pLayout);
	}

	T* layout()		{return m_pLayout;}

protected:
	T*	m_pLayout;
};