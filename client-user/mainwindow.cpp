#include "mainwindow.h"

#include "chargepage.h"
#include "netclient.h"
#include "pilelistpage.h"
#include "profilepage.h"
#include "stationlistpage.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

// 服务器地址（与登录页保持一致）
static constexpr const char *kServerHost = "127.0.0.1";
static constexpr quint16     kServerPort = 9000;

MainWindow::MainWindow(const QString &nickname, const QString &phone, double balance,
                       QWidget *parent)
    : QWidget(parent)
    , m_net(new NetClient(this))
    , m_contentStack(new QStackedWidget)
    , m_homeStack(new QStackedWidget)
    , m_navGroup(new QButtonGroup(this))
    , m_stationPage(new StationListPage(m_net))
    , m_pilePage(new PileListPage(m_net))
    , m_chargePage(new ChargePage)
    , m_profilePage(new ProfilePage)
    , m_nickname(nickname)
    , m_phone(phone)
    , m_balance(balance)
{
    setWindowTitle(QStringLiteral("充电用户端"));
    resize(420, 680);

    if (!m_net->isConnected())
        m_net->connectToServer(kServerHost, kServerPort);

    // ---- 首页子栈：充电站列表 ⇄ 桩列表 ----
    m_homeStack->addWidget(m_stationPage);
    m_homeStack->addWidget(m_pilePage);

    // ---- 内容区：三个页面 ----
    m_contentStack->addWidget(m_homeStack);    // 0 首页
    m_contentStack->addWidget(m_chargePage);   // 1 充电
    m_contentStack->addWidget(m_profilePage);  // 2 我的

    // 中转：把登录用户信息传给「我的」页
    m_profilePage->setUserInfo(m_nickname, m_phone, m_balance);

    // ---- 底部导航：三个按钮均分撑满宽度 ----
    auto *navBar = new QWidget(this);
    navBar->setObjectName("navBar");
    auto *navLayout = new QHBoxLayout(navBar);
    navLayout->setContentsMargins(0, 0, 0, 0);
    navLayout->setSpacing(0);

    struct NavItem { QString name; int index; };
    const NavItem items[] = {
        { QStringLiteral("首页"), 0 },
        { QStringLiteral("充电"), 1 },
        { QStringLiteral("我的"), 2 },
    };
    for (const auto &item : items) {
        auto *btn = new QPushButton(item.name, navBar);
        btn->setObjectName("navBtn");
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        navLayout->addWidget(btn, 1);
        m_navGroup->addButton(btn, item.index);
    }
    m_navGroup->setExclusive(true);
    m_navGroup->button(0)->setChecked(true);
    connect(m_navGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_contentStack->setCurrentIndex(id);
    });

    // ---- 整体布局：内容区 + 底部导航 ----
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_contentStack, 1);
    layout->addWidget(navBar);

    // 首页点卡片 → 切到桩列表子页
    connect(m_stationPage, &StationListPage::stationSelected,
            this, [this](qint64 id, const QString &name) {
                m_pilePage->loadStation(id, name);
                m_homeStack->setCurrentWidget(m_pilePage);
            });

    // 桩列表返回 → 回到充电站列表
    connect(m_pilePage, &PileListPage::back, this, [this]() {
        m_homeStack->setCurrentWidget(m_stationPage);
    });
}
