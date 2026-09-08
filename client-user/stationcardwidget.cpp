#include "stationcardwidget.h"

#include "uitheme.h"
#include "windowhelper.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonValue>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSize>
#include <QSizePolicy>
#include <QVBoxLayout>

#include <cmath>


namespace {

// ============================================================================
// 安全读取 JSON 数值
// ============================================================================
bool readNumber(
    const QJsonObject &object,
    const QString &key,
    double &value)
{
    const QJsonValue jsonValue =
        object.value(
            key);


    if (!jsonValue.isDouble()) {

        return false;
    }


    value =
        jsonValue.toDouble();


    return std::isfinite(
        value);
}


// ============================================================================
// 经纬度合法性检查
// ============================================================================
bool validCoordinate(
    double lat,
    double lng)
{
    return
        std::isfinite(lat) &&
        std::isfinite(lng) &&
        lat >= -90.0 &&
        lat <= 90.0 &&
        lng >= -180.0 &&
        lng <= 180.0;
}

} // namespace


// ============================================================================
// 构造函数
// ============================================================================
StationCardWidget::StationCardWidget(
    const QJsonObject &station,
    QWidget *parent)
    : QFrame(parent)
{
    setObjectName(
        QStringLiteral(
            "stationCard"));


    setAttribute(
        Qt::WA_StyledBackground,
        true);


    setAttribute(
        Qt::WA_Hover,
        true);


    setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Preferred);


    UiTheme::applyCardShadow(
        this,
        22,
        5);


    // =========================================================================
    // 业务主键
    // =========================================================================
    m_stationId =
        station.value(
                   QStringLiteral(
                       "id"))
            .toVariant()
            .toLongLong();


    m_name =
        station.value(
                   QStringLiteral(
                       "name"))
            .toString()
            .trimmed();


    if (m_name.isEmpty()) {

        m_name =
            QStringLiteral(
                "--");
    }


    const QString address =
        station.value(
                   QStringLiteral(
                       "address"))
            .toString()
            .trimmed();


    // =========================================================================
    // 字段安全读取
    // =========================================================================
    double price =
        0.0;


    const bool hasPrice =
        readNumber(
            station,
            QStringLiteral(
                "price"),
            price)
        &&
        price >= 0.0;


    double totalValue =
        0.0;


    double idleValue =
        0.0;


    const bool hasTotal =
        readNumber(
            station,
            QStringLiteral(
                "total"),
            totalValue)
        &&
        totalValue >= 0.0;


    const bool hasIdle =
        readNumber(
            station,
            QStringLiteral(
                "idle"),
            idleValue)
        &&
        idleValue >= 0.0;


    const int total =
        static_cast<int>(
            totalValue);


    const int idle =
        static_cast<int>(
            idleValue);


    // =========================================================================
    // 经纬度
    // =========================================================================
    double latitude =
        0.0;


    double longitude =
        0.0;


    const bool hasLatitude =
        readNumber(
            station,
            QStringLiteral(
                "latitude"),
            latitude);


    const bool hasLongitude =
        readNumber(
            station,
            QStringLiteral(
                "longitude"),
            longitude);


    if (hasLatitude &&
        hasLongitude &&
        validCoordinate(
            latitude,
            longitude)) {

        m_latitude =
            latitude;


        m_longitude =
            longitude;


        m_hasCoordinate =
            true;
    }


    // =========================================================================
    // 距离
    // =========================================================================
    double distance =
        0.0;


    if (readNumber(
            station,
            QStringLiteral(
                "distance"),
            distance)
        &&
        distance >= 0.0) {

        m_distance =
            distance;


        m_hasDistance =
            true;
    }


    // =========================================================================
    // 状态
    // =========================================================================
    const bool hasPileInfo =
        hasIdle &&
        hasTotal;


    const bool full =
        hasPileInfo &&
        idle == 0;


    // =========================================================================
    // 整体布局
    // =========================================================================
    auto *mainLayout =
        new QVBoxLayout(
            this);


    mainLayout->setObjectName(
        QStringLiteral(
            "stationCardLayout"));


    mainLayout->setContentsMargins(
        0,
        0,
        0,
        0);


    mainLayout->setSpacing(
        0);


    // 上半部深色站点信息区。
    auto *heroPanel =
        new QFrame(this);

    heroPanel->setObjectName(
        QStringLiteral("stationHeroPanel"));

    heroPanel->setAttribute(
        Qt::WA_StyledBackground,
        true);

    auto *heroPanelLayout =
        new QVBoxLayout(heroPanel);

    heroPanelLayout->setObjectName(
        QStringLiteral("stationHeroPanelLayout"));

    heroPanelLayout->setContentsMargins(18, 17, 14, 15);
    heroPanelLayout->setSpacing(10);


    // =========================================================================
    // 顶部视觉区
    //
    // 左侧充电桩图片
    // 右侧站名 / 单价 / 地址
    // =========================================================================
    auto *heroRow =
        new QHBoxLayout;


    heroRow->setObjectName(
        QStringLiteral(
            "stationHeroLayout"));


    heroRow->setSpacing(
        12);


    // -------------------------------------------------------------------------
    // 充电桩缩略图背景
    // -------------------------------------------------------------------------
    auto *visualFrame =
        new QFrame(
            this);


    visualFrame->setObjectName(
        QStringLiteral(
            "stationVisualFrame"));


    visualFrame->setAttribute(
        Qt::WA_StyledBackground,
        true);


    auto *visualLayout =
        new QVBoxLayout(
            visualFrame);


    visualLayout->setContentsMargins(
        6,
        6,
        6,
        6);


    visualLayout->setAlignment(
        Qt::AlignCenter);


    auto *chargerImage =
        new QLabel(
            visualFrame);


    chargerImage->setObjectName(
        QStringLiteral(
            "stationChargerImage"));


    chargerImage->setAlignment(
        Qt::AlignCenter);


    visualLayout->addWidget(
        chargerImage);


    // -------------------------------------------------------------------------
    // 右侧主要信息
    // -------------------------------------------------------------------------
    auto *infoLayout =
        new QVBoxLayout;


    infoLayout->setObjectName(
        QStringLiteral(
            "stationInfoLayout"));


    infoLayout->setSpacing(
        7);


    // -------------------------------------------------------------------------
    // 站名 + 单价
    // -------------------------------------------------------------------------
    auto *titleRow =
        new QHBoxLayout;


    titleRow->setObjectName(
        QStringLiteral(
            "stationTitleRow"));


    titleRow->setSpacing(
        8);


    auto *nameLabel =
        new QLabel(
            m_name,
            this);


    nameLabel->setObjectName(
        QStringLiteral(
            "stationNameLabel"));


    nameLabel->setWordWrap(
        true);


    QString priceText =
        QStringLiteral(
            "--");


    if (hasPrice) {

        priceText =
            QStringLiteral(
                "￥%1/度")
                .arg(
                    price,
                    0,
                    'f',
                    2);
    }


    auto *priceLabel =
        new QLabel(
            priceText,
            this);


    priceLabel->setObjectName(
        QStringLiteral(
            "stationPriceLabel"));


    priceLabel->setAlignment(
        Qt::AlignRight |
        Qt::AlignTop);


    titleRow->addWidget(
        nameLabel,
        1);


    titleRow->addWidget(
        priceLabel);


    infoLayout->addLayout(
        titleRow);


    // -------------------------------------------------------------------------
    // 地址
    // -------------------------------------------------------------------------
    auto *addressRow =
        new QHBoxLayout;


    addressRow->setObjectName(
        QStringLiteral(
            "stationAddressLayout"));


    addressRow->setSpacing(
        6);


    auto *locationIcon =
        new QLabel(
            this);


    locationIcon->setObjectName(
        QStringLiteral(
            "stationAddressIcon"));


    locationIcon->setAlignment(
        Qt::AlignTop |
        Qt::AlignHCenter);


    auto *addressLabel =
        new QLabel(
            address.isEmpty()
                ? QStringLiteral(
                      "站点地址暂未提供")
                : address,
            this);


    addressLabel->setObjectName(
        QStringLiteral(
            "stationAddressLabel"));


    addressLabel->setWordWrap(
        true);


    addressRow->addWidget(
        locationIcon);


    addressRow->addWidget(
        addressLabel,
        1);


    infoLayout->addLayout(
        addressRow);


    heroRow->addLayout(
        infoLayout,
        1);


    // 参考图：站点文字在左，充电桩图片在右。
    // 给独立悬浮的充电桩图片预留空间。图片本身不进入布局，
    // 才能跨越深色区和白色服务区。
    heroRow->addSpacing(106);


    heroPanelLayout->addLayout(
        heroRow);


    // =========================================================================
    // 状态 + 距离
    // =========================================================================
    auto *metaRow =
        new QHBoxLayout;


    metaRow->setObjectName(
        QStringLiteral(
            "stationMetaLayout"));


    metaRow->setSpacing(
        10);


    QString statusText;


    QString statusProperty;


    if (!hasPileInfo) {

        statusText =
            QStringLiteral(
                "状态待更新");


        statusProperty =
            QStringLiteral(
                "unknown");

    } else if (full) {

        statusText =
            QStringLiteral(
                "已满");


        statusProperty =
            QStringLiteral(
                "full");

    } else {

        statusText =
            QStringLiteral(
                "可预约");


        statusProperty =
            QStringLiteral(
                "available");
    }


    auto *statusBadge =
        new QLabel(
            statusText,
            this);


    statusBadge->setObjectName(
        QStringLiteral(
            "stationStatusBadge"));


    statusBadge->setProperty(
        "stationState",
        statusProperty);


    statusBadge->setAlignment(
        Qt::AlignCenter);


    const QString distanceText =
        m_hasDistance
            ? QStringLiteral(
                  "距您 %1 km")
                  .arg(
                      m_distance,
                      0,
                      'f',
                      1)
            : QStringLiteral(
                  "距离暂不可用");


    // -------------------------------------------------------------------------
    // NO.8：
    // 距离本身继续作为可点击的路线规划入口
    // -------------------------------------------------------------------------
    auto *distanceButton =
        new QPushButton(
            distanceText,
            this);


    distanceButton->setObjectName(
        QStringLiteral(
            "stationDistanceButton"));


    distanceButton->setFlat(
        true);


    distanceButton->setCursor(
        Qt::PointingHandCursor);


    distanceButton->setIcon(
        QIcon(
            QStringLiteral(
                ":/icons/navigation.svg")));


    metaRow->addWidget(
        statusBadge);


    metaRow->addStretch();


    metaRow->addWidget(
        distanceButton);


    mainLayout->addWidget(
        heroPanel);


    // 下半部白色服务区。
    auto *servicePanel =
        new QFrame(this);

    servicePanel->setObjectName(
        QStringLiteral("stationServicePanel"));

    servicePanel->setAttribute(
        Qt::WA_StyledBackground,
        true);

    auto *serviceLayout =
        new QVBoxLayout(servicePanel);

    serviceLayout->setObjectName(
        QStringLiteral("stationServiceLayout"));

    serviceLayout->setContentsMargins(16, 14, 16, 16);
    serviceLayout->setSpacing(11);

    auto *serviceTitle =
        new QLabel(
            QStringLiteral("充电服务"),
            servicePanel);

    serviceTitle->setObjectName(
        QStringLiteral("stationServiceTitle"));

    serviceLayout->addWidget(
        serviceTitle);


    // 状态与距离也属于服务特征，和桩数一起进入下方横滑卡片。
    metaRow->removeWidget(statusBadge);
    metaRow->removeWidget(distanceButton);


    // =========================================================================
    // 充电桩数据区域
    // =========================================================================
    auto *metricsPanel =
        new QFrame(
            this);


    metricsPanel->setObjectName(
        QStringLiteral(
            "stationMetricsPanel"));


    metricsPanel->setAttribute(
        Qt::WA_StyledBackground,
        true);


    auto *metricsLayout =
        new QHBoxLayout(
            metricsPanel);


    metricsLayout->setObjectName(
        QStringLiteral(
            "stationMetricsLayout"));


    metricsLayout->setContentsMargins(
        14,
        12,
        14,
        12);


    metricsLayout->setSpacing(
        14);


    // 四张同尺寸特征卡横向排列；窄屏时由滚动区左右滑动。
    auto *featureScroll =
        new QScrollArea(servicePanel);

    featureScroll->setObjectName(
        QStringLiteral("stationFeatureScroll"));

    featureScroll->setWidgetResizable(false);
    featureScroll->setFrameShape(QFrame::NoFrame);
    featureScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    featureScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto *featureContainer =
        new QWidget(featureScroll);

    featureContainer->setObjectName(
        QStringLiteral("stationFeatureContainer"));

    auto *featureLayout =
        new QHBoxLayout(featureContainer);

    featureLayout->setObjectName(
        QStringLiteral("stationFeatureLayout"));

    featureLayout->setContentsMargins(0, 0, 0, 0);
    featureLayout->setSpacing(10);

    const auto makeFeatureCard =
        [featureContainer](
            const QString &caption,
            QWidget *valueWidget) {

            auto *card = new QFrame(featureContainer);
            card->setObjectName(QStringLiteral("stationFeatureCard"));
            card->setAttribute(Qt::WA_StyledBackground, true);

            auto *cardLayout = new QVBoxLayout(card);
            cardLayout->setContentsMargins(12, 11, 12, 11);
            cardLayout->setSpacing(5);

            auto *captionLabel = new QLabel(caption, card);
            captionLabel->setObjectName(QStringLiteral("stationMetricCaption"));

            valueWidget->setParent(card);
            cardLayout->addWidget(captionLabel);
            cardLayout->addWidget(valueWidget);
            cardLayout->addStretch();
            return card;
        };


    // -------------------------------------------------------------------------
    // 空闲桩
    // -------------------------------------------------------------------------
    auto *idleCard =
        new QFrame(metricsPanel);

    idleCard->setObjectName(
        QStringLiteral("stationFeatureCard"));

    idleCard->setAttribute(
        Qt::WA_StyledBackground,
        true);

    auto *idleBlock =
        new QVBoxLayout(idleCard);

    idleBlock->setContentsMargins(12, 11, 12, 11);


    idleBlock->setSpacing(
        3);


    const QString idleMetricText =
        hasIdle
            ? QString::number(
                  idle)
            : QStringLiteral(
                  "--");


    auto *idleMetric =
        new QLabel(
            idleMetricText,
            metricsPanel);


    idleMetric->setObjectName(
        QStringLiteral(
            "stationIdleMetricValue"));


    if (!hasIdle) {

        idleMetric->setProperty(
            "availability",
            QStringLiteral(
                "unknown"));

    } else if (idle > 0) {

        idleMetric->setProperty(
            "availability",
            QStringLiteral(
                "available"));

    } else {

        idleMetric->setProperty(
            "availability",
            QStringLiteral(
                "full"));
    }


    idleBlock->addWidget(
        idleMetric);


    // -------------------------------------------------------------------------
    // 总桩数
    // -------------------------------------------------------------------------
    auto *totalCard =
        new QFrame(metricsPanel);

    totalCard->setObjectName(
        QStringLiteral("stationFeatureCard"));

    totalCard->setAttribute(
        Qt::WA_StyledBackground,
        true);

    auto *totalBlock =
        new QVBoxLayout(totalCard);

    totalBlock->setContentsMargins(12, 11, 12, 11);


    totalBlock->setSpacing(
        3);


    const QString totalMetricText =
        hasTotal
            ? QString::number(
                  total)
            : QStringLiteral(
                  "--");


    auto *totalMetric =
        new QLabel(
            totalMetricText,
            metricsPanel);


    totalMetric->setObjectName(
        QStringLiteral(
            "stationMetricValue"));


    totalBlock->addWidget(
        totalMetric);


    // 旧的双列容器只用于创建既有数据控件，转移控件所有权后移除。
    idleMetric->setParent(nullptr);
    totalMetric->setParent(nullptr);
    delete idleCard;
    delete totalCard;
    delete metricsPanel;
    delete metaRow;

    auto *statusCard = makeFeatureCard(QStringLiteral("站点状态"), statusBadge);
    auto *distanceCard = makeFeatureCard(QStringLiteral("站点距离"), distanceButton);
    auto *idleFeatureCard = makeFeatureCard(QStringLiteral("空闲充电桩"), idleMetric);
    auto *totalFeatureCard = makeFeatureCard(QStringLiteral("充电桩总数"), totalMetric);

    featureLayout->addWidget(statusCard);
    featureLayout->addWidget(distanceCard);
    featureLayout->addWidget(idleFeatureCard);
    featureLayout->addWidget(totalFeatureCard);

    const QList<QFrame *> featureCards = {
        statusCard,
        distanceCard,
        idleFeatureCard,
        totalFeatureCard
    };

    for (QFrame *card : featureCards) {
        card->setFixedSize(132, 84);
    }

    featureContainer->setFixedSize(4 * 132 + 3 * 10, 84);
    featureScroll->setFixedHeight(88);
    featureScroll->setWidget(featureContainer);
    serviceLayout->addWidget(featureScroll);


    // =========================================================================
    // 操作按钮
    // =========================================================================
    auto *buttonRow =
        new QHBoxLayout;


    buttonRow->setObjectName(
        QStringLiteral(
            "stationButtonLayout"));


    buttonRow->setSpacing(
        9);


    // -------------------------------------------------------------------------
    // 查看充电桩
    // -------------------------------------------------------------------------
    auto *pileButton =
        new QPushButton(
            QStringLiteral(
                "查看充电桩"),
            this);


    pileButton->setObjectName(
        QStringLiteral(
            "stationPileButton"));


    pileButton->setCursor(
        Qt::PointingHandCursor);


    pileButton->setIcon(
        QIcon(
            QStringLiteral(
                ":/icons/plug.svg")));


    // -------------------------------------------------------------------------
    // 路线规划
    // -------------------------------------------------------------------------
    auto *navigationButton =
        new QPushButton(
            QStringLiteral(
                "导航"),
            this);


    navigationButton->setObjectName(
        QStringLiteral(
            "stationNavigationButton"));


    navigationButton->setCursor(
        Qt::PointingHandCursor);


    navigationButton->setIcon(
        QIcon(
            QStringLiteral(
                ":/icons/navigation-white.svg")));


    // -------------------------------------------------------------------------
    // 原业务判断保持不变
    //
    // 已满时禁用查看充电桩
    // -------------------------------------------------------------------------
    pileButton->setEnabled(
        !full &&
        m_stationId > 0);


    // -------------------------------------------------------------------------
    // 没有有效终点坐标不能进入路线规划
    // -------------------------------------------------------------------------
    const bool canNavigate =
        m_stationId > 0 &&
        m_hasCoordinate;


    navigationButton->setEnabled(
        canNavigate);


    distanceButton->setEnabled(
        canNavigate);


    buttonRow->addWidget(
        pileButton,
        1);


    buttonRow->addWidget(
        navigationButton,
        1);


    serviceLayout->addLayout(
        buttonRow);


    mainLayout->addWidget(
        servicePanel);

    // 两层无缝衔接，外层黑色底板从白卡圆角后方露出。
    mainLayout->setSpacing(0);


    // =========================================================================
    // 信号：
    // 查看充电桩
    // =========================================================================
    connect(
        pileButton,
        &QPushButton::clicked,
        this,
        [this]() {

            emit stationSelected(
                m_stationId,
                m_name);
        });


    // =========================================================================
    // 信号：
    // 路线规划
    //
    // 同一个 lambda 同时给：
    // 1. 路线按钮
    // 2. 距离按钮
    // =========================================================================
    const auto requestNavigation =
        [this]() {

            if (m_stationId <= 0 ||
                !m_hasCoordinate) {

                return;
            }


            emit navigationRequested(
                m_stationId,
                m_name,
                m_latitude,
                m_longitude,
                m_distance);
        };


    connect(
        navigationButton,
        &QPushButton::clicked,
        this,
        requestNavigation);


    connect(
        distanceButton,
        &QPushButton::clicked,
        this,
        requestNavigation);


    applyResponsiveStyle();
}


// ============================================================================
// Resize
// ============================================================================
void StationCardWidget::resizeEvent(
    QResizeEvent *event)
{
    QFrame::resizeEvent(
        event);


    applyResponsiveStyle();
}


// ============================================================================
// 响应式样式
// ============================================================================
void StationCardWidget::applyResponsiveStyle()
{
    QWidget *scaleBase =
        window()
            ? window()
            : this;


    const int nameFont =
        scaledUi(
            scaleBase,
            17);


    const int priceFont =
        scaledUi(
            scaleBase,
            14);


    const int addressFont =
        scaledUi(
            scaleBase,
            11);


    const int badgeFont =
        scaledUi(
            scaleBase,
            11);


    const int distanceFont =
        scaledUi(
            scaleBase,
            11);


    const int metricCaptionFont =
        scaledUi(
            scaleBase,
            11);


    const int metricValueFont =
        scaledUi(
            scaleBase,
            19);


    const int buttonFont =
        scaledUi(
            scaleBase,
            12);


    const int cardRadius =
        scaledUi(
            scaleBase,
            22);


    const int visualRadius =
        scaledUi(
            scaleBase,
            18);


    const int smallRadius =
        scaledUi(
            scaleBase,
            11);


    const int visualWidth =
        scaledUi(
            scaleBase,
            94);


    const int visualHeight =
        scaledUi(
            scaleBase,
            108);


    const int chargerWidth =
        scaledUi(
            scaleBase,
            82);


    const int chargerHeight =
        scaledUi(
            scaleBase,
            100);


    const int addressIconSize =
        scaledUi(
            scaleBase,
            15);


    const int smallIconSize =
        scaledUi(
            scaleBase,
            15);


    const int buttonIconSize =
        scaledUi(
            scaleBase,
            17);


    const int buttonHeight =
        scaledUi(
            scaleBase,
            43);


    // =========================================================================
    // Card / 文本
    // =========================================================================
    QString style =
        QStringLiteral(

            "QFrame#stationCard{"
            "background:#171D27;"
            "border:1px solid %2;"
            "border-radius:%3px;"
            "}"

            "QFrame#stationCard:hover{"
            "border-color:%4;"
            "}"

            "QFrame#stationHeroPanel{"
            "background:#171D27;"
            "border:none;"
            "border-top-left-radius:%3px;"
            "border-top-right-radius:%3px;"
            "border-bottom-left-radius:0;"
            "border-bottom-right-radius:0;"
            "}"

            "QFrame#stationServicePanel{"
            "background:%1;"
            "border:none;"
            "border-radius:%3px;"
            "}"

            "QLabel#stationServiceTitle{"
            "background:transparent;"
            "color:%7;"
            "border:none;"
            "font-size:%10px;"
            "font-weight:850;"
            "}"

            "QFrame#stationVisualFrame{"
            "background:transparent;"
            "border:none;"
            "border-radius:%6px;"
            "}"

            "QLabel#stationChargerImage{"
            "background:transparent;"
            "border:none;"
            "}"

            "QLabel#stationNameLabel{"
            "background:transparent;"
            "color:#FFFFFF;"
            "font-size:%8px;"
            "font-weight:800;"
            "}"

            "QLabel#stationPriceLabel{"
            "background:transparent;"
            "color:%9;"
            "font-size:%10px;"
            "font-weight:800;"
            "}"

            "QLabel#stationAddressIcon{"
            "background:transparent;"
            "border:none;"
            "}"

            "QLabel#stationAddressLabel{"
            "background:transparent;"
            "color:#B9C1CB;"
            "font-size:%12px;"
            "}");

    style =
        style
            .arg(
                UiTheme::surface())          // %1

            .arg(
                UiTheme::border())           // %2

            .arg(
                cardRadius)                  // %3

            .arg(
                UiTheme::borderStrong())     // %4

            .arg(
                UiTheme::surfaceSoft())      // %5

            .arg(
                visualRadius)                // %6

            .arg(
                UiTheme::textPrimary())      // %7

            .arg(
                nameFont)                    // %8

            .arg(
                UiTheme::limeStrong())       // %9

            .arg(
                priceFont)                   // %10

            .arg(
                UiTheme::textSecondary())    // %11

            .arg(
                addressFont);                // %12


    // =========================================================================
    // 状态与距离
    // =========================================================================
    style +=
        QStringLiteral(

            "QLabel#stationStatusBadge{"
            "border:none;"
            "border-radius:%1px;"
            "font-size:%2px;"
            "font-weight:700;"
            "padding:5px 10px;"
            "}"

            "QLabel#stationStatusBadge"
            "[stationState=\"available\"]{"
            "background:%3;"
            "color:%4;"
            "}"

            "QLabel#stationStatusBadge"
            "[stationState=\"full\"]{"
            "background:#FCEBEB;"
            "color:%5;"
            "}"

            "QLabel#stationStatusBadge"
            "[stationState=\"unknown\"]{"
            "background:#303844;"
            "color:#CFD5DC;"
            "}"

            "QPushButton#stationDistanceButton{"
            "background:#2A323E;"
            "color:#FFFFFF;"
            "border:none;"
            "border-radius:%1px;"
            "font-size:%9px;"
            "font-weight:650;"
            "padding:6px 9px;"
            "}"

            "QPushButton#stationDistanceButton:hover{"
            "background:#37414E;"
            "}"

            "QPushButton#stationDistanceButton:disabled{"
            "background:#F2F4F3;"
            "color:#ABB2AE;"
            "}");

    style =
        style
            .arg(
                smallRadius)                 // %1

            .arg(
                badgeFont)                   // %2

            .arg(
                UiTheme::limeSoft())         // %3

            .arg(
                UiTheme::limeStrong())       // %4

            .arg(
                UiTheme::danger())           // %5

            .arg(
                UiTheme::textSecondary())    // %6

            .arg(
                UiTheme::surfaceSoft())      // %7

            .arg(
                UiTheme::textPrimary())      // %8

            .arg(
                distanceFont);               // %9


    // =========================================================================
    // 数据区域
    // =========================================================================
    style +=
        QStringLiteral(

            "QFrame#stationMetricsPanel{"
            "background:transparent;"
            "border:none;"
            "}"

            "QFrame#stationFeatureCard{"
            "background:%1;"
            "border:1px solid %9;"
            "border-radius:%2px;"
            "}"

            "QScrollArea#stationFeatureScroll,"
            "QWidget#stationFeatureContainer{"
            "background:transparent;"
            "border:none;"
            "}"

            "QScrollBar:horizontal{"
            "height:4px;"
            "background:transparent;"
            "margin:0;"
            "}"

            "QScrollBar::handle:horizontal{"
            "background:#D5DDD9;"
            "border-radius:2px;"
            "min-width:28px;"
            "}"

            "QScrollBar::add-line:horizontal,"
            "QScrollBar::sub-line:horizontal{"
            "width:0;"
            "}"

            "QLabel#stationMetricCaption{"
            "background:transparent;"
            "color:%3;"
            "font-size:%4px;"
            "}"

            "QLabel#stationMetricValue{"
            "background:transparent;"
            "color:%5;"
            "font-size:%6px;"
            "font-weight:800;"
            "}"

            "QLabel#stationIdleMetricValue{"
            "background:transparent;"
            "font-size:%6px;"
            "font-weight:800;"
            "}"

            "QLabel#stationIdleMetricValue"
            "[availability=\"available\"]{"
            "color:%7;"
            "}"

            "QLabel#stationIdleMetricValue"
            "[availability=\"full\"]{"
            "color:%8;"
            "}"

            "QLabel#stationIdleMetricValue"
            "[availability=\"unknown\"]{"
            "color:%3;"
            "}"

            "QFrame#stationMetricDivider{"
            "background:%9;"
            "border:none;"
            "max-width:1px;"
            "}");

    style =
        style
            .arg(
                UiTheme::surfaceSoft())      // %1

            .arg(
                scaledUi(
                    scaleBase,
                    16))                     // %2

            .arg(
                UiTheme::textSecondary())    // %3

            .arg(
                metricCaptionFont)           // %4

            .arg(
                UiTheme::textPrimary())      // %5

            .arg(
                metricValueFont)             // %6

            .arg(
                UiTheme::limeStrong())       // %7

            .arg(
                UiTheme::danger())           // %8

            .arg(
                UiTheme::border());          // %9


    // =========================================================================
    // 操作按钮
    // =========================================================================
    style +=
        QStringLiteral(

            // ----------------------------------------------------------------
            // 查看充电桩：
            // 浅色次按钮
            // ----------------------------------------------------------------
            "QPushButton#stationPileButton{"
            "background:%1;"
            "color:%2;"
            "border:1px solid %3;"
            "border-radius:%4px;"
            "font-size:%5px;"
            "font-weight:700;"
            "padding:9px 14px;"
            "}"

            "QPushButton#stationPileButton:hover{"
            "background:#E9EEEB;"
            "}"

            "QPushButton#stationPileButton:pressed{"
            "background:#E1E7E3;"
            "}"

            "QPushButton#stationPileButton:disabled{"
            "background:#F1F3F2;"
            "color:#A5ACA8;"
            "border-color:#E8EBE9;"
            "}"

            // ----------------------------------------------------------------
            // 路线：
            // 深色主 CTA
            // ----------------------------------------------------------------
            "QPushButton#stationNavigationButton{"
            "background:%6;"
            "color:#FFFFFF;"
            "border:none;"
            "border-radius:%4px;"
            "font-size:%5px;"
            "font-weight:750;"
            "padding:9px 14px;"
            "}"

            "QPushButton#stationNavigationButton:hover{"
            "background:%7;"
            "}"

            "QPushButton#stationNavigationButton:pressed{"
            "background:#10151C;"
            "}"

            "QPushButton#stationNavigationButton:disabled{"
            "background:#DDE2DF;"
            "color:#989F9B;"
            "}");

    style =
        style
            .arg(
                UiTheme::primarySoft())      // %1

            .arg(
                UiTheme::textPrimary())      // %2

            .arg(
                UiTheme::border())           // %3

            .arg(
                smallRadius)                 // %4

            .arg(
                buttonFont)                  // %5

            .arg(
                UiTheme::dark())             // %6

            .arg(
                UiTheme::darkHover());       // %7


    setStyleSheet(
        style);


    // =========================================================================
    // 卡片整体尺寸
    // =========================================================================
    setMinimumHeight(
        scaledUi(
            scaleBase,
            272));


    // =========================================================================
    // 卡片主布局
    // =========================================================================
    if (auto *mainLayout =
            qobject_cast<QVBoxLayout *>(
                layout())) {

        mainLayout->setContentsMargins(
            0,
            0,
            0,
            0);


        mainLayout->setSpacing(
            0);
    }


    if (auto *heroPanelLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral("stationHeroPanelLayout"))) {

        heroPanelLayout->setContentsMargins(
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 17),
            scaledUi(scaleBase, 14),
            scaledUi(scaleBase, 15));

        heroPanelLayout->setSpacing(
            scaledUi(scaleBase, 10));
    }


    if (auto *serviceLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral("stationServiceLayout"))) {

        serviceLayout->setContentsMargins(
            scaledUi(scaleBase, 16),
            scaledUi(scaleBase, 14),
            scaledUi(scaleBase, 16),
            scaledUi(scaleBase, 16));

        serviceLayout->setSpacing(
            scaledUi(scaleBase, 11));
    }


    if (auto *mainLayout =
            qobject_cast<QVBoxLayout *>(layout())) {

        mainLayout->setSpacing(
            0);
    }


    if (auto *featureLayout =
            findChild<QHBoxLayout *>(
                QStringLiteral("stationFeatureLayout"))) {

        const int cardWidth = scaledUi(scaleBase, 132);
        const int cardHeight = scaledUi(scaleBase, 84);
        const int gap = scaledUi(scaleBase, 10);

        featureLayout->setSpacing(gap);

        if (auto *container =
                findChild<QWidget *>(
                    QStringLiteral("stationFeatureContainer"))) {

            container->setFixedSize(
                4 * cardWidth + 3 * gap,
                cardHeight);
        }

        for (auto *card :
             findChildren<QFrame *>(
                 QStringLiteral("stationFeatureCard"))) {

            card->setFixedSize(cardWidth, cardHeight);
        }
    }


    if (auto *featureScroll =
            findChild<QScrollArea *>(
                QStringLiteral("stationFeatureScroll"))) {

        featureScroll->setFixedHeight(
            scaledUi(scaleBase, 88));
    }


    // =========================================================================
    // Hero
    // =========================================================================
    if (auto *heroLayout =
            findChild<QHBoxLayout *>(
                QStringLiteral(
                    "stationHeroLayout"))) {

        heroLayout->setSpacing(
            scaledUi(
                scaleBase,
                12));
    }


    if (auto *infoLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "stationInfoLayout"))) {

        infoLayout->setSpacing(
            scaledUi(
                scaleBase,
                7));
    }


    if (auto *titleRow =
            findChild<QHBoxLayout *>(
                QStringLiteral(
                    "stationTitleRow"))) {

        titleRow->setSpacing(
            scaledUi(
                scaleBase,
                8));
    }


    // =========================================================================
    // 充电桩视觉图
    // =========================================================================
    if (auto *visualFrame =
            findChild<QFrame *>(
                QStringLiteral(
                    "stationVisualFrame"))) {

        visualFrame->setFixedSize(
            visualWidth,
            visualHeight);


        if (auto *heroPanel =
                findChild<QFrame *>(
                    QStringLiteral("stationHeroPanel"))) {

            const int imageX =
                width() -
                visualWidth -
                scaledUi(scaleBase, 20);

            const int imageY =
                heroPanel->geometry().bottom() -
                visualHeight +
                scaledUi(scaleBase, 36);

            visualFrame->move(
                imageX,
                imageY);
        }
    }


    if (auto *chargerImage =
            findChild<QLabel *>(
                QStringLiteral(
                    "stationChargerImage"))) {

        chargerImage->setFixedSize(
            chargerWidth,
            chargerHeight);


        const QPixmap source(
            QStringLiteral(
                ":/images/charger-main.png"));


        if (!source.isNull()) {

            chargerImage->setPixmap(
                source.scaled(
                    chargerWidth,
                    chargerHeight,
                    Qt::KeepAspectRatio,
                    Qt::SmoothTransformation));

        } else {

            chargerImage->setPixmap(
                QIcon(
                    QStringLiteral(
                        ":/icons/plug.svg"))
                    .pixmap(
                        QSize(
                            smallIconSize * 2,
                            smallIconSize * 2)));
        }


        // 图片最后抬到最上层，使其底座能落在白色卡片之上。
        if (auto *visualFrame =
                findChild<QFrame *>(
                    QStringLiteral("stationVisualFrame"))) {

            visualFrame->raise();
        }
    }


    // =========================================================================
    // 地址图标
    // =========================================================================
    if (auto *addressIcon =
            findChild<QLabel *>(
                QStringLiteral(
                    "stationAddressIcon"))) {

        addressIcon->setFixedSize(
            addressIconSize,
            addressIconSize);


        addressIcon->setPixmap(
            QIcon(
                QStringLiteral(
                    ":/icons/location.svg"))
                .pixmap(
                    QSize(
                        addressIconSize,
                        addressIconSize)));
    }


    // =========================================================================
    // 状态 / 距离
    // =========================================================================
    if (auto *metaLayout =
            findChild<QHBoxLayout *>(
                QStringLiteral(
                    "stationMetaLayout"))) {

        metaLayout->setSpacing(
            scaledUi(
                scaleBase,
                10));
    }


    if (auto *distanceButton =
            findChild<QPushButton *>(
                QStringLiteral(
                    "stationDistanceButton"))) {

        distanceButton->setIconSize(
            QSize(
                smallIconSize,
                smallIconSize));
    }


    // =========================================================================
    // 数据面板
    // =========================================================================
    if (auto *metricsLayout =
            findChild<QHBoxLayout *>(
                QStringLiteral(
                    "stationMetricsLayout"))) {

        metricsLayout->setContentsMargins(
            scaledUi(
                scaleBase,
                14),

            scaledUi(
                scaleBase,
                12),

            scaledUi(
                scaleBase,
                14),

            scaledUi(
                scaleBase,
                12));


        metricsLayout->setSpacing(
            scaledUi(
                scaleBase,
                14));
    }


    // =========================================================================
    // 按钮
    // =========================================================================
    if (auto *buttonLayout =
            findChild<QHBoxLayout *>(
                QStringLiteral(
                    "stationButtonLayout"))) {

        buttonLayout->setSpacing(
            scaledUi(
                scaleBase,
                9));
    }


    if (auto *pileButton =
            findChild<QPushButton *>(
                QStringLiteral(
                    "stationPileButton"))) {

        pileButton->setMinimumHeight(
            buttonHeight);


        pileButton->setIconSize(
            QSize(
                buttonIconSize,
                buttonIconSize));
    }


    if (auto *navigationButton =
            findChild<QPushButton *>(
                QStringLiteral(
                    "stationNavigationButton"))) {

        navigationButton->setMinimumHeight(
            buttonHeight);


        navigationButton->setIconSize(
            QSize(
                buttonIconSize,
                buttonIconSize));
    }
}
