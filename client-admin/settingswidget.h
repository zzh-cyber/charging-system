#pragma once

#include <QWidget>

class NetClient;
class QLabel;
class QComboBox;
class QCheckBox;

class SettingsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsWidget(NetClient *netClient, QWidget *parent = nullptr);
    void reloadValues();

signals:
    void displaySettingsChanged();
    void refreshSettingsChanged();
    void sidebarPreferenceChanged(bool expanded);
    void themeChanged(bool dark);

private:
    void initUi();
    void updateConnectionInfo();
    QWidget *createRow(const QString &title, const QString &description, QWidget *control, QWidget *parent);

    NetClient *m_net = nullptr;
    QComboBox *m_defaultPage = nullptr;
    QCheckBox *m_rememberPage = nullptr;
    QCheckBox *m_sidebarExpanded = nullptr;
    QCheckBox *m_darkMode = nullptr;
    QComboBox *m_fontSize = nullptr;
    QComboBox *m_tableDensity = nullptr;
    QCheckBox *m_monitorRefresh = nullptr;
    QComboBox *m_monitorInterval = nullptr;
    QCheckBox *m_pauseHidden = nullptr;
    QLabel *m_connection = nullptr;
    QLabel *m_address = nullptr;
    QLabel *m_lastCommunication = nullptr;
};
