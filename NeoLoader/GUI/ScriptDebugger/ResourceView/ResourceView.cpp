#include "GlobalHeader.h"
#include "ResourceView.h"
#include "XmlHighlighter.h"

CResourceView::CResourceView(qint64 ID, QWidget *parent)
 :QScriptDebuggerCustomViewInterface(parent, 0)
{
	m_ID = ID;
}

int CResourceView::find(const QString &exp, int options)
{
	QTextEdit* ed = GetEdit();
	if(!ed)
		return 0;
    QTextCursor cursor = ed->textCursor();
    if (options & 0x100) {
        // start searching from the beginning of selection
        if (cursor.hasSelection()) {
            int len = cursor.selectedText().length();
            cursor.clearSelection();
            cursor.setPosition(cursor.position() - len);
            ed->setTextCursor(cursor);
        }
        options &= ~0x100;
    }
    int ret = 0;
    if (ed->find(exp, QTextDocument::FindFlags(options))) {
        ret |= 0x1;
    } else {
        QTextCursor curse = cursor;
        curse.movePosition(QTextCursor::Start);
        ed->setTextCursor(curse);
        if (ed->find(exp, QTextDocument::FindFlags(options)))
            ret |= 0x1 | 0x2;
        else
            ed->setTextCursor(cursor);
    }
    return ret;
}

//////////////////////////////////////////////////////////////////////////////
//

#include <QLibrary>

CWebKitView::CreateWebView CWebKitView::m_CreateWebView = NULL;

bool CWebKitView::Init()
{
	if(!m_CreateWebView)
		m_CreateWebView = (CreateWebView)QLibrary::resolve("WebView","CreateWebView");
	return m_CreateWebView != NULL;
}

CWebKitView::CWebKitView(qint64 ID, QWidget *parent)
 : CResourceView(ID, parent)
{
	QVBoxLayout* pVbox = new QVBoxLayout(this);
    pVbox->setMargin(0);

	//m_pWebView = new QWebView();
	//m_pWebView->setUrl(QUrl("about:blank"));
	//m_pWebView->settings()->setAttribute(QWebSettings::JavascriptEnabled, 0);

	if(m_CreateWebView)
	{
		m_pWebView = m_CreateWebView(false);

		pVbox->addWidget(m_pWebView);
	}

	m_UpdatePending = false;
}

void CWebKitView::setData(const QVariant& var)
{
	QVariantMap Resource = var.toMap();

	QString HtmlCode = Resource["TextData"].toString();
	if(m_HtmlCode != HtmlCode)
	{
		m_HtmlCode = HtmlCode;
		if(!m_UpdatePending)
		{
			m_UpdatePending = true;
			QTimer::singleShot(500, this, SLOT(UpdateWebView()));
		}
	}
}

void CWebKitView::UpdateWebView()
{
	m_UpdatePending = false;
	//m_pWebView->setHtml(m_HtmlCode);
	if(m_pWebView)
		QMetaObject::invokeMethod(m_pWebView, "SetHtml", Qt::AutoConnection, Q_ARG(const QString&, m_HtmlCode));
}

//////////////////////////////////////////////////////////////////////////////
//

CHtmlCodeView::CHtmlCodeView(qint64 ID, QWidget *parent)
 : CResourceView(ID, parent)
{
	m_pWebCode = new QTextEdit();
	//m_pWebCode->setTabStopWidth (QFontMetrics(m_pWebCode->currentFont()).width(TAB_SPACES));
	m_pWebCode->setLineWrapMode(QTextEdit::NoWrap);
	new XmlHighlighter(m_pWebCode->document());

    QVBoxLayout* pVbox = new QVBoxLayout(this);
    pVbox->setMargin(0);
    pVbox->addWidget(m_pWebCode);
}

void CHtmlCodeView::setData(const QVariant& var)
{
	QVariantMap Resource = var.toMap();

	QString HtmlCode = Resource["TextData"].toString();
	if(m_HtmlCode != HtmlCode)
	{
		m_HtmlCode = HtmlCode;
		m_pWebCode->setPlainText(HtmlCode);
	}
}

//////////////////////////////////////////////////////////////////////////////
//

CDomTreeView::CDomTreeView(qint64 ID, QWidget *parent)
 : CResourceView(ID, parent)
{
	m_pWebTree = new QTreeWidget();
	m_pWebTree->setHeaderLabels(QString("DomTree").split("|"));

    QVBoxLayout* pVbox = new QVBoxLayout(this);
    pVbox->setMargin(0);
    pVbox->addWidget(m_pWebTree);
}

void CDomTreeView::setData(const QVariant& var)
{
	QVariantMap Resource = var.toMap();

	m_HtmlCode = Resource["TextData"].toString();
	if(!Resource.contains("DomTree"))
		return;

	QMap<qint64, QTreeWidgetItem*> NodeMap;
	for(int i=0; i<m_pWebTree->topLevelItemCount();i++)
	{
		QTreeWidgetItem* pItem = m_pWebTree->topLevelItem(i);
		qint64 ID = pItem->data(0, Qt::UserRole).toULongLong();
		Q_ASSERT(!NodeMap.contains(ID));
		NodeMap.insert(ID,pItem);
	}

	foreach(const QVariant& node, Resource["DomTree"].toList())
	{
		QVariantMap Node = node.toMap();
		qint64 ID = Node["ID"].toULongLong();

		QTreeWidgetItem* pNodeItem = NodeMap.take(ID);
		if(!pNodeItem)
		{
			pNodeItem = new QTreeWidgetItem();
			pNodeItem->setData(0, Qt::UserRole, ID);
			m_pWebTree->addTopLevelItem(pNodeItem);
			pNodeItem->setExpanded(true);
		}

		UpdateTree(pNodeItem, Node);
	}

	// whats left is to be deleted 
	foreach(QTreeWidgetItem* pItem, NodeMap)
		delete pItem;
}

void CDomTreeView::UpdateTree(QTreeWidgetItem* pNodeItem, QVariantMap Node)
{
	QString Type = Node["Type"].toString();

	QMap<qint64, QTreeWidgetItem*> NodeMap;
	for(int i=0; i<pNodeItem->childCount();i++)
	{
		QTreeWidgetItem* pItem = pNodeItem->child(i);
		qint64 ID = pItem->data(0, Qt::UserRole).toULongLong();
		Q_ASSERT(!NodeMap.contains(ID));
		NodeMap.insert(ID,pItem);
	}

	if(Type == "Text")
	{
		QString Text = Node["Content"].toString().replace(QRegExp("[\r\n]"), " ").simplified();
		if(Text.isEmpty()) // dont show empty text nodes
		{
			delete pNodeItem;
			return;
		}
		else
			pNodeItem->setText(0, Text);
	}
	else if (Type == "Comment")
		pNodeItem->setText(0, "<!--" + Node["Content"].toString().replace(QRegExp("[\r\n]"), " ") + "-->");
	else if (Type == "CData")
		pNodeItem->setText(0, "CData: " + Node["Content"].toString());
	else
	{
		QString Markup = "<" + Node["Markup"].toString();
		if(!Node["Attributes"].toStringList().isEmpty())
			Markup += " " + Node["Attributes"].toStringList().join(" ");
		QString End = Node["End"].toString();
		QVariantList SubNodes = Node["Nodes"].toList();
		if(SubNodes.isEmpty() && End.indexOf(QRegExp("[^/ \t\r\n]+")) == -1) // self closed tag no children
			Markup += End + ">";
		else
		{
			Markup += ">";
			
			if(SubNodes.count() == 1 && SubNodes.at(0).toMap()["Type"].toString() == "Text")
				Markup += SubNodes.at(0).toMap()["Content"].toString().replace(QRegExp("[\r\n]"), " ").simplified();
			else
			{
				foreach(const QVariant& subnode, SubNodes)
				{
					QVariantMap SubNode = subnode.toMap();
					qint64 ID = SubNode["ID"].toULongLong();

					QTreeWidgetItem* pSubNodeItem = NodeMap.take(ID);
					if(!pSubNodeItem)
					{
						pSubNodeItem = new QTreeWidgetItem();
						pSubNodeItem->setData(0, Qt::UserRole, ID);
						pNodeItem->addChild(pSubNodeItem);
						pSubNodeItem->setExpanded(true);
					}

					UpdateTree(pSubNodeItem, SubNode);
				}
			}

			Markup += "<" + End + ">";
		}
		pNodeItem->setText(0, Markup);
	}

	// whats left is to be deleted 
	foreach(QTreeWidgetItem* pItem, NodeMap)
		delete pItem;
}

//////////////////////////////////////////////////////////////////////////////
//

CRawTraceView::CRawTraceView(qint64 ID, QWidget *parent)
 : CResourceView(ID, parent)
{
	m_pRawTrace = new QTextEdit();
	//m_pWebCode->setTabStopWidth (QFontMetrics(m_pWebCode->currentFont()).width(TAB_SPACES));
	m_pRawTrace->setLineWrapMode(QTextEdit::NoWrap);

    QVBoxLayout* pVbox = new QVBoxLayout(this);
    pVbox->setMargin(0);
    pVbox->addWidget(m_pRawTrace);
}

QString DumpMap(const QVariantMap& Map)
{
	QString Str;
	foreach(const QString& Name, Map.keys())
		Str.append(Name + ": " + Map[Name].toString() + "\r\n");
	return Str;
}

void CRawTraceView::setData(const QVariant& var)
{
	QVariantMap Resource = var.toMap();

	QString RawTrace;

	foreach(const QVariant& vRedirect, Resource["Redirects"].toList())
	{
		QVariantMap Redirect = vRedirect.toMap();
		RawTrace.append("Url: " + Redirect["Url"].toString() + "\r\n\r\n");
		RawTrace.append("Request Header:\r\n" + DumpMap(Redirect["Request"].toMap()) + "\r\n");
		if(Redirect.contains("Post"))
			RawTrace.append(Redirect["Post"].toString() + "\r\n");
		RawTrace.append("Response Header:\r\n" + DumpMap(Redirect["Response"].toMap()) + "\r\n");
		if(Redirect.contains("Text"))
			RawTrace.append(Redirect["Text"].toString() + "\r\n");

		RawTrace.append("\r\n");
	}

	RawTrace.append("Url: " + Resource["Url"].toString() + "\r\n\r\n");
	RawTrace.append("Request Header:\r\n" + DumpMap(Resource["Request"].toMap()) + "\r\n");
	if(Resource.contains("Post"))
		RawTrace.append(Resource["Post"].toString() + "\r\n");
	RawTrace.append("Response Header:\r\n" + DumpMap(Resource["Response"].toMap()) + "\r\n");
	if(Resource.contains("Text"))
		RawTrace.append(Resource["Text"].toString() + "\r\n");

	if(m_RawTrace != RawTrace)
	{
		m_RawTrace = RawTrace;
		m_pRawTrace->setPlainText(RawTrace);
	}
}

//////////////////////////////////////////////////////////////////////////////
//

CImageView::CImageView(qint64 ID, QWidget *parent)
 : CResourceView(ID, parent)
{
	m_pWebImage = new QLabel();

    QVBoxLayout* pVbox = new QVBoxLayout(this);
    pVbox->setMargin(0);
    pVbox->addWidget(m_pWebImage);
}

void CImageView::setData(const QVariant& var)
{
	QVariantMap Resource = var.toMap();

	QByteArray WebData = Resource["BinaryData"].toByteArray();
	if(m_WebData != WebData)
	{
		m_WebData = WebData;
		QPixmap Pixmap;
		Pixmap.loadFromData(WebData);
		m_pWebImage->setPixmap(Pixmap);
	}
}
