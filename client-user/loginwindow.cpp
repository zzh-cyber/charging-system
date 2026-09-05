#include "loginwindow.h"

#include "mainwindow.h"
#include "netclient.h"
#include "protocol.h"
#include "uitheme.h"
#include "windowhelper.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QDebug>


// ============================================================================
// 服务器地址
// ============================================================================
static constexpr const char *kServerHost =
    "127.0.0.1";

static constexpr quint16 kServerPort =
    9000;


// ============================================================================
// 构造函数
// ============================================================================
LoginWindow::LoginWindow(
    QWidget *parent)
    : QWidget(parent)
    , m_net(new NetClient(this))
{
    setObjectName(
        QStringLiteral("loginWindow"));

    setWindowTitle(
        QStringLiteral(
            "充电用户端 - 登录"));

    applyPhoneWindow(this);


    // ========================================================================
    // 登录主卡片
    // ========================================================================
    auto *card =
        new QFrame(this);

    card->setObjectName(
        QStringLiteral("loginCard"));

    UiTheme::applyCardShadow(
        card,
        20,
        5);

    auto *cardLayout =
        new QVBoxLayout(card);

    cardLayout->setContentsMargins(
        30,
        30,
        30,
        28);

    cardLayout->setSpacing(
        12);


    // ========================================================================
    // 品牌标题
    // ========================================================================
    auto *brand =
        new QLabel(
            QStringLiteral(
                "新能源充电"),
            card);

    brand->setObjectName(
        QStringLiteral("loginBrand"));

    brand->setAlignment(
        Qt::AlignLeft |
        Qt::AlignVCenter);

    cardLayout->addWidget(
        brand);


    // ========================================================================
    // 品牌副标题
    // ========================================================================
    auto *subTitle =
        new QLabel(
            QStringLiteral(
                "便捷找桩 · 快速预约 · 安心充电"),
            card);

    subTitle->setObjectName(
        QStringLiteral(
            "loginSubTitle"));

    subTitle->setAlignment(
        Qt::AlignLeft |
        Qt::AlignVCenter);

    cardLayout->addWidget(
        subTitle);

    cardLayout->addSpacing(
        14);


    // ========================================================================
    // 登录区域标题
    // ========================================================================
    auto *sectionTitle =
        new QLabel(
            QStringLiteral(
                "手机号免密登录"),
            card);

    sectionTitle->setObjectName(
        QStringLiteral(
            "loginSectionTitle"));

    cardLayout->addWidget(
        sectionTitle);


    // ========================================================================
    // 登录说明
    // ========================================================================
    auto *tip =
        new QLabel(
            QStringLiteral(
                "无需验证码，输入手机号即可登录；"
                "新用户可直接注册"),
            card);

    tip->setObjectName(
        QStringLiteral(
            "loginTip"));

    tip->setWordWrap(true);

    cardLayout->addWidget(
        tip);

    cardLayout->addSpacing(
        8);


    // ========================================================================
    // 手机号输入框
    // ========================================================================
    m_phoneEdit =
        new QLineEdit(card);

    m_phoneEdit->setObjectName(
        QStringLiteral(
            "loginPhoneEdit"));

    m_phoneEdit->setPlaceholderText(
        QStringLiteral(
            "请输入11位手机号"));

    m_phoneEdit->setMaxLength(
        11);

    m_phoneEdit->setAlignment(
        Qt::AlignLeft |
        Qt::AlignVCenter);

    // 只允许数字输入
    m_phoneEdit->setInputMethodHints(
        Qt::ImhDigitsOnly);

    // 输入阶段允许 1~11 位，
    // 提交时再进行严格校验
    auto *phoneValidator =
        new QRegularExpressionValidator(
            QRegularExpression(
                QStringLiteral(
                    "^1\\d{0,10}$")),
            m_phoneEdit);

    m_phoneEdit->setValidator(
        phoneValidator);

    cardLayout->addWidget(
        m_phoneEdit);

    cardLayout->addSpacing(
        4);


    // ========================================================================
    // 登录按钮
    // ========================================================================
    m_loginBtn =
        new QPushButton(
            QStringLiteral("登录"),
            card);

    m_loginBtn->setObjectName(
        QStringLiteral(
            "loginPrimaryButton"));

    m_loginBtn->setCursor(
        Qt::PointingHandCursor);

    cardLayout->addWidget(
        m_loginBtn);


    // ========================================================================
    // 注册按钮
    // ========================================================================
    m_registerBtn =
        new QPushButton(
            QStringLiteral("注册"),
            card);

    m_registerBtn->setObjectName(
        QStringLiteral(
            "loginSecondaryButton"));

    m_registerBtn->setCursor(
        Qt::PointingHandCursor);

    cardLayout->addWidget(
        m_registerBtn);

    cardLayout->addSpacing(
        4);


    // ========================================================================
    // 动态提示
    // ========================================================================
    m_hint =
        new QLabel(
            QStringLiteral(
                "已有账号直接登录，"
                "新用户可使用当前手机号注册"),
            card);

    m_hint->setObjectName(
        QStringLiteral(
            "loginHint"));

    m_hint->setAlignment(
        Qt::AlignLeft |
        Qt::AlignVCenter);

    m_hint->setWordWrap(true);

    cardLayout->addWidget(
        m_hint);


    // ========================================================================
    // 功能说明卡片
    // ========================================================================
    auto *infoCard =
        new QFrame(this);

    infoCard->setObjectName(
        QStringLiteral(
            "loginInfoCard"));

    UiTheme::applyCardShadow(
        infoCard,
        14,
        3);

    auto *infoLayout =
        new QVBoxLayout(
            infoCard);

    infoLayout->setContentsMargins(
        22,
        18,
        22,
        18);

    infoLayout->setSpacing(
        12);


    // ========================================================================
    // 功能说明标题
    // ========================================================================
    auto *infoTitle =
        new QLabel(
            QStringLiteral(
                "登录后可使用"),
            infoCard);

    infoTitle->setObjectName(
        QStringLiteral(
            "loginInfoTitle"));

    infoLayout->addWidget(
        infoTitle);


    // ========================================================================
    // 功能信息行生成函数
    // ========================================================================
    const auto addInfoRow =
        [infoLayout, infoCard](
            const QString &name,
            const QString &description) {

            auto *row =
                new QHBoxLayout;

            row->setSpacing(
                12);


            auto *nameLabel =
                new QLabel(
                    name,
                    infoCard);

            nameLabel->setObjectName(
                QStringLiteral(
                    "loginInfoKey"));

            nameLabel->setMinimumWidth(
                90);


            auto *descriptionLabel =
                new QLabel(
                    description,
                    infoCard);

            descriptionLabel->setObjectName(
                QStringLiteral(
                    "loginInfoValue"));

            descriptionLabel->setAlignment(
                Qt::AlignRight |
                Qt::AlignVCenter);

            descriptionLabel->setWordWrap(
                true);


            row->addWidget(
                nameLabel);

            row->addStretch();

            row->addWidget(
                descriptionLabel,
                1);


            infoLayout->addLayout(
                row);
        };


    addInfoRow(
        QStringLiteral(
            "附近充电站"),
        QStringLiteral(
            "定位后按距离查看可用站点"));


    addInfoRow(
        QStringLiteral(
            "充电预约"),
        QStringLiteral(
            "查看桩状态并预约空闲电桩"));


    addInfoRow(
        QStringLiteral(
            "订单与钱包"),
        QStringLiteral(
            "管理充电流程、支付与账户余额"));


    // ========================================================================
    // 页面总布局
    // ========================================================================
    auto *layout =
        new QVBoxLayout(this);

    layout->setContentsMargins(
        22,
        24,
        22,
        24);

    layout->setSpacing(
        14);

    layout->addStretch(
        1);

    layout->addWidget(
        card);

    layout->addWidget(
        infoCard);

    layout->addStretch(
        1);


    // ========================================================================
    // 登录按钮事件
    // ========================================================================
    connect(
        m_loginBtn,
        &QPushButton::clicked,
        this,
        &LoginWindow::onLoginClicked);


    // ========================================================================
    // 注册按钮事件
    // ========================================================================
    connect(
        m_registerBtn,
        &QPushButton::clicked,
        this,
        &LoginWindow::onRegisterClicked);


    applyResponsiveStyle();
}


// ============================================================================
// 响应式窗口
// ============================================================================
void LoginWindow::resizeEvent(
    QResizeEvent *event)
{
    QWidget::resizeEvent(
        event);

    applyResponsiveStyle();
}


// ============================================================================
// 响应式 UI
// ============================================================================
void LoginWindow::applyResponsiveStyle()
{
    const int brandFont =
        scaledUi(
            this,
            27);

    const int sectionFont =
        scaledUi(
            this,
            17);

    const int normalFont =
        scaledUi(
            this,
            14);

    const int smallFont =
        scaledUi(
            this,
            12);

    const int tinyFont =
        scaledUi(
            this,
            11);

    const int controlFont =
        scaledUi(
            this,
            15);

    const int cardRadius =
        scaledUi(
            this,
            20);

    const int controlRadius =
        scaledUi(
            this,
            12);


    // ========================================================================
    // 页面和文字基础样式
    // ========================================================================
    setStyleSheet(
        QStringLiteral(

            // 页面
            "QWidget#loginWindow{"
            "background:%1;"
            "color:%2;"
            "}"

            // 品牌
            "QLabel#loginBrand{"
            "background:transparent;"
            "color:%2;"
            "font-size:%3px;"
            "font-weight:800;"
            "}"

            // 品牌副标题
            "QLabel#loginSubTitle{"
            "background:transparent;"
            "color:%4;"
            "font-size:%5px;"
            "font-weight:500;"
            "}"

            // 登录区域标题
            "QLabel#loginSectionTitle{"
            "background:transparent;"
            "color:%2;"
            "font-size:%6px;"
            "font-weight:700;"
            "}"

            // 登录说明 / 动态提示
            "QLabel#loginTip,"
            "QLabel#loginHint{"
            "background:transparent;"
            "color:%4;"
            "font-size:%7px;"
            "}"

            // 功能说明标题
            "QLabel#loginInfoTitle{"
            "background:transparent;"
            "color:%2;"
            "font-size:%5px;"
            "font-weight:700;"
            "}"

            // 功能名称
            "QLabel#loginInfoKey{"
            "background:transparent;"
            "color:%2;"
            "font-size:%7px;"
            "font-weight:600;"
            "}"

            // 功能描述
            "QLabel#loginInfoValue{"
            "background:transparent;"
            "color:%4;"
            "font-size:%8px;"
            "}")

        .arg(
            UiTheme::pageBackground())   // %1

        .arg(
            UiTheme::textPrimary())      // %2

        .arg(
            brandFont)                   // %3

        .arg(
            UiTheme::textSecondary())    // %4

        .arg(
            normalFont)                  // %5

        .arg(
            sectionFont)                 // %6

        .arg(
            smallFont)                   // %7

        .arg(
            tinyFont));                  // %8


    // ========================================================================
    // 手机号输入框
    // ========================================================================
    if (m_phoneEdit) {

        m_phoneEdit->setStyleSheet(
            UiTheme::inputStyle(
                controlFont,
                controlRadius));

        m_phoneEdit->setMinimumHeight(
            scaledUi(
                this,
                46));
    }


    // ========================================================================
    // 登录按钮
    // ========================================================================
    if (m_loginBtn) {

        m_loginBtn->setStyleSheet(
            UiTheme::primaryButtonStyle(
                controlFont,
                controlRadius));

        m_loginBtn->setMinimumHeight(
            scaledUi(
                this,
                46));
    }


    // ========================================================================
    // 注册按钮
    // ========================================================================
    if (m_registerBtn) {

        m_registerBtn->setStyleSheet(
            UiTheme::secondaryButtonStyle(
                controlFont,
                controlRadius));

        m_registerBtn->setMinimumHeight(
            scaledUi(
                this,
                46));
    }


    // ========================================================================
    // 登录主卡片
    // ========================================================================
    if (auto *card =
            findChild<QFrame *>(
                QStringLiteral(
                    "loginCard"))) {

        card->setStyleSheet(
            QStringLiteral(
                "QFrame#loginCard{"
                "background:%1;"
                "border:1px solid %2;"
                "border-radius:%3px;"
                "}")
                .arg(
                    UiTheme::surface())
                .arg(
                    UiTheme::border())
                .arg(
                    cardRadius));


        if (auto *cardLayout =
                qobject_cast<QVBoxLayout *>(
                    card->layout())) {

            cardLayout->setContentsMargins(
                scaledUi(this, 30),
                scaledUi(this, 30),
                scaledUi(this, 30),
                scaledUi(this, 28));

            cardLayout->setSpacing(
                scaledUi(
                    this,
                    12));
        }
    }


    // ========================================================================
    // 功能说明卡片
    // ========================================================================
    if (auto *infoCard =
            findChild<QFrame *>(
                QStringLiteral(
                    "loginInfoCard"))) {

        infoCard->setStyleSheet(
            QStringLiteral(
                "QFrame#loginInfoCard{"
                "background:%1;"
                "border:1px solid %2;"
                "border-radius:%3px;"
                "}")
                .arg(
                    UiTheme::surfaceSoft())
                .arg(
                    UiTheme::border())
                .arg(
                    cardRadius));


        if (auto *infoLayout =
                qobject_cast<QVBoxLayout *>(
                    infoCard->layout())) {

            infoLayout->setContentsMargins(
                scaledUi(this, 22),
                scaledUi(this, 18),
                scaledUi(this, 22),
                scaledUi(this, 18));

            infoLayout->setSpacing(
                scaledUi(
                    this,
                    12));
        }
    }


    // ========================================================================
    // 页面外围间距
    // ========================================================================
    if (auto *outerLayout =
            qobject_cast<QVBoxLayout *>(
                layout())) {

        outerLayout->setContentsMargins(
            scaledUi(this, 22),
            scaledUi(this, 24),
            scaledUi(this, 22),
            scaledUi(this, 24));

        outerLayout->setSpacing(
            scaledUi(
                this,
                14));
    }
}


// ============================================================================
// 登录
// ============================================================================
void LoginWindow::onLoginClicked()
{
    const QString phone =
        m_phoneEdit
            ->text()
            .trimmed();

    const QRegularExpression phoneRegex(
        QStringLiteral(
            "^1\\d{10}$"));


    if (!phoneRegex
             .match(phone)
             .hasMatch()) {

        QMessageBox::warning(
            this,
            QStringLiteral(
                "手机号格式错误"),
            QStringLiteral(
                "请输入以 1 开头的 11 位手机号"));

        return;
    }


    m_hint->setText(
        QStringLiteral(
            "正在登录，请稍候…"));

    m_hint->setStyleSheet(
        QStringLiteral(
            "color:#7A837E;"));


    m_loginBtn->setEnabled(
        false);

    m_loginBtn->setText(
        QStringLiteral(
            "登录中…"));


    const QString maskedPhone =
        phone.left(3)
        + QStringLiteral("****")
        + phone.right(4);

    qInfo()
        << "login request phone:"
        << maskedPhone;


    const QJsonObject resp =
        sendLoginRequest(
            phone,
            false);


    m_loginBtn->setEnabled(
        true);

    m_loginBtn->setText(
        QStringLiteral(
            "登录"));


    if (resp.isEmpty())
        return;


    const int code =
        resp.value("code")
            .toInt();

    const QString msg =
        resp.value("msg")
            .toString();


    if (code != Protocol::Ok) {

        const QJsonObject result =
            resp.value("data")
                .toObject();

        const bool canRegister =
            result.value(
                "can_register")
                .toBool();


        if (canRegister ||
            msg.contains(
                QStringLiteral(
                    "尚未注册")) ||
            msg.contains(
                QStringLiteral(
                    "未注册")) ||
            msg.contains(
                QStringLiteral(
                    "不存在"))) {

            m_hint->setText(
                QStringLiteral(
                    "该手机号尚未注册，请点击下方注册"));

            m_hint->setStyleSheet(
                QStringLiteral(
                    "color:#D79A4B;"));

            return;
        }


        QMessageBox::warning(
            this,
            QStringLiteral(
                "登录失败"),
            msg);

        return;
    }


    enterMainWindow(
        resp.value("data")
            .toObject());
}


// ============================================================================
// 注册
// ============================================================================
void LoginWindow::onRegisterClicked()
{
    const QString phone =
        m_phoneEdit
            ->text()
            .trimmed();


    const QRegularExpression phoneRegex(
        QStringLiteral(
            "^1\\d{10}$"));


    if (!phoneRegex
             .match(phone)
             .hasMatch()) {

        m_hint->setText(
            QStringLiteral(
                "请输入正确的11位手机号"));

        m_hint->setStyleSheet(
            QStringLiteral(
                "color:#7A837E;"));

        return;
    }


    m_registerBtn->setEnabled(
        false);

    m_loginBtn->setEnabled(
        false);


    m_hint->setText(
        QStringLiteral(
            "正在注册，请稍候…"));

    m_hint->setStyleSheet(
        QStringLiteral(
            "color:#7A837E;"));


    const QJsonObject resp =
        sendLoginRequest(
            phone,
            true);


    m_registerBtn->setEnabled(
        true);

    m_loginBtn->setEnabled(
        true);


    if (resp.isEmpty())
        return;


    const int code =
        resp.value("code")
            .toInt();

    const QString msg =
        resp.value("msg")
            .toString();


    if (code != Protocol::Ok) {

        QMessageBox::warning(
            this,
            QStringLiteral(
                "注册失败"),
            msg);

        return;
    }


    enterMainWindow(
        resp.value("data")
            .toObject());
}


// ============================================================================
// 登录请求
// ============================================================================
QJsonObject LoginWindow::sendLoginRequest(
    const QString &phone,
    bool registerMode)
{
    if (!m_net->isConnected() &&
        !m_net->connectToServer(
            kServerHost,
            kServerPort)) {

        m_hint->setText(
            QStringLiteral(
                "连接服务器失败"));

        m_hint->setStyleSheet(
            QStringLiteral(
                "color:#C96C66;"));


        QMessageBox::warning(
            this,
            registerMode
                ? QStringLiteral(
                      "注册失败")
                : QStringLiteral(
                      "登录失败"),
            QStringLiteral(
                "连接服务器失败"));

        return {};
    }


    QJsonObject data;

    data["phone"] =
        phone;

    data["register"] =
        registerMode;


    return m_net->request(
        Protocol::makeRequest(
            Protocol::MsgType::Login,
            data));
}


// ============================================================================
// 登录成功后进入主窗口
// ============================================================================
void LoginWindow::enterMainWindow(
    const QJsonObject &userData)
{
    const QString token =
        userData.value(
            "token")
            .toString();


    if (token.isEmpty()) {

        QMessageBox::warning(
            this,
            QStringLiteral(
                "登录失败"),
            QStringLiteral(
                "服务器未返回 token"));

        return;
    }


    auto *mainWin =
        new MainWindow(
            userData.value("id")
                .toVariant()
                .toLongLong(),

            userData.value(
                "nickname")
                .toString(),

            userData.value(
                "phone")
                .toString(),

            userData.value(
                "balance")
                .toDouble(),

            token);


    mainWin->setAttribute(
        Qt::WA_DeleteOnClose);

    mainWin->show();

    close();
}
