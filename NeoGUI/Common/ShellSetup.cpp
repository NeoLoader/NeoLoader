#include "GlobalHeader.h"
#include "ShellSetup.h"

#include <QSettings>

#ifdef Q_OS_WIN
#include <shlobj.h>

/*static QVariantHash readHive(QSettings* settings, const QString &hive)
{
    QVariantHash keyValues;

    settings->beginGroup(hive);
    foreach (const QString &key, settings->allKeys())
        keyValues.insert(key, settings->value(key));
    settings->endGroup();

    return keyValues;
}*/
#endif

bool CShellSetup::InstallType(const QString& extension, const QString& openPath, bool allUsers, const QString& iconPath, const QString& description, const QString& contentType, QString progId)
{
#ifdef Q_OS_WIN
    QSettings settings(QLatin1String(allUsers ? "HKEY_LOCAL_MACHINE" : "HKEY_CURRENT_USER")
        , QSettings::NativeFormat);

    if (progId.isEmpty())
        progId = QString::fromLatin1("%1_auto_file").arg(extension);
    const QString classesProgId = QString::fromLatin1("Software/Classes/") + progId;
    const QString classesFileType = QString::fromLatin1("Software/Classes/.%2").arg(extension);
    const QString classesApplications = QString::fromLatin1("Software/Classes/Applications/") + progId;

    // register new values
    settings.setValue(QString::fromLatin1("%1/Default").arg(classesFileType), progId);
    settings.setValue(QString::fromLatin1("%1/OpenWithProgIds/%2").arg(classesFileType, progId), QString());
    settings.setValue(QString::fromLatin1("%1/shell/Open/Command/Default").arg(classesProgId), openPath);
    settings.setValue(QString::fromLatin1("%1/shell/Open/Command/Default").arg(classesApplications), openPath);

    // content type (optional)
    if (!contentType.isEmpty())
        settings.setValue(QString::fromLatin1("%1/Content Type").arg(classesFileType), contentType);

    // description (optional)
    if (!description.isEmpty())
        settings.setValue(QString::fromLatin1("%1/Default").arg(classesProgId), description);

     // icon (optional)
    if (!iconPath.isEmpty())
        settings.setValue(QString::fromLatin1("%1/DefaultIcon/Default").arg(classesProgId), iconPath);

    // force the shell to invalidate its cache
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);

    return true;
#else
    qDebug() << QObject::tr("Registering file types is only supported on Windows.");
    return false;
#endif
}

bool CShellSetup::TestType(const QString& extension, const QString& openPath, QString progId)
{
#ifdef Q_OS_WIN
    QSettings settings(QLatin1String("HKEY_CLASSES_ROOT")
        , QSettings::NativeFormat);

    if (progId.isEmpty())
        progId = QString::fromLatin1("%1_auto_file").arg(extension);
    const QString classesProgId = QString::fromLatin1("") + progId;
    const QString classesFileType = QString::fromLatin1(".%2").arg(extension);
    const QString classesApplications = QString::fromLatin1("Applications/") + progId;

    if(settings.value(QString::fromLatin1("%1/Default").arg(classesFileType)) != progId)
		return false;
    if(settings.value(QString::fromLatin1("%1/shell/Open/Command/Default").arg(classesProgId)) != openPath)
		return false;
    if(settings.value(QString::fromLatin1("%1/shell/Open/Command/Default").arg(classesApplications)) != openPath)
		return false;

    return true;
#else
    qDebug() << QObject::tr("Registering file types is only supported on Windows.");
    return false;
#endif
}

bool CShellSetup::UninstallType(const QString& extension, bool allUsers, QString progId)
{
#ifdef Q_OS_WIN

    QSettings settings(QLatin1String(allUsers ? "HKEY_LOCAL_MACHINE" : "HKEY_CURRENT_USER")
        , QSettings::NativeFormat);

    if (progId.isEmpty())
        progId = QString::fromLatin1("%1_auto_file").arg(extension);
    const QString classesProgId = QString::fromLatin1("Software/Classes/") + progId;
    const QString classesFileType = QString::fromLatin1("Software/Classes/.%2").arg(extension);
    const QString classesApplications = QString::fromLatin1("Software/Classes/Applications/") + progId;

    // remove ProgId and Applications entry
	settings.remove(classesFileType);
    settings.remove(classesProgId);
    settings.remove(classesApplications);

    // force the shell to invalidate its cache
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);

    return true;
#else
    qDebug() << QObject::tr("Registering file types is only supported on Windows.");
    return false;
#endif
}

bool CShellSetup::InstallProtocol(const QString& scheme, const QString& openPath, bool allUsers, const QString& iconPath, const QString& description)
{
#ifdef Q_OS_WIN
    QSettings settings(QLatin1String(allUsers ? "HKEY_LOCAL_MACHINE" : "HKEY_CURRENT_USER")
        , QSettings::NativeFormat);

	const QString classesScheme = QString::fromLatin1("Software/Classes/%2").arg(scheme);

    // register new values
    settings.setValue(QString::fromLatin1("%1/Shell/Open/Command/Default").arg(classesScheme), openPath);
	settings.setValue(QString::fromLatin1("%1/URL Protocol").arg(classesScheme), QString::fromLatin1(""));
    
    // description (optional)
    if (!description.isEmpty())
        settings.setValue(QString::fromLatin1("%1/Default").arg(classesScheme), description);

     // icon (optional)
    if (!iconPath.isEmpty())
        settings.setValue(QString::fromLatin1("%1/DefaultIcon/Default").arg(classesScheme), iconPath);

    // force the shell to invalidate its cache
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);

    return true;
#else
    qDebug() << QObject::tr("Registering uri schemes is only supported on Windows.");
    return false;
#endif
}

bool CShellSetup::TestProtocol(const QString& scheme, const QString& openPath)
{
#ifdef Q_OS_WIN
    QSettings settings(QLatin1String("HKEY_CLASSES_ROOT")
        , QSettings::NativeFormat);

    const QString classesScheme = QString::fromLatin1("%2").arg(scheme);

    if(settings.value(QString::fromLatin1("%1/Shell/Open/Command/Default").arg(classesScheme)) != openPath)
		return false;

    return true;
#else
    qDebug() << QObject::tr("Registering uri schemes is only supported on Windows.");
    return false;
#endif
}

bool CShellSetup::UninstallProtocol(const QString& scheme, bool allUsers)
{
#ifdef Q_OS_WIN
    QSettings settings(QLatin1String(allUsers ? "HKEY_LOCAL_MACHINE" : "HKEY_CURRENT_USER")
        , QSettings::NativeFormat);

	const QString classesScheme = QString::fromLatin1("Software/Classes/%2").arg(scheme);

	settings.remove(classesScheme);

    // force the shell to invalidate its cache
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);

    return true;
#else
    qDebug() << QObject::tr("Registering uri schemes is only supported on Windows.");
    return false;
#endif
}