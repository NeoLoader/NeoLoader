#pragma once

typedef bool(*SettingEnabler)(const QVariantMap&);

struct SField
{
	SField(QWidget*	widget, QFormLayout::ItemRole role, const QString& setting = "", SettingEnabler enabler = NULL, const QStringList& other = QStringList())
	: pWidget(widget), Role(role), Setting(setting), Enabler(enabler), Other(other) {}

	QWidget*				pWidget;
	QFormLayout::ItemRole	Role;
	QString					Setting;
	SettingEnabler			Enabler;
	QStringList				Other;
};

class CSettingsView: public QWidget
{
	Q_OBJECT

public:
	CSettingsView(QMultiMap<int, SField> Layout, const QString& Title = "", QWidget *parent = 0);

	void				ShowSettings(const QList<uint64>& IDs);

#ifdef CRAWLER
	void				ShowSite(const QString& SiteName);
#endif

	void 				ApplySetting(const QStringList& Keys, uint64 FileID = 0);

	static void			UpdateSetting(QWidget* pWidget, const QVariant& Value);
	static QVariant		ApplySetting(QWidget* pWidget);

public slots:
	virtual void		UpdateSettings();
	virtual void		ApplySettings();

protected:
	friend class CSettingsUpdateJob;
	friend class CSettingsApplyJob;

	QList<uint64>		m_IDs;
#ifdef CRAWLER
	QString				m_SiteName;
#endif

	QVBoxLayout*		m_pMainLayout;

	QStringList			m_OptionsKeys;
	QMap<QString, QWidget*> m_Settings;
	QMap<QWidget*, SettingEnabler> m_Enablers;
};

///////////////////////////////////////////////////
//

class CSettingsViewEx : public CSettingsView
{
	Q_OBJECT

public:
	CSettingsViewEx(QMultiMap<int, SField> Layout, const QString& Title = "", const QStringList& Guard = QStringList(), QWidget *parent = 0);

public slots:
	virtual void		UpdateSettings();
	virtual void		ApplySettings();

protected:
	QVariantMap			m_Guard;
};

#include "SettingsWidgets.h"