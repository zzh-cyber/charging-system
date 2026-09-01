#include "loginwindow.h"

#include "netclient.h"
#include "protocol.h"

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
    resize(360, 260);

    auto *title = new QLabel(QStringLiteral("新能源汽车充电"), this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size:20px;font-weight:bold;");

    auto *tip = new QLabel(QStringLiteral("手机号免密登录 · 首次登录自动注册"), this);
    tip->setAlignment(Qt::AlignCenter);
    tip->setStyleSheet("color:gray;");

    m_phoneEdit = new QLineEdit(this);
    m_phoneEdit->setPlaceholderText(QStringLiteral("请输入11位手机号"));
    m_phoneEdit->setMaxLength(11);

    m_loginBtn = new QPushButton(QStringLiteral("登录"), this);

    m_hint = new QLabel(QStringLiteral("演示账号：13800138001 / 13800138002"), this);
    m_hint->setAlignment(Qt::AlignCenter);
    m_hint->setStyleSheet("color:gray;font-size:12px;");

    auto *layout = new QVBoxLayout(this);
    layout->addStretch();
    layout->addWidget(title);
    layout->addWidget(tip);
    layout->addSpacing(10);
    layout->addWidget(m_phoneEdit);
    layout->addWidget(m_loginBtn);
    layout->addWidget(m_hint);
    layout->addStretch();

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

    const QJsonObject u = resp.value("data").toObject();
    QMessageBox::information(
        this, QStringLiteral("登录成功"),
        QStringLiteral("欢迎，%1\n手机号：%2\n钱包余额：￥%3")
            .arg(u.value("nickname").toString())
            .arg(u.value("phone").toString())
            .arg(u.value("balance").toDouble()));

    // TODO(用户端负责人)：登录成功后进入主界面（充电站列表 / 充电流程 / 我的）
}
