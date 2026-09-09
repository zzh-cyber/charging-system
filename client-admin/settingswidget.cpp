#include "settingswidget.h"

#include "adminsettings.h"
#include "netclient.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSysInfo>
#include <QVBoxLayout>

namespace {
QFrame *card(const QString &title, QWidget *parent, QVBoxLayout **body)
{
    auto *frame = new QFrame(parent);
    frame->setObjectName(QStringLiteral("settingsCard"));
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(22, 19, 22, 20);
    layout->setSpacing(14);
    auto *label = new QLabel(title, frame);
    label->setObjectName(QStringLiteral("settingsSectionTitle"));
    layout->addWidget(label);
    *body = layout;
    return frame;
}
}

SettingsWidget::SettingsWidget(NetClient *netClient, QWidget *parent)
    : QWidget(parent), m_net(netClient)
{
    setObjectName(QStringLiteral("settingsPage"));
    initUi();
    reloadValues();
    if (m_net) {
        connect(m_net, &NetClient::disconnected, this, &SettingsWidget::updateConnectionInfo);
        connect(m_net, &NetClient::reconnected, this, &SettingsWidget::updateConnectionInfo);
        connect(m_net, &NetClient::responseReceived, this, [this](const QJsonObject &) { updateConnectionInfo(); });
    }
}

QWidget *SettingsWidget::createRow(const QString &title, const QString &description, QWidget *control, QWidget *parent)
{
    auto *row = new QWidget(parent);
    row->setObjectName(QStringLiteral("settingsRow"));
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 2, 0, 2);
    layout->setSpacing(24);
    auto *texts = new QVBoxLayout;
    texts->setSpacing(3);
    auto *name = new QLabel(title, row);
    name->setObjectName(QStringLiteral("settingsItemTitle"));
    texts->addWidget(name);
    if (!description.isEmpty()) {
        auto *hint = new QLabel(description, row);
        hint->setObjectName(QStringLiteral("settingsItemDescription"));
        hint->setWordWrap(true);
        texts->addWidget(hint);
    }
    layout->addLayout(texts, 1);
    control->setMinimumWidth(180);
    layout->addWidget(control, 0, Qt::AlignRight | Qt::AlignVCenter);
    return row;
}

void SettingsWidget::initUi()
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("settingsScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *canvas = new QWidget(scroll);
    canvas->setObjectName(QStringLiteral("settingsCanvas"));
    auto *canvasLayout = new QHBoxLayout(canvas);
    canvasLayout->setContentsMargins(24, 22, 24, 28);
    auto *content = new QWidget(canvas);
    content->setObjectName(QStringLiteral("settingsContent"));
    content->setMaximumWidth(900);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);
    auto *heading = new QLabel(QStringLiteral("系统设置"), content);
    heading->setObjectName(QStringLiteral("settingsHeading"));
    auto *subtitle = new QLabel(QStringLiteral("管理当前 PC 客户端的偏好、显示与刷新行为"), content);
    subtitle->setObjectName(QStringLiteral("settingsSubtitle"));
    layout->addWidget(heading);
    layout->addWidget(subtitle);
    layout->addSpacing(4);

    QVBoxLayout *body = nullptr;
    auto *appearance = card(QStringLiteral("外观设置"), content, &body);
    m_darkMode = new QCheckBox(QStringLiteral("开启"), appearance);
    body->addWidget(createRow(QStringLiteral("深色模式"), QStringLiteral("切换管理端的界面显示风格"), m_darkMode, appearance));
    layout->addWidget(appearance);

    auto *preferences = card(QStringLiteral("客户端偏好"), content, &body);
    m_defaultPage = new QComboBox(preferences);
    const QList<QPair<QString, QString>> pages = {{QStringLiteral("工作台"), QStringLiteral("workbench")}, {QStringLiteral("运营总览"), QStringLiteral("dashboard")}, {QStringLiteral("实时监控"), QStringLiteral("monitor")}, {QStringLiteral("电站管理"), QStringLiteral("station")}, {QStringLiteral("电桩管理"), QStringLiteral("pile")}, {QStringLiteral("订单管理"), QStringLiteral("order")}, {QStringLiteral("用户管理"), QStringLiteral("user")}};
    for (const auto &page : pages) m_defaultPage->addItem(page.first, page.second);
    m_rememberPage = new QCheckBox(QStringLiteral("开启"), preferences);
    m_sidebarExpanded = new QCheckBox(QStringLiteral("开启"), preferences);
    body->addWidget(createRow(QStringLiteral("启动默认页面"), QStringLiteral("登录成功后默认进入的管理页面"), m_defaultPage, preferences));
    body->addWidget(createRow(QStringLiteral("记住上次访问页面"), QStringLiteral("开启后，下次登录优先恢复上一次访问页面"), m_rememberPage, preferences));
    body->addWidget(createRow(QStringLiteral("启动时展开侧边栏"), QStringLiteral("控制管理端启动后的导航栏状态"), m_sidebarExpanded, preferences));
    layout->addWidget(preferences);

    auto *refresh = card(QStringLiteral("数据刷新"), content, &body);
    m_monitorRefresh = new QCheckBox(QStringLiteral("开启"), refresh);
    m_monitorInterval = new QComboBox(refresh);
    for (int seconds : {5, 10, 15, 30}) m_monitorInterval->addItem(QStringLiteral("%1 秒").arg(seconds), seconds * 1000);
    m_pauseHidden = new QCheckBox(QStringLiteral("开启"), refresh);
    body->addWidget(createRow(QStringLiteral("实时监控自动刷新"), QStringLiteral("关闭后仍可使用“立即刷新”"), m_monitorRefresh, refresh));
    body->addWidget(createRow(QStringLiteral("实时监控刷新间隔"), QStringLiteral("修改后立即更新现有刷新计时器"), m_monitorInterval, refresh));
    body->addWidget(createRow(QStringLiteral("页面不可见时暂停刷新"), QStringLiteral("减少隐藏页面不必要的网络请求"), m_pauseHidden, refresh));
    layout->addWidget(refresh);

    auto *diagnostics = card(QStringLiteral("连接与诊断"), content, &body);
    m_connection = new QLabel(diagnostics); m_connection->setObjectName(QStringLiteral("settingsConnectionValue"));
    m_address = new QLabel(diagnostics); m_address->setObjectName(QStringLiteral("settingsValue"));
    m_lastCommunication = new QLabel(diagnostics); m_lastCommunication->setObjectName(QStringLiteral("settingsValue"));
    body->addWidget(createRow(QStringLiteral("服务端状态"), QString(), m_connection, diagnostics));
    body->addWidget(createRow(QStringLiteral("服务地址"), QStringLiteral("当前客户端实际连接目标，只读"), m_address, diagnostics));
    body->addWidget(createRow(QStringLiteral("最近成功通信"), QStringLiteral("最近一次收到服务端响应的时间"), m_lastCommunication, diagnostics));
    layout->addWidget(diagnostics);

    auto *about = card(QStringLiteral("关于系统"), content, &body);
    auto value = [about](const QString &text) { auto *label = new QLabel(text, about); label->setObjectName(QStringLiteral("settingsValue")); return label; };
    body->addWidget(createRow(QStringLiteral("产品"), QString(), value(QStringLiteral("充电桩综合运营管理系统 · PC 管理端")), about));
    body->addWidget(createRow(QStringLiteral("客户端版本"), QString(), value(QStringLiteral("v1.0.0")), about));
    body->addWidget(createRow(QStringLiteral("Qt 版本"), QString(), value(QString::fromLatin1(qVersion())), about));
    body->addWidget(createRow(QStringLiteral("通信方式"), QString(), value(QStringLiteral("TCP / JSON")), about));
    body->addWidget(createRow(QStringLiteral("构建日期"), QString(), value(QString::fromLatin1(__DATE__)), about));
    layout->addWidget(about);

    auto *reset = new QPushButton(QStringLiteral("恢复默认设置"), content);
    reset->setObjectName(QStringLiteral("settingsResetButton"));
    layout->addWidget(reset, 0, Qt::AlignLeft);
    layout->addStretch();
    canvasLayout->addStretch(); canvasLayout->addWidget(content, 1); canvasLayout->addStretch();
    scroll->setWidget(canvas); outer->addWidget(scroll);

    connect(m_defaultPage, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { AdminSettings::setValue(QStringLiteral("navigation/defaultPage"), m_defaultPage->currentData()); });
    connect(m_rememberPage, &QCheckBox::toggled, this, [](bool on) { AdminSettings::setValue(QStringLiteral("navigation/rememberLastPage"), on); });
    connect(m_sidebarExpanded, &QCheckBox::toggled, this, [this](bool on) { AdminSettings::setValue(QStringLiteral("navigation/sidebarExpanded"), on); emit sidebarPreferenceChanged(on); });
    connect(m_darkMode, &QCheckBox::toggled, this, [this](bool on) { AdminSettings::setValue(QStringLiteral("appearance/theme"), on ? QStringLiteral("dark") : QStringLiteral("light")); emit themeChanged(on); });
    connect(m_monitorRefresh, &QCheckBox::toggled, this, [this](bool on) { AdminSettings::setValue(QStringLiteral("monitor/autoRefresh"), on); m_monitorInterval->setEnabled(on); emit refreshSettingsChanged(); });
    connect(m_monitorInterval, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { AdminSettings::setValue(QStringLiteral("monitor/refreshInterval"), m_monitorInterval->currentData()); emit refreshSettingsChanged(); });
    connect(m_pauseHidden, &QCheckBox::toggled, this, [this](bool on) { AdminSettings::setValue(QStringLiteral("refresh/pauseWhenHidden"), on); emit refreshSettingsChanged(); });
    connect(reset, &QPushButton::clicked, this, [this] {
        const QString text = QStringLiteral("将重置客户端偏好和自动刷新设置。\n\n不会影响用户、订单、电站、电桩或数据库业务数据。");
        if (QMessageBox::question(this, QStringLiteral("恢复默认设置？"), text, QMessageBox::Cancel | QMessageBox::Ok, QMessageBox::Cancel) != QMessageBox::Ok) return;
        AdminSettings::resetPreferences(); reloadValues(); emit displaySettingsChanged(); emit refreshSettingsChanged(); emit sidebarPreferenceChanged(true); emit themeChanged(false);
    });
}

void SettingsWidget::reloadValues()
{
    const QSignalBlocker b1(m_defaultPage), b2(m_rememberPage), b3(m_sidebarExpanded), b4(m_darkMode);
    const QSignalBlocker b5(m_monitorRefresh), b6(m_monitorInterval), b7(m_pauseHidden);
    m_defaultPage->setCurrentIndex(qMax(0, m_defaultPage->findData(AdminSettings::defaultPage())));
    m_rememberPage->setChecked(AdminSettings::rememberLastPage()); m_sidebarExpanded->setChecked(AdminSettings::sidebarExpanded());
    m_darkMode->setChecked(AdminSettings::darkMode());
    m_monitorRefresh->setChecked(AdminSettings::monitorAutoRefresh()); m_monitorInterval->setCurrentIndex(qMax(0, m_monitorInterval->findData(AdminSettings::monitorRefreshInterval()))); m_monitorInterval->setEnabled(m_monitorRefresh->isChecked());
    m_pauseHidden->setChecked(AdminSettings::pauseWhenHidden()); updateConnectionInfo();
}

void SettingsWidget::updateConnectionInfo()
{
    const bool connected = m_net && m_net->isConnected();
    m_connection->setText(connected ? QStringLiteral("● 已连接") : QStringLiteral("● 未连接"));
    m_connection->setProperty("connected", connected); m_connection->style()->unpolish(m_connection); m_connection->style()->polish(m_connection);
    m_address->setText(m_net && !m_net->serverHost().isEmpty() ? QStringLiteral("%1:%2").arg(m_net->serverHost()).arg(m_net->serverPort()) : QStringLiteral("--"));
    m_lastCommunication->setText(m_net && m_net->lastSuccessfulCommunication().isValid() ? m_net->lastSuccessfulCommunication().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QStringLiteral("--"));
}
