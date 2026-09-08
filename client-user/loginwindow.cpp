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
#include <QPainter>
#include <QPainterPath>
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


namespace
{

// 登录页浅色主视觉背景：用代码绘制柔和渐变与城市剪影，
// 车辆和充电桩仍使用项目内现有透明 PNG。
class LoginHeroFrame final : public QFrame
{
public:
    explicit LoginHeroFrame(QWidget *parent = nullptr)
        : QFrame(parent)
    {
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QFrame::paintEvent(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        QLinearGradient background(0, 0, width(), height());
        background.setColorAt(0.0, QColor("#FFFFFF"));
        background.setColorAt(0.55, QColor("#F4F8F6"));
        background.setColorAt(1.0, QColor("#EAF2EE"));
        painter.fillRect(rect(), background);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(218, 229, 224, 72));
        const int ground = qRound(height() * 0.72);
        const int unit = qMax(16, width() / 18);

        for (int i = 0; i < 15; ++i) {
            const int buildingHeight =
                unit * (2 + (i * 7) % 5) / 2;
            painter.drawRect(i * unit - unit / 2,
                             ground - buildingHeight,
                             unit - 3,
                             buildingHeight);
        }

        painter.setBrush(QColor("#70E889"));
        painter.drawRoundedRect(QRectF(width() - 48, 66, 25, 3), 1.5, 1.5);

    }
};


class LoginLowerFrame final : public QFrame
{
public:
    explicit LoginLowerFrame(QWidget *parent = nullptr)
        : QFrame(parent)
    {
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QFrame::paintEvent(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), QColor("#171D27"));

        const QWidget *benefits =
            findChild<QWidget *>(QStringLiteral("loginBenefits"));

        const qreal boundary =
            benefits
                ? benefits->geometry().top()
                : height() * 0.72;

        QPainterPath whiteArea;
        whiteArea.moveTo(0, 0);
        whiteArea.lineTo(width(), 0);
        whiteArea.lineTo(width(), boundary - 22);

        // 连续波浪：右侧低谷 → 中右波峰 → 中部低谷 → 左侧波峰。
        // 相比之前单一的 V 形弧线，这里有两组平滑起伏。
        whiteArea.cubicTo(width() * 0.91, boundary - 7,
                          width() * 0.84, boundary + 5,
                          width() * 0.74, boundary + 7);
        whiteArea.cubicTo(width() * 0.64, boundary + 9,
                          width() * 0.59, boundary - 10,
                          width() * 0.49, boundary - 8);
        whiteArea.cubicTo(width() * 0.39, boundary - 6,
                          width() * 0.32, boundary + 10,
                          width() * 0.22, boundary + 6);
        whiteArea.cubicTo(width() * 0.13, boundary + 3,
                          width() * 0.07, boundary - 13,
                          0, boundary - 20);
        whiteArea.closeSubpath();
        painter.fillPath(whiteArea, Qt::white);
    }
};

} // namespace


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
        0,
        0,
        0,
        0);

    contentLayout->setSpacing(
        0);


    // =========================================================================
    // Hero 主视觉区域
    // =========================================================================
    auto *heroCard =
        new LoginHeroFrame(
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
        28,
        20,
        28,
        14);

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

    // 参考图顶部 Logo 位按要求去除。
    eyebrow->hide();


    // -------------------------------------------------------------------------
    // 品牌标题
    // -------------------------------------------------------------------------
    auto *brand =
        new QLabel(
            QStringLiteral(
                "绿色出行\n充电更美好的未来"),
            heroCard);

    brand->setObjectName(
        QStringLiteral(
            "loginBrand"));

    brand->setAlignment(
        Qt::AlignRight | Qt::AlignTop);


    heroLayout->addWidget(
        brand,
        0,
        Qt::AlignRight);


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

    heroTitle->hide();


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

    subTitle->hide();


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
        0);


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


    // 让充电桩向左压到车尾，形成贴合而不是两个独立图片。
    chargerImage->setStyleSheet(
        QStringLiteral(
            "QLabel#loginChargerImage{margin-left:-28px;}"));


    visualRow->addStretch(1);

    visualRow->addWidget(
        carImage);

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

    accentLine->hide();


    contentLayout->addWidget(
        heroCard);


    // 白色表单与底部深色优势区共用一个自绘容器，形成参考图波浪边界。
    auto *lowerPanel = new LoginLowerFrame(content);
    lowerPanel->setObjectName(QStringLiteral("loginLowerPanel"));

    auto *lowerLayout = new QVBoxLayout(lowerPanel);
    lowerLayout->setContentsMargins(0, 0, 0, 0);
    lowerLayout->setSpacing(0);

    // =========================================================================
    // 登录主卡片
    // =========================================================================
    auto *card =
        new QFrame(
            lowerPanel);

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
                "欢迎登录"),
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
                "连接附近充电站，开启便捷出行"),
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

    tip->hide();


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

    phoneCaption->hide();


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


    // 手机图标直接绘制，避免部分运行环境不加载 SVG 插件。
    QPixmap phonePixmap(28, 28);
    phonePixmap.fill(Qt::transparent);
    QPainter phonePainter(&phonePixmap);
    phonePainter.setRenderHint(QPainter::Antialiasing);
    phonePainter.setPen(QPen(QColor("#5F6973"), 2));
    phonePainter.setBrush(Qt::NoBrush);
    phonePainter.drawRoundedRect(QRectF(8, 4, 12, 20), 2.5, 2.5);
    phonePainter.drawLine(QPointF(12, 20), QPointF(16, 20));
    phonePainter.end();

    m_phoneEdit->addAction(
        QIcon(phonePixmap),
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
                "登录  →"),
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
                "注册账号"),
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
                "新用户可直接注册"),
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


    lowerLayout->addWidget(
        card);


    // =========================================================================
    // 底部优势区（无点击行为）
    // =========================================================================
    auto *benefits = new QFrame(lowerPanel);
    benefits->setObjectName(QStringLiteral("loginBenefits"));
    benefits->setAttribute(Qt::WA_StyledBackground, true);

    auto *benefitLayout = new QHBoxLayout(benefits);
    benefitLayout->setContentsMargins(22, 36, 22, 18);
    benefitLayout->setSpacing(8);

    const QStringList benefitItems = {
        QStringLiteral("◇\n安全可靠"),
        QStringLiteral("♧\n绿色低碳"),
        QStringLiteral("ϟ\n便捷高效")
    };

    for (const QString &text : benefitItems) {
        auto *label = new QLabel(text, benefits);
        label->setObjectName(QStringLiteral("loginBenefitItem"));
        label->setAlignment(Qt::AlignCenter);
        benefitLayout->addWidget(label, 1);
    }

    lowerLayout->addWidget(benefits);

    contentLayout->addWidget(lowerPanel);


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
    QString loginQss =
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
            "background:transparent;"
            "border:none;"
            "border-radius:0;"
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
            "color:#7E8893;"
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
            "font-size:11px;"
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
            "background:transparent;"
            "border:none;"
            "border-radius:0;"
            "}"

            "QLabel#loginWelcome{"
            "background:transparent;"
            "color:%2;"
            "font-size:24px;"
            "font-weight:800;"
            "}"

            "QLabel#loginSectionTitle{"
            "background:transparent;"
            "color:%2;"
            "font-size:16px;"
            "font-weight:700;"
            "}"

            "QLabel#loginTip{"
            "background:transparent;"
            "color:%15;"
            "font-size:12px;"
            "}"

            "QLabel#loginPhoneCaption{"
            "background:transparent;"
            "color:%2;"
            "font-size:12px;"
            "font-weight:650;"
            "}"

            "QLabel#loginHint{"
            "background:transparent;"
            "color:%15;"
            "font-size:11px;"
            "padding-top:3px;"
            "}"

            "QFrame#loginBenefits{"
            "background:transparent;"
            "border:none;"
            "}"

            "QFrame#loginLowerPanel{"
            "background:transparent;"
            "border:none;"
            "}"

            "QLabel#loginBenefitItem{"
            "background:transparent;"
            "color:#C3CBD2;"
            "border:none;"
            "font-size:11px;"
            "font-weight:650;"
            "}")

        ;

    // Replace highest-numbered tokens first.  Replacing %1 first would also
    // partially match %10..%17 and produce invalid QSS such as #74EC8Bpx.
    loginQss
        .replace(QStringLiteral("%17"), QString::number(tinyFont))
        .replace(QStringLiteral("%16"), QString::number(smallFont))
        .replace(QStringLiteral("%15"), UiTheme::textSecondary())
        .replace(QStringLiteral("%14"), QString::number(sectionFont))
        .replace(QStringLiteral("%13"), QString::number(welcomeFont))
        .replace(QStringLiteral("%12"), QString::number(cardRadius))
        .replace(QStringLiteral("%11"), UiTheme::border())
        .replace(QStringLiteral("%10"), QString::number(tinyFont))
        .replace(QStringLiteral("%9"), QString::number(heroFont))
        .replace(QStringLiteral("%8"), QString::number(brandFont))
        .replace(QStringLiteral("%7"), QString::number(eyebrowFont))
        .replace(QStringLiteral("%6"), QString::number(scaledUi(this, 9)))
        .replace(QStringLiteral("%5"), UiTheme::lime())
        .replace(QStringLiteral("%4"), QString::number(heroRadius))
        .replace(QStringLiteral("%3"), UiTheme::dark())
        .replace(QStringLiteral("%2"), UiTheme::textPrimary())
        .replace(QStringLiteral("%1"), UiTheme::pageBackground());

    setStyleSheet(loginQss);


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
            QStringLiteral(
                "QPushButton{"
                "background:#70E889;"
                "color:#171D27;"
                "border:none;"
                "border-radius:%1px;"
                "font-size:%2px;"
                "font-weight:850;"
                "}"
                "QPushButton:hover{background:#63E27C;}"
                "QPushButton:pressed{background:#52D96E;}"
                "QPushButton:disabled{background:#DCE3DF;color:#9AA39E;}"
            ).arg(controlRadius).arg(controlFont));


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
            scaledUi(this, 28),
            scaledUi(this, 20),
            scaledUi(this, 28),
            scaledUi(this, 14));

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

        carLabel->setAlignment(
            Qt::AlignRight | Qt::AlignBottom);

        const int carWidth =
            scaledUi(
                this,
                330);

        const int carHeight =
            scaledUi(
                this,
                170);


        carLabel->setMinimumHeight(
            carHeight);

        carLabel->setMaximumHeight(
            carHeight);


        const QPixmap source(
            QStringLiteral(
                ":/images/car-suv.png"));


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
                94);

        const int chargerHeight =
            scaledUi(
                this,
                170);


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
            scaledUi(this, 34),
            scaledUi(this, 22),
            scaledUi(this, 34),
            scaledUi(this, 46));

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
            0,
            0,
            0,
            0);

        contentLayout->setSpacing(
            0);
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
            "登录  →"));


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
