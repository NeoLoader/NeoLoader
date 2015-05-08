#pragma once

#include "../../../NeoScriptTools/JSDebugging/JSScriptDebugger.h"

class CResourceWidget;
class CSessionLogWidget;
class CScriptRepositoryDialog;
class CJSScriptDebuggerFrontend;
#include <QFileDialog>
#include <QComboBox>
#include <QHBoxLayout>
#include <QPushButton>

class CNeoScriptDebugger: public CJSScriptDebugger
{
    Q_OBJECT
public:
#if QT_VERSION < 0x050000
    CNeoScriptDebugger(QWidget *parent = 0, Qt::WFlags flags = 0);
#else
    CNeoScriptDebugger(QWidget *parent = 0, Qt::WindowFlags flags = 0);
#endif
    ~CNeoScriptDebugger();

	virtual void attachTo(CJSScriptDebuggerFrontend *frontend);

	CJSScriptDebuggerFrontend* frontend();

private slots:
	void UpdateResource(qint64 ID);
	void ShowResource(qint64 ID);
	void ProcessCustom(const QVariant& var);

	void OnNew();
	void OnLoad();
	void OnSave();
	void OnSaveAs();
	void OnDelete();
	void OnClear();

	void OnDefault();
	void OnHtmlCode();
	void OnRawTrace();
	void OnDomTree();
	
	void OnCopyUrl();

	void OnSetInterceptor();
	void OnClearInterceptor();

	void SaveScript(const QString& FileName, const QString& Script = QString());
	void LoadScript(const QString& FileName);
	void DeleteScript(const QString& FileName);

	void SetInterceptor(bool bSet);

protected:
	void timerEvent(QTimerEvent *e);

	virtual void setup();

    virtual QToolBar *createStandardToolBar(QWidget *parent = 0);
    virtual QMenu *createStandardMenu(QWidget *parent = 0);
	virtual QMenu *createFileMenu(QWidget *parent = 0);
	virtual QMenu *createResourceMenu(QWidget *parent = 0);

    virtual QWidget *widget(int widget) const;
    virtual QAction *action(int action) const;

	int m_TimerId;

private:
	CResourceWidget* m_ResourceWidget;
	CSessionLogWidget* m_SessionLogWidget;
	CScriptRepositoryDialog* m_ScriptRepositoryDialog;

	QAction* m_NewAction;
	QAction* m_LoadAction;
	QAction* m_SaveAction;
	QAction* m_SaveAsAction;
	QAction* m_DeleteAction;
	QAction* m_ClearAction;

	QAction* m_DefaultAction;
	QAction* m_HtmlCodeAction;
	QAction* m_RawTraceAction;
	QAction* m_DomTreeAction;
	QAction* m_CopyUrlAction;
	QAction* m_HoldAction;
	QAction* m_SetInterceptorAction;
	QAction* m_ClearInterceptorAction;

	bool	m_WebViewAvailable;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//


class CPathCombo: public QWidget
{
	Q_OBJECT
public:
	CPathCombo(bool bDirs = false, QWidget *parent = 0)
	 :QWidget(parent) 
	{
		m_bDirs = bDirs;

		QHBoxLayout* pLayout = new QHBoxLayout(this);
		pLayout->setMargin(0);
		m_pCombo = new QComboBox(this);
		m_pCombo->setEditable(true);
		connect(m_pCombo, SIGNAL(textChanged(const QString &)), this, SIGNAL(textChanged(const QString &)));
		pLayout->addWidget(m_pCombo);
		QPushButton* pButton = new QPushButton("...");
		pButton->setMaximumWidth(25);
		connect(pButton, SIGNAL(pressed()), this, SLOT(Browse()));
		pLayout->addWidget(pButton);
	}

	QComboBox*			GetEdit()						{return m_pCombo;}

	void				SetText(const QString& Text)	{m_pCombo->setCurrentText(Text);}
	QString				GetText()						{return m_pCombo->currentText();}
	void				AddItems(const QStringList& Items) {m_pCombo->addItems(Items);}

signals:
	void				textChanged(const QString& text);

private slots:
	void				Browse()
	{
		QString FilePath = m_bDirs
			? QFileDialog::getExistingDirectory(this, tr("Select Directory"))
			: QFileDialog::getOpenFileName(0, tr("Browse"), "", QString("Any File (*.*)"));
		if(!FilePath.isEmpty())
			SetText(FilePath);
	}

protected:
	QComboBox*		m_pCombo;
	bool			m_bDirs;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

class CNeoScriptDebugging: public QObject
{
	Q_OBJECT
public:
	CNeoScriptDebugging(uint64 ID);
	~CNeoScriptDebugging();

	static void OpenNew();
	static void CloseAll();
	static void Dispatch(const QVariantMap& Response);

signals:
	void SendResponse(const QVariant& var);

public slots:
    void ProcessRequest(const QVariant& var);
	void Detach();

protected:

	uint64				m_ID;
	CNeoScriptDebugger* m_pScriptDebugger;
	static QMap<uint64, CNeoScriptDebugging*> m_Debuggers;
};
