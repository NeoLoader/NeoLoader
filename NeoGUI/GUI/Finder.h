#pragma once

class CFinder: public QWidget
{
	Q_OBJECT

public:
	CFinder(QWidget *parent = 0);
	~CFinder();

signals:
	void				SetFilter(const QRegExp& Exp);

public slots:
	void				Open();
	void				OnUpdate();
	void				Close();

private:

	QHBoxLayout*		m_pSearchLayout;

	QLineEdit*			m_pSearch;
	QCheckBox*			m_pCaseSensitive;
	QCheckBox*			m_pRegExp;
};