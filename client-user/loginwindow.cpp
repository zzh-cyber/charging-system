#include "loginwindow.h"

#include "appmessagebox.h"
#include "mainwindow.h"
#include "netclient.h"
#include "protocol.h"
#include "uitheme.h"
#include "windowhelper.h"

#include <QDebug>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSizePolicy>
#include <QVBoxLayout>


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
        QStringLiteral(
            "loginWindow"));

    setWindowTitle(
        QStringLiteral(
            "充电用户端 - 登录"));

    applyPhoneWindow(
        this);


    // =========================================================================
    // 根布局
    // =========================================================================
    auto *rootLayout =
        new QVBoxLayout(this);

    rootLayout->setContentsMargins(
        0,
        0,
        0,
        0);

    rootLayout->setSpacing(
        0);


    // =========================================================================
    // 小屏幕兜底：
    // 正常高度不会滚动，窗口高度不足时允许纵向滚动
    // =========================================================================
    auto *scrollArea =
        new QScrollArea(this);

    scrollArea->setObjectName(
        QStringLiteral(
            "loginScrollArea"));

    scrollArea->setWidgetResizable(
        true);

    scrollArea->setFrameShape(
        QFrame::NoFrame);

    scrollArea->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAlwaysOff);


    auto *content =
        new QWidget;

    content->setObjectName(
        QStringLiteral(
            "loginContent"));


    auto *contentLayout =
        new QVBoxLayout(
            content);

    contentLayout->setObjectName(
        QStringLiteral(
            "loginContentLayout"));

    contentLayout->setContentsMargins(
        18,
        18,
        18,
        18);

    contentLayout->setSpacing(
        14);


    // =========================================================================
    // Hero 主视觉区域
    // =========================================================================
    auto *heroCard =
        new QFrame(
            content);

    heroCard->setObjectName(
        QStringLiteral(
            "loginHeroCard"));

    heroCard->setAttribute(
        Qt::WA_StyledBackground,
        true);

    UiTheme::applyHeroShadow(
        heroCard,
        34,
        9);


    auto *heroLayout =
        new QVBoxLayout(
            heroCard);

    heroLayout->setObjectName(
        QStringLiteral(
            "loginHeroLayout"));

    heroLayout->setContentsMargins(
        24,
        23,
        24,
        18);

    heroLayout->setSpacing(
        6);


    // -------------------------------------------------------------------------
    // 顶部小标签
    // -------------------------------------------------------------------------
    auto *eyebrow =
        new QLabel(
            QStringLiteral(
                "GREEN ENERGY"),
            heroCard);

    eyebrow->setObjectName(
        QStringLiteral(
            "loginEyebrow"));


    heroLayout->addWidget(
        eyebrow,
        0,
        Qt::AlignLeft);


    // -------------------------------------------------------------------------
    // 品牌标题
    // -------------------------------------------------------------------------
    auto *brand =
        new QLabel(
            QStringLiteral(
                "新能源充电"),
            heroCard);

    brand->setObjectName(
        QStringLiteral(
            "loginBrand"));


    heroLayout->addWidget(
        brand);


    // -------------------------------------------------------------------------
    // 主文案
    // -------------------------------------------------------------------------
    auto *heroTitle =
        new QLabel(
            QStringLiteral(
                "让充电出行，\n更简单"),
            heroCard);

    heroTitle->setObjectName(
        QStringLiteral(
            "loginHeroTitle"));

    heroTitle->setWordWrap(
        true);


    heroLayout->addWidget(
        heroTitle);


    // -------------------------------------------------------------------------
    // 副文案
    // -------------------------------------------------------------------------
    auto *subTitle =
        new QLabel(
            QStringLiteral(
                "CHARGE · DRIVE · LIVE"),
            heroCard);

    subTitle->setObjectName(
        QStringLiteral(
            "loginSubTitle"));


    heroLayout->addWidget(
        subTitle);


    heroLayout->addSpacing(
        5);


    // =========================================================================
    // 汽车 + 充电桩主视觉
    // =========================================================================
    auto *visualRow =
        new QHBoxLayout;

    visualRow->setObjectName(
        QStringLiteral(
            "loginVisualLayout"));

    visualRow->setContentsMargins(
        0,
        0,
        0,
        0);

    visualRow->setSpacing(
        2);


    auto *carImage =
        new QLabel(
            heroCard);

    carImage->setObjectName(
        QStringLiteral(
            "loginCarImage"));

    carImage->setAlignment(
        Qt::AlignLeft |
        Qt::AlignBottom);

    carImage->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Preferred);


    auto *chargerImage =
        new QLabel(
            heroCard);

    chargerImage->setObjectName(
        QStringLiteral(
            "loginChargerImage"));

    chargerImage->setAlignment(
        Qt::AlignRight |
        Qt::AlignBottom);

    chargerImage->setSizePolicy(
        QSizePolicy::Preferred,
        QSizePolicy::Preferred);


    visualRow->addWidget(
        carImage,
        1);

    visualRow->addWidget(
        chargerImage);


    heroLayout->addLayout(
        visualRow);


    // -------------------------------------------------------------------------
    // 底部荧光绿视觉线
    // -------------------------------------------------------------------------
    auto *accentLine =
        new QFrame(
            heroCard);

    accentLine->setObjectName(
        QStringLiteral(
            "loginHeroAccent"));

    accentLine->setFixedHeight(
        5);


    heroLayout->addWidget(
        accentLine);


    contentLayout->addWidget(
        heroCard);


    // =========================================================================
    // 登录主卡片
    // =========================================================================
    auto *card =
        new QFrame(
            content);

    card->setObjectName(
        QStringLiteral(
            "loginCard"));

    card->setAttribute(
        Qt::WA_StyledBackground,
        true);

    UiTheme::applyCardShadow(
        card,
        28,
        7);


    auto *cardLayout =
        new QVBoxLayout(
            card);

    cardLayout->setObjectName(
        QStringLiteral(
            "loginCardLayout"));

    cardLayout->setContentsMargins(
        24,
        22,
        24,
        24);

    cardLayout->setSpacing(
        10);


    // =========================================================================
    // 欢迎标题
    // =========================================================================
    auto *welcomeLabel =
        new QLabel(
            QStringLiteral(
                "欢迎回来"),
            card);

    welcomeLabel->setObjectName(
        QStringLiteral(
            "loginWelcome"));


    cardLayout->addWidget(
        welcomeLabel);


    // =========================================================================
    // 登录区域标题
    // =========================================================================
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


    // =========================================================================
    // 登录说明
    // =========================================================================
    auto *tip =
        new QLabel(
            QStringLiteral(
                "无需验证码，输入手机号即可登录；"
                "新用户可直接注册"),
            card);

    tip->setObjectName(
        QStringLiteral(
            "loginTip"));

    tip->setWordWrap(
        true);


    cardLayout->addWidget(
        tip);


    cardLayout->addSpacing(
        5);


    // =========================================================================
    // 手机号标题
    // =========================================================================
    auto *phoneCaption =
        new QLabel(
            QStringLiteral(
                "手机号码"),
            card);

    phoneCaption->setObjectName(
        QStringLiteral(
            "loginPhoneCaption"));


    cardLayout->addWidget(
        phoneCaption);


    // =========================================================================
    // 手机号输入框
    // =========================================================================
    m_phoneEdit =
        new QLineEdit(
            card);

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


    // 手机 SVG 图标
    m_phoneEdit->addAction(
        QIcon(
            QStringLiteral(
                ":/icons/phone.svg")),
        QLineEdit::LeadingPosition);


    cardLayout->addWidget(
        m_phoneEdit);


    cardLayout->addSpacing(
        5);


    // =========================================================================
    // 登录按钮
    // =========================================================================
    m_loginBtn =
        new QPushButton(
            QStringLiteral(
                "登录"),
            card);

    m_loginBtn->setObjectName(
        QStringLiteral(
            "loginPrimaryButton"));

    m_loginBtn->setCursor(
        Qt::PointingHandCursor);


    cardLayout->addWidget(
        m_loginBtn);


    // =========================================================================
    // 注册按钮
    // =========================================================================
    m_registerBtn =
        new QPushButton(
            QStringLiteral(
                "注册"),
            card);

    m_registerBtn->setObjectName(
        QStringLiteral(
            "loginSecondaryButton"));

    m_registerBtn->setCursor(
        Qt::PointingHandCursor);


    cardLayout->addWidget(
        m_registerBtn);


    // =========================================================================
    // 动态提示
    // =========================================================================
    m_hint =
        new QLabel(
            QStringLiteral(
                "已有账号直接登录，新用户可使用当前手机号注册"),
            card);

    m_hint->setObjectName(
        QStringLiteral(
            "loginHint"));

    m_hint->setAlignment(
        Qt::AlignLeft |
        Qt::AlignVCenter);

    m_hint->setWordWrap(
        true);


    cardLayout->addWidget(
        m_hint);


    contentLayout->addWidget(
        card);


    contentLayout->addStretch(
        1);


    scrollArea->setWidget(
        content);

    rootLayout->addWidget(
        scrollArea);


    // =========================================================================
    // 登录按钮事件
    // =========================================================================
    connect(
        m_loginBtn,
        &QPushButton::clicked,
        this,
        &LoginWindow::onLoginClicked);


    // =========================================================================
    // 注册按钮事件
    // =========================================================================
    connect(
        m_registerBtn,
        &QPushButton::clicked,
        this,
        &LoginWindow::onRegisterClicked);


    // 回车登录
    connect(
        m_phoneEdit,
        &QLineEdit::returnPressed,
        m_loginBtn,
        &QPushButton::click);


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
    const int eyebrowFont =
        scaledUi(
            this,
            10);

    const int brandFont =
        scaledUi(
            this,
            15);

    const int heroFont =
        scaledUi(
            this,
            29);

    const int welcomeFont =
        scaledUi(
            this,
            24);

    const int sectionFont =
        scaledUi(
            this,
            16);

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

    const int heroRadius =
        scaledUi(
            this,
            26);

    const int cardRadius =
        scaledUi(
            this,
            24);

    const int controlRadius =
        scaledUi(
            this,
            15);


    // =========================================================================
    // 页面整体
    // =========================================================================
    setStyleSheet(
        QStringLiteral(

            // ================================================================
            // 页面
            // ================================================================
            "QWidget#loginWindow{"
            "background:%1;"
            "color:%2;"
            "}"

            "QWidget#loginContent{"
            "background:%1;"
            "}"

            "QScrollArea#loginScrollArea{"
            "background:%1;"
            "border:none;"
            "}"

            // ================================================================
            // Hero
            // ================================================================
            "QFrame#loginHeroCard{"
            "background:%3;"
            "border:none;"
            "border-radius:%4px;"
            "}"

            "QLabel#loginEyebrow{"
            "background:%5;"
            "color:#18301F;"
            "border:none;"
            "border-radius:%6px;"
            "font-size:%7px;"
            "font-weight:800;"
            "padding:4px 9px;"
            "}"

            "QLabel#loginBrand{"
            "background:transparent;"
            "color:#C9D0D6;"
            "font-size:%8px;"
            "font-weight:650;"
            "}"

            "QLabel#loginHeroTitle{"
            "background:transparent;"
            "color:#FFFFFF;"
            "font-size:%9px;"
            "font-weight:800;"
            "}"

            "QLabel#loginSubTitle{"
            "background:transparent;"
            "color:#8E99A5;"
            "font-size:%10px;"
            "font-weight:600;"
            "}"

            "QLabel#loginCarImage,"
            "QLabel#loginChargerImage{"
            "background:transparent;"
            "border:none;"
            "}"

            "QFrame#loginHeroAccent{"
            "background:%5;"
            "border:none;"
            "border-radius:2px;"
            "}"

            // ================================================================
            // 登录卡
            // ================================================================
            "QFrame#loginCard{"
            "background:#FFFFFF;"
            "border:1px solid %11;"
            "border-radius:%12px;"
            "}"

            "QLabel#loginWelcome{"
            "background:transparent;"
            "color:%2;"
            "font-size:%13px;"
            "font-weight:800;"
            "}"

            "QLabel#loginSectionTitle{"
            "background:transparent;"
            "color:%2;"
            "font-size:%14px;"
            "font-weight:700;"
            "}"

            "QLabel#loginTip{"
            "background:transparent;"
            "color:%15;"
            "font-size:%16px;"
            "}"

            "QLabel#loginPhoneCaption{"
            "background:transparent;"
            "color:%2;"
            "font-size:%16px;"
            "font-weight:650;"
            "}"

            "QLabel#loginHint{"
            "background:transparent;"
            "color:%15;"
            "font-size:%17px;"
            "padding-top:3px;"
            "}")

        .arg(
            UiTheme::pageBackground())     // %1

        .arg(
            UiTheme::textPrimary())        // %2

        .arg(
            UiTheme::dark())               // %3

        .arg(
            heroRadius)                    // %4

        .arg(
            UiTheme::lime())               // %5

        .arg(
            scaledUi(this, 9))             // %6

        .arg(
            eyebrowFont)                   // %7

        .arg(
            brandFont)                     // %8

        .arg(
            heroFont)                      // %9

        .arg(
            tinyFont)                      // %10

        .arg(
            UiTheme::border())             // %11

        .arg(
            cardRadius)                    // %12

        .arg(
            welcomeFont)                   // %13

        .arg(
            sectionFont)                   // %14

        .arg(
            UiTheme::textSecondary())      // %15

        .arg(
            smallFont)                     // %16

        .arg(
            tinyFont));                    // %17


    // =========================================================================
    // 手机号输入框
    // =========================================================================
    if (m_phoneEdit) {

        m_phoneEdit->setStyleSheet(
            UiTheme::inputStyle(
                controlFont,
                controlRadius));


        m_phoneEdit->setMinimumHeight(
            scaledUi(
                this,
                50));
    }


    // =========================================================================
    // 登录按钮
    // =========================================================================
    if (m_loginBtn) {

        m_loginBtn->setStyleSheet(
            UiTheme::primaryButtonStyle(
                controlFont,
                controlRadius));


        m_loginBtn->setMinimumHeight(
            scaledUi(
                this,
                50));
    }


    // =========================================================================
    // 注册按钮
    // =========================================================================
    if (m_registerBtn) {

        m_registerBtn->setStyleSheet(
            UiTheme::secondaryButtonStyle(
                controlFont,
                controlRadius));


        m_registerBtn->setMinimumHeight(
            scaledUi(
                this,
                50));
    }


    // =========================================================================
    // Hero 布局
    // =========================================================================
    if (auto *heroLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "loginHeroLayout"))) {

        heroLayout->setContentsMargins(
            scaledUi(this, 24),
            scaledUi(this, 22),
            scaledUi(this, 24),
            scaledUi(this, 18));

        heroLayout->setSpacing(
            scaledUi(
                this,
                6));
    }


    // =========================================================================
    // 汽车主视觉
    // =========================================================================
    if (auto *carLabel =
            findChild<QLabel *>(
                QStringLiteral(
                    "loginCarImage"))) {

        const int carWidth =
            scaledUi(
                this,
                255);

        const int carHeight =
            scaledUi(
                this,
                118);


        carLabel->setMinimumHeight(
            carHeight);

        carLabel->setMaximumHeight(
            carHeight);


        const QPixmap source(
            QStringLiteral(
                ":/images/car-main.png"));


        if (!source.isNull()) {

            carLabel->setPixmap(
                source.scaled(
                    carWidth,
                    carHeight,
                    Qt::KeepAspectRatio,
                    Qt::SmoothTransformation));
        }
    }


    // =========================================================================
    // 充电桩主视觉
    // =========================================================================
    if (auto *chargerLabel =
            findChild<QLabel *>(
                QStringLiteral(
                    "loginChargerImage"))) {

        const int chargerWidth =
            scaledUi(
                this,
                68);

        const int chargerHeight =
            scaledUi(
                this,
                118);


        chargerLabel->setFixedSize(
            chargerWidth,
            chargerHeight);


        const QPixmap source(
            QStringLiteral(
                ":/images/charger-main.png"));


        if (!source.isNull()) {

            chargerLabel->setPixmap(
                source.scaled(
                    chargerWidth,
                    chargerHeight,
                    Qt::KeepAspectRatio,
                    Qt::SmoothTransformation));
        }
    }


    // =========================================================================
    // 登录卡内部间距
    // =========================================================================
    if (auto *cardLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "loginCardLayout"))) {

        cardLayout->setContentsMargins(
            scaledUi(this, 24),
            scaledUi(this, 22),
            scaledUi(this, 24),
            scaledUi(this, 24));

        cardLayout->setSpacing(
            scaledUi(
                this,
                10));
    }


    // =========================================================================
    // 页面外围间距
    // =========================================================================
    if (auto *contentLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "loginContentLayout"))) {

        contentLayout->setContentsMargins(
            scaledUi(this, 18),
            scaledUi(this, 18),
            scaledUi(this, 18),
            scaledUi(this, 18));

        contentLayout->setSpacing(
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

        AppMessageBox::warning(
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
            "color:#7E8893;"));


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
                    "color:#E6AE46;"));

            return;
        }


        AppMessageBox::warning(
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
                "color:#7E8893;"));

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
            "color:#7E8893;"));


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

        AppMessageBox::warning(
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
                "color:#E26868;"));


        AppMessageBox::warning(
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

        AppMessageBox::warning(
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
