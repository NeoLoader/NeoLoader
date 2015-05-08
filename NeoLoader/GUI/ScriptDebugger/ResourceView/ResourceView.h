#pragma once

#include "../../../../NeoScriptTools/debugging/qscriptdebuggercustomviewinterface.h"

#if QT_VERSION < 0x050000
#include <QtGui>
#else
#include <QWidget>
#include <QTextEdit>
#include <QTreeWidget>
#include <QLabel>
#include <QVBoxLayout>
#endif

class CResourceView: public QScriptDebuggerCustomViewInterface
{
	Q_OBJECT
public:
	CResourceView(qint64 ID, QWidget *parent = 0);

	qint64 customID() {return m_ID;}
	int find(const QString &exp, int options = 0);
	QString text() {return "";}

protected:
	virtual QTextEdit* GetEdit() {return NULL;}

	qint64 m_ID;
};

//////////////////////////////////////////////////////////////////////////////
//

class CWebKitView: public CResourceView
{
	Q_OBJECT
public:
	CWebKitView(qint64 ID, QWidget *parent = 0);

	static bool Init();

	static QScriptDebuggerCustomViewInterface*		factory(qint64 ID, void* param)	{return new CWebKitView(ID);}

	void setData(const QVariant& var);

	QString text() {return m_HtmlCode;}

public slots:
	void UpdateWebView();

private:
	typedef QWidget* (*CreateWebView)(bool bJS);
	static CreateWebView m_CreateWebView;
	QWidget			*m_pWebView;

	//QWebView		*m_pWebView;
	QString			m_HtmlCode;
	bool			m_UpdatePending;
};

//////////////////////////////////////////////////////////////////////////////
//

class CHtmlCodeView: public CResourceView
{
	Q_OBJECT
public:
	CHtmlCodeView(qint64 ID, QWidget *parent = 0);

	static QScriptDebuggerCustomViewInterface*		factory(qint64 ID, void* param)	{return new CHtmlCodeView(ID);}

	void setData(const QVariant& var);

	QString text() {return m_HtmlCode;}

protected:
	virtual QTextEdit* GetEdit() {return m_pWebCode;}

private:
	QTextEdit		*m_pWebCode;
	QString			m_HtmlCode;
};

//////////////////////////////////////////////////////////////////////////////
//

class CDomTreeView: public CResourceView
{
	Q_OBJECT
public:
	CDomTreeView(qint64 ID, QWidget *parent = 0);

	static QScriptDebuggerCustomViewInterface*		factory(qint64 ID, void* param)	{return new CDomTreeView(ID);}

	void setData(const QVariant& var);

	QString text() {return m_HtmlCode;}

protected:
	void			UpdateTree(QTreeWidgetItem* pNodeItem, QVariantMap Node);

private:
	QTreeWidget		*m_pWebTree;
	QString			m_HtmlCode;
};

//////////////////////////////////////////////////////////////////////////////
//

class CRawTraceView: public CResourceView
{
	Q_OBJECT
public:
	CRawTraceView(qint64 ID, QWidget *parent = 0);

	static QScriptDebuggerCustomViewInterface*		factory(qint64 ID, void* param)	{return new CRawTraceView(ID);}

	void setData(const QVariant& var);

protected:
	virtual QTextEdit* GetEdit() {return m_pRawTrace;}

private:
	QTextEdit		*m_pRawTrace;
	QString			m_RawTrace;
};

//////////////////////////////////////////////////////////////////////////////
//

class CImageView: public CResourceView
{
	Q_OBJECT
public:
	CImageView(qint64 ID, QWidget *parent = 0);

	static QScriptDebuggerCustomViewInterface*		factory(qint64 ID, void* param)	{return new CImageView(ID);}

	void setData(const QVariant& var);

private:
	QLabel			*m_pWebImage;
	QByteArray		m_WebData;
};
