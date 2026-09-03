#pragma once

// ============================================================================
// 充电用户端 - 主窗口
// 登录成功后进入；持有共享 NetClient。
// 首页增加地址定位功能。
// ============================================================================

#include <QString>
#include <QWidget>

class QButtonGroup;
class QStackedWidget;
class QComboBox;
class QLineEdit;
class QPushButton;
class QLabel;

class NetClient;
class StationListPage;
class PileListPage;
class ChargePage;
class ProfilePage;
class LocationManager;

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(qint64 userId,
                    const QString &nickname,
                    const QString &phone,
                    double balance,
                    QWidget *parent = nullptr);


private:
    NetClient        *m_net;

    QStackedWidget   *m_contentStack;
    QStackedWidget   *m_homeStack;
    QButtonGroup     *m_navGroup;

    StationListPage  *m_stationPage;
    PileListPage     *m_pilePage;
    ChargePage       *m_chargePage;
    ProfilePage      *m_profilePage;

    // ------------------------------------------------------------------------
    // 地址定位
    // ------------------------------------------------------------------------
    LocationManager  *m_locationManager;

    QComboBox        *m_regionCombo;
    QLineEdit        *m_addressEdit;
    QPushButton      *m_locationBtn;
    QLabel           *m_locationTip;

    QString           m_nickname;
    QString           m_phone;
    double            m_balance;
    qint64 m_userId = 0;

};
