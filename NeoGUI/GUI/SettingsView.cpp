#include "GlobalHeader.h"
#include "SettingsView.h"
#include "../NeoGUI.h"

CSettingsView::CSettingsView(QMultiMap<int, SField> Layout, const QString& Title, QWidget *parent)
:QWidget(parent)
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);

//#define OPT_TREE

#ifdef OPT_TREE
	QTreeWidget* pTree = new QTreeWidget();
	pTree->setColumnCount(2);
	pTree->headerItem()->setHidden(true); 
	pTree->setSelectionMode(QAbstractItemView::NoSelection);
#else
	QMap<int, QFormLayout*> SettingsLayout;
#endif

	foreach(int i, Layout.uniqueKeys())
	{
		foreach(const SField& Field, Layout.values(i))
		{
			if(Field.Role == QFormLayout::SpanningRole)
			{
				QFont Font = Field.pWidget->font();
				Font.setBold(true);
				Field.pWidget->setFont(Font);
			}

#ifdef OPT_TREE
			while (pTree->topLevelItemCount() <= i)
				pTree->addTopLevelItem(new QTreeWidgetItem());

			if (Field.Role == QFormLayout::LabelRole)
			{
				if (QLabel* pLabel = qobject_cast<QLabel*>(Field.pWidget))
					pTree->topLevelItem(i)->setText(0, pLabel->text());
				else
					pTree->setItemWidget(pTree->topLevelItem(i), 0, Field.pWidget);
			}
			else //if (Field.Role == QFormLayout::FieldRole)
			{
				pTree->setItemWidget(pTree->topLevelItem(i), 1, Field.pWidget);
			}
#else
			int j = i / 100;
			QFormLayout*& pSettingsLayout = SettingsLayout[j];
			if (pSettingsLayout == NULL)
			{
				pSettingsLayout = new QFormLayout();
				pSettingsLayout->setLabelAlignment(Qt::AlignRight);
			}
			pSettingsLayout->setWidget(i - j, Field.Role, Field.pWidget);
#endif
				

			if(!Field.Setting.isEmpty())
			{
				m_OptionsKeys.append(Field.Setting);
				m_Settings.insert(Field.Setting, Field.pWidget);
				if(Field.Enabler)
				{
					m_Enablers[Field.pWidget] = Field.Enabler;
					if(!Field.Other.isEmpty())
						m_OptionsKeys.append(Field.Other);
				}
			}
		}
	}

#ifdef OPT_TREE
	pTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);

	m_pMainLayout->addWidget(pTree);
#else
	QWidget* pWidget = new QWidget();
	QHBoxLayout* pLayout = new QHBoxLayout();
	pLayout->setMargin(0);

	QMap<int, QGroupBox*> SettingsWidget;
	QStringList Titles = Title.split("|");
	foreach(int j, SettingsLayout.keys())
	{
		SettingsWidget[j] = new QGroupBox(Titles.count() > j ? Titles[j] : "");
		SettingsWidget[j]->setLayout(SettingsLayout[j]);
		pLayout->addWidget(SettingsWidget[j]);
	}

	if (SettingsLayout.count() > 1)
	{
		foreach(int j, SettingsLayout.keys())
			SettingsWidget[j]->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Expanding);

		QWidget* pWidget = new QWidget();
		pWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		pLayout->addWidget(pWidget);
	}

	pWidget->setLayout(pLayout);

	m_pMainLayout->addWidget(pWidget);
#endif

	setLayout(m_pMainLayout);
}

void CSettingsView::ShowSettings(const QList<uint64>& IDs)
{
	m_IDs = IDs;
	UpdateSettings();
}

#ifdef CRAWLER
void CSettingsView::ShowSite(const QString& SiteName)
{
	m_SiteName = SiteName;
	UpdateSettings();
}
#endif

void CSettingsView::UpdateSetting(QWidget* pWidget, const QVariant& Value)
{
	if(QNumEdit* pEdit = qobject_cast<QNumEdit*>(pWidget))
		pEdit->SetValue(Value.toInt());
	else if(QLineEdit* pEdit = qobject_cast<QLineEdit*>(pWidget))
		pEdit->setText(Value.toString());
	else if(QSpinBox* pEdit = qobject_cast<QSpinBox*>(pWidget))
		pEdit->setValue(Value.toInt());
	else if(CTxtEdit* pEdit = qobject_cast<CTxtEdit*>(pWidget))
		pEdit->SetText(Value.toString());
	else if(CAnonSlider* pAnon = qobject_cast<CAnonSlider*>(pWidget))
		pAnon->GetSlider()->setValue(Value.toInt());
	else if(QSortOptions* pCombo = qobject_cast<QSortOptions*>(pWidget))
		pCombo->SetValue(Value.toString());
	else if(QComboBox* pCombo = qobject_cast<QComboBox*>(pWidget))
	{
		if(pCombo->isEditable())
		{
			int Index = pCombo->findText(Value.toString());
			if(Index != -1)
				pCombo->setCurrentIndex(Index);
			else
				pCombo->setCurrentText(Value.toString());
		}
		else
		{
			int Index = pCombo->findData(Value.toString());
			pCombo->setCurrentIndex(Index != -1 ? Index : 0);
		}
	}
	else if(QCheckBoxEx* pCheck = qobject_cast<QCheckBoxEx*>(pWidget))
		pCheck->SetState(Value.toInt());
	else if(QCheckBox* pCheck = qobject_cast<QCheckBox*>(pWidget))
	{
		if(pCheck->isTristate())
			pCheck->setCheckState((Qt::CheckState)Value.toInt());
		else
			pCheck->setChecked(Value.toBool());
	}
	else if(CMultiLineEdit* pMultiLineEdit = qobject_cast<CMultiLineEdit*>(pWidget))
		pMultiLineEdit->SetLines(Value.toStringList());
	else {
		ASSERT(0);}
}

class CSettingsUpdateJob: public CInterfaceJob
{
public:
	CSettingsUpdateJob(CSettingsView* pView, const QStringList& Keys, uint64 FileID = 0)
	{
		m_pView = pView;
		m_Request["Options"] = Keys;

		if(FileID)
		{
			m_Command = "GetFile";
			m_Request["ID"] = FileID;
		}
#ifdef CRAWLER
		else if(!pView->m_SiteName.isEmpty())
		{
			m_Command = "GetCrawler";
			m_Request["SiteName"] = pView->m_SiteName;
		}
#endif
		else
			m_Command = "GetCore";
	}

	virtual QString			GetCommand()	{return m_Command;}
	virtual void			HandleResponse(const QVariantMap& Response) 
	{
		if(m_pView)
		{
			QVariantMap Options = Response["Options"].toMap();
			foreach(const QString& Key, Options.keys())
			{
				if(QWidget* pWidget = m_pView->m_Settings.value(Key))
					CSettingsView::UpdateSetting(pWidget, Options[Key]);
			}

			foreach(QWidget* pWidget, m_pView->m_Enablers.keys())
			{
				if(SettingEnabler Enabler = m_pView->m_Enablers.value(pWidget))
					pWidget->setEnabled(Enabler(Options));
			}
		}
	}

	virtual void			Finish(bool bOK)
	{
		if(m_pView)
		{
			m_pView->setEnabled(bOK);
			if(!bOK)
				QTimer::singleShot(250,m_pView,SLOT(UpdateSettings()));
		}
		CInterfaceJob::Finish(bOK); // delete this
	}

protected:
	QString					m_Command;
	QPointer<CSettingsView>	m_pView; // Note: this can be deleted at any time
};

void CSettingsView::UpdateSettings()
{
	setEnabled(false);

	if(m_IDs.isEmpty())
	{
		CSettingsUpdateJob* pSettingsUpdateJob = new CSettingsUpdateJob(this, m_OptionsKeys);
		theGUI->ScheduleJob(pSettingsUpdateJob);
	}
	else
	{
		// G-ToDo: load all fils and display a Settings composite, also appl only changed values
		CSettingsUpdateJob* pSettingsUpdateJob = new CSettingsUpdateJob(this, m_OptionsKeys, m_IDs.first());
		theGUI->ScheduleJob(pSettingsUpdateJob);
	}
}

QVariant CSettingsView::ApplySetting(QWidget* pWidget)
{
	QVariant Value;
	if(QNumEdit* pEdit = qobject_cast<QNumEdit*>(pWidget))
		Value = pEdit->GetValue();
	else if(QLineEdit* pEdit = qobject_cast<QLineEdit*>(pWidget))
		Value = pEdit->text();
	else if(QSpinBox* pEdit = qobject_cast<QSpinBox*>(pWidget))
		Value = pEdit->value();
	else if(CTxtEdit* pEdit = qobject_cast<CTxtEdit*>(pWidget))
		Value = pEdit->GetText();
	else if(CAnonSlider* pAnon = qobject_cast<CAnonSlider*>(pWidget))
		Value = pAnon->GetSlider()->value();
	else if(QSortOptions* pCombo = qobject_cast<QSortOptions*>(pWidget))
		Value = pCombo->GetValue();
	else if(QComboBox* pCombo = qobject_cast<QComboBox*>(pWidget))
	{
		if(pCombo->isEditable())
			Value = pCombo->currentText();
		else
			Value = pCombo->itemData(pCombo->currentIndex());
	}
	else if(QCheckBoxEx* pCheck = qobject_cast<QCheckBoxEx*>(pWidget))
		Value = pCheck->GetState();
	else if(QCheckBox* pCheck = qobject_cast<QCheckBox*>(pWidget))
	{
		if(pCheck->isTristate())
			Value = pCheck->checkState();
		else
			Value = pCheck->isChecked();
	}
	else if(CMultiLineEdit* pMultiLineEdit = qobject_cast<CMultiLineEdit*>(pWidget))
		Value = pMultiLineEdit->GetLines();
	else {
		ASSERT(0);}
	return Value;
}

class CSettingsApplyJob: public CInterfaceJob
{
public:
	CSettingsApplyJob(CSettingsView* pView, const QStringList& Keys, uint64 FileID = 0)
	{
		if(FileID)
		{
			m_Command = "SetFile";
			m_Request["ID"] = FileID;
		}
#ifdef CRAWLER
		else if(!pView->m_SiteName.isEmpty())
		{
			m_Command = "SetCrawler";
			m_Request["SiteName"] = pView->m_SiteName;
		}
#endif
		else
			m_Command = "SetCore";

		QVariantMap Options;
		foreach(const QString& Key, Keys)
		{
			if(QWidget* pWidget = pView->m_Settings.value(Key))
				Options[Key] = CSettingsView::ApplySetting(pWidget);
		}
		m_Request["Options"] = Options;
	}

	virtual QString			GetCommand()	{return m_Command;}
	virtual void			HandleResponse(const QVariantMap& Response) 
	{
		if(m_pView) // reload Settings to check if values ware applyed proeprly
			m_pView->UpdateSettings();
	}
protected:
	QString					m_Command;
	QPointer<CSettingsView>	m_pView; // Note: this can be deleted at any time
};

void CSettingsView::ApplySetting(const QStringList& Keys, uint64 FileID)
{
	CSettingsApplyJob* pSettingsApplyJob = new CSettingsApplyJob(this, Keys, FileID);
	theGUI->ScheduleJob(pSettingsApplyJob);
}

void CSettingsView::ApplySettings()
{
	if(!isEnabled())
		return;

	if(m_IDs.isEmpty())
		ApplySetting(m_Settings.keys());
	else
	{
		foreach(uint64 FileID, m_IDs)
			ApplySetting(m_Settings.keys(), FileID);
	}
}

///////////////////////////////////////////////////
//

CSettingsViewEx::CSettingsViewEx(QMultiMap<int, SField> Layout, const QString& Title, const QStringList& Guard, QWidget *parent)
:CSettingsView(Layout, Title, parent)
{
	foreach(const QString& Key, Guard)
		m_Guard[Key] = QVariant();
}

void CSettingsViewEx::UpdateSettings()
{
	foreach(const QString& Key, m_Settings.keys())
	{
		if(QWidget* pWidget = m_Settings.value(Key))
		{
			QVariant Value = theGUI->Cfg()->GetSetting(Split2(Key, "/").second);
			CSettingsView::UpdateSetting(pWidget, Value);

			if(m_Guard.contains(Key))
				m_Guard[Key] = Value;
		}
	}
}

void CSettingsViewEx::ApplySettings()
{
	bool bReset = false;
	foreach(const QString& Key, m_Settings.keys())
	{
		if(QWidget* pWidget = m_Settings.value(Key))
		{
			QVariant Value = CSettingsView::ApplySetting(pWidget);
			theGUI->Cfg()->SetSetting(Split2(Key, "/").second, Value);

			if(m_Guard.contains(Key) && m_Guard[Key] != Value)
				bReset = true;
		}
	}

	if(theGUI->Cfg()->GetString("Core/Mode") != "Unified") // copy to separated core
		CSettingsView::ApplySettings();

	if(bReset)
		QTimer::singleShot(0, theGUI, SLOT(DoReset()));
}

