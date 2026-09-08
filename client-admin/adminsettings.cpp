#include "adminsettings.h"

#include <QApplication>
#include <QFont>
#include <QHeaderView>
#include <QSettings>
#include <QTableView>
#include <QVariant>

namespace {
QSettings settings()
{
    return QSettings(QStringLiteral("ChargingSystem"), QStringLiteral("AdminClient"));
}
}

QString AdminSettings::defaultPage() { return settings().value(QStringLiteral("navigation/defaultPage"), QStringLiteral("dashboard")).toString(); }
bool AdminSettings::rememberLastPage() { return settings().value(QStringLiteral("navigation/rememberLastPage"), false).toBool(); }
QString AdminSettings::lastPage() { return settings().value(QStringLiteral("navigation/lastPage"), QStringLiteral("dashboard")).toString(); }
bool AdminSettings::sidebarExpanded() { return settings().value(QStringLiteral("navigation/sidebarExpanded"), true).toBool(); }
QString AdminSettings::fontSize() { return settings().value(QStringLiteral("ui/fontSize"), QStringLiteral("normal")).toString(); }
QString AdminSettings::tableDensity() { return settings().value(QStringLiteral("ui/tableDensity"), QStringLiteral("comfortable")).toString(); }
bool AdminSettings::monitorAutoRefresh() { return settings().value(QStringLiteral("monitor/autoRefresh"), true).toBool(); }
int AdminSettings::monitorRefreshInterval() { return settings().value(QStringLiteral("monitor/refreshInterval"), 10000).toInt(); }
bool AdminSettings::pauseWhenHidden() { return settings().value(QStringLiteral("refresh/pauseWhenHidden"), true).toBool(); }

void AdminSettings::setValue(const QString &key, const QVariant &value)
{
    QSettings s = settings();
    s.setValue(key, value);
}

void AdminSettings::resetPreferences()
{
    QSettings s = settings();
    const QStringList groups = {QStringLiteral("navigation"), QStringLiteral("ui"),
                                QStringLiteral("monitor"), QStringLiteral("dashboard"),
                                QStringLiteral("refresh")};
    for (const QString &group : groups) s.remove(group);
}

void AdminSettings::applyDisplaySettings()
{
    const QString size = fontSize();
    int pixels = 13;
    if (size == QStringLiteral("compact")) pixels = 12;
    else if (size == QStringLiteral("large")) pixels = 14;
    else if (size == QStringLiteral("extra_large")) pixels = 16;
    QFont font = qApp->font();
    font.setPixelSize(pixels);
    qApp->setFont(font);

    const QString density = tableDensity();
    int rowHeight = 38;
    if (density == QStringLiteral("compact")) rowHeight = 32;
    else if (density == QStringLiteral("spacious")) rowHeight = 44;
    for (QWidget *widget : QApplication::allWidgets()) {
        if (auto *table = qobject_cast<QTableView *>(widget))
            table->verticalHeader()->setDefaultSectionSize(rowHeight);
    }
}
