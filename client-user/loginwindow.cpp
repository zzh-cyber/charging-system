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
#include <QResizeEvent>



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
    brand->setObjectName("loginBrand");
    brand->setAlignment(Qt::AlignCenter);
    brand->setStyleSheet(
        "font-weight:bold;"
        "color:#1d4ed8;");


    auto *subTitle = new QLabel(QStringLiteral("让绿色出行更便捷"), this);
    subTitle->setObjectName("loginSubTitle");
    subTitle->setAlignment(Qt::AlignCenter);
    subTitle->setStyleSheet(
    "color:#86909c;");


    auto *tip = new QLabel(QStringLiteral("手机号免密 · 登录或注册 · 不发送验证码"), this);
    tip->setObjectName("loginTip");
    tip->setAlignment(Qt::AlignCenter);
    tip->setStyleSheet(
    "color:#c0c4cc;");


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
    m_registerBtn =
        new QPushButton(
            QStringLiteral("注 册"),
            this);

    m_hint = new QLabel(
        QStringLiteral("已有账号点登录；新号点注册。演示：13800138001"),
        this);
    m_hint->setObjectName("loginHint");
    m_hint->setAlignment(Qt::AlignCenter);
    m_hint->setStyleSheet(
    "color:#c0c4cc;");

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
    cardLayout->addWidget(m_registerBtn);
    cardLayout->addSpacing(6);
    cardLayout->addWidget(m_hint);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->addStretch();
    layout->addWidget(card);
    layout->addStretch();

    connect(m_loginBtn, &QPushButton::clicked, this, &LoginWindow::onLoginClicked);
    connect(
        m_registerBtn,
        &QPushButton::clicked,
        this,
        &LoginWindow::onRegisterClicked);
    applyResponsiveStyle();
}

void LoginWindow::resizeEvent(
    QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    applyResponsiveStyle();
}


void LoginWindow::applyResponsiveStyle()
{
    const int brandFont =
        scaledUi(this, 26);

    const int normalFont =
        scaledUi(this, 14);

    const int tipFont =
        scaledUi(this, 12);

    const int controlFont =
        scaledUi(this, 15);

    const int paddingV =
        scaledUi(this, 10);

    const int paddingH =
        scaledUi(this, 12);

    const int buttonPaddingH =
        scaledUi(this, 16);

    setStyleSheet(
        QStringLiteral(
            "QLabel#loginBrand{"
            "font-size:%1px;"
            "}"

            "QLabel#loginSubTitle{"
            "font-size:%2px;"
            "}"

            "QLabel#loginTip,"
            "QLabel#loginHint{"
            "font-size:%3px;"
            "}"

            "QLineEdit{"
            "font-size:%4px;"
            "padding:%5px %6px;"
            "}"

            "QPushButton{"
            "font-size:%4px;"
            "padding:%5px %7px;"
            "}")
            .arg(brandFont)
            .arg(normalFont)
            .arg(tipFont)
            .arg(controlFont)
            .arg(paddingV)
            .arg(paddingH)
            .arg(buttonPaddingH));


    // 登录卡片内部边距随窗口缩放
    if (auto *card =
            findChild<QFrame *>(
                QStringLiteral("loginCard"))) {

        if (auto *cardLayout =
                qobject_cast<QVBoxLayout *>(
                    card->layout())) {

            cardLayout->setContentsMargins(
                scaledUi(this, 36),
                scaledUi(this, 40),
                scaledUi(this, 36),
                scaledUi(this, 36));

            cardLayout->setSpacing(
                scaledUi(this, 14));
        }
    }


    // 页面外围边距随窗口缩放
    if (auto *outerLayout =
            qobject_cast<QVBoxLayout *>(
                layout())) {

        const int margin =
            scaledUi(this, 20);

        outerLayout->setContentsMargins(
            margin,
            margin,
            margin,
            margin);
    }
}

void LoginWindow::onLoginClicked()
{
    const QString phone = m_phoneEdit->text().trimmed();
    const QRegularExpression phoneRegex(QStringLiteral("^1\\d{10}$"));
    if (!phoneRegex.match(phone).hasMatch()) {
        QMessageBox::warning(
            this,
            QStringLiteral("手机号格式错误"),
            QStringLiteral("请输入以 1 开头的 11 位手机号"));
        return;
    }

    m_hint->setText(QStringLiteral("正在登录，请稍候…"));
    m_hint->setStyleSheet("color:#86909c;");
    m_loginBtn->setEnabled(false);
    m_loginBtn->setText(QStringLiteral("登录中…"));

    const QString maskedPhone = phone.left(3) + QStringLiteral("****") + phone.right(4);
    qInfo() << "login request phone:" << maskedPhone;

    const QJsonObject resp = sendLoginRequest(phone, false);

    m_loginBtn->setEnabled(true);
    m_loginBtn->setText(QStringLiteral("登 录"));

    if (resp.isEmpty())
        return;

    const int code = resp.value("code").toInt();
    const QString msg = resp.value("msg").toString();
    if (code != Protocol::Ok) {
        const QJsonObject result = resp.value("data").toObject();
        const bool canRegister = result.value("can_register").toBool();
        if (canRegister || msg.contains(QStringLiteral("尚未注册"))
            || msg.contains(QStringLiteral("未注册"))
            || msg.contains(QStringLiteral("不存在"))) {
            m_hint->setText(QStringLiteral("该手机号尚未注册，请点击下方注册"));
            m_hint->setStyleSheet("color:#e6a23c;");
            return;
        }
        QMessageBox::warning(this, QStringLiteral("登录失败"), msg);
        return;
    }

    enterMainWindow(resp.value("data").toObject());
}

void LoginWindow::onRegisterClicked()
{
    const QString phone = m_phoneEdit->text().trimmed();
    const QRegularExpression phoneRegex(QStringLiteral("^1\\d{10}$"));
    if (!phoneRegex.match(phone).hasMatch()) {
        m_hint->setText(QStringLiteral("请输入正确的11位手机号"));
        m_hint->setStyleSheet("color:#c0c4cc;");
        return;
    }

    m_registerBtn->setEnabled(false);
    m_loginBtn->setEnabled(false);
    m_hint->setText(QStringLiteral("正在注册，请稍候…"));
    m_hint->setStyleSheet("color:#86909c;");

    const QJsonObject resp = sendLoginRequest(phone, true);

    m_registerBtn->setEnabled(true);
    m_loginBtn->setEnabled(true);

    if (resp.isEmpty())
        return;

    const int code = resp.value("code").toInt();
    const QString msg = resp.value("msg").toString();
    if (code != Protocol::Ok) {
        QMessageBox::warning(this, QStringLiteral("注册失败"), msg);
        return;
    }

    enterMainWindow(resp.value("data").toObject());
}

QJsonObject LoginWindow::sendLoginRequest(const QString &phone, bool registerMode)
{
    if (!m_net->isConnected() &&
        !m_net->connectToServer(kServerHost, kServerPort)) {
        m_hint->setText(QStringLiteral("连接服务器失败"));
        m_hint->setStyleSheet("color:#e5484d;");
        QMessageBox::warning(
            this,
            registerMode ? QStringLiteral("注册失败") : QStringLiteral("登录失败"),
            QStringLiteral("连接服务器失败"));
        return {};
    }

    QJsonObject data;
    data["phone"] = phone;
    data["register"] = registerMode;
    return m_net->request(Protocol::makeRequest(Protocol::MsgType::Login, data));
}

void LoginWindow::enterMainWindow(const QJsonObject &userData)
{
    const QString token = userData.value("token").toString();
    if (token.isEmpty()) {
        QMessageBox::warning(
            this,
            QStringLiteral("登录失败"),
            QStringLiteral("服务器未返回 token"));
        return;
    }

    auto *mainWin = new MainWindow(
        userData.value("id").toVariant().toLongLong(),
        userData.value("nickname").toString(),
        userData.value("phone").toString(),
        userData.value("balance").toDouble(),
        token);
    mainWin->setAttribute(Qt::WA_DeleteOnClose);
    mainWin->show();
    close();
}
