#include "navigationpage.h"

#include "uitheme.h"
#include "windowhelper.h"

#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSizePolicy>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>


namespace {

// ============================================================================
// 坐标检查
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


// ============================================================================
// NO.11：步行路线最大距离保护
//
// 高德步行路径规划适用于 100 km 以内路线。
// 超过该范围时不再发送 walking 请求，
// 避免直接向用户显示 OVER_DIRECTION_RANGE。
// ============================================================================
constexpr double kMaxWalkingDistanceKm =
    100.0;


// ============================================================================
// 根据经纬度计算两点直线距离（Haversine）
// ============================================================================
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

            return
                degrees *
                3.14159265358979323846 /
                180.0;
        };


    const double lat1 =
        toRadians(
            fromLat);

    const double lat2 =
        toRadians(
            toLat);

    const double deltaLat =
        toRadians(
            toLat -
            fromLat);

    const double deltaLng =
        toRadians(
            toLng -
            fromLng);


    const double sinLat =
        std::sin(
            deltaLat /
            2.0);

    const double sinLng =
        std::sin(
            deltaLng /
            2.0);


    const double a =
        sinLat *
            sinLat +
        std::cos(lat1) *
            std::cos(lat2) *
            sinLng *
            sinLng;


    const double clampedA =
        std::clamp(
            a,
            0.0,
            1.0);


    const double c =
        2.0 *
        std::atan2(
            std::sqrt(
                clampedA),
            std::sqrt(
                1.0 -
                clampedA));


    return
        kEarthRadiusKm *
        c;
}


// ============================================================================
// 高德坐标格式：经度,纬度
// ============================================================================
QString coordinateText(
    double lng,
    double lat)
{
    return QStringLiteral("%1,%2")
        .arg(
            lng,
            0,
            'f',
            6)
        .arg(
            lat,
            0,
            'f',
            6);
}


// ============================================================================
// JSON 数字兼容
//
// 高德部分字段是字符串：
// "distance":"1234"
//
// 某些情况下也可能直接返回数字。
// ============================================================================
double jsonDouble(
    const QJsonValue &value,
    double fallback = 0.0)
{
    if (value.isDouble()) {
        return value.toDouble();
    }


    if (value.isString()) {

        bool ok = false;

        const double result =
            value.toString().toDouble(
                &ok);

        if (ok) {
            return result;
        }
    }


    return fallback;
}


qint64 jsonInt64(
    const QJsonValue &value,
    qint64 fallback = 0)
{
    if (value.isDouble()) {

        return static_cast<qint64>(
            value.toDouble());
    }


    if (value.isString()) {

        bool ok = false;

        const qint64 result =
            value.toString().toLongLong(
                &ok);

        if (ok) {
            return result;
        }
    }


    return fallback;
}


// ============================================================================
// 将可能是 Array / Object 的字段统一转成 Array
// ============================================================================
QJsonArray jsonArrayValue(
    const QJsonValue &value)
{
    if (value.isArray()) {
        return value.toArray();
    }


    QJsonArray result;

    if (value.isObject()) {
        result.append(
            value.toObject());
    }


    return result;
}


// ============================================================================
// 从单个 polyline 字符串提取坐标
//
// 不死依赖 ";"，直接使用正则提取：
// 113.123456,22.123456
// ============================================================================
QStringList extractCoordinates(
    const QString &polyline)
{
    QStringList result;


    const QRegularExpression regex(
        QStringLiteral(
            "(-?\\d+(?:\\.\\d+)?),"
            "(-?\\d+(?:\\.\\d+)?)"));


    auto iterator =
        regex.globalMatch(
            polyline);


    QString lastPoint;


    while (iterator.hasNext()) {

        const QRegularExpressionMatch match =
            iterator.next();


        bool lngOk = false;
        bool latOk = false;


        const double lng =
            match.captured(1)
                .toDouble(
                    &lngOk);


        const double lat =
            match.captured(2)
                .toDouble(
                    &latOk);


        if (!lngOk ||
            !latOk ||
            !validCoordinate(
                lat,
                lng)) {

            continue;
        }


        const QString point =
            coordinateText(
                lng,
                lat);


        if (point == lastPoint) {
            continue;
        }


        result.append(
            point);

        lastPoint =
            point;
    }


    return result;
}


// ============================================================================
// 从高德 paths.steps 中提取完整路线
// ============================================================================
QStringList extractRoutePoints(
    const QJsonObject &path)
{
    QStringList result;


    const QString pathPolyline =
        path.value(
                QStringLiteral(
                    "polyline"))
            .toString();


    if (!pathPolyline.isEmpty()) {

        result =
            extractCoordinates(
                pathPolyline);
    }


    const QJsonArray steps =
        jsonArrayValue(
            path.value(
                QStringLiteral(
                    "steps")));


    QString lastPoint =
        result.isEmpty()
            ? QString()
            : result.last();


    for (const QJsonValue &stepValue :
         steps) {

        const QJsonObject step =
            stepValue.toObject();


        const QString polyline =
            step.value(
                    QStringLiteral(
                        "polyline"))
                .toString();


        if (polyline.isEmpty()) {
            continue;
        }


        const QStringList points =
            extractCoordinates(
                polyline);


        for (const QString &point :
             points) {

            if (point ==
                lastPoint) {

                continue;
            }


            result.append(
                point);

            lastPoint =
                point;
        }
    }


    return result;
}


// ============================================================================
// Static Map URL 不能无限长
//
// 路线点太多时均匀采样。
// 起点和终点会在调用处再次确保保留。
// ============================================================================
QStringList simplifyPoints(
    const QStringList &points,
    int maxPoints = 48)
{
    if (points.size() <=
        maxPoints) {

        return points;
    }


    QStringList result;

    result.reserve(
        maxPoints);


    const int lastIndex =
        points.size() - 1;


    for (int i = 0;
         i < maxPoints;
         ++i) {

        const double ratio =
            static_cast<double>(i) /
            static_cast<double>(
                maxPoints - 1);


        const int index =
            static_cast<int>(
                std::round(
                    ratio *
                    lastIndex));


        if (result.isEmpty() ||
            result.last() !=
                points.at(index)) {

            result.append(
                points.at(index));
        }
    }


    if (result.isEmpty() ||
        result.last() !=
            points.last()) {

        result.append(
            points.last());
    }


    return result;
}


// ============================================================================
// 时长格式化
// ============================================================================
QString formatDuration(
    qint64 seconds)
{
    if (seconds <= 0) {
        return QStringLiteral("--");
    }


    const qint64 hours =
        seconds / 3600;


    qint64 minutes =
        (seconds % 3600) /
        60;


    if (hours == 0 &&
        minutes == 0) {

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


// ============================================================================
// 尝试从 step 的 cost 中累计耗时
// ============================================================================
qint64 stepDuration(
    const QJsonObject &path)
{
    const QJsonArray steps =
        jsonArrayValue(
            path.value(
                QStringLiteral(
                    "steps")));


    qint64 total = 0;


    for (const QJsonValue &stepValue :
         steps) {

        const QJsonObject step =
            stepValue.toObject();


        const QJsonObject cost =
            step.value(
                    QStringLiteral(
                        "cost"))
                .toObject();


        qint64 duration =
            jsonInt64(
                cost.value(
                    QStringLiteral(
                        "duration")));


        if (duration <= 0) {

            duration =
                jsonInt64(
                    step.value(
                        QStringLiteral(
                            "duration")));
        }


        if (duration > 0) {

            total +=
                duration;
        }
    }


    return total;
}

} // namespace


// ============================================================================
// Constructor
// ============================================================================
NavigationPage::NavigationPage(
    QWidget *parent)
    : QWidget(parent)
    , m_networkManager(
          new QNetworkAccessManager(
              this))
{
    setObjectName(
        QStringLiteral(
            "navigationPage"));


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
                "一键导航"),
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
        &NavigationPage::back);


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
    // Map card
    // ========================================================================
    auto *mapCard =
        new QFrame(
            content);

    mapCard->setObjectName(
        QStringLiteral(
            "navigationCard"));

    UiTheme::applyCardShadow(
        mapCard,
        18,
        4);


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


    // ========================================================================
    // No WebEngine.
    //
    // Static Map is downloaded as image and displayed in QLabel.
    // ========================================================================
    m_mapLabel =
        new QLabel(
            mapCard);

    m_mapLabel->setObjectName(
        QStringLiteral(
            "navigationMapImage"));

    m_mapLabel->setAlignment(
        Qt::AlignCenter);

    m_mapLabel->setWordWrap(
        true);

    m_mapLabel->setMinimumHeight(
        360);

    m_mapLabel->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Expanding);


    mapLayout->addWidget(
        m_mapLabel);


    mainLayout->addWidget(
        mapCard);

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
QString NavigationPage::webServiceKey() const
{
    return qEnvironmentVariable(
               "AMAP_WEB_SERVICE_KEY")
        .trimmed();
}


// ============================================================================
// NO.9 real route planning
// ============================================================================
void NavigationPage::loadRoute()
{
    if (!m_hasRouteRequest) {
        return;
    }


    // ========================================================================
    // Coordinate validation
    // ========================================================================
    if (!validCoordinate(
            m_currentRequest.fromLat,
            m_currentRequest.fromLng) ||
        !validCoordinate(
            m_currentRequest.toLat,
            m_currentRequest.toLng)) {

        m_loadProgress->hide();


        m_loadStatusLabel->setText(
            QStringLiteral(
                "坐标无效"));


        m_routeSummaryLabel->setText(
            QStringLiteral(
                "请重新定位后再进行导航"));


        setMapPlaceholder(
            QStringLiteral(
                "无法规划路线"),
            QStringLiteral(
                "起点或终点坐标无效"));

        return;
    }


    // ========================================================================
    // 当前出行方式
    // ========================================================================
    const bool walking =
        m_currentRequest.mode ==
        QStringLiteral(
            "walking");


    // ========================================================================
    // NO.11：步行距离保护
    //
    // 高德步行规划最大支持约 100 km。
    //
    // 如果两点的直线距离已经超过 100 km，
    // 实际道路步行距离只会更长，因此直接阻止请求。
    // ========================================================================
    if (walking) {

        const double straightKm =
            straightLineDistanceKm(
                m_currentRequest.fromLat,
                m_currentRequest.fromLng,
                m_currentRequest.toLat,
                m_currentRequest.toLng);


        if (straightKm >
            kMaxWalkingDistanceKm) {

            // ================================================================
            // 让正在进行的旧请求失效。
            //
            // 否则用户刚从驾车切到步行时，
            // 旧驾车地图可能稍后返回并覆盖这里的提示。
            // ================================================================
            ++m_requestId;


            if (m_routeReply) {

                m_routeReply->abort();

                m_routeReply =
                    nullptr;
            }


            if (m_mapReply) {

                m_mapReply->abort();

                m_mapReply =
                    nullptr;
            }


            m_originalMapPixmap =
                QPixmap();


            m_loadProgress->hide();


            m_loadStatusLabel->setText(
                QStringLiteral(
                    "距离过远"));


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
                QStringLiteral(
                    "暂不支持步行规划"),
                QStringLiteral(
                    "起终点距离过远，请切换为「驾车」"));


            return;
        }
    }


    // ========================================================================
    // API Key
    // ========================================================================
    const QString key =
        webServiceKey();


    if (key.isEmpty()) {

        m_loadProgress->hide();


        m_loadStatusLabel->setText(
            QStringLiteral(
                "缺少地图 Key"));


        m_routeSummaryLabel->setText(
            QStringLiteral(
                "未配置高德 Web 服务 API Key"));


        setMapPlaceholder(
            QStringLiteral(
                "地图暂不可用"),
            QStringLiteral(
                "请配置 AMAP_WEB_SERVICE_KEY"));

        return;
    }


    // ========================================================================
    // New request
    // ========================================================================
    const quint64 requestId =
        ++m_requestId;


    // Abort old route request
    if (m_routeReply) {

        m_routeReply->abort();

        m_routeReply =
            nullptr;
    }


    // Abort old map request
    if (m_mapReply) {

        m_mapReply->abort();

        m_mapReply =
            nullptr;
    }


    m_originalMapPixmap =
        QPixmap();


    setMapPlaceholder(
        QStringLiteral(
            "正在规划路线"),
        QStringLiteral(
            "正在连接高德地图服务…"));


    // ========================================================================
    // AMap Route Planning 2.0
    // ========================================================================
    QUrl url(
        walking
            ? QStringLiteral(
                  "https://restapi.amap.com/v5/direction/walking")
            : QStringLiteral(
                  "https://restapi.amap.com/v5/direction/driving"));


    QUrlQuery query;


    query.addQueryItem(
        QStringLiteral(
            "key"),
        key);


    query.addQueryItem(
        QStringLiteral(
            "origin"),
        coordinateText(
            m_currentRequest.fromLng,
            m_currentRequest.fromLat));


    query.addQueryItem(
        QStringLiteral(
            "destination"),
        coordinateText(
            m_currentRequest.toLng,
            m_currentRequest.toLat));


    // cost:
    //   route duration
    //
    // polyline:
    //   road coordinate sequence
    query.addQueryItem(
        QStringLiteral(
            "show_fields"),
        QStringLiteral(
            "cost,polyline"));


    query.addQueryItem(
        QStringLiteral(
            "output"),
        QStringLiteral(
            "json"));


    // 驾车默认使用高德推荐
    if (!walking) {

        query.addQueryItem(
            QStringLiteral(
                "strategy"),
            QStringLiteral(
                "32"));
    }


    url.setQuery(
        query);


    // ========================================================================
    // UI loading
    // ========================================================================
    m_loadStatusLabel->setText(
        walking
            ? QStringLiteral(
                  "正在规划步行路线…")
            : QStringLiteral(
                  "正在规划驾车路线…"));


    m_routeSummaryLabel->setText(
        QStringLiteral(
            "正在获取真实路线数据"));


    m_loadProgress->setRange(
        0,
        0);

    m_loadProgress->show();


    // ========================================================================
    // Request
    // ========================================================================
    QNetworkRequest request(
        url);


    m_routeReply =
        m_networkManager->get(
            request);


    QNetworkReply *reply =
        m_routeReply;


    // ========================================================================
    // Timeout
    // ========================================================================
    QTimer::singleShot(
        8000,
        reply,
        [reply]() {

            if (!reply->isRunning()) {
                return;
            }


            reply->setProperty(
                "routeTimedOut",
                true);


            reply->abort();
        });


    // ========================================================================
    // Finish
    // ========================================================================
    connect(
        reply,
        &QNetworkReply::finished,
        this,
        [this,
         reply,
         requestId]() {

            if (m_routeReply ==
                reply) {

                m_routeReply =
                    nullptr;
            }


            // ================================================================
            // Old request
            // ================================================================
            if (requestId !=
                m_requestId) {

                reply->deleteLater();

                return;
            }


            const bool timedOut =
                reply->property(
                         "routeTimedOut")
                    .toBool();


            // ================================================================
            // Network error
            // ================================================================
            if (reply->error() !=
                QNetworkReply::NoError) {

                m_loadProgress->hide();


                const QString errorMessage =
                    timedOut
                        ? QStringLiteral(
                              "路线请求超时")
                        : reply->errorString();


                m_loadStatusLabel->setText(
                    timedOut
                        ? QStringLiteral(
                              "路线规划超时")
                        : QStringLiteral(
                              "路线规划失败"));


                m_routeSummaryLabel->setText(
                    errorMessage);


                setMapPlaceholder(
                    QStringLiteral(
                        "路线规划失败"),
                    timedOut
                        ? QStringLiteral(
                              "请求超时，请稍后重试")
                        : QStringLiteral(
                              "请检查网络后重试"));


                reply->deleteLater();

                return;
            }


            const QByteArray data =
                reply->readAll();


            reply->deleteLater();


            // ================================================================
            // Parse JSON
            // ================================================================
            QJsonParseError parseError;


            const QJsonDocument document =
                QJsonDocument::fromJson(
                    data,
                    &parseError);


            if (parseError.error !=
                    QJsonParseError::NoError ||
                !document.isObject()) {

                m_loadProgress->hide();


                m_loadStatusLabel->setText(
                    QStringLiteral(
                        "路线数据解析失败"));


                m_routeSummaryLabel->setText(
                    QStringLiteral(
                        "高德返回的数据不是有效 JSON"));


                setMapPlaceholder(
                    QStringLiteral(
                        "路线数据异常"),
                    QStringLiteral(
                        "请稍后重新尝试"));


                return;
            }


            const QJsonObject root =
                document.object();


            // ================================================================
            // AMap business error
            // ================================================================
            if (root.value(
                        QStringLiteral(
                            "status"))
                    .toString() !=
                QStringLiteral("1")) {

                QString info =
                    root.value(
                            QStringLiteral(
                                "info"))
                        .toString();


                // ------------------------------------------------------------
                // infocode 正常情况下是字符串，
                // 同时兼容返回数字的情况。
                // ------------------------------------------------------------
                const QJsonValue infoCodeValue =
                    root.value(
                        QStringLiteral(
                            "infocode"));


                QString infoCode =
                    infoCodeValue
                        .toString();


                if (infoCode.isEmpty() &&
                    infoCodeValue.isDouble()) {

                    infoCode =
                        QString::number(
                            static_cast<qint64>(
                                infoCodeValue
                                    .toDouble()));
                }


                // ------------------------------------------------------------
                // 高德：
                //
                // 20803
                // OVER_DIRECTION_RANGE
                //
                // 起终点距离过长。
                // ------------------------------------------------------------
                const bool overDirectionRange =
                    info ==
                        QStringLiteral(
                            "OVER_DIRECTION_RANGE") ||
                    infoCode ==
                        QStringLiteral(
                            "20803");


                if (overDirectionRange &&
                    m_currentRequest.mode ==
                        QStringLiteral(
                            "walking")) {

                    const double straightKm =
                        straightLineDistanceKm(
                            m_currentRequest.fromLat,
                            m_currentRequest.fromLng,
                            m_currentRequest.toLat,
                            m_currentRequest.toLng);


                    m_loadProgress->hide();


                    m_loadStatusLabel->setText(
                        QStringLiteral(
                            "距离过远"));


                    m_routeSummaryLabel->setText(
                        QStringLiteral(
                            "当前直线距离约 %1 km，"
                            "无法规划步行路线，请选择驾车")
                            .arg(
                                straightKm,
                                0,
                                'f',
                                1));


                    setMapPlaceholder(
                        QStringLiteral(
                            "暂不支持步行规划"),
                        QStringLiteral(
                            "起终点距离过远，请切换为「驾车」"));


                    return;
                }


                // ------------------------------------------------------------
                // 其它高德业务错误
                // ------------------------------------------------------------
                if (info.isEmpty()) {

                    info =
                        QStringLiteral(
                            "高德地图服务返回错误");
                }


                m_loadProgress->hide();


                m_loadStatusLabel->setText(
                    QStringLiteral(
                        "路线规划失败"));


                m_routeSummaryLabel->setText(
                    info);


                setMapPlaceholder(
                    QStringLiteral(
                        "未获取到路线"),
                    info);


                return;
            }


            // ================================================================
            // Route
            // ================================================================
            const QJsonObject route =
                root.value(
                        QStringLiteral(
                            "route"))
                    .toObject();


            const QJsonArray paths =
                jsonArrayValue(
                    route.value(
                        QStringLiteral(
                            "paths")));


            if (paths.isEmpty()) {

                m_loadProgress->hide();


                m_loadStatusLabel->setText(
                    QStringLiteral(
                        "未找到路线"));


                m_routeSummaryLabel->setText(
                    QStringLiteral(
                        "高德没有返回可用路线方案"));


                setMapPlaceholder(
                    QStringLiteral(
                        "没有可用路线"),
                    QStringLiteral(
                        "请尝试其它出行方式"));


                return;
            }


            // ================================================================
            // Use first route
            // ================================================================
            const QJsonObject path =
                paths.at(0)
                    .toObject();


            const double distanceMeters =
                jsonDouble(
                    path.value(
                        QStringLiteral(
                            "distance")));


            // ================================================================
            // Duration
            // ================================================================
            qint64 durationSeconds =
                0;


            const QJsonObject cost =
                path.value(
                        QStringLiteral(
                            "cost"))
                    .toObject();


            durationSeconds =
                jsonInt64(
                    cost.value(
                        QStringLiteral(
                            "duration")));


            if (durationSeconds <=
                0) {

                durationSeconds =
                    jsonInt64(
                        path.value(
                            QStringLiteral(
                                "duration")));
            }


            if (durationSeconds <=
                0) {

                durationSeconds =
                    stepDuration(
                        path);
            }


            // ================================================================
            // Polyline
            // ================================================================
            QStringList points =
                extractRoutePoints(
                    path);


            if (points.isEmpty()) {

                m_loadProgress->hide();


                m_loadStatusLabel->setText(
                    QStringLiteral(
                        "路线轨迹不可用"));


                m_routeSummaryLabel->setText(
                    QStringLiteral(
                        "高德未返回 polyline 路线点"));


                setMapPlaceholder(
                    QStringLiteral(
                        "无法绘制路线"),
                    QStringLiteral(
                        "路线数据缺少轨迹"));


                return;
            }


            // ================================================================
            // Make sure exact start/end are present
            // ================================================================
            const QString start =
                coordinateText(
                    m_currentRequest.fromLng,
                    m_currentRequest.fromLat);


            const QString target =
                coordinateText(
                    m_currentRequest.toLng,
                    m_currentRequest.toLat);


            if (points.first() !=
                start) {

                points.prepend(
                    start);
            }


            if (points.last() !=
                target) {

                points.append(
                    target);
            }


            points =
                simplifyPoints(
                    points,
                    48);


            if (points.isEmpty() ||
                points.first() !=
                    start) {

                points.prepend(
                    start);
            }


            if (points.last() !=
                target) {

                points.append(
                    target);
            }


            // ================================================================
            // Download static map
            // ================================================================
            requestStaticMap(
                points,
                distanceMeters,
                durationSeconds,
                requestId);
        });
}


// ============================================================================
// Download AMap Static Map
// ============================================================================
void NavigationPage::requestStaticMap(
    const QStringList &points,
    double routeDistanceMeters,
    qint64 routeDurationSeconds,
    quint64 requestId)
{
    if (requestId !=
        m_requestId) {

        return;
    }


    const QString key =
        webServiceKey();


    if (key.isEmpty()) {
        return;
    }


    const QString start =
        coordinateText(
            m_currentRequest.fromLng,
            m_currentRequest.fromLat);


    const QString target =
        coordinateText(
            m_currentRequest.toLng,
            m_currentRequest.toLat);


    // ========================================================================
    // Static Map URL
    //
    // Do not provide location/zoom.
    // AMap automatically calculates viewport from markers + paths.
    // ========================================================================
    QUrl url(
        QStringLiteral(
            "https://restapi.amap.com/v3/staticmap"));


    QUrlQuery query;


    query.addQueryItem(
        QStringLiteral(
            "key"),
        key);


    query.addQueryItem(
        QStringLiteral(
            "size"),
        QStringLiteral(
            "900*520"));


    query.addQueryItem(
        QStringLiteral(
            "scale"),
        QStringLiteral(
            "1"));


    query.addQueryItem(
        QStringLiteral(
            "markers"),
        QStringLiteral(
            "mid,0x315B4D,A:%1|"
            "mid,0xD79A4B,B:%2")
            .arg(
                start,
                target));


    query.addQueryItem(
        QStringLiteral(
            "paths"),
        QStringLiteral(
            "8,0x315B4D,1,,:%1")
            .arg(
                points.join(
                    QLatin1Char(';'))));


    query.addQueryItem(
        QStringLiteral(
            "traffic"),
        QStringLiteral(
            "0"));


    url.setQuery(
        query);


    // ========================================================================
    // Route summary
    // ========================================================================
    const QString modeText =
        m_currentRequest.mode ==
                QStringLiteral(
                    "walking")
            ? QStringLiteral(
                  "步行")
            : QStringLiteral(
                  "驾车");


    QString summary =
        modeText;


    if (routeDistanceMeters >
        0.0) {

        summary +=
            QStringLiteral(
                " · %1 km")
                .arg(
                    routeDistanceMeters /
                        1000.0,
                    0,
                    'f',
                    1);
    }


    if (routeDurationSeconds >
        0) {

        summary +=
            QStringLiteral(
                " · 预计 %1")
                .arg(
                    formatDuration(
                        routeDurationSeconds));
    }


    m_routeSummaryLabel->setText(
        summary);


    // ========================================================================
    // Loading map image
    // ========================================================================
    m_loadStatusLabel->setText(
        QStringLiteral(
            "路线规划完成，正在加载地图…"));


    m_loadProgress->setRange(
        0,
        0);

    m_loadProgress->show();


    setMapPlaceholder(
        QStringLiteral(
            "正在加载地图"),
        QStringLiteral(
            "路线已经规划完成"));


    QNetworkRequest request(
        url);


    m_mapReply =
        m_networkManager->get(
            request);


    QNetworkReply *reply =
        m_mapReply;


    // ========================================================================
    // Timeout
    // ========================================================================
    QTimer::singleShot(
        8000,
        reply,
        [reply]() {

            if (!reply->isRunning()) {
                return;
            }


            reply->setProperty(
                "mapTimedOut",
                true);


            reply->abort();
        });


    // ========================================================================
    // Finished
    // ========================================================================
    connect(
        reply,
        &QNetworkReply::finished,
        this,
        [this,
         reply,
         requestId]() {

            if (m_mapReply ==
                reply) {

                m_mapReply =
                    nullptr;
            }


            if (requestId !=
                m_requestId) {

                reply->deleteLater();

                return;
            }


            const bool timedOut =
                reply->property(
                         "mapTimedOut")
                    .toBool();


            // ================================================================
            // Network failure
            // ================================================================
            if (reply->error() !=
                QNetworkReply::NoError) {

                m_loadProgress->hide();


                m_loadStatusLabel->setText(
                    timedOut
                        ? QStringLiteral(
                              "地图加载超时")
                        : QStringLiteral(
                              "地图加载失败"));


                setMapPlaceholder(
                    QStringLiteral(
                        "地图加载失败"),
                    timedOut
                        ? QStringLiteral(
                              "请求超时，请稍后再试")
                        : QStringLiteral(
                              "请检查网络连接"));


                reply->deleteLater();

                return;
            }


            const QByteArray data =
                reply->readAll();


            reply->deleteLater();


            // ================================================================
            // Load image
            // ================================================================
            QPixmap pixmap;


            if (!pixmap.loadFromData(
                    data)) {

                m_loadProgress->hide();


                QString info =
                    QStringLiteral(
                        "高德未返回有效地图图片");


                QJsonParseError error;


                const QJsonDocument document =
                    QJsonDocument::fromJson(
                        data,
                        &error);


                if (error.error ==
                        QJsonParseError::NoError &&
                    document.isObject()) {

                    const QString apiInfo =
                        document.object()
                            .value(
                                QStringLiteral(
                                    "info"))
                            .toString();


                    if (!apiInfo.isEmpty()) {

                        info =
                            apiInfo;
                    }
                }


                m_loadStatusLabel->setText(
                    QStringLiteral(
                        "地图加载失败"));


                setMapPlaceholder(
                    QStringLiteral(
                        "地图不可用"),
                    info);


                return;
            }


            // ================================================================
            // Success
            // ================================================================
            m_originalMapPixmap =
                pixmap;


            m_loadProgress->hide();


            m_loadStatusLabel->setText(
                QStringLiteral(
                    "路线规划完成"));


            m_mapLabel->setText(
                QString());


            rescaleMapPixmap();
        });
}


// ============================================================================
// Placeholder
// ============================================================================
void NavigationPage::setMapPlaceholder(
    const QString &title,
    const QString &message)
{
    if (!m_mapLabel) {
        return;
    }


    m_originalMapPixmap =
        QPixmap();


    m_mapLabel->setPixmap(
        QPixmap());


    m_mapLabel->setText(
        QStringLiteral(
            "<div style=\"text-align:center;\">"
            "<div style=\""
            "font-size:18px;"
            "font-weight:700;"
            "color:#315B4D;"
            "margin-bottom:8px;\">"
            "%1"
            "</div>"
            "<div style=\""
            "font-size:13px;"
            "color:#7A837E;\">"
            "%2"
            "</div>"
            "</div>")
            .arg(
                title.toHtmlEscaped(),
                message.toHtmlEscaped()));
}


// ============================================================================
// Scale map image without distortion
// ============================================================================
void NavigationPage::rescaleMapPixmap()
{
    if (!m_mapLabel ||
        m_originalMapPixmap.isNull()) {

        return;
    }


    QSize targetSize =
        m_mapLabel->size();


    if (targetSize.width() <= 0 ||
        targetSize.height() <= 0) {

        return;
    }


    m_mapLabel->setPixmap(
        m_originalMapPixmap.scaled(
            targetSize,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation));
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


    QTimer::singleShot(
        0,
        this,
        [this]() {

            rescaleMapPixmap();
        });
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
            "QLabel#navigationMapImage{"
            "background:#FAF8F3;"
            "color:#7A837E;"
            "border:1px solid #E7E3DA;"
            "border-radius:%1px;"
            "padding:4px;"
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


    if (m_mapLabel) {

        m_mapLabel->setMinimumHeight(
            scaledUi(
                scaleBase,
                360));
    }
}
