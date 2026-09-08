#ifndef ADMINMAINWINDOW_H
#define ADMINMAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QStackedWidget>

class QLabel;
class QPushButton;
class PileStatusWidget;
class SettingsWidget;

class NetClient;

class AdminMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit AdminMainWindow(NetClient *netClient = nullptr, QWidget *parent = nullptr);
    ~AdminMainWindow();

private slots:
    void onMenuSelected(int index);
    void toggleSidebar();

private:
    void initUI();
    void setSidebarCollapsed(bool collapsed);
    void applyRefreshSettings();

    NetClient *m_net;
    QListWidget *sidebarList;
    QStackedWidget *contentStack;
    QWidget *m_sidebarContainer;
    QPushButton *m_toggleButton;
    QLabel *m_pageTitle;
    QLabel *m_connectionStatus;
    QLabel *m_lastUpdate;
    bool m_sidebarCollapsed = false;
    QStringList m_menuLabels;
    QStringList m_pageIds;
    PileStatusWidget *m_monitorPage = nullptr;
    SettingsWidget *m_settingsPage = nullptr;
};

#endif // ADMINMAINWINDOW_H
