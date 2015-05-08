#pragma once

#if QT_VERSION < 0x050000
#include <QtGui>
#else
#include <QWidget>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QFormLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QSplitter>
#include <QHeaderView>
#include <QApplication>
#include <QMessageBox>
#include <QInputDialog>
#include <QCloseEvent>
#include <QComboBox>
#include <QStackedLayout>
#include <QDialogButtonBox>
#endif
#include "Common/Dialog.h"

class CConnector : public QDialogEx
{	
	Q_OBJECT

public:
	CConnector(QWidget *pMainWindow = 0);
	~CConnector();

private slots:
	void				OnModeChanged(int Index);
	void				OnConnect();
	void				OnClicked(QAbstractButton* pButton);

protected:
	void				timerEvent(QTimerEvent *e);
	void				closeEvent(QCloseEvent *e);

	void				Load();
	void				Apply();

	int					m_uTimerID;
	int					m_Mode;

private:
	QFormLayout*		m_pMainLayout;
	
	QLabel*				m_pLabel;

	QComboBox*			m_pMode;

	QLineEdit*			m_pPassword;

	QLineEdit*			m_pPipeName;

	QLineEdit*			m_pHostPort;
	QLineEdit*			m_pHostName;

	QComboBox*			m_pAutoConnect;

	/*QPushButton*		m_pServiceBtn;
	QLineEdit*			m_pServiceName;*/

	//QPushButton*		m_pStartBtn;
	QPushButton*		m_pConnectBtn;

	QDialogButtonBox*	m_pButtonBox;
};
