#include "stationcardwidget.h"

#include "uitheme.h"
#include "windowhelper.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
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


    if (!jsonValue.isDouble())
        return false;


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

    setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Preferred);


    UiTheme::applyCardShadow(
        this,
        14,
        3);


    // ========================================================================
    // 业务主键
    // ========================================================================
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


    // ========================================================================
    // 字段安全读取
    // ========================================================================
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


    // ========================================================================
    // 经纬度
    // ========================================================================
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


    // ========================================================================
    // 距离
    // ========================================================================
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


    // ========================================================================
    // 状态
    // ========================================================================
    const bool hasPileInfo =
        hasIdle &&
        hasTotal;


    const bool full =
        hasPileInfo &&
        idle == 0;


    // ========================================================================
    // 主布局
    // ========================================================================
    auto *mainLayout =
        new QVBoxLayout(this);

    mainLayout->setObjectName(
        QStringLiteral(
            "stationCardLayout"));

    mainLayout->setContentsMargins(
        18,
        16,
        18,
        16);

    mainLayout->setSpacing(
        10);


    // ========================================================================
    // 第一行：站名 + 单价
    // ========================================================================
    auto *topRow =
        new QHBoxLayout;

    topRow->setSpacing(
        12);


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


    topRow->addWidget(
        nameLabel,
        1);

    topRow->addWidget(
        priceLabel);


    mainLayout->addLayout(
        topRow);


    // ========================================================================
    // 地址
    // ========================================================================
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


    mainLayout->addWidget(
        addressLabel);


    // ========================================================================
    // 状态 + 距离
    // ========================================================================
    auto *metaRow =
        new QHBoxLayout;

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
                "暂无空闲");

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


    // NO.8：
    // 距离本身继续保留为可点击的导航入口
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


    metaRow->addWidget(
        statusBadge);

    metaRow->addStretch();

    metaRow->addWidget(
        distanceButton);


    mainLayout->addLayout(
        metaRow);


    // ========================================================================
    // 数据信息面板
    // ========================================================================
    auto *metricsPanel =
        new QFrame(this);

    metricsPanel->setObjectName(
        QStringLiteral(
            "stationMetricsPanel"));


    auto *metricsLayout =
        new QHBoxLayout(
            metricsPanel);

    metricsLayout->setObjectName(
        QStringLiteral(
            "stationMetricsLayout"));

    metricsLayout->setContentsMargins(
        14,
        11,
        14,
        11);

    metricsLayout->setSpacing(
        14);


    // ------------------------------------------------------------------------
    // 空闲桩
    // ------------------------------------------------------------------------
    auto *idleBlock =
        new QVBoxLayout;

    idleBlock->setSpacing(
        3);


    auto *idleCaption =
        new QLabel(
            QStringLiteral(
                "空闲充电桩"),
            metricsPanel);

    idleCaption->setObjectName(
        QStringLiteral(
            "stationMetricCaption"));


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
            "stationMetricValue"));


    idleBlock->addWidget(
        idleCaption);

    idleBlock->addWidget(
        idleMetric);


    // ------------------------------------------------------------------------
    // 分隔线
    // ------------------------------------------------------------------------
    auto *divider =
        new QFrame(
            metricsPanel);

    divider->setObjectName(
        QStringLiteral(
            "stationMetricDivider"));

    divider->setFrameShape(
        QFrame::VLine);


    // ------------------------------------------------------------------------
    // 总桩数
    // ------------------------------------------------------------------------
    auto *totalBlock =
        new QVBoxLayout;

    totalBlock->setSpacing(
        3);


    auto *totalCaption =
        new QLabel(
            QStringLiteral(
                "充电桩总数"),
            metricsPanel);

    totalCaption->setObjectName(
        QStringLiteral(
            "stationMetricCaption"));


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
        totalCaption);

    totalBlock->addWidget(
        totalMetric);


    metricsLayout->addLayout(
        idleBlock,
        1);

    metricsLayout->addWidget(
        divider);

    metricsLayout->addLayout(
        totalBlock,
        1);


    mainLayout->addWidget(
        metricsPanel);


    // ========================================================================
    // 操作按钮
    // ========================================================================
    auto *buttonRow =
        new QHBoxLayout;

    buttonRow->setObjectName(
        QStringLiteral(
            "stationButtonLayout"));

    buttonRow->setSpacing(
        9);


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


    // 已满时保持原有逻辑：
    // 弱化并禁用查看充电桩
    pileButton->setEnabled(
        !full &&
        m_stationId > 0);


    // 没有有效终点坐标不能导航
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
        navigationButton);


    mainLayout->addLayout(
        buttonRow);


    // ========================================================================
    // 信号：查看充电桩
    // ========================================================================
    connect(
        pileButton,
        &QPushButton::clicked,
        this,
        [this]() {

            emit stationSelected(
                m_stationId,
                m_name);
        });


    // ========================================================================
    // 信号：导航
    // ========================================================================
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
// 响应式
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
            15);

    const int addressFont =
        scaledUi(
            scaleBase,
            12);

    const int badgeFont =
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
            18);

    const int buttonFont =
        scaledUi(
            scaleBase,
            13);

    const int cardRadius =
        scaledUi(
            scaleBase,
            18);

    const int smallRadius =
        scaledUi(
            scaleBase,
            10);


    setStyleSheet(
        QStringLiteral(

            // ================================================================
            // 卡片
            // ================================================================
            "QFrame#stationCard{"
            "background:%1;"
            "border:1px solid %2;"
            "border-radius:%3px;"
            "}"

            "QFrame#stationCard:hover{"
            "border-color:#CCD9D2;"
            "}"

            // ================================================================
            // 站点名称
            // ================================================================
            "QLabel#stationNameLabel{"
            "background:transparent;"
            "color:%4;"
            "font-size:%5px;"
            "font-weight:800;"
            "}"

            // ================================================================
            // 单价
            // ================================================================
            "QLabel#stationPriceLabel{"
            "background:transparent;"
            "color:%6;"
            "font-size:%7px;"
            "font-weight:800;"
            "}"

            // ================================================================
            // 地址
            // ================================================================
            "QLabel#stationAddressLabel{"
            "background:transparent;"
            "color:%8;"
            "font-size:%9px;"
            "}"

            // ================================================================
            // 状态 Badge
            // ================================================================
            "QLabel#stationStatusBadge{"
            "border:none;"
            "border-radius:%10px;"
            "font-size:%11px;"
            "font-weight:700;"
            "padding:5px 9px;"
            "}"

            "QLabel#stationStatusBadge[stationState=\"available\"]{"
            "background:#EAF3ED;"
            "color:#4F8668;"
            "}"

            "QLabel#stationStatusBadge[stationState=\"full\"]{"
            "background:#F7ECEA;"
            "color:#C96C66;"
            "}"

            "QLabel#stationStatusBadge[stationState=\"unknown\"]{"
            "background:#F1F0EC;"
            "color:#7A837E;"
            "}"

            // ================================================================
            // 距离入口
            // ================================================================
            "QPushButton#stationDistanceButton{"
            "background:transparent;"
            "border:none;"
            "color:%12;"
            "font-size:%9px;"
            "font-weight:600;"
            "padding:3px 0;"
            "}"

            "QPushButton#stationDistanceButton:hover{"
            "text-decoration:underline;"
            "}"

            "QPushButton#stationDistanceButton:disabled{"
            "color:#A0A6A2;"
            "text-decoration:none;"
            "}"

            // ================================================================
            // 数据面板
            // ================================================================
            "QFrame#stationMetricsPanel{"
            "background:%13;"
            "border:1px solid %2;"
            "border-radius:%14px;"
            "}"

            "QLabel#stationMetricCaption{"
            "background:transparent;"
            "color:%8;"
            "font-size:%15px;"
            "}"

            "QLabel#stationMetricValue{"
            "background:transparent;"
            "color:%4;"
            "font-size:%16px;"
            "font-weight:800;"
            "}"

            "QFrame#stationMetricDivider{"
            "background:%2;"
            "border:none;"
            "max-width:1px;"
            "}"

            // ================================================================
            // 查看充电桩：主按钮
            // ================================================================
            "QPushButton#stationPileButton{"
            "background:%12;"
            "color:#FFFFFF;"
            "border:none;"
            "border-radius:%14px;"
            "font-size:%17px;"
            "font-weight:700;"
            "padding:9px 15px;"
            "}"

            "QPushButton#stationPileButton:hover{"
            "background:%18;"
            "}"

            "QPushButton#stationPileButton:disabled{"
            "background:#D7DAD7;"
            "color:#999F9C;"
            "}"

            // ================================================================
            // 导航：次按钮
            // ================================================================
            "QPushButton#stationNavigationButton{"
            "background:%19;"
            "color:%12;"
            "border:1px solid #D6E1DA;"
            "border-radius:%14px;"
            "font-size:%17px;"
            "font-weight:700;"
            "padding:9px 18px;"
            "}"

            "QPushButton#stationNavigationButton:hover{"
            "background:#DFE9E3;"
            "}"

            "QPushButton#stationNavigationButton:disabled{"
            "background:#F1F1EE;"
            "color:#A8ADA9;"
            "border-color:#E4E4DF;"
            "}")

        .arg(
            UiTheme::surface())            // %1

        .arg(
            UiTheme::border())             // %2

        .arg(
            cardRadius)                    // %3

        .arg(
            UiTheme::textPrimary())        // %4

        .arg(
            nameFont)                      // %5

        .arg(
            UiTheme::accent())             // %6

        .arg(
            priceFont)                     // %7

        .arg(
            UiTheme::textSecondary())      // %8

        .arg(
            addressFont)                   // %9

        .arg(
            smallRadius)                   // %10

        .arg(
            badgeFont)                     // %11

        .arg(
            UiTheme::primary())            // %12

        .arg(
            UiTheme::surfaceSoft())        // %13

        .arg(
            smallRadius)                   // %14

        .arg(
            metricCaptionFont)             // %15

        .arg(
            metricValueFont)               // %16

        .arg(
            buttonFont)                    // %17

        .arg(
            UiTheme::primaryHover())       // %18

        .arg(
            UiTheme::primarySoft()));      // %19


    // ========================================================================
    // 卡片内部间距
    // ========================================================================
    if (auto *mainLayout =
            qobject_cast<QVBoxLayout *>(
                layout())) {

        mainLayout->setContentsMargins(
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 16),
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 16));

        mainLayout->setSpacing(
            scaledUi(
                scaleBase,
                10));
    }


    // ========================================================================
    // 数据面板间距
    // ========================================================================
    if (auto *metricsLayout =
            findChild<QHBoxLayout *>(
                QStringLiteral(
                    "stationMetricsLayout"))) {

        metricsLayout->setContentsMargins(
            scaledUi(scaleBase, 14),
            scaledUi(scaleBase, 11),
            scaledUi(scaleBase, 14),
            scaledUi(scaleBase, 11));

        metricsLayout->setSpacing(
            scaledUi(
                scaleBase,
                14));
    }


    // ========================================================================
    // 按钮间距
    // ========================================================================
    if (auto *buttonLayout =
            findChild<QHBoxLayout *>(
                QStringLiteral(
                    "stationButtonLayout"))) {

        buttonLayout->setSpacing(
            scaledUi(
                scaleBase,
                9));
    }
}
