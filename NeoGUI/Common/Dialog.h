#pragma once

class QDialogEx : public QDialog
{
	Q_OBJECT
public:
	QDialogEx(QWidget *parent = NULL);
	~QDialogEx();

	static int GetOpenCount() { return m_OpenCount; }

protected:
	static int m_OpenCount;
};