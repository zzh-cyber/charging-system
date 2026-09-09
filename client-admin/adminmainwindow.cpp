#include "adminmainwindow.h"
#include "dashboardwidget.h"
#include "pilestatuswidget.h"
#include "usermanagerwidget.h"
#include "pilemanagerwidget.h"
#include "stationmanagerwidget.h"
#include "ordermanagerwidget.h"
#include "settingswidget.h"
#include "adminsettings.h"
#include "workbenchwidget.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QListWidgetItem>
#include <QPushButton>
#include <QFrame>
#include <QDateTime>

AdminMainWindow::AdminMainWindow(NetClient *netClient, QWidget *parent)
    : QMainWindow(parent)
    , m_net(netClient)
{
    initUI();
}

AdminMainWindow::~AdminMainWindow()
{
}

void AdminMainWindow::initUI()
{
    setProperty("darkTheme", AdminSettings::darkMode());
    setWindowTitle("充电桩综合运营管理系统 - PC服务端");
    resize(1200, 800);

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QHBoxLayout *rootLayout = new QHBoxLayout(centralWidget);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_sidebarContainer = new QFrame(centralWidget);
    m_sidebarContainer->setObjectName(QStringLiteral("sidebarShell"));
    m_sidebarContainer->setFixedWidth(224);
    auto *sidebarLayout = new QVBoxLayout(m_sidebarContainer);
    sidebarLayout->setContentsMargins(18, 24, 18, 22);
    sidebarLayout->setSpacing(0);
    auto *brand = new QLabel(QStringLiteral("◆  充电管理系统"), m_sidebarContainer);
    brand->setObjectName(QStringLiteral("sidebarBrand"));
    auto *brandCaption = new QLabel(QStringLiteral("OPERATIONS CONSOLE"), m_sidebarContainer);
    brandCaption->setObjectName(QStringLiteral("sidebarBrandCaption"));
    sidebarLayout->addWidget(brand);
    sidebarLayout->addWidget(brandCaption);
    sidebarLayout->addSpacing(42);
    sidebarLayout->addSpacing(0);
    sidebarLayout->addSpacing(10);

    sidebarList = new QListWidget(m_sidebarContainer);
    sidebarList->setObjectName("sidebar");
    sidebarList->setFrameShape(QFrame::NoFrame);
    sidebarList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_menuLabels = {
        QStringLiteral("工作台"), QStringLiteral("运营总览"), QStringLiteral("实时监控"),
        QStringLiteral("电站管理"), QStringLiteral("电桩管理"),
        QStringLiteral("订单管理"), QStringLiteral("用户管理"),
        QStringLiteral("系统设置")
    };
    const QStringList icons = {"✦", "◈", "◉", "⌂", "▤", "▦", "♙", "⚙"};
    for (int i = 0; i < m_menuLabels.size(); ++i) {
        auto *item = new QListWidgetItem(icons[i] + QStringLiteral("   ") + m_menuLabels[i], sidebarList);
        item->setData(Qt::UserRole, icons[i]);
        item->setToolTip(m_menuLabels[i]);
        item->setSizeHint(QSize(188, 46));
    }
    sidebarLayout->addWidget(sidebarList, 1);
    auto *profileLine = new QFrame(m_sidebarContainer);
    profileLine->setObjectName(QStringLiteral("sidebarProfileLine"));
    profileLine->setFixedHeight(1);
    auto *profile = new QLabel(QStringLiteral("●   管理员\n     系统运营账户"), m_sidebarContainer);
    profile->setObjectName(QStringLiteral("sidebarProfile"));
    sidebarLayout->addWidget(profileLine);
    sidebarLayout->addSpacing(16);
    sidebarLayout->addWidget(profile);
    rootLayout->addWidget(m_sidebarContainer);

    auto *workspace = new QWidget(centralWidget);
    workspace->setObjectName(QStringLiteral("workspace"));
    auto *workspaceLayout = new QVBoxLayout(workspace);
    workspaceLayout->setContentsMargins(0, 0, 0, 0);
    workspaceLayout->setSpacing(0);

    auto *header = new QFrame(workspace);
    header->setObjectName("appHeader");
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(26, 16, 26, 14);
    m_toggleButton = new QPushButton(QStringLiteral("☰"), header);
    m_toggleButton->setObjectName("menuToggle");
    m_toggleButton->setToolTip(QStringLiteral("展开/收起导航栏"));
    m_pageTitle = new QLabel(QStringLiteral("运营总览"), header);
    m_pageTitle->setObjectName("pageTitle");
    m_connectionStatus = new QLabel(QStringLiteral("● 后端已连接"), header);
    m_connectionStatus->setObjectName("connectionStatus");
    m_lastUpdate = new QLabel(QStringLiteral("最后更新  %1").arg(QDateTime::currentDateTime().toString("HH:mm:ss")), header);
    m_lastUpdate->setObjectName("lastUpdate");
    headerLayout->addWidget(m_toggleButton);
    headerLayout->addSpacing(14);
    headerLayout->addWidget(m_pageTitle);
    headerLayout->addStretch();
    headerLayout->addWidget(m_connectionStatus);
    headerLayout->addSpacing(18);
    headerLayout->addWidget(m_lastUpdate);
    workspaceLayout->addWidget(header);

    auto *body = new QWidget(workspace);
    body->setObjectName(QStringLiteral("appBody"));
    QHBoxLayout *mainLayout = new QHBoxLayout(body);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 右侧业务内容区
    contentStack = new QStackedWidget(this);

    contentStack->addWidget(new WorkbenchWidget(m_net, this));
    // 索引 1: 数据总览
    contentStack->addWidget(new DashboardWidget(m_net, this));

    // 索引 1: 电桩状态
    m_monitorPage = new PileStatusWidget(m_net, this);
    contentStack->addWidget(m_monitorPage);

    // 索引 2: 电站管理
    contentStack->addWidget(new StationManagerWidget(m_net, this));

    // 索引 3: 电桩管理
    contentStack->addWidget(new PileManagerWidget(m_net, this));

    // 索引 4: 订单管理
    contentStack->addWidget(new OrderManagerWidget(m_net, this));

    // 索引 5: 用户管理
    contentStack->addWidget(new UserManagerWidget(m_net, this));

    m_settingsPage = new SettingsWidget(m_net, this);
    contentStack->addWidget(m_settingsPage);
    if (auto *workbench = qobject_cast<WorkbenchWidget *>(contentStack->widget(0))) {
        connect(workbench, &WorkbenchWidget::openRealtime, this, [this](const QString &) { sidebarList->setCurrentRow(2); });
        connect(workbench, &WorkbenchWidget::openOrders, this, [this](const QString &) { sidebarList->setCurrentRow(5); });
    }

    mainLayout->addWidget(contentStack);
    workspaceLayout->addWidget(body, 1);
    rootLayout->addWidget(workspace, 1);

    connect(sidebarList, &QListWidget::currentRowChanged, this, &AdminMainWindow::onMenuSelected);
    connect(m_toggleButton, &QPushButton::clicked, this, &AdminMainWindow::toggleSidebar);
    m_pageIds = {QStringLiteral("workbench"), QStringLiteral("dashboard"), QStringLiteral("monitor"), QStringLiteral("station"), QStringLiteral("pile"), QStringLiteral("order"), QStringLiteral("user"), QStringLiteral("settings")};
    connect(m_settingsPage, &SettingsWidget::displaySettingsChanged, this, [] { AdminSettings::applyDisplaySettings(); });
    connect(m_settingsPage, &SettingsWidget::themeChanged, this, [this](bool dark) {
        setProperty("darkTheme", dark);
        qApp->setStyleSheet(qApp->styleSheet());
        update();
        if (auto *dashboard = contentStack->findChild<DashboardWidget *>())
            dashboard->setDarkTheme(dark);
        if (m_monitorPage)
            m_monitorPage->setDarkTheme(dark);
    });
    connect(m_settingsPage, &SettingsWidget::refreshSettingsChanged, this, &AdminMainWindow::applyRefreshSettings);
    connect(m_settingsPage, &SettingsWidget::sidebarPreferenceChanged, this, [this](bool expanded) { setSidebarCollapsed(!expanded); });
    AdminSettings::applyDisplaySettings();
    if (auto *dashboard = contentStack->findChild<DashboardWidget *>())
        dashboard->setDarkTheme(AdminSettings::darkMode());
    if (m_monitorPage)
        m_monitorPage->setDarkTheme(AdminSettings::darkMode());
    setSidebarCollapsed(!AdminSettings::sidebarExpanded());
    QString initialPage = AdminSettings::rememberLastPage() ? AdminSettings::lastPage() : AdminSettings::defaultPage();
    int initialIndex = m_pageIds.indexOf(initialPage);
    if (initialIndex < 0) initialIndex = 0;
    sidebarList->setCurrentRow(initialIndex);
    applyRefreshSettings();
}

void AdminMainWindow::onMenuSelected(int index)
{
    if (index >= 0 && index < contentStack->count()) {
        contentStack->setCurrentIndex(index);
        m_pageTitle->setText(m_menuLabels.value(index));
        AdminSettings::setValue(QStringLiteral("navigation/lastPage"), m_pageIds.value(index, QStringLiteral("dashboard")));
        if (contentStack->currentWidget() == m_settingsPage) m_settingsPage->reloadValues();
        applyRefreshSettings();
    }
}

void AdminMainWindow::applyRefreshSettings()
{
    if (!m_monitorPage || !contentStack) return;
    m_monitorPage->applyRefreshSettings(AdminSettings::monitorAutoRefresh(),
                                        AdminSettings::monitorRefreshInterval(),
                                        AdminSettings::pauseWhenHidden(),
                                        contentStack->currentWidget() == m_monitorPage);
}

void AdminMainWindow::toggleSidebar()
{
    setSidebarCollapsed(!m_sidebarCollapsed);
    AdminSettings::setValue(QStringLiteral("navigation/sidebarExpanded"), !m_sidebarCollapsed);
}

void AdminMainWindow::setSidebarCollapsed(bool collapsed)
{
    m_sidebarCollapsed = collapsed;
    m_sidebarContainer->setFixedWidth(collapsed ? 76 : 224);
    if (auto *caption = m_sidebarContainer->findChild<QLabel *>(QStringLiteral("sidebarBrandCaption")))
        caption->setVisible(!collapsed);
    if (auto *caption = m_sidebarContainer->findChild<QLabel *>(QStringLiteral("sidebarCaption")))
        caption->setVisible(!collapsed);
    if (auto *profile = m_sidebarContainer->findChild<QLabel *>(QStringLiteral("sidebarProfile")))
        profile->setVisible(!collapsed);
    for (int i = 0; i < sidebarList->count(); ++i) {
        auto *item = sidebarList->item(i);
        item->setText(collapsed ? item->data(Qt::UserRole).toString()
                                : item->data(Qt::UserRole).toString() + QStringLiteral("  ") + m_menuLabels[i]);
        item->setTextAlignment(collapsed ? Qt::AlignCenter : Qt::AlignLeft | Qt::AlignVCenter);
        item->setSizeHint(QSize(collapsed ? 40 : 188, 46));
    }
}
