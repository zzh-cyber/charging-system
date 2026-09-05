#include "loginwindow.h"

#include "mainwindow.h"
#include "netclient.h"
#include "protocol.h"
#include "windowhelper.h"

#include <QFrame>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QDebug>
#include <QRegularExpression>
#include <QRegularExpressionValidator>


// 服务器地址（本机联调用 127.0.0.1）
static constexpr const char *kServerHost = "127.0.0.1";
static constexpr quint16     kServerPort = 9000;

LoginWindow::LoginWindow(QWidget *parent)
    : QWidget(parent)
    , m_net(new NetClient(this))
{
    setWindowTitle(QStringLiteral("充电用户端 - 登录"));
    applyPhoneWindow(this);

    auto *brand = new QLabel(QStringLiteral("新能源充电"), this);
    brand->setAlignment(Qt::AlignCenter);
    brand->setStyleSheet("font-size:26px;font-weight:bold;color:#1d4ed8;");

    auto *subTitle = new QLabel(QStringLiteral("让绿色出行更便捷"), this);
    subTitle->setAlignment(Qt::AlignCenter);
    subTitle->setStyleSheet("font-size:14px;color:#86909c;");

    auto *tip = new QLabel(QStringLiteral("手机号免密登录 · 不发送验证码"), this);
    tip->setAlignment(Qt::AlignCenter);
    tip->setStyleSheet("font-size:12px;color:#c0c4cc;");

    m_phoneEdit = new QLineEdit(this);

    m_phoneEdit->setPlaceholderText(
        QStringLiteral("请输入11位手机号"));

    m_phoneEdit->setMaxLength(11);

    m_phoneEdit->setAlignment(
        Qt::AlignCenter);

    // 只允许数字输入
    m_phoneEdit->setInputMethodHints(
        Qt::ImhDigitsOnly);

    // 输入阶段允许 1~11 位，提交时再做严格校验
    auto *phoneValidator =
        new QRegularExpressionValidator(
            QRegularExpression(
                QStringLiteral("^1\\d{0,10}$")),
            m_phoneEdit);

    m_phoneEdit->setValidator(
        phoneValidator);


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
    layout->addStretch();
    layout->addWidget(card);
    layout->addStretch();

    connect(m_loginBtn, &QPushButton::clicked, this, &LoginWindow::onLoginClicked);
}

void LoginWindow::onLoginClicked()
{
    const QString phone =
        m_phoneEdit->text().trimmed();

    const QRegularExpression phoneRegex(
        QStringLiteral("^1\\d{10}$"));

    if (!phoneRegex.match(phone).hasMatch()) {

        QMessageBox::warning(
            this,
            QStringLiteral("手机号格式错误"),
            QStringLiteral(
                "请输入以 1 开头的 11 位手机号"));

        return;
    }
    m_hint->setText(
        QStringLiteral("正在登录，请稍候…"));

    m_hint->setStyleSheet(
        "color:#86909c;font-size:12px;");

    m_loginBtn->setEnabled(false);

    m_loginBtn->setText(
        QStringLiteral("登录中…"));

    const QString maskedPhone =
        phone.left(3)
        + QStringLiteral("****")
        + phone.right(4);

    qInfo()
        << "login request phone:"
        << maskedPhone;


    if (!m_net->isConnected() &&
        !m_net->connectToServer(
            kServerHost,
            kServerPort)) {

        m_loginBtn->setEnabled(true);
        m_loginBtn->setText(
            QStringLiteral("登 录"));

        m_hint->setText(
            QStringLiteral("连接服务器失败"));

        m_hint->setStyleSheet(
            "color:#e5484d;font-size:12px;");

        return;
    }


    QJsonObject data;
    data["phone"] = phone;
    const QJsonObject resp =
        m_net->request(Protocol::makeRequest(Protocol::MsgType::Login, data));

    m_loginBtn->setEnabled(true);

    m_loginBtn->setText(
        QStringLiteral("登 录"));
    const int code = resp.value("code").toInt();
    const QString msg = resp.value("msg").toString();
    if (code != Protocol::Ok) {
        QMessageBox::warning(this, QStringLiteral("登录失败"), msg);
        return;
    }

    // 登录成功 → 进入主界面，并把用户信息传进去（供「我的」页显示）
    const QJsonObject u = resp.value("data").toObject();

    const QString token = u.value("token").toString();

    if (token.isEmpty()) {
        QMessageBox::warning(
            this,
            QStringLiteral("登录失败"),
            QStringLiteral("服务器未返回 token"));
        return;
    }

    auto *mainWin = new MainWindow(
        u.value("id").toVariant().toLongLong(),
        u.value("nickname").toString(),
        u.value("phone").toString(),
        u.value("balance").toDouble(),
        token);


    mainWin->setAttribute(Qt::WA_DeleteOnClose);
    mainWin->show();
    close();
}
