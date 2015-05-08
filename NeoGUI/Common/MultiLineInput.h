#pragma once
#include "Dialog.h"

class CMultiLineInput: public QDialogEx
{
public:
	CMultiLineInput(QWidget* parent = 0)
		: QDialogEx(parent)
	{
		QGridLayout* m_pMainLayout = new QGridLayout(this);
 
		m_pLabel = new QLabel();
		m_pMainLayout->addWidget(m_pLabel, 0, 0, 1, 1);

		m_TextEdit = new QPlainTextEdit();
		m_pMainLayout->addWidget(m_TextEdit, 1, 0, 1, 1);

		m_pButtonBox = new QDialogButtonBox();
		m_pButtonBox->setOrientation(Qt::Horizontal);
		m_pButtonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);
		m_pMainLayout->addWidget(m_pButtonBox, 2, 0, 1, 1);
 
		connect(m_pButtonBox,SIGNAL(accepted()),this,SLOT(accept()));
		connect(m_pButtonBox,SIGNAL(rejected()),this,SLOT(reject()));
	}

	static QString GetInput(QWidget* parent, const QString& Prompt, const QString& Default = "", bool* bOK = NULL)
	{
		CMultiLineInput MultiLineInput(parent);
		MultiLineInput.m_pLabel->setText(Prompt);
		MultiLineInput.m_TextEdit->setPlainText(Default);
		if(!MultiLineInput.exec())
		{
			if(bOK)
				*bOK = false;
			return "";
		}

		if(bOK)
			*bOK = true;
		return MultiLineInput.m_TextEdit->toPlainText();
	}

protected:
	QGridLayout*		m_pMainLayout;
	QLabel*				m_pLabel;
	QPlainTextEdit*		m_TextEdit;
	QDialogButtonBox *	m_pButtonBox;
};