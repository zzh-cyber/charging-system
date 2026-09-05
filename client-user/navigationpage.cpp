#include "navigationpage.h"

#include "uitheme.h"
#include "windowhelper.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QVBoxLayout>


NavigationPage::NavigationPage(
    QWidget *parent)
    : QWidget(parent)
{
    setObjectName(
        QStringLiteral(
            "navigationPage"));


    // ========================================================================
    // 页面根布局
    // ========================================================================
    auto *rootLayout =
        new QVBoxLayout(this);

    rootLayout->setContentsMargins(
        0,
        0,
        0,
        0);

    rootLayout->setSpacing(
        0);


    // ========================================================================
    // 滚动区域
    // ========================================================================
    auto *scrollArea =
        new QScrollArea(this);

    scrollArea->setObjectName(
        QStringLiteral(
            "navigationScrollArea"));

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
            "navigationContent"));


    auto *mainLayout =
        new QVBoxLayout(
            content);

    mainLayout->setObjectName(
        QStringLiteral(
            "navigationMainLayout"));

    mainLayout->setContentsMargins(
        18,
        18,
        18,
        18);

    mainLayout->setSpacing(
        14);


    // ========================================================================
    // 页头
    // ========================================================================
    auto *header =
        new QHBoxLayout;

    header->setObjectName(
        QStringLiteral(
            "navigationHeaderLayout"));

    header->setSpacing(
        10);


    auto *backButton =
        new QPushButton(
            QStringLiteral(
                "← 返回"),
            content);

    backButton->setObjectName(
        QStringLiteral(
            "navigationBackButton"));

    backButton->setCursor(
        Qt::PointingHandCursor);


    auto *title =
        new QLabel(
            QStringLiteral(
                "一键导航"),
            content);

    title->setObjectName(
        QStringLiteral(
            "navigationTitle"));


    header->addWidget(
        backButton);

    header->addWidget(
        title,
        1);


    connect(
        backButton,
        &QPushButton::clicked,
        this,
        &NavigationPage::back);


    mainLayout->addLayout(
        header);


    // ========================================================================
    // 目标站点卡
    // ========================================================================
    auto *stationCard =
        new QFrame(
            content);

    stationCard->setObjectName(
        QStringLiteral(
            "navigationStationCard"));

    UiTheme::applyCardShadow(
        stationCard,
        16,
        4);


    auto *stationLayout =
        new QVBoxLayout(
            stationCard);

    stationLayout->setObjectName(
        QStringLiteral(
            "navigationStationLayout"));

    stationLayout->setContentsMargins(
        18,
        16,
        18,
        16);

    stationLayout->setSpacing(
        7);


    auto *stationTitle =
        new QLabel(
            QStringLiteral(
                "导航至"),
            stationCard);

    stationTitle->setObjectName(
        QStringLiteral(
            "navigationStationTitle"));


    m_stationLabel =
        new QLabel(
            stationCard);

    m_stationLabel->setObjectName(
        QStringLiteral(
            "navigationStationLabel"));

    m_stationLabel->setWordWrap(
        true);


    stationLayout->addWidget(
        stationTitle);

    stationLayout->addWidget(
        m_stationLabel);


    mainLayout->addWidget(
        stationCard);


    // ========================================================================
    // 路线摘要卡
    // ========================================================================
    auto *routeCard =
        new QFrame(
            content);

    routeCard->setObjectName(
        QStringLiteral(
            "navigationRouteCard"));

    UiTheme::applyCardShadow(
        routeCard,
        16,
        4);


    auto *routeLayout =
        new QVBoxLayout(
            routeCard);

    routeLayout->setObjectName(
        QStringLiteral(
            "navigationRouteLayout"));

    routeLayout->setContentsMargins(
        18,
        16,
        18,
        16);

    routeLayout->setSpacing(
        12);


    // ========================================================================
    // 路线卡标题
    // ========================================================================
    auto *routeHeader =
        new QHBoxLayout;


    auto *routeTitle =
        new QLabel(
            QStringLiteral(
                "路线信息"),
            routeCard);

    routeTitle->setObjectName(
        QStringLiteral(
            "navigationRouteTitle"));


    auto *routeBadge =
        new QLabel(
            QStringLiteral(
                "直线距离"),
            routeCard);

    routeBadge->setObjectName(
        QStringLiteral(
            "navigationRouteBadge"));

    routeBadge->setAlignment(
        Qt::AlignCenter);


    routeHeader->addWidget(
        routeTitle);

    routeHeader->addStretch();

    routeHeader->addWidget(
        routeBadge);


    routeLayout->addLayout(
        routeHeader);


    // ========================================================================
    // 起点卡
    // ========================================================================
    auto *startCard =
        new QFrame(
            routeCard);

    startCard->setObjectName(
        QStringLiteral(
            "navigationPointCard"));


    auto *startLayout =
        new QHBoxLayout(
            startCard);

    startLayout->setObjectName(
        QStringLiteral(
            "navigationStartLayout"));

    startLayout->setContentsMargins(
        13,
        11,
        13,
        11);

    startLayout->setSpacing(
        12);


    auto *startMarker =
        new QLabel(
            QStringLiteral(
                "起"),
            startCard);

    startMarker->setObjectName(
        QStringLiteral(
            "navigationStartMarker"));

    startMarker->setAlignment(
        Qt::AlignCenter);


    auto *startTextLayout =
        new QVBoxLayout;

    startTextLayout->setSpacing(
        3);


    auto *startTitle =
        new QLabel(
            QStringLiteral(
                "当前位置"),
            startCard);

    startTitle->setObjectName(
        QStringLiteral(
            "navigationSmallTitle"));


    m_startLabel =
        new QLabel(
            startCard);

    m_startLabel->setObjectName(
        QStringLiteral(
            "navigationCoordinateLabel"));

    m_startLabel->setWordWrap(
        true);


    startTextLayout->addWidget(
        startTitle);

    startTextLayout->addWidget(
        m_startLabel);


    startLayout->addWidget(
        startMarker);

    startLayout->addLayout(
        startTextLayout,
        1);


    routeLayout->addWidget(
        startCard);


    // ========================================================================
    // 中间箭头
    // ========================================================================
    auto *arrow =
        new QLabel(
            QStringLiteral(
                "↓"),
            routeCard);

    arrow->setObjectName(
        QStringLiteral(
            "navigationArrow"));

    arrow->setAlignment(
        Qt::AlignCenter);


    routeLayout->addWidget(
        arrow);


    // ========================================================================
    // 终点卡
    // ========================================================================
    auto *targetCard =
        new QFrame(
            routeCard);

    targetCard->setObjectName(
        QStringLiteral(
            "navigationPointCard"));


    auto *targetLayout =
        new QHBoxLayout(
            targetCard);

    targetLayout->setObjectName(
        QStringLiteral(
            "navigationTargetLayout"));

    targetLayout->setContentsMargins(
        13,
        11,
        13,
        11);

    targetLayout->setSpacing(
        12);


    auto *targetMarker =
        new QLabel(
            QStringLiteral(
                "终"),
            targetCard);

    targetMarker->setObjectName(
        QStringLiteral(
            "navigationTargetMarker"));

    targetMarker->setAlignment(
        Qt::AlignCenter);


    auto *targetTextLayout =
        new QVBoxLayout;

    targetTextLayout->setSpacing(
        3);


    auto *targetTitle =
        new QLabel(
            QStringLiteral(
                "目的地"),
            targetCard);

    targetTitle->setObjectName(
        QStringLiteral(
            "navigationSmallTitle"));


    m_targetLabel =
        new QLabel(
            targetCard);

    m_targetLabel->setObjectName(
        QStringLiteral(
            "navigationCoordinateLabel"));

    m_targetLabel->setWordWrap(
        true);


    targetTextLayout->addWidget(
        targetTitle);

    targetTextLayout->addWidget(
        m_targetLabel);


    targetLayout->addWidget(
        targetMarker);

    targetLayout->addLayout(
        targetTextLayout,
        1);


    routeLayout->addWidget(
        targetCard);


    // ========================================================================
    // 距离区域
    // ========================================================================
    auto *distanceCard =
        new QFrame(
            routeCard);

    distanceCard->setObjectName(
        QStringLiteral(
            "navigationDistanceCard"));


    auto *distanceLayout =
        new QHBoxLayout(
            distanceCard);

    distanceLayout->setObjectName(
        QStringLiteral(
            "navigationDistanceLayout"));

    distanceLayout->setContentsMargins(
        14,
        11,
        14,
        11);


    auto *distanceTitle =
        new QLabel(
            QStringLiteral(
                "距离"),
            distanceCard);

    distanceTitle->setObjectName(
        QStringLiteral(
            "navigationDistanceTitle"));


    m_distanceLabel =
        new QLabel(
            distanceCard);

    m_distanceLabel->setObjectName(
        QStringLiteral(
            "navigationDistanceLabel"));

    m_distanceLabel->setAlignment(
        Qt::AlignRight |
        Qt::AlignVCenter);


    distanceLayout->addWidget(
        distanceTitle);

    distanceLayout->addStretch();

    distanceLayout->addWidget(
        m_distanceLabel);


    routeLayout->addWidget(
        distanceCard);


    mainLayout->addWidget(
        routeCard);

    mainLayout->addStretch();


    scrollArea->setWidget(
        content);

    rootLayout->addWidget(
        scrollArea);


    applyResponsiveStyle();
}


// ============================================================================
// 设置导航数据
// 这里保持原逻辑、原格式、原默认显示不变
// ============================================================================
void NavigationPage::setNavigationData(
    const QString &stationName,
    double startLat,
    double startLng,
    double targetLat,
    double targetLng,
    double distance)
{
    m_stationLabel->setText(
        stationName);


    m_startLabel->setText(
        QStringLiteral(
            "%1, %2")
            .arg(
                startLat,
                0,
                'f',
                6)
            .arg(
                startLng,
                0,
                'f',
                6));


    m_targetLabel->setText(
        QStringLiteral(
            "%1, %2")
            .arg(
                targetLat,
                0,
                'f',
                6)
            .arg(
                targetLng,
                0,
                'f',
                6));


    if (distance >= 0.0) {

        m_distanceLabel->setText(
            QStringLiteral(
                "直线距离约 %1 km")
                .arg(
                    distance,
                    0,
                    'f',
                    1));

    } else {

        m_distanceLabel->setText(
            QStringLiteral(
                "距离 -- km"));
    }
}


// ============================================================================
// Resize
// ============================================================================
void NavigationPage::resizeEvent(
    QResizeEvent *event)
{
    QWidget::resizeEvent(
        event);

    applyResponsiveStyle();
}


// ============================================================================
// 响应式样式
// ============================================================================
void NavigationPage::applyResponsiveStyle()
{
    QWidget *scaleBase =
        window()
            ? window()
            : this;


    const int titleFont =
        scaledUi(
            scaleBase,
            22);

    const int stationFont =
        scaledUi(
            scaleBase,
            19);

    const int routeTitleFont =
        scaledUi(
            scaleBase,
            15);

    const int normalFont =
        scaledUi(
            scaleBase,
            14);

    const int smallFont =
        scaledUi(
            scaleBase,
            12);

    const int tinyFont =
        scaledUi(
            scaleBase,
            11);

    const int distanceFont =
        scaledUi(
            scaleBase,
            15);

    const int cardRadius =
        scaledUi(
            scaleBase,
            18);

    const int smallRadius =
        scaledUi(
            scaleBase,
            10);


    // ========================================================================
    // 页面整体样式
    // ========================================================================
    setStyleSheet(
        QStringLiteral(

            // ================================================================
            // 页面
            // ================================================================
            "QWidget#navigationPage{"
            "background:transparent;"
            "color:%1;"
            "}"

            "QWidget#navigationContent{"
            "background:transparent;"
            "}"

            "QScrollArea#navigationScrollArea{"
            "background:transparent;"
            "border:none;"
            "}"

            // ================================================================
            // 返回按钮
            // ================================================================
            "QPushButton#navigationBackButton{"
            "background:%2;"
            "color:%3;"
            "border:1px solid #D6E1DA;"
            "border-radius:%4px;"
            "font-size:%5px;"
            "font-weight:700;"
            "padding:7px 13px;"
            "}"

            "QPushButton#navigationBackButton:hover{"
            "background:#DFE9E3;"
            "}"

            // ================================================================
            // 标题
            // ================================================================
            "QLabel#navigationTitle{"
            "background:transparent;"
            "color:%1;"
            "font-size:%6px;"
            "font-weight:800;"
            "}"

            // ================================================================
            // 目标站点卡
            // ================================================================
            "QFrame#navigationStationCard{"
            "background:%7;"
            "border:1px solid %8;"
            "border-radius:%9px;"
            "}"

            "QLabel#navigationStationTitle{"
            "background:transparent;"
            "color:%10;"
            "font-size:%11px;"
            "}"

            "QLabel#navigationStationLabel{"
            "background:transparent;"
            "color:%1;"
            "font-size:%12px;"
            "font-weight:800;"
            "}"

            // ================================================================
            // 路线卡
            // ================================================================
            "QFrame#navigationRouteCard{"
            "background:%7;"
            "border:1px solid %8;"
            "border-radius:%9px;"
            "}"

            "QLabel#navigationRouteTitle{"
            "background:transparent;"
            "color:%1;"
            "font-size:%13px;"
            "font-weight:800;"
            "}"

            "QLabel#navigationRouteBadge{"
            "background:%2;"
            "color:%3;"
            "border:none;"
            "border-radius:%4px;"
            "font-size:%14px;"
            "font-weight:700;"
            "padding:4px 9px;"
            "}"

            // ================================================================
            // 起点终点
            // ================================================================
            "QFrame#navigationPointCard{"
            "background:%15;"
            "border:1px solid %8;"
            "border-radius:%4px;"
            "}"

            "QLabel#navigationSmallTitle{"
            "background:transparent;"
            "color:%10;"
            "font-size:%14px;"
            "}"

            "QLabel#navigationCoordinateLabel{"
            "background:transparent;"
            "color:%1;"
            "font-size:%5px;"
            "font-weight:600;"
            "}"

            // ================================================================
            // 起点 Marker
            // ================================================================
            "QLabel#navigationStartMarker{"
            "background:#EAF3ED;"
            "color:#4F8668;"
            "border:none;"
            "border-radius:%4px;"
            "font-size:%14px;"
            "font-weight:800;"
            "min-width:30px;"
            "min-height:30px;"
            "}"

            // ================================================================
            // 终点 Marker
            // ================================================================
            "QLabel#navigationTargetMarker{"
            "background:#FFF3DF;"
            "color:#A86D1E;"
            "border:none;"
            "border-radius:%4px;"
            "font-size:%14px;"
            "font-weight:800;"
            "min-width:30px;"
            "min-height:30px;"
            "}"

            // ================================================================
            // 箭头
            // ================================================================
            "QLabel#navigationArrow{"
            "background:transparent;"
            "color:%3;"
            "font-size:%16px;"
            "font-weight:700;"
            "}"

            // ================================================================
            // 距离
            // ================================================================
            "QFrame#navigationDistanceCard{"
            "background:%2;"
            "border:1px solid #DCE5DF;"
            "border-radius:%4px;"
            "}"

            "QLabel#navigationDistanceTitle{"
            "background:transparent;"
            "color:%10;"
            "font-size:%11px;"
            "}"

            "QLabel#navigationDistanceLabel{"
            "background:transparent;"
            "color:%3;"
            "font-size:%17px;"
            "font-weight:800;"
            "}")

        .arg(
            UiTheme::textPrimary())      // %1

        .arg(
            UiTheme::primarySoft())      // %2

        .arg(
            UiTheme::primary())          // %3

        .arg(
            smallRadius)                 // %4

        .arg(
            normalFont)                  // %5

        .arg(
            titleFont)                   // %6

        .arg(
            UiTheme::surface())          // %7

        .arg(
            UiTheme::border())           // %8

        .arg(
            cardRadius)                  // %9

        .arg(
            UiTheme::textSecondary())    // %10

        .arg(
            smallFont)                   // %11

        .arg(
            stationFont)                 // %12

        .arg(
            routeTitleFont)              // %13

        .arg(
            tinyFont)                    // %14

        .arg(
            UiTheme::surfaceSoft())      // %15

        .arg(
            scaledUi(
                scaleBase,
                22))                     // %16

        .arg(
            distanceFont));              // %17


    // ========================================================================
    // 页面整体边距
    // ========================================================================
    if (auto *mainLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "navigationMainLayout"))) {

        mainLayout->setContentsMargins(
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 18));

        mainLayout->setSpacing(
            scaledUi(
                scaleBase,
                14));
    }


    // ========================================================================
    // 页头
    // ========================================================================
    if (auto *headerLayout =
            findChild<QHBoxLayout *>(
                QStringLiteral(
                    "navigationHeaderLayout"))) {

        headerLayout->setSpacing(
            scaledUi(
                scaleBase,
                10));
    }


    // ========================================================================
    // 站点卡
    // ========================================================================
    if (auto *stationLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "navigationStationLayout"))) {

        stationLayout->setContentsMargins(
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 16),
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 16));

        stationLayout->setSpacing(
            scaledUi(
                scaleBase,
                7));
    }


    // ========================================================================
    // 路线卡
    // ========================================================================
    if (auto *routeLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "navigationRouteLayout"))) {

        routeLayout->setContentsMargins(
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 16),
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 16));

        routeLayout->setSpacing(
            scaledUi(
                scaleBase,
                12));
    }


    // ========================================================================
    // 起点布局
    // ========================================================================
    if (auto *startLayout =
            findChild<QHBoxLayout *>(
                QStringLiteral(
                    "navigationStartLayout"))) {

        startLayout->setContentsMargins(
            scaledUi(scaleBase, 13),
            scaledUi(scaleBase, 11),
            scaledUi(scaleBase, 13),
            scaledUi(scaleBase, 11));

        startLayout->setSpacing(
            scaledUi(
                scaleBase,
                12));
    }


    // ========================================================================
    // 终点布局
    // ========================================================================
    if (auto *targetLayout =
            findChild<QHBoxLayout *>(
                QStringLiteral(
                    "navigationTargetLayout"))) {

        targetLayout->setContentsMargins(
            scaledUi(scaleBase, 13),
            scaledUi(scaleBase, 11),
            scaledUi(scaleBase, 13),
            scaledUi(scaleBase, 11));

        targetLayout->setSpacing(
            scaledUi(
                scaleBase,
                12));
    }


    // ========================================================================
    // 距离卡
    // ========================================================================
    if (auto *distanceLayout =
            findChild<QHBoxLayout *>(
                QStringLiteral(
                    "navigationDistanceLayout"))) {

        distanceLayout->setContentsMargins(
            scaledUi(scaleBase, 14),
            scaledUi(scaleBase, 11),
            scaledUi(scaleBase, 14),
            scaledUi(scaleBase, 11));
    }
}
