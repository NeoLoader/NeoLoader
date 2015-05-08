#pragma once

class CShellSetup: public QObject
{
	Q_OBJECT

signals:
	void Changed();

public:
    bool InstallType(const QString& extension, const QString& openPath, bool allUsers = false, const QString& iconPath = "", const QString& description = "", const QString& contentType = "", QString progId = "");
	bool TestType(const QString& extension, const QString& openPath, QString progId = "");
    bool UninstallType(const QString& extension, bool allUsers = false, QString progId = "");

	bool InstallProtocol(const QString& scheme, const QString& openPath, bool allUsers = false, const QString& iconPath = "", const QString& description = "");
	bool TestProtocol(const QString& scheme, const QString& openPath);
    bool UninstallProtocol(const QString& scheme, bool allUsers = false);
};

