#pragma once
#include "../Common/Dialog.h"

class CHosterUploader : public QDialogEx
{
	Q_OBJECT
public:
	CHosterUploader(bool bSelectAction = true, QWidget *parent = 0);
    ~CHosterUploader();

	QStringList			GetHosters();
	QString				GetAction()	{return m_pUploadMode ? m_pUploadMode->itemData(m_pUploadMode->currentIndex()).toString() : "";}

protected:
	QGridLayout*		m_pMainLayout;
	QTreeWidget*		m_pUploadTree;
	QComboBox*			m_pUploadMode;
	QDialogButtonBox*	m_pButtonBox;
	
};