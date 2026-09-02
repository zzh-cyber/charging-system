#include "adminmainwindow.h"
#include "usermanagerwidget.h"
#include "pilemanagerwidget.h"
#include "stationmanagerwidget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QListWidgetItem>

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

    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 1. 左侧菜单栏
    sidebarList = new QListWidget(this);
    sidebarList->setFixedWidth(200);

    QStringList menuItems = {
        "数据总览",
        "电站管理",
        "电桩管理",
        "订单管理",
        "用户管理"
    };

    for (const QString &itemText : menuItems) {
        QListWidgetItem *item = new QListWidgetItem(itemText, sidebarList);
        item->setSizeHint(QSize(200, 55));
        item->setTextAlignment(Qt::AlignCenter);
    }

    // 2. 右侧内容区
    contentStack = new QStackedWidget(this);

    // 索引 0: 数据总览 
    QWidget *pageOverview = new QWidget();
    QVBoxLayout *layoutOverview = new QVBoxLayout(pageOverview);
    QLabel *labelOverview = new QLabel("数据总览 界面内容区（开发中）", pageOverview);
    labelOverview->setAlignment(Qt::AlignCenter);
    labelOverview->setStyleSheet("font-size: 20px; color: #888888;");
    layoutOverview->addWidget(labelOverview);
    contentStack->addWidget(pageOverview);

    // 索引 1: 电站管理 
    contentStack->addWidget(new StationManagerWidget(m_net, this));

    // 索引 2: 电桩管理 
    contentStack->addWidget(new PileManagerWidget(m_net, this));

    // 索引 3: 订单管理 
    QWidget *pageOrder = new QWidget();
    QVBoxLayout *layoutOrder = new QVBoxLayout(pageOrder);
    QLabel *labelOrder = new QLabel("订单管理 界面内容区（开发中）", pageOrder);
    labelOrder->setAlignment(Qt::AlignCenter);
    labelOrder->setStyleSheet("font-size: 20px; color: #888888;");
    layoutOrder->addWidget(labelOrder);
    contentStack->addWidget(pageOrder);

    // 索引 4: 用户管理 
    contentStack->addWidget(new UserManagerWidget(m_net, this));

    mainLayout->addWidget(sidebarList);
    mainLayout->addWidget(contentStack);

    connect(sidebarList, &QListWidget::currentRowChanged, this, &AdminMainWindow::onMenuSelected);
    sidebarList->setCurrentRow(0);
}

void AdminMainWindow::onMenuSelected(int index)
{
    if (index >= 0 && index < contentStack->count()) {
        contentStack->setCurrentIndex(index);
    }
}