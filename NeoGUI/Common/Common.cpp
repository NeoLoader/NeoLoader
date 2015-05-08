#include "GlobalHeader.h"
#include "Common.h"
#include "../../Framework/Settings.h"
#include "../../Framework/OtherFunctions.h"

void MakeFileIcon(const QString& Ext)
{
	if(Ext.length() > 10)
		return;

	QString CachePath = CSettings::GetSettingsDir() + "/Cache/Icons/";
	CreateDir(CachePath);

	QIcon Icon;
	if(Ext == ".")
		Icon = QFileIconProvider().icon(QFileIconProvider::Folder);
	else
	{
		QFile TmpFile(CachePath + "tmp." + Ext);
		TmpFile.open(QIODevice::WriteOnly);

		Icon = QFileIconProvider().icon(QFileInfo(TmpFile.fileName()));

		TmpFile.close();
		TmpFile.remove();
	}
	
	int Sizes[2] = {16,32};
	for(int i=0; i<ARRSIZE(Sizes); i++)
	{
		QFile IconFile(CachePath + Ext + QString::number(Sizes[i]) + ".png");
		IconFile.open(QIODevice::WriteOnly);
		Icon.pixmap(Sizes[i], Sizes[i]).save(&IconFile, "png");
		IconFile.close();
	}
}

QIcon GetFileIcon(const QString& Ext, int Size)
{
	static QMap<QString, QIcon> m_Icons;

	if(!m_Icons.contains(Ext))
	{
		QString IconPath;
		if(Ext == "....")
			IconPath = ":/Icons/Collection";
		else if(Ext == "...")
			IconPath = ":/Icons/Multi";
		else
		{
			IconPath = CSettings::GetSettingsDir() + "/Cache/Icons/" + Ext + QString::number(Size) + ".png";
			if(!QFile::exists(IconPath))
			{
				MakeFileIcon(Ext);
				//IconPath = ":/Icon" + QString::number(Size) + ".png";
			}
		}
		m_Icons[Ext] = QIcon(IconPath);
	}
	return m_Icons[Ext];
}

QString FormatSize(uint64 Size)
{
	double Div;
	if(Size > (uint64)(Div = 1.0*1024*1024*1024*1024*1024*1024))
		return QString::number(double(Size)/Div, 'f', 2) + "EB";
	if(Size > (uint64)(Div = 1.0*1024*1024*1024*1024*1024))
		return QString::number(double(Size)/Div, 'f', 2) + "PB";
	if(Size > (uint64)(Div = 1.0*1024*1024*1024*1024))
		return QString::number(double(Size)/Div, 'f', 2) + "TB";
	if(Size > (uint64)(Div = 1.0*1024*1024*1024))
		return QString::number(double(Size)/Div, 'f', 2) + "GB";
	if(Size > (uint64)(Div = 1.0*1024*1024))
		return QString::number(double(Size)/Div, 'f', 2) + "MB";
	if(Size > (uint64)(Div = 1.0*1024))
		return QString::number(double(Size)/Div, 'f', 2) + "kB";
	return QString::number(double(Size)) + "B";
}

QString FormatTime(uint64 Time)
{
	int seconds = Time % 60;
	Time /= 60;
	int minutes = Time % 60;
	Time /= 60;
	int hours = Time % 24;
	int days = Time / 24;
	if((hours == 0) && (days == 0))
		return QString().sprintf("%02d:%02d", minutes, seconds);
	if (days == 0)
		return QString().sprintf("%02d:%02d:%02d", hours, minutes, seconds);
	return QString().sprintf("%dd%02d:%02d:%02d", days, hours, minutes, seconds);
}


void GrayScale (QImage& Image)
{
	if (Image.depth () == 32)
	{
		uchar* r = (Image.bits ());
		uchar* g = (Image.bits () + 1);
		uchar* b = (Image.bits () + 2);

#if QT_VERSION < 0x050000
		uchar* end = (Image.bits() + Image.numBytes ());
#else
		uchar* end = (Image.bits() + Image.byteCount ());
#endif

		while (r != end)
		{
			*r = *g = *b = (((*r + *g) >> 1) + *b) >> 1; // (r + b + g) / 3

			r += 4;
			g += 4;
			b += 4;
		}
	}
	else
	{
#if QT_VERSION < 0x050000
		for (int i = 0; i < Image.numColors (); i++)
#else
		for (int i = 0; i < Image.colorCount (); i++)
#endif
		{
			uint r = qRed (Image.color (i));
			uint g = qGreen (Image.color (i));
			uint b = qBlue (Image.color (i));

			uint gray = (((r + g) >> 1) + b) >> 1;

			Image.setColor (i, qRgba (gray, gray, gray, qAlpha (Image.color (i))));
		}
	}
}

QAction* MakeAction(QToolBar* pParent, const QString& IconFile, const QString& Text)
{
	QAction* pAction = new QAction(Text, pParent);
	
	QImage Image(IconFile);
	QIcon Icon;
	Icon.addPixmap(QPixmap::fromImage(Image), QIcon::Normal);
	GrayScale(Image);
	Icon.addPixmap(QPixmap::fromImage(Image), QIcon::Disabled);
	pAction->setIcon(Icon);
	
	pParent->addAction(pAction);
	return pAction;
}

QMenu* MakeMenu(QMenu* pParent, const QString& Text, const QString& IconFile)
{
	if(!IconFile.isEmpty())
	{
		QImage Image(IconFile);
		QIcon Icon;
		Icon.addPixmap(QPixmap::fromImage(Image), QIcon::Normal);
		GrayScale(Image);
		Icon.addPixmap(QPixmap::fromImage(Image), QIcon::Disabled);
		return pParent->addMenu(Icon, Text);
	}
	return pParent->addMenu(Text);
}

QAction* MakeAction(QMenu* pParent, const QString& Text, const QString& IconFile)
{
	QAction* pAction = new QAction(Text, pParent);
	if(!IconFile.isEmpty())
	{
		QImage Image(IconFile);
		QIcon Icon;
		Icon.addPixmap(QPixmap::fromImage(Image), QIcon::Normal);
		GrayScale(Image);
		Icon.addPixmap(QPixmap::fromImage(Image), QIcon::Disabled);
		pAction->setIcon(Icon);
	}
	pParent->addAction(pAction);
	return pAction;
}

QAction* MakeAction(QActionGroup* pGroup, QMenu* pParent, const QString& Text, const QVariant& Data)
{
	QAction* pAction = new QAction(Text, pParent);
	pAction->setCheckable(true);
	pAction->setData(Data);
	pAction->setActionGroup(pGroup);
	pParent->addAction(pAction);
	return pAction;
}