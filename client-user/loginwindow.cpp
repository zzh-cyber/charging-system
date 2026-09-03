#include "loginwindow.h"

#include "mainwindow.h"
#include "netclient.h"
#include "protocol.h"

#include <QFrame>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

// 服务器地址（本机联调用 127.0.0.1）
static constexpr const char *kServerHost = "127.0.0.1";
static constexpr quint16     kServerPort = 9000;

LoginWindow::LoginWindow(QWidget *parent)
    : QWidget(parent)
    , m_net(new NetClient(this))
{
    setWindowTitle(QStringLiteral("充电用户端 - 登录"));
    setFixedSize(400, 460);

    auto *brand = new QLabel(QStringLiteral("新能源充电"), this);
    brand->setAlignment(Qt::AlignCenter);
    brand->setStyleSheet("font-size:26px;font-weight:bold;color:#1d4ed8;");

    auto *subTitle = new QLabel(QStringLiteral("让绿色出行更便捷"), this);
    subTitle->setAlignment(Qt::AlignCenter);
    subTitle->setStyleSheet("font-size:14px;color:#86909c;");

    auto *tip = new QLabel(QStringLiteral("手机号免密登录 · 首次登录自动注册"), this);
    tip->setAlignment(Qt::AlignCenter);
    tip->setStyleSheet("font-size:12px;color:#c0c4cc;");

    m_phoneEdit = new QLineEdit(this);
    m_phoneEdit->setPlaceholderText(QStringLiteral("请输入11位手机号"));
    m_phoneEdit->setMaxLength(11);
    m_phoneEdit->setAlignment(Qt::AlignCenter);

    m_loginBtn = new QPushButton(QStringLiteral("登 录"), this);

    m_hint = new QLabel(QStringLiteral("演示账号：13800138001 / 13800138002"), this);
    m_hint->setAlignment(Qt::AlignCenter);
    m_hint->setStyleSheet("color:#c0c4cc;font-size:12px;");

    auto *card = new QFrame(this);
    card->setObjectName("loginCard");
    card->setStyleSheet(
        "#loginCard{background:#ffffff;border:1px solid #d6e4ff;border-radius:16px;}");

    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(36, 40, 36, 36);
    cardLayout->setSpacing(14);
    cardLayout->addWidget(brand);
    cardLayout->addWidget(subTitle);
    cardLayout->addSpacing(8);
    cardLayout->addWidget(tip);
    cardLayout->addSpacing(10);
    cardLayout->addWidget(m_phoneEdit);
    cardLayout->addWidget(m_loginBtn);
    cardLayout->addSpacing(6);
    cardLayout->addWidget(m_hint);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->addWidget(card);

    connect(m_loginBtn, &QPushButton::clicked, this, &LoginWindow::onLoginClicked);
}

void LoginWindow::onLoginClicked()
{
    const QString phone = m_phoneEdit->text().trimmed();
    if (phone.size() != 11) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("请输入11位手机号"));
        return;
    }

    if (!m_net->isConnected() && !m_net->connectToServer(kServerHost, kServerPort)) {
        QMessageBox::critical(this, QStringLiteral("错误"),
                              QStringLiteral("无法连接服务器，请确认服务器已启动"));
        return;
    }

    QJsonObject data;
    data["phone"] = phone;
    const QJsonObject resp =
        m_net->request(Protocol::makeRequest(Protocol::MsgType::Login, data));

    const int code = resp.value("code").toInt();
    const QString msg = resp.value("msg").toString();
    if (code != Protocol::Ok) {
        QMessageBox::warning(this, QStringLiteral("登录失败"), msg);
        return;
    }

    // 登录成功 → 进入主界面，并把用户信息传进去（供「我的」页显示）
    const QJsonObject u = resp.value("data").toObject();
    auto *mainWin = new MainWindow(
        u.value("nickname").toString(),
        u.value("phone").toString(),
        u.value("balance").toDouble());
    mainWin->setAttribute(Qt::WA_DeleteOnClose);
    mainWin->show();
    close();
}
