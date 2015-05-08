#pragma once

void MakeFileIcon(const QString& Ext);
QIcon GetFileIcon(const QString& Ext, int Size);

QString FormatSize(uint64 Size);
QString	FormatTime(uint64 Time);

void GrayScale (QImage& Image);

QAction* MakeAction(QToolBar* pParent, const QString& IconFile, const QString& Text = "");
QMenu* MakeMenu(QMenu* pParent, const QString& Text, const QString& IconFile = "");
QAction* MakeAction(QMenu* pParent, const QString& Text, const QString& IconFile = "");
QAction* MakeAction(QActionGroup* pGroup, QMenu* pParent, const QString& Text, const QVariant& Data);