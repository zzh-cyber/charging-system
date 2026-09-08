#pragma once

#include <QString>

class AdminSettings
{
public:
    static QString defaultPage();
    static bool rememberLastPage();
    static QString lastPage();
    static bool sidebarExpanded();
    static QString fontSize();
    static QString tableDensity();
    static bool monitorAutoRefresh();
    static int monitorRefreshInterval();
    static bool pauseWhenHidden();

    static void setValue(const QString &key, const QVariant &value);
    static void resetPreferences();
    static void applyDisplaySettings();
};
