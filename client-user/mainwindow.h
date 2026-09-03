#pragma once

// ============================================================================
// 充电用户端 - 主窗口
// 登录成功后进入；持有共享 NetClient。
// 底部导航「首页 / 充电 / 我的」三个按钮均分撑满，内容区用 QStackedWidget 切换。
//   - 首页：内部再用 QStackedWidget 承载「充电站列表 ⇄ 桩列表」。
//   - 充电：B 的 ChargePage。
//   - 我的：B 的 ProfilePage，登录信息在此中转。
// ============================================================================

#include <QString>
#include <QWidget>

class QButtonGroup;
class QStackedWidget;
class NetClient;
class StationListPage;
class PileListPage;
class ChargePage;
class ProfilePage;

class MainWindow : public QWidget
{
    Q_OBJECT
public:
    explicit MainWindow(const QString &nickname, const QString &phone, double balance,
                        QWidget *parent = nullptr);

private:
    NetClient        *m_net;
    QStackedWidget   *m_contentStack;  // 内容区：首页 / 充电 / 我的
    QStackedWidget   *m_homeStack;     // 「首页」内的子栈
    QButtonGroup     *m_navGroup;      // 底部导航按钮组
    StationListPage  *m_stationPage;
    PileListPage     *m_pilePage;
    ChargePage       *m_chargePage;
    ProfilePage      *m_profilePage;
    QString           m_nickname;
    QString           m_phone;
    double            m_balance;
};
