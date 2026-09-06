#include "adminmainwindow.h"
#include "dashboardwidget.h"
#include "pilestatuswidget.h"
#include "usermanagerwidget.h"
#include "pilemanagerwidget.h"
#include "stationmanagerwidget.h"

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
    setWindowTitle("充电桩综合运营管理系统 - PC服务端");
    resize(1200, 800);

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *rootLayout = new QVBoxLayout(centralWidget);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto *header = new QFrame(centralWidget);
    header->setObjectName("appHeader");
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(18, 12, 18, 12);
    m_toggleButton = new QPushButton(QStringLiteral("☰"), header);
    m_toggleButton->setObjectName("menuToggle");
    m_toggleButton->setToolTip(QStringLiteral("展开/收起导航栏"));
    m_pageTitle = new QLabel(QStringLiteral("运营总览"), header);
    m_pageTitle->setObjectName("pageTitle");
    m_connectionStatus = new QLabel(QStringLiteral("● 后端已连接"), header);
    m_connectionStatus->setObjectName("connectionStatus");
    m_lastUpdate = new QLabel(QStringLiteral("最后更新  %1").arg(QDateTime::currentDateTime().toString("HH:mm:ss")), header);
    m_lastUpdate->setObjectName("lastUpdate");
    auto *notice = new QPushButton(QStringLiteral("🔔  通知"), header);
    notice->setObjectName("headerAction");
    auto *admin = new QPushButton(QStringLiteral("管理员  ▾"), header);
    admin->setObjectName("headerAction");
    headerLayout->addWidget(m_toggleButton);
    headerLayout->addSpacing(14);
    headerLayout->addWidget(m_pageTitle);
    headerLayout->addStretch();
    headerLayout->addWidget(m_connectionStatus);
    headerLayout->addSpacing(18);
    headerLayout->addWidget(m_lastUpdate);
    headerLayout->addSpacing(14);
    headerLayout->addWidget(notice);
    headerLayout->addWidget(admin);
    rootLayout->addWidget(header);

    auto *body = new QWidget(centralWidget);
    QHBoxLayout *mainLayout = new QHBoxLayout(body);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 1. 左侧菜单栏
    sidebarList = new QListWidget(this);
    sidebarList->setObjectName("sidebar");
    sidebarList->setFixedWidth(210);

    m_menuLabels = {
        QStringLiteral("运营总览"), QStringLiteral("实时监控"),
        QStringLiteral("电站管理"), QStringLiteral("电桩管理"),
        QStringLiteral("订单管理"), QStringLiteral("用户管理"),
        QStringLiteral("告警中心"), QStringLiteral("系统设置")
    };

    const QStringList icons = {"⌂", "◉", "▣", "▤", "▥", "♙", "!", "⚙"};
    for (int i = 0; i < m_menuLabels.size(); ++i) {
        QListWidgetItem *item = new QListWidgetItem(icons[i] + QStringLiteral("  ") + m_menuLabels[i], sidebarList);
        item->setData(Qt::UserRole, icons[i]);
        item->setToolTip(m_menuLabels[i]);
        item->setSizeHint(QSize(210, 48));
    }

    // 2. 右侧内容区
    contentStack = new QStackedWidget(this);

    // 索引 0: 数据总览
    contentStack->addWidget(new DashboardWidget(m_net, this));

    // 索引 1: 电桩状态
    contentStack->addWidget(new PileStatusWidget(m_net, this));

    // 索引 2: 电站管理
    contentStack->addWidget(new StationManagerWidget(m_net, this));

    // 索引 3: 电桩管理
    contentStack->addWidget(new PileManagerWidget(m_net, this));

    // 索引 4: 订单管理
    QWidget *pageOrder = new QWidget();
    QVBoxLayout *layoutOrder = new QVBoxLayout(pageOrder);
    QLabel *labelOrder = new QLabel("订单管理 界面内容区（开发中）", pageOrder);
    labelOrder->setAlignment(Qt::AlignCenter);
    labelOrder->setObjectName("placeholderLabel");
    layoutOrder->addWidget(labelOrder);
    contentStack->addWidget(pageOrder);

    // 索引 5: 用户管理
    contentStack->addWidget(new UserManagerWidget(m_net, this));

    // 预留后续模块页面，导航项先保持可用
    for (const QString &name : {QStringLiteral("告警中心"), QStringLiteral("系统设置")}) {
        auto *placeholder = new QWidget();
        auto *placeholderLayout = new QVBoxLayout(placeholder);
        auto *placeholderLabel = new QLabel(name + QStringLiteral("（即将上线）"), placeholder);
        placeholderLabel->setAlignment(Qt::AlignCenter);
        placeholderLabel->setObjectName("placeholderLabel");
        placeholderLayout->addWidget(placeholderLabel);
        contentStack->addWidget(placeholder);
    }

    mainLayout->addWidget(sidebarList);
    mainLayout->addWidget(contentStack);
    rootLayout->addWidget(body);

    connect(sidebarList, &QListWidget::currentRowChanged, this, &AdminMainWindow::onMenuSelected);
    connect(m_toggleButton, &QPushButton::clicked, this, &AdminMainWindow::toggleSidebar);
    sidebarList->setCurrentRow(0);
}

void AdminMainWindow::onMenuSelected(int index)
{
    if (index >= 0 && index < contentStack->count()) {
        contentStack->setCurrentIndex(index);
        m_pageTitle->setText(m_menuLabels.value(index));
    }
}

void AdminMainWindow::toggleSidebar()
{
    setSidebarCollapsed(!m_sidebarCollapsed);
}

void AdminMainWindow::setSidebarCollapsed(bool collapsed)
{
    m_sidebarCollapsed = collapsed;
    sidebarList->setFixedWidth(collapsed ? 60 : 210);
    for (int i = 0; i < sidebarList->count(); ++i) {
        auto *item = sidebarList->item(i);
        item->setText(collapsed ? item->data(Qt::UserRole).toString()
                                : item->data(Qt::UserRole).toString() + QStringLiteral("  ") + m_menuLabels[i]);
        item->setTextAlignment(collapsed ? Qt::AlignCenter : Qt::AlignLeft | Qt::AlignVCenter);
        item->setSizeHint(QSize(collapsed ? 60 : 210, 48));
    }
}
