#include "navigationpage.h"

#include "amapwidget.h"
#include "routeplanner.h"

#include "uitheme.h"
#include "windowhelper.h"

#include <QButtonGroup>
#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStackedLayout>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>


namespace
{

bool validCoordinate(
    double lat,
    double lng)
{
    return std::isfinite(lat)
        && std::isfinite(lng)
        && lat >= -90.0
        && lat <= 90.0
        && lng >= -180.0
        && lng <= 180.0;
}

constexpr double kMaxWalkingDistanceKm =
    100.0;

double straightLineDistanceKm(
    double fromLat,
    double fromLng,
    double toLat,
    double toLng)
{
    constexpr double kEarthRadiusKm =
        6371.0088;

    const auto toRadians =
        [](double degrees) {
            return degrees *
                3.14159265358979323846 /
                180.0;
        };

    const double lat1 =
        toRadians(fromLat);
    const double lat2 =
        toRadians(toLat);
    const double deltaLat =
        toRadians(toLat - fromLat);
    const double deltaLng =
        toRadians(toLng - fromLng);

    const double sinLat =
        std::sin(deltaLat / 2.0);
    const double sinLng =
        std::sin(deltaLng / 2.0);

    const double a =
        sinLat * sinLat +
        std::cos(lat1) *
            std::cos(lat2) *
            sinLng * sinLng;

    const double clampedA =
        std::clamp(a, 0.0, 1.0);

    return kEarthRadiusKm *
        2.0 *
        std::atan2(
            std::sqrt(clampedA),
            std::sqrt(1.0 - clampedA));
}

QString formatDuration(
    qint64 seconds)
{
    if (seconds <= 0) {
        return QStringLiteral("--");
    }

    const qint64 hours =
        seconds / 3600;
    qint64 minutes =
        (seconds % 3600) / 60;

    if (hours == 0 && minutes == 0) {
        minutes = 1;
    }

    if (hours > 0) {
        return QStringLiteral(
                   "%1小时%2分钟")
            .arg(hours)
            .arg(minutes);
    }

    return QStringLiteral(
               "%1分钟")
        .arg(minutes);
}

QString formatRouteDistance(
    double meters)
{
    if (meters <= 0.0) {
        return QStringLiteral("--");
    }

    if (meters < 1000.0) {
        return QStringLiteral("%1 米")
            .arg(meters, 0, 'f', 0);
    }

    return QStringLiteral("%1 km")
        .arg(meters / 1000.0, 0, 'f', 1);
}

QString actionIcon(
    const QString &action)
{
    if (action.contains(
            QStringLiteral("掉头")) ||
        action.contains(
            QStringLiteral("调头"))) {

        return QStringLiteral("↶");
    }

    if (action.contains(
            QStringLiteral("左前"))) {

        return QStringLiteral("↰");
    }

    if (action.contains(
            QStringLiteral("右前"))) {

        return QStringLiteral("↱");
    }

    if (action.contains(
            QStringLiteral("左转")) ||
        action == QStringLiteral("左")) {

        return QStringLiteral("←");
    }

    if (action.contains(
            QStringLiteral("右转")) ||
        action == QStringLiteral("右")) {

        return QStringLiteral("→");
    }

    if (action.contains(
            QStringLiteral("直行")) ||
        action.contains(
            QStringLiteral("向前"))) {

        return QStringLiteral("↑");
    }

    return QString();
}

} // namespace


// ============================================================================
// Constructor
// ============================================================================
NavigationPage::NavigationPage(
    QWidget *parent)
    : QWidget(parent)
    , m_routePlanner(
          new RoutePlanner(this))
{
    setObjectName(
        QStringLiteral(
            "navigationPage"));

    m_simulationTimer = new QTimer(this);
    m_simulationTimer->setInterval(3000);

    connect(
        m_simulationTimer,
        &QTimer::timeout,
        this,
        [this]() {
            if (m_currentStepIndex + 1 >=
                m_routeSteps.size()) {
                m_simulationTimer->stop();
                m_simulationLabel->setText(
                    QStringLiteral("模拟导航已完成"));
                m_continueButton->setText(
                    QStringLiteral("重新模拟"));
                return;
            }

            ++m_currentStepIndex;
            showSimulationStep();
        });


    connect(
        m_routePlanner,
        &RoutePlanner::routeReady,
        this,
        [this](
            quint64,
            const RouteResult &result) {

            const QString modeText =
                m_currentRequest.mode ==
                        QStringLiteral("walking")
                    ? QStringLiteral("步行")
                    : QStringLiteral("驾车");

            QString summary =
                modeText;

            if (result.distanceMeters > 0.0) {
                summary +=
                    QStringLiteral(" · %1 km")
                        .arg(
                            result.distanceMeters / 1000.0,
                            0,
                            'f',
                            1);
            }

            if (result.durationSeconds > 0) {
                summary +=
                    QStringLiteral(" · 预计 %1")
                        .arg(
                            formatDuration(
                                result.durationSeconds));
            }

            m_routeSummaryLabel->setText(
                summary);
            m_routeDistanceValue->setText(
                formatRouteDistance(
                    result.distanceMeters));
            m_routeDurationValue->setText(
                formatDuration(
                    result.durationSeconds));
            m_routeEtaValue->setText(
                result.durationSeconds > 0
                    ? QDateTime::currentDateTime()
                          .addSecs(result.durationSeconds)
                          .toString(QStringLiteral("HH:mm"))
                    : QStringLiteral("--"));
            m_continueButton->setEnabled(true);
            m_routeSteps = result.steps;
            m_currentStepIndex =
                m_routeSteps.isEmpty() ? -1 : 0;
            m_simulationLabel->setText(
                QStringLiteral("模拟导航未开始"));
            m_continueButton->setText(
                QStringLiteral("继续导航"));
            m_loadProgress->hide();
            m_loadStatusLabel->setText(
                QStringLiteral("路线规划完成"));

            if (result.steps.isEmpty()) {
                m_stepActionLabel->setText(
                    QStringLiteral("—"));
                m_stepInstructionLabel->setText(
                    QStringLiteral(
                        "暂无可用的路线步骤"));
                m_stepDetailLabel->setText(
                    QStringLiteral(
                        "高德未返回分步导航信息"));
            } else {
                const RouteStep &step =
                    result.steps.first();
                const QString icon =
                    actionIcon(step.action);

                m_stepActionLabel->setText(
                    icon.isEmpty()
                        ? QStringLiteral("—")
                        : icon);
                m_stepInstructionLabel->setText(
                    step.instruction.isEmpty()
                        ? QStringLiteral(
                              "该步骤未提供文字指引")
                        : step.instruction);

                QStringList details;

                if (!step.road.isEmpty()) {
                    details.append(step.road);
                }

                if (step.distanceMeters > 0.0) {
                    details.append(
                        QStringLiteral("%1 米")
                            .arg(
                                step.distanceMeters,
                                0,
                                'f',
                                0));
                }

                m_stepDetailLabel->setText(
                    details.isEmpty()
                        ? QStringLiteral(
                              "未提供道路与距离信息")
                        : details.join(
                              QStringLiteral(" · ")));

            }

            m_mapWidget->setRoute(
                result.points);
        });


    connect(
        m_routePlanner,
        &RoutePlanner::routeError,
        this,
        [this](
            quint64,
            const QString &message) {

            m_loadProgress->hide();
            m_routeDistanceValue->setText(
                QStringLiteral("--"));
            m_routeDurationValue->setText(
                QStringLiteral("--"));
            m_routeEtaValue->setText(
                QStringLiteral("--"));
            m_continueButton->setEnabled(false);
            m_routeSteps.clear();
            m_currentStepIndex = -1;
            m_simulationLabel->setText(
                QStringLiteral("模拟导航不可用"));
            m_stepActionLabel->setText(
                QStringLiteral("—"));
            m_stepInstructionLabel->setText(
                QStringLiteral(
                    "暂无可用的路线步骤"));
            m_stepDetailLabel->setText(
                QStringLiteral(
                    "路线规划成功后显示第一条指引"));
            setMapPlaceholder(
                QString(),
                QString());

            if (message ==
                QStringLiteral(
                    "起点或终点坐标无效")) {

                m_loadStatusLabel->setText(
                    QStringLiteral("坐标无效"));
                m_routeSummaryLabel->setText(
                    QStringLiteral(
                        "请重新定位后再进行导航"));
                return;
            }

            if (message ==
                QStringLiteral(
                    "未配置高德 Web 服务 API Key")) {

                m_loadStatusLabel->setText(
                    QStringLiteral("缺少地图 Key"));
                m_routeSummaryLabel->setText(message);
                return;
            }

            if (message ==
                QStringLiteral("路线请求超时")) {

                m_loadStatusLabel->setText(
                    QStringLiteral("路线规划超时"));
                m_routeSummaryLabel->setText(message);
                return;
            }

            if (message ==
                QStringLiteral(
                    "高德返回的数据不是有效 JSON")) {

                m_loadStatusLabel->setText(
                    QStringLiteral("路线数据解析失败"));
                m_routeSummaryLabel->setText(message);
                return;
            }

            if (message ==
                    QStringLiteral("OVER_DIRECTION_RANGE") &&
                m_currentRequest.mode ==
                    QStringLiteral("walking")) {

                const double straightKm =
                    straightLineDistanceKm(
                        m_currentRequest.fromLat,
                        m_currentRequest.fromLng,
                        m_currentRequest.toLat,
                        m_currentRequest.toLng);

                m_loadStatusLabel->setText(
                    QStringLiteral("距离过远"));
                m_routeSummaryLabel->setText(
                    QStringLiteral(
                        "当前直线距离约 %1 km，"
                        "无法规划步行路线，请选择驾车")
                        .arg(
                            straightKm,
                            0,
                            'f',
                            1));
                return;
            }

            if (message ==
                QStringLiteral(
                    "高德没有返回可用路线方案")) {

                m_loadStatusLabel->setText(
                    QStringLiteral("未找到路线"));
                m_routeSummaryLabel->setText(message);
                return;
            }

            if (message ==
                QStringLiteral(
                    "高德未返回 polyline 路线点")) {

                m_loadStatusLabel->setText(
                    QStringLiteral("路线轨迹不可用"));
                m_routeSummaryLabel->setText(message);
                return;
            }

            m_loadStatusLabel->setText(
                QStringLiteral("路线规划失败"));
            m_routeSummaryLabel->setText(message);
        });


    // ========================================================================
    // Root
    // ========================================================================
    auto *rootLayout =
        new QVBoxLayout(this);

    rootLayout->setContentsMargins(
        0,
        0,
        0,
        0);

    rootLayout->setSpacing(0);


    // ========================================================================
    // Scroll
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

    scrollArea
        ->setHorizontalScrollBarPolicy(
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
    // Header
    // ========================================================================
    auto *headerLayout =
        new QHBoxLayout;

    headerLayout->setSpacing(
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


    auto *pageTitle =
        new QLabel(
            QStringLiteral(
                "导航到充电站"),
            content);

    pageTitle->setObjectName(
        QStringLiteral(
            "navigationTitle"));


    headerLayout->addWidget(
        backButton);

    headerLayout->addWidget(
        pageTitle,
        1);


    connect(
        backButton,
        &QPushButton::clicked,
        this,
        [this]() {
            stopAndResetSimulation();
            emit back();
        });


    mainLayout->addLayout(
        headerLayout);


    // ========================================================================
    // Station card
    // ========================================================================
    auto *stationCard =
        new QFrame(
            content);

    stationCard->setObjectName(
        QStringLiteral(
            "navigationCard"));

    UiTheme::applyCardShadow(
        stationCard,
        18,
        4);


    auto *stationLayout =
        new QVBoxLayout(
            stationCard);

    stationLayout->setContentsMargins(
        18,
        15,
        18,
        15);

    stationLayout->setSpacing(
        5);


    auto *stationCaption =
        new QLabel(
            QStringLiteral(
                "导航至"),
            stationCard);

    stationCaption->setObjectName(
        QStringLiteral(
            "navigationCaption"));


    m_stationLabel =
        new QLabel(
            QStringLiteral("--"),
            stationCard);

    m_stationLabel->setObjectName(
        QStringLiteral(
            "navigationStationName"));

    m_stationLabel->setWordWrap(
        true);


    stationLayout->addWidget(
        stationCaption);

    stationLayout->addWidget(
        m_stationLabel);


    mainLayout->addWidget(
        stationCard);


    // ========================================================================
    // Route card
    // ========================================================================
    auto *routeCard =
        new QFrame(
            content);

    routeCard->setObjectName(
        QStringLiteral(
            "navigationCard"));

    UiTheme::applyCardShadow(
        routeCard,
        18,
        4);


    auto *routeLayout =
        new QVBoxLayout(
            routeCard);

    routeLayout->setContentsMargins(
        18,
        16,
        18,
        16);

    routeLayout->setSpacing(
        11);


    auto *routeHeader =
        new QHBoxLayout;


    auto *routeTitle =
        new QLabel(
            QStringLiteral(
                "路线信息"),
            routeCard);

    routeTitle->setObjectName(
        QStringLiteral(
            "navigationSectionTitle"));


    m_routeModeLabel =
        new QLabel(
            QStringLiteral(
                "驾车路线"),
            routeCard);

    m_routeModeLabel->setObjectName(
        QStringLiteral(
            "navigationBadge"));

    m_routeModeLabel->setAlignment(
        Qt::AlignCenter);


    routeHeader->addWidget(
        routeTitle);

    routeHeader->addStretch();

    routeHeader->addWidget(
        m_routeModeLabel);


    routeLayout->addLayout(
        routeHeader);


    // ========================================================================
    // Start
    // ========================================================================
    auto *startRow =
        new QFrame(
            routeCard);

    startRow->setObjectName(
        QStringLiteral(
            "navigationPointRow"));


    auto *startLayout =
        new QHBoxLayout(
            startRow);

    startLayout->setContentsMargins(
        13,
        10,
        13,
        10);

    startLayout->setSpacing(
        11);


    auto *startIcon =
        new QLabel(
            QStringLiteral("起"),
            startRow);

    startIcon->setObjectName(
        QStringLiteral(
            "navigationStartIcon"));

    startIcon->setAlignment(
        Qt::AlignCenter);


    auto *startTextLayout =
        new QVBoxLayout;

    startTextLayout->setSpacing(
        2);


    auto *startCaption =
        new QLabel(
            QStringLiteral(
                "当前位置"),
            startRow);

    startCaption->setObjectName(
        QStringLiteral(
            "navigationCaption"));


    m_startLabel =
        new QLabel(
            QStringLiteral("--"),
            startRow);

    m_startLabel->setObjectName(
        QStringLiteral(
            "navigationCoordinate"));

    m_startLabel->setWordWrap(
        true);


    startTextLayout->addWidget(
        startCaption);

    startTextLayout->addWidget(
        m_startLabel);


    startLayout->addWidget(
        startIcon);

    startLayout->addLayout(
        startTextLayout,
        1);


    routeLayout->addWidget(
        startRow);


    auto *arrow =
        new QLabel(
            QStringLiteral("↓"),
            routeCard);

    arrow->setObjectName(
        QStringLiteral(
            "navigationArrow"));

    arrow->setAlignment(
        Qt::AlignCenter);


    routeLayout->addWidget(
        arrow);


    // ========================================================================
    // Target
    // ========================================================================
    auto *targetRow =
        new QFrame(
            routeCard);

    targetRow->setObjectName(
        QStringLiteral(
            "navigationPointRow"));


    auto *targetLayout =
        new QHBoxLayout(
            targetRow);

    targetLayout->setContentsMargins(
        13,
        10,
        13,
        10);

    targetLayout->setSpacing(
        11);


    auto *targetIcon =
        new QLabel(
            QStringLiteral("终"),
            targetRow);

    targetIcon->setObjectName(
        QStringLiteral(
            "navigationTargetIcon"));

    targetIcon->setAlignment(
        Qt::AlignCenter);


    auto *targetTextLayout =
        new QVBoxLayout;

    targetTextLayout->setSpacing(
        2);


    auto *targetCaption =
        new QLabel(
            QStringLiteral(
                "目的地"),
            targetRow);

    targetCaption->setObjectName(
        QStringLiteral(
            "navigationCaption"));


    m_targetLabel =
        new QLabel(
            QStringLiteral("--"),
            targetRow);

    m_targetLabel->setObjectName(
        QStringLiteral(
            "navigationCoordinate"));

    m_targetLabel->setWordWrap(
        true);


    targetTextLayout->addWidget(
        targetCaption);

    targetTextLayout->addWidget(
        m_targetLabel);


    targetLayout->addWidget(
        targetIcon);

    targetLayout->addLayout(
        targetTextLayout,
        1);


    routeLayout->addWidget(
        targetRow);


    // ========================================================================
    // Straight-line distance
    // ========================================================================
    auto *distanceRow =
        new QFrame(
            routeCard);

    distanceRow->setObjectName(
        QStringLiteral(
            "navigationDistanceRow"));


    auto *distanceLayout =
        new QHBoxLayout(
            distanceRow);

    distanceLayout->setContentsMargins(
        13,
        10,
        13,
        10);


    auto *distanceCaption =
        new QLabel(
            QStringLiteral(
                "距离"),
            distanceRow);

    distanceCaption->setObjectName(
        QStringLiteral(
            "navigationCaption"));


    m_distanceLabel =
        new QLabel(
            QStringLiteral(
                "距离 -- km"),
            distanceRow);

    m_distanceLabel->setObjectName(
        QStringLiteral(
            "navigationDistance"));

    m_distanceLabel->setAlignment(
        Qt::AlignRight |
        Qt::AlignVCenter);


    distanceLayout->addWidget(
        distanceCaption);

    distanceLayout->addStretch();

    distanceLayout->addWidget(
        m_distanceLabel);


    routeLayout->addWidget(
        distanceRow);


    mainLayout->addWidget(
        routeCard);


    // ========================================================================
    // NO.11 mode switch
    // ========================================================================
    auto *modeCard =
        new QFrame(
            content);

    modeCard->setObjectName(
        QStringLiteral(
            "navigationCard"));


    auto *modeLayout =
        new QHBoxLayout(
            modeCard);

    modeLayout->setContentsMargins(
        14,
        11,
        14,
        11);

    modeLayout->setSpacing(
        8);


    auto *modeCaption =
        new QLabel(
            QStringLiteral(
                "出行方式"),
            modeCard);

    modeCaption->setObjectName(
        QStringLiteral(
            "navigationModeTitle"));


    m_driveButton =
        new QPushButton(
            QStringLiteral(
                "驾车"),
            modeCard);

    m_walkButton =
        new QPushButton(
            QStringLiteral(
                "步行"),
            modeCard);


    m_driveButton->setObjectName(
        QStringLiteral(
            "navigationModeButton"));

    m_walkButton->setObjectName(
        QStringLiteral(
            "navigationModeButton"));


    m_driveButton->setCheckable(
        true);

    m_walkButton->setCheckable(
        true);


    m_driveButton->setCursor(
        Qt::PointingHandCursor);

    m_walkButton->setCursor(
        Qt::PointingHandCursor);


    auto *buttonGroup =
        new QButtonGroup(
            this);

    buttonGroup->setExclusive(
        true);

    buttonGroup->addButton(
        m_driveButton);

    buttonGroup->addButton(
        m_walkButton);


    m_driveButton->setChecked(
        true);


    connect(
        m_driveButton,
        &QPushButton::clicked,
        this,
        [this]() {

            setRouteMode(
                QStringLiteral(
                    "driving"));
        });


    connect(
        m_walkButton,
        &QPushButton::clicked,
        this,
        [this]() {

            setRouteMode(
                QStringLiteral(
                    "walking"));
        });


    modeLayout->addWidget(
        modeCaption);

    modeLayout->addStretch();

    modeLayout->addWidget(
        m_driveButton);

    modeLayout->addWidget(
        m_walkButton);


    mainLayout->addWidget(
        modeCard);


    // ========================================================================
    // First real route step
    // ========================================================================
    auto *stepCard =
        new QFrame(content);

    stepCard->setObjectName(
        QStringLiteral(
            "navigationStepCard"));


    auto *stepLayout =
        new QHBoxLayout(stepCard);

    stepLayout->setContentsMargins(
        14,
        12,
        14,
        12);

    stepLayout->setSpacing(12);


    m_stepActionLabel =
        new QLabel(
            QStringLiteral("—"),
            stepCard);

    m_stepActionLabel->setObjectName(
        QStringLiteral(
            "navigationStepAction"));

    m_stepActionLabel->setAlignment(
        Qt::AlignCenter);


    auto *stepTextLayout =
        new QVBoxLayout;

    stepTextLayout->setSpacing(3);


    m_stepInstructionLabel =
        new QLabel(
            QStringLiteral(
                "暂无可用的路线步骤"),
            stepCard);

    m_stepInstructionLabel->setObjectName(
        QStringLiteral(
            "navigationStepInstruction"));

    m_stepInstructionLabel->setWordWrap(true);


    m_stepDetailLabel =
        new QLabel(
            QStringLiteral(
                "路线规划成功后显示第一条指引"),
            stepCard);

    m_stepDetailLabel->setObjectName(
        QStringLiteral(
            "navigationStepDetail"));

    m_stepDetailLabel->setWordWrap(true);


    stepTextLayout->addWidget(
        m_stepInstructionLabel);

    stepTextLayout->addWidget(
        m_stepDetailLabel);


    stepLayout->addWidget(
        m_stepActionLabel);

    stepLayout->addLayout(
        stepTextLayout,
        1);


    // ========================================================================
    // Map card
    // ========================================================================
    auto *mapCard =
        new QFrame(
            content);

    mapCard->setObjectName(
        QStringLiteral(
            "navigationCard"));

    auto *mapLayout =
        new QVBoxLayout(
            mapCard);

    mapLayout->setContentsMargins(
        14,
        14,
        14,
        14);

    mapLayout->setSpacing(
        9);


    auto *mapHeader =
        new QHBoxLayout;


    auto *mapTitle =
        new QLabel(
            QStringLiteral(
                "地图路线"),
            mapCard);

    mapTitle->setObjectName(
        QStringLiteral(
            "navigationSectionTitle"));


    m_loadStatusLabel =
        new QLabel(
            QStringLiteral(
                "请选择充电站"),
            mapCard);

    m_loadStatusLabel->setObjectName(
        QStringLiteral(
            "navigationLoadStatus"));

    m_loadStatusLabel->setAlignment(
        Qt::AlignRight |
        Qt::AlignVCenter);


    mapHeader->addWidget(
        mapTitle);

    mapHeader->addStretch();

    mapHeader->addWidget(
        m_loadStatusLabel);


    mapLayout->addLayout(
        mapHeader);


    // ========================================================================
    // Real route summary
    // ========================================================================
    m_routeSummaryLabel =
        new QLabel(
            QStringLiteral(
                "路线距离与预计耗时将在规划完成后显示"),
            mapCard);

    m_routeSummaryLabel->setObjectName(
        QStringLiteral(
            "navigationRouteSummary"));

    m_routeSummaryLabel->setWordWrap(
        true);


    mapLayout->addWidget(
        m_routeSummaryLabel);


    // ========================================================================
    // Network progress
    // ========================================================================
    m_loadProgress =
        new QProgressBar(
            mapCard);

    m_loadProgress->setObjectName(
        QStringLiteral(
            "navigationLoadProgress"));

    m_loadProgress->setTextVisible(
        false);

    m_loadProgress->hide();


    mapLayout->addWidget(
        m_loadProgress);


    auto *mapHost =
        new QWidget(mapCard);

    mapHost->setObjectName(
        QStringLiteral(
            "navigationMapHost"));

    mapHost->setMinimumHeight(480);
    mapHost->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Expanding);

    auto *mapStack =
        new QStackedLayout(mapHost);

    mapStack->setContentsMargins(0, 0, 0, 0);
    mapStack->setStackingMode(
        QStackedLayout::StackAll);


    m_mapWidget =
        new AmapWidget(mapHost);

    m_mapWidget->setObjectName(
        QStringLiteral(
            "navigationMap"));

    m_mapWidget->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Expanding);


    auto *mapOverlay =
        new QWidget(mapHost);

    mapOverlay->setObjectName(
        QStringLiteral(
            "navigationMapOverlay"));
    mapOverlay->setAttribute(
        Qt::WA_TransparentForMouseEvents,
        true);

    auto *overlayLayout =
        new QVBoxLayout(mapOverlay);

    overlayLayout->setContentsMargins(
        16,
        16,
        16,
        16);
    overlayLayout->setSpacing(0);
    overlayLayout->addWidget(
        stepCard,
        0,
        Qt::AlignTop);
    overlayLayout->addStretch();

    mapStack->addWidget(m_mapWidget);
    mapStack->addWidget(mapOverlay);
    mapStack->setCurrentWidget(mapOverlay);


    mapLayout->addWidget(
        mapHost,
        1);


    // ========================================================================
    // Real route metrics
    // ========================================================================
    auto *metricsPanel =
        new QFrame(mapCard);

    metricsPanel->setObjectName(
        QStringLiteral(
            "navigationMetricsPanel"));

    auto *metricsLayout =
        new QHBoxLayout(metricsPanel);

    metricsLayout->setContentsMargins(8, 8, 8, 8);
    metricsLayout->setSpacing(0);

    const auto addMetric =
        [metricsPanel, metricsLayout](
            const QString &caption,
            QLabel **valueLabel) {
            auto *cell = new QWidget(metricsPanel);
            cell->setObjectName(
                QStringLiteral("navigationMetricCell"));

            auto *cellLayout = new QVBoxLayout(cell);
            cellLayout->setContentsMargins(6, 2, 6, 2);
            cellLayout->setSpacing(2);

            auto *captionLabel =
                new QLabel(caption, cell);
            captionLabel->setObjectName(
                QStringLiteral("navigationMetricCaption"));
            captionLabel->setAlignment(Qt::AlignCenter);

            *valueLabel =
                new QLabel(QStringLiteral("--"), cell);
            (*valueLabel)->setObjectName(
                QStringLiteral("navigationMetricValue"));
            (*valueLabel)->setAlignment(Qt::AlignCenter);

            cellLayout->addWidget(captionLabel);
            cellLayout->addWidget(*valueLabel);
            metricsLayout->addWidget(cell, 1);
        };

    addMetric(
        QStringLiteral("距离"),
        &m_routeDistanceValue);
    addMetric(
        QStringLiteral("耗时"),
        &m_routeDurationValue);
    addMetric(
        QStringLiteral("预计到达"),
        &m_routeEtaValue);

    mapLayout->addWidget(metricsPanel);


    mainLayout->addWidget(
        mapCard,
        1);


    // ========================================================================
    // Bottom actions
    // ========================================================================
    m_simulationLabel =
        new QLabel(
            QStringLiteral("模拟导航未开始"),
            content);
    m_simulationLabel->setObjectName(
        QStringLiteral("navigationSimulationLabel"));

    mainLayout->addWidget(
        m_simulationLabel,
        0,
        Qt::AlignRight);

    auto *bottomActions =
        new QHBoxLayout;

    bottomActions->setSpacing(10);

    auto *finishButton =
        new QPushButton(
            QStringLiteral("结束导航"),
            content);

    finishButton->setObjectName(
        QStringLiteral(
            "navigationFinishButton"));
    finishButton->setCursor(Qt::PointingHandCursor);

    m_continueButton =
        new QPushButton(
            QStringLiteral("继续导航"),
            content);

    m_continueButton->setObjectName(
        QStringLiteral(
            "navigationContinueButton"));
    m_continueButton->setCursor(Qt::PointingHandCursor);
    m_continueButton->setEnabled(false);

    bottomActions->addWidget(finishButton, 1);
    bottomActions->addWidget(m_continueButton, 1);

    connect(
        finishButton,
        &QPushButton::clicked,
        this,
        [this]() {
            stopAndResetSimulation();
            emit back();
        });

    connect(
        m_continueButton,
        &QPushButton::clicked,
        this,
        [this]() {
            if (m_routeSteps.isEmpty()) {
                return;
            }

            if (m_currentStepIndex < 0 ||
                m_currentStepIndex >= m_routeSteps.size() - 1) {
                m_currentStepIndex = 0;
                showSimulationStep();
            }

            m_simulationLabel->setText(
                QStringLiteral("模拟导航进行中"));
            m_continueButton->setText(
                QStringLiteral("模拟导航中"));
            m_simulationTimer->start();
        });

    mainLayout->addLayout(bottomActions);

    mainLayout->addStretch();


    scrollArea->setWidget(
        content);

    rootLayout->addWidget(
        scrollArea);


    setMapPlaceholder(
        QStringLiteral(
            "地图路线"),
        QStringLiteral(
            "请选择充电站后查看真实路线"));


    applyResponsiveStyle();
}


// ============================================================================
// Set RouteRequest
// ============================================================================
void NavigationPage::setNavigationData(
    const RouteRequest &request)
{
    stopAndResetSimulation();

    m_currentRequest =
        request;

    m_hasRouteRequest =
        true;


    if (m_currentRequest.mode !=
        QStringLiteral(
            "walking")) {

        m_currentRequest.mode =
            QStringLiteral(
                "driving");
    }


    // ========================================================================
    // Station
    // ========================================================================
    m_stationLabel->setText(
        request.toName
                .trimmed()
                .isEmpty()
            ? QStringLiteral("--")
            : request.toName);


    // ========================================================================
    // Start
    // ========================================================================
    m_startLabel->setText(
        QStringLiteral(
            "%1, %2")
            .arg(
                request.fromLat,
                0,
                'f',
                6)
            .arg(
                request.fromLng,
                0,
                'f',
                6));


    // ========================================================================
    // Target
    // ========================================================================
    m_targetLabel->setText(
        QStringLiteral(
            "%1, %2")
            .arg(
                request.toLat,
                0,
                'f',
                6)
            .arg(
                request.toLng,
                0,
                'f',
                6));


    // ========================================================================
    // Straight-line distance
    // ========================================================================
    if (request.distance >=
        0.0) {

        m_distanceLabel->setText(
            QStringLiteral(
                "直线距离约 %1 km")
                .arg(
                    request.distance,
                    0,
                    'f',
                    1));

    } else {

        m_distanceLabel->setText(
            QStringLiteral(
                "距离 -- km"));
    }


    const bool walking =
        m_currentRequest.mode ==
        QStringLiteral(
            "walking");


    m_driveButton->setChecked(
        !walking);

    m_walkButton->setChecked(
        walking);


    m_routeModeLabel->setText(
        walking
            ? QStringLiteral(
                  "步行路线")
            : QStringLiteral(
                  "驾车路线"));


    loadRoute();
}


// ============================================================================
// NO.11 switch route mode
// ============================================================================
void NavigationPage::setRouteMode(
    const QString &mode)
{
    if (!m_hasRouteRequest) {
        return;
    }


    const QString normalized =
        mode ==
                QStringLiteral(
                    "walking")
            ? QStringLiteral(
                  "walking")
            : QStringLiteral(
                  "driving");


    if (m_currentRequest.mode ==
        normalized) {

        return;
    }


    m_currentRequest.mode =
        normalized;


    const bool walking =
        normalized ==
        QStringLiteral(
            "walking");


    m_driveButton->setChecked(
        !walking);

    m_walkButton->setChecked(
        walking);


    m_routeModeLabel->setText(
        walking
            ? QStringLiteral(
                  "步行路线")
            : QStringLiteral(
                  "驾车路线"));


    loadRoute();
}


// ============================================================================
// Read Web Service Key
// ============================================================================
// ============================================================================
// NO.9 real route planning
// ============================================================================
void NavigationPage::loadRoute()
{
    if (!m_hasRouteRequest) {
        return;
    }

    stopAndResetSimulation();

    const bool walking =
        m_currentRequest.mode ==
        QStringLiteral("walking");

    m_stepActionLabel->setText(
        QStringLiteral("—"));
    m_stepInstructionLabel->setText(
        QStringLiteral(
            "正在获取路线步骤"));
    m_stepDetailLabel->setText(
        QStringLiteral(
            "请稍候"));
    m_routeDistanceValue->setText(
        QStringLiteral("--"));
    m_routeDurationValue->setText(
        QStringLiteral("--"));
    m_routeEtaValue->setText(
        QStringLiteral("--"));
    m_continueButton->setEnabled(false);

    if (walking) {
        const double straightKm =
            straightLineDistanceKm(
                m_currentRequest.fromLat,
                m_currentRequest.fromLng,
                m_currentRequest.toLat,
                m_currentRequest.toLng);

        if (straightKm >
            kMaxWalkingDistanceKm) {

            m_routePlanner->cancelPending();
            m_loadProgress->hide();
            m_loadStatusLabel->setText(
                QStringLiteral("距离过远"));
            m_routeSummaryLabel->setText(
                QStringLiteral(
                    "当前直线距离约 %1 km，"
                    "超出步行路线规划范围，请选择驾车")
                    .arg(
                        straightKm,
                        0,
                        'f',
                        1));
            setMapPlaceholder(
                QString(),
                QString());
            return;
        }
    }

    setMapPlaceholder(
        QString(),
        QString());

    m_loadStatusLabel->setText(
        QStringLiteral("路线规划中"));

    m_routeSummaryLabel->setText(
        walking
            ? QStringLiteral("正在规划步行路线…")
            : QStringLiteral("正在规划驾车路线…"));

    m_loadProgress->setRange(0, 0);
    m_loadProgress->show();

    m_routePlanner->planRoute(
        m_currentRequest);
}


// ============================================================================
// Placeholder
// ============================================================================
void NavigationPage::setMapPlaceholder(
    const QString &title,
    const QString &message)
{
    Q_UNUSED(title);
    Q_UNUSED(message);

    if (m_mapWidget) {
        m_mapWidget->setRoute(
            QStringList());
    }
}


// ============================================================================
// Simulated navigation step progression
// ============================================================================
void NavigationPage::showSimulationStep()
{
    if (m_currentStepIndex < 0 ||
        m_currentStepIndex >= m_routeSteps.size()) {
        return;
    }

    const RouteStep &step =
        m_routeSteps.at(m_currentStepIndex);
    const QString icon = actionIcon(step.action);

    m_stepActionLabel->setText(
        icon.isEmpty() ? QStringLiteral("—") : icon);
    m_stepInstructionLabel->setText(
        step.instruction.isEmpty()
            ? QStringLiteral("该步骤未提供文字指引")
            : step.instruction);

    QStringList details;
    if (!step.road.isEmpty()) {
        details.append(step.road);
    }
    if (step.distanceMeters > 0.0) {
        details.append(
            QStringLiteral("%1 米")
                .arg(step.distanceMeters, 0, 'f', 0));
    }

    m_stepDetailLabel->setText(
        details.isEmpty()
            ? QStringLiteral("未提供道路与距离信息")
            : details.join(QStringLiteral(" · ")));
}

void NavigationPage::stopAndResetSimulation()
{
    if (m_simulationTimer) {
        m_simulationTimer->stop();
    }

    m_routeSteps.clear();
    m_currentStepIndex = -1;

    if (m_simulationLabel) {
        m_simulationLabel->setText(
            QStringLiteral("模拟导航未开始"));
    }

    if (m_continueButton) {
        m_continueButton->setText(
            QStringLiteral("继续导航"));
        m_continueButton->setEnabled(false);
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
// Responsive UI
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


    const int normalFont =
        scaledUi(
            scaleBase,
            14);


    const int smallFont =
        scaledUi(
            scaleBase,
            12);


    const int cardRadius =
        scaledUi(
            scaleBase,
            18);


    const int smallRadius =
        scaledUi(
            scaleBase,
            10);


    // ========================================================================
    // Style
    // ========================================================================
    setStyleSheet(
        QStringLiteral(

            "QWidget#navigationPage{"
            "background:transparent;"
            "color:#202824;"
            "}"

            "QWidget#navigationContent{"
            "background:transparent;"
            "}"

            "QScrollArea#navigationScrollArea{"
            "background:transparent;"
            "border:none;"
            "}"


            // Back
            "QPushButton#navigationBackButton{"
            "background:#E9F0EC;"
            "color:#315B4D;"
            "border:1px solid #D6E1DA;"
            "border-radius:%1px;"
            "padding:7px 13px;"
            "font-size:%2px;"
            "font-weight:700;"
            "}"

            "QPushButton#navigationBackButton:hover{"
            "background:#DFE9E3;"
            "}"


            // Title
            "QLabel#navigationTitle{"
            "background:transparent;"
            "color:#202824;"
            "font-size:%3px;"
            "font-weight:800;"
            "}"


            // Cards
            "QFrame#navigationCard{"
            "background:#FFFFFF;"
            "border:1px solid #E7E3DA;"
            "border-radius:%4px;"
            "}"


            // Caption
            "QLabel#navigationCaption{"
            "background:transparent;"
            "color:#7A837E;"
            "font-size:%5px;"
            "}"


            // Station
            "QLabel#navigationStationName{"
            "background:transparent;"
            "color:#202824;"
            "font-size:%6px;"
            "font-weight:800;"
            "}"


            // Section title
            "QLabel#navigationSectionTitle{"
            "background:transparent;"
            "color:#202824;"
            "font-size:%2px;"
            "font-weight:800;"
            "}"


            // Badge
            "QLabel#navigationBadge{"
            "background:#E9F0EC;"
            "color:#315B4D;"
            "border:none;"
            "border-radius:%1px;"
            "padding:4px 9px;"
            "font-size:%5px;"
            "font-weight:700;"
            "}"


            // Point rows
            "QFrame#navigationPointRow{"
            "background:#FAF8F3;"
            "border:1px solid #E7E3DA;"
            "border-radius:%1px;"
            "}"


            "QLabel#navigationCoordinate{"
            "background:transparent;"
            "color:#202824;"
            "font-size:%2px;"
            "font-weight:600;"
            "}"


            // Start
            "QLabel#navigationStartIcon{"
            "background:#EAF3ED;"
            "color:#4F8668;"
            "border:none;"
            "border-radius:%1px;"
            "font-size:%5px;"
            "font-weight:800;"
            "min-width:30px;"
            "min-height:30px;"
            "}"


            // End
            "QLabel#navigationTargetIcon{"
            "background:#FFF3DF;"
            "color:#A86D1E;"
            "border:none;"
            "border-radius:%1px;"
            "font-size:%5px;"
            "font-weight:800;"
            "min-width:30px;"
            "min-height:30px;"
            "}"


            // Arrow
            "QLabel#navigationArrow{"
            "background:transparent;"
            "color:#315B4D;"
            "font-size:%3px;"
            "font-weight:700;"
            "}"


            // Distance
            "QFrame#navigationDistanceRow{"
            "background:#E9F0EC;"
            "border:1px solid #DCE5DF;"
            "border-radius:%1px;"
            "}"

            "QLabel#navigationDistance{"
            "background:transparent;"
            "color:#315B4D;"
            "font-size:%2px;"
            "font-weight:800;"
            "}"


            // Mode title
            "QLabel#navigationModeTitle{"
            "background:transparent;"
            "color:#202824;"
            "font-size:%2px;"
            "font-weight:700;"
            "}"


            // Mode buttons
            "QPushButton#navigationModeButton{"
            "background:#FAF8F3;"
            "color:#7A837E;"
            "border:1px solid #E1DDD4;"
            "border-radius:%1px;"
            "padding:8px 16px;"
            "font-size:%2px;"
            "font-weight:700;"
            "}"

            "QPushButton#navigationModeButton:hover{"
            "background:#F0EEE8;"
            "}"

            "QPushButton#navigationModeButton:checked{"
            "background:#315B4D;"
            "color:#FFFFFF;"
            "border-color:#315B4D;"
            "}"


            // First route step
            "QFrame#navigationStepCard{"
            "background:rgba(255,255,255,238);"
            "border:1px solid #B9D0EE;"
            "border-radius:%4px;"
            "}"

            "QWidget#navigationMapOverlay{"
            "background:transparent;"
            "}"

            "QLabel#navigationStepAction{"
            "background:#2F80ED;"
            "color:#FFFFFF;"
            "border:none;"
            "border-radius:%1px;"
            "font-size:%3px;"
            "font-weight:800;"
            "min-width:42px;"
            "min-height:42px;"
            "}"

            "QLabel#navigationStepInstruction{"
            "background:transparent;"
            "color:#202824;"
            "font-size:%2px;"
            "font-weight:700;"
            "}"

            "QLabel#navigationStepDetail{"
            "background:transparent;"
            "color:#66757F;"
            "font-size:%5px;"
            "}"


            // Status
            "QLabel#navigationLoadStatus{"
            "background:transparent;"
            "color:#7A837E;"
            "font-size:%5px;"
            "}"


            // Summary
            "QLabel#navigationRouteSummary{"
            "background:#FAF8F3;"
            "color:#315B4D;"
            "border:1px solid #E7E3DA;"
            "border-radius:%1px;"
            "padding:8px 10px;"
            "font-size:%5px;"
            "font-weight:600;"
            "}"


            // Route metrics
            "QFrame#navigationMetricsPanel{"
            "background:#F5F8F6;"
            "border:1px solid #DCE5DF;"
            "border-radius:%1px;"
            "}"

            "QWidget#navigationMetricCell{"
            "background:transparent;"
            "border:none;"
            "}"

            "QLabel#navigationMetricCaption{"
            "background:transparent;"
            "color:#7A837E;"
            "font-size:%5px;"
            "}"

            "QLabel#navigationMetricValue{"
            "background:transparent;"
            "color:#202824;"
            "font-size:%2px;"
            "font-weight:800;"
            "}"


            "QLabel#navigationSimulationLabel{"
            "background:#EAF2FF;"
            "color:#2F6FBB;"
            "border:1px solid #C9DCF8;"
            "border-radius:%1px;"
            "padding:3px 8px;"
            "font-size:%5px;"
            "font-weight:700;"
            "}"

            // Bottom actions
            "QPushButton#navigationFinishButton,"
            "QPushButton#navigationContinueButton{"
            "border-radius:%1px;"
            "padding:11px 18px;"
            "font-size:%2px;"
            "font-weight:800;"
            "}"

            "QPushButton#navigationFinishButton{"
            "background:#FFFFFF;"
            "color:#315B4D;"
            "border:1px solid #BFCFC6;"
            "}"

            "QPushButton#navigationFinishButton:hover{"
            "background:#F0F5F2;"
            "}"

            "QPushButton#navigationContinueButton{"
            "background:#315B4D;"
            "color:#FFFFFF;"
            "border:1px solid #315B4D;"
            "}"

            "QPushButton#navigationContinueButton:hover{"
            "background:#274B40;"
            "}"

            "QPushButton#navigationContinueButton:disabled{"
            "background:#AEBBB4;"
            "color:#EEF1EF;"
            "border-color:#AEBBB4;"
            "}"


            // Progress
            "QProgressBar#navigationLoadProgress{"
            "background:#E7E5DF;"
            "border:none;"
            "border-radius:4px;"
            "min-height:8px;"
            "max-height:8px;"
            "}"

            "QProgressBar#navigationLoadProgress::chunk{"
            "background:#315B4D;"
            "border-radius:4px;"
            "}"


            // Map
            "AmapWidget#navigationMap{"
            "background:#FAF8F3;"
            "border:1px solid #E7E3DA;"
            "border-radius:%1px;"
            "}")

        .arg(
            smallRadius)      // %1

        .arg(
            normalFont)       // %2

        .arg(
            titleFont)        // %3

        .arg(
            cardRadius)       // %4

        .arg(
            smallFont)        // %5

        .arg(
            stationFont));    // %6


    if (m_mapWidget) {

        m_mapWidget->setMinimumHeight(
            scaledUi(
                scaleBase,
                480));
    }
}
