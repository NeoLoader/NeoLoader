#pragma once
#include "../NeoGUI.h"
#include <QPainter>
#include "../Common/Common.h"

class CSourceCell: public QWidget
{
	Q_OBJECT
public:
	CSourceCell(QWidget *parent = 0)
	: QWidget(parent)
	{
		setMaximumHeight(16);
		//setMaximumWidth(40);

		QHBoxLayout* pLayout = new QHBoxLayout();
		pLayout->setMargin(0);
		pLayout->setSpacing(3);
		pLayout->setAlignment(Qt::AlignRight);
		//pLayout->setAlignment(Qt::AlignLeft);

		//m_pText = new QLabel();
		//pLayout->addWidget(m_pText);

		for(int i=0; i < eCount; i++)
		{
			QLabel* pLabel = new QLabel();
			pLayout->addWidget(pLabel);
			m_Icons.append(qMakePair(pLabel, -1));
		}
		
		setLayout(pLayout);
	}

	void SetValue(const QVariantMap& File)
	{
		bool bStarted = File["FileState"] == "Started" || File["FileState"] == "Paused";
		bool bEnabled = bStarted || File["FileState"] == "Pending";
		QStringList Hashes = File["Hashes"].toStringList();
		bool bUseArchive = Hashes.contains("arch") && (File["HosterDl"].toInt() == 3);
		int Neo =		((Hashes.contains("neo") || Hashes.contains("neox"))	? 0x01 | (File["NeoShare"].toBool() && !bUseArchive ? 0x00 : 0x02)		| (bEnabled ? 0x00 : 0x04)													: 0x00);
		int Torrent =	(Hashes.contains("btih")								? 0x01 | (File["Torrent"].toBool()	&& !bUseArchive ? 0x00 : 0x02)		| ((bEnabled && (!bStarted || File["Torrenting"].toBool())) ? 0x00 : 0x04)	: 0x00);
		int Mule =		(Hashes.contains("ed2k")								? 0x01 | (File["Ed2kShare"].toBool() && !bUseArchive ? 0x00 : 0x02)		| (bEnabled ? 0x00 : 0x04)													: 0x00);
		int Hosters =	(Hashes.contains("arch")								? 0x01 | (bUseArchive ? 0x00 : 0x02)									| (bEnabled ? 0x00 : 0x04)													: 0x00);

		UpdateIcon(eNeo, "neo", Neo);
		UpdateIcon(eTorrent, "torrent", Torrent);
		UpdateIcon(eMule, "emule", Mule);
		UpdateIcon(eHosters, "hosters", Hosters);

		//if(Value != m_pText->text())
		//	m_pText->setText(Value);
	}
protected:
	void UpdateIcon(int Icon, const QString& Name, int Value)
	{
		if(m_Icons[Icon].second == Value)
			return;

		QLabel* pLabel = m_Icons[Icon].first;
		if(Value)
		{
			QImage Image(QString(":/Icons/Sources/%1.png").arg(Name));
			if(Value & 0x04)
				GrayScale(Image);
			if(Value & 0x02)
			{
				QPixmap Overlay(QString(":/Icons/Sources/No.png"));

				QPixmap Result(Image.width(), Image.height());
				Result.fill(Qt::transparent); // force alpha channel
				QPainter painter(&Result);
				painter.drawPixmap(0, 0, QPixmap::fromImage(Image));
				painter.drawPixmap(0, 0, Overlay);

				Image = Result.toImage();
			}
			pLabel->setPixmap(QPixmap::fromImage(Image));

			pLabel->setVisible(true);
		}
		else
			pLabel->setVisible(false);

		m_Icons[Icon].second = Value;
	}
	//QLabel*				m_pText;

	enum EIcons{
		eNeo,
		eTorrent,
		eMule,
		eHosters,
		eCount
	};
	QList<QPair<QLabel*, int> > m_Icons;
};