#include "amapconfig.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>

namespace
{

QString configFilePath()
{
    const QDir applicationDir(
        QCoreApplication::applicationDirPath());

    return QDir::cleanPath(
        applicationDir.filePath(
            QStringLiteral(
                "../../config/amap.ini")));
}

QString configValue(
    const QString &key)
{
    QSettings settings(
        configFilePath(),
        QSettings::IniFormat);

    return settings.value(key)
        .toString()
        .trimmed();
}

} // namespace

QString AmapConfig::jsApiKey()
{
    return configValue(
        QStringLiteral(
            "amap/js_api_key"));
}

QString AmapConfig::jsApiSecret()
{
    return configValue(
        QStringLiteral(
            "amap/js_api_secret"));
}

QString AmapConfig::webServiceKey()
{
    return configValue(
        QStringLiteral(
            "amap/web_service_key"));
}
