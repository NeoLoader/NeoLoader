#include "GlobalHeader.h"
#include "OnlineCompleter.h"
#include "../../../qjson/src/parser.h"

COnlineCompleter::COnlineCompleter(QWidget *parent)
:QCompleter(parent)
{
	m_pNet = new QNetworkAccessManager();
	connect(m_pNet, SIGNAL(finished(QNetworkReply*)), this, SLOT(OnFinished(QNetworkReply*)));

	m_pModel = new QStringListModel();
	setModel(m_pModel);

	m_LastLength = 0;
	m_LastCount = -1;
}

void COnlineCompleter::OnUpdate(const QString& Text)
{
	if(Text.isEmpty() || Text.length() < m_LastLength)
	{
		m_LastLength = 0;
		m_LastCount = -1;
		return;
	}
	m_LastLength = Text.length();
	if(Text.length() >= 2 && (m_LastCount == -1 || m_LastCount >= 10))
	{
		QNetworkRequest Resuest(QString("http://downloadstube.net/api/autocomplete.php?q=%1&limit=10").arg(Text));
		m_pNet->get(Resuest);
	}
}

void COnlineCompleter::OnFinished(QNetworkReply* pReply)
{
	QJson::Parser json;
	bool ok;
	QVariantMap Response = json.parse (pReply->readAll(), &ok).toMap();
	pReply->deleteLater();
	if(!ok)
		return;

	QVariantList List = Response["data"].toList();
	m_LastCount = List.count();
	foreach(const QVariant& vWord, List)
	{
		QVariantMap Word = vWord.toMap();
		QString sWord = Word["keyword"].toString();
		if(!m_List.contains(sWord))
			m_List.append(sWord);
	}
	m_pModel->setStringList(m_List);
	this->complete();
}
