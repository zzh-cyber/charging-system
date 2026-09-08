#include "navigationpage.h"

#include "uitheme.h"
#include "windowhelper.h"

#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
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
#include <QSize>
#include <QSizePolicy>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>


namespace
{

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
// ============================================================================
constexpr double kMaxWalkingDistanceKm =
    100.0;


// ============================================================================
// Haversine 直线距离
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
// 高德坐标：经度,纬度
// ============================================================================
QString coordinateText(
    double lng,
    double lat)
{
    return
        QStringLiteral(
            "%1,%2")
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
// ============================================================================
double jsonDouble(
    const QJsonValue &value,
    double fallback = 0.0)
{
    if (value.isDouble()) {

        return
            value.toDouble();
    }


    if (value.isString()) {

        bool ok =
            false;


        const double result =
            value.toString()
                .toDouble(
                    &ok);


        if (ok) {

            return
                result;
        }
    }


    return
        fallback;
}


qint64 jsonInt64(
    const QJsonValue &value,
    qint64 fallback = 0)
{
    if (value.isDouble()) {

        return
            static_cast<qint64>(
                value.toDouble());
    }


    if (value.isString()) {

        bool ok =
            false;


        const qint64 result =
            value.toString()
                .toLongLong(
                    &ok);


        if (ok) {

            return
                result;
        }
    }


    return
        fallback;
}


// ============================================================================
// Array / Object -> Array
// ============================================================================
QJsonArray jsonArrayValue(
    const QJsonValue &value)
{
    if (value.isArray()) {

        return
            value.toArray();
    }


    QJsonArray result;


    if (value.isObject()) {

        result.append(
            value.toObject());
    }


    return
        result;
}


// ============================================================================
// polyline 提取坐标
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


        bool lngOk =
            false;

        bool latOk =
            false;


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


        if (point ==
            lastPoint) {

            continue;
        }


        result.append(
            point);


        lastPoint =
            point;
    }


    return
        result;
}


// ============================================================================
// 路线点
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


    return
        result;
}


// ============================================================================
// Static Map 点数量保护
// ============================================================================
QStringList simplifyPoints(
    const QStringList &points,
    int maxPoints = 48)
{
    if (points.size() <=
        maxPoints) {

        return
            points;
    }


    QStringList result;

    result.reserve(
        maxPoints);


    const int lastIndex =
        points.size() -
        1;


    for (int i = 0;
         i < maxPoints;
         ++i) {

        const double ratio =
            static_cast<double>(i) /
            static_cast<double>(
                maxPoints -
                1);


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


    return
        result;
}


// ============================================================================
// 时长格式
// ============================================================================
QString formatDuration(
    qint64 seconds)
{
    if (seconds <= 0) {

        return
            QStringLiteral(
                "--");
    }


    const qint64 hours =
        seconds /
        3600;


    qint64 minutes =
        (seconds %
         3600) /
        60;


    if (hours == 0 &&
        minutes == 0) {

        minutes =
            1;
    }


    if (hours > 0) {

        return
            QStringLiteral(
                "%1小时%2分钟")
                .arg(
                    hours)
                .arg(
                    minutes);
    }


    return
        QStringLiteral(
            "%1分钟")
            .arg(
                minutes);
}


// ============================================================================
// Step duration
// ============================================================================
qint64 stepDuration(
    const QJsonObject &path)
{
    const QJsonArray steps =
        jsonArrayValue(
            path.value(
                QStringLiteral(
                    "steps")));


    qint64 total =
        0;


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


    return
        total;
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


    // =========================================================================
    // Root
    // =========================================================================
    auto *rootLayout =
        new QVBoxLayout(
            this);

    rootLayout->setContentsMargins(
        0,
        0,
        0,
        0);

    rootLayout->setSpacing(
        0);


    // =========================================================================
    // Scroll
    // =========================================================================
    auto *scrollArea =
        new QScrollArea(
            this);

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
        12);


    // =========================================================================
    // 1. Header
    // =========================================================================
    auto *headerLayout =
        new QHBoxLayout;

    headerLayout->setObjectName(
        QStringLiteral(
            "navigationHeaderLayout"));

    headerLayout->setSpacing(
        10);


    auto *backButton =
        new QPushButton(
            QStringLiteral(
                "返回"),
            content);

    backButton->setObjectName(
        QStringLiteral(
            "navigationBackButton"));

    backButton->setCursor(
        Qt::PointingHandCursor);

    backButton->setIcon(
        QIcon(
            QStringLiteral(
                ":/icons/back.svg")));


    auto *headerText =
        new QVBoxLayout;

    headerText->setSpacing(
        2);


    auto *pageTitle =
        new QLabel(
            QStringLiteral(
                "路线规划"),
            content);

    pageTitle->setObjectName(
        QStringLiteral(
            "navigationTitle"));


    auto *pageSubtitle =
        new QLabel(
            QStringLiteral(
                "查看路线概览、距离与预计耗时"),
            content);

    pageSubtitle->setObjectName(
        QStringLiteral(
            "navigationSubtitle"));

    pageSubtitle->setWordWrap(
        true);


    headerText->addWidget(
        pageTitle);

    headerText->addWidget(
        pageSubtitle);


    headerLayout->addWidget(
        backButton,
        0,
        Qt::AlignVCenter);

    headerLayout->addLayout(
        headerText,
        1);


    connect(
        backButton,
        &QPushButton::clicked,
        this,
        &NavigationPage::back);


    mainLayout->addLayout(
        headerLayout);


    // =========================================================================
    // 2. Destination Summary
    //
    // 原来的“导航至 + 站点卡”保留，
    // 但压缩成地图前的目的地摘要，而不是占很大面积。
    // =========================================================================
    auto *destinationCard =
        new QFrame(
            content);

    destinationCard->setObjectName(
        QStringLiteral(
            "navigationDestinationCard"));

    destinationCard->setAttribute(
        Qt::WA_StyledBackground,
        true);


    UiTheme::applyCardShadow(
        destinationCard,
        20,
        4);


    auto *destinationLayout =
        new QHBoxLayout(
            destinationCard);

    destinationLayout->setObjectName(
        QStringLiteral(
            "navigationDestinationLayout"));

    destinationLayout->setContentsMargins(
        15,
        13,
        15,
        13);

    destinationLayout->setSpacing(
        11);


    auto *destinationIcon =
        new QLabel(
            destinationCard);

    destinationIcon->setObjectName(
        QStringLiteral(
            "navigationDestinationIcon"));

    destinationIcon->setAlignment(
        Qt::AlignCenter);


    auto *destinationText =
        new QVBoxLayout;

    destinationText->setSpacing(
        3);


    auto *stationCaption =
        new QLabel(
            QStringLiteral(
                "导航至"),
            destinationCard);

    stationCaption->setObjectName(
        QStringLiteral(
            "navigationCaption"));


    m_stationLabel =
        new QLabel(
            QStringLiteral(
                "--"),
            destinationCard);

    m_stationLabel->setObjectName(
        QStringLiteral(
            "navigationStationName"));

    m_stationLabel->setWordWrap(
        true);


    destinationText->addWidget(
        stationCaption);

    destinationText->addWidget(
        m_stationLabel);


    m_routeModeLabel =
        new QLabel(
            QStringLiteral(
                "驾车路线"),
            destinationCard);

    m_routeModeLabel->setObjectName(
        QStringLiteral(
            "navigationBadge"));

    m_routeModeLabel->setAlignment(
        Qt::AlignCenter);


    destinationLayout->addWidget(
        destinationIcon);

    destinationLayout->addLayout(
        destinationText,
        1);

    destinationLayout->addWidget(
        m_routeModeLabel,
        0,
        Qt::AlignTop);


    mainLayout->addWidget(
        destinationCard);


    // =========================================================================
    // 3. LARGE MAP
    //
    // 这次地图成为真正的页面主体。
    // =========================================================================
    auto *mapCard =
        new QFrame(
            content);

    mapCard->setObjectName(
        QStringLiteral(
            "navigationMapCard"));

    mapCard->setAttribute(
        Qt::WA_StyledBackground,
        true);


    UiTheme::applyHeroShadow(
        mapCard,
        30,
        7);


    auto *mapLayout =
        new QVBoxLayout(
            mapCard);

    mapLayout->setObjectName(
        QStringLiteral(
            "navigationMapLayout"));

    mapLayout->setContentsMargins(
        10,
        10,
        10,
        10);

    mapLayout->setSpacing(
        8);


    // -------------------------------------------------------------------------
    // Map Header
    // -------------------------------------------------------------------------
    auto *mapHeader =
        new QHBoxLayout;

    mapHeader->setObjectName(
        QStringLiteral(
            "navigationMapHeader"));

    mapHeader->setSpacing(
        8);


    auto *mapTitleBlock =
        new QVBoxLayout;

    mapTitleBlock->setSpacing(
        1);


    auto *mapTitle =
        new QLabel(
            QStringLiteral(
                "地图路线"),
            mapCard);

    mapTitle->setObjectName(
        QStringLiteral(
            "navigationMapTitle"));


    auto *mapCaption =
        new QLabel(
            QStringLiteral(
                "静态路线预览"),
            mapCard);

    mapCaption->setObjectName(
        QStringLiteral(
            "navigationMapCaption"));


    mapTitleBlock->addWidget(
        mapTitle);

    mapTitleBlock->addWidget(
        mapCaption);


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


    mapHeader->addLayout(
        mapTitleBlock);

    mapHeader->addStretch();

    mapHeader->addWidget(
        m_loadStatusLabel);


    mapLayout->addLayout(
        mapHeader);


    // -------------------------------------------------------------------------
    // Map Image
    // -------------------------------------------------------------------------
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
        430);

    m_mapLabel->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Expanding);


    mapLayout->addWidget(
        m_mapLabel,
        1);


    // -------------------------------------------------------------------------
    // Progress
    // -------------------------------------------------------------------------
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


    // -------------------------------------------------------------------------
    // Route Summary
    //
    // 原来的路线说明保留，但变成地图下方的“信息胶囊”。
    // -------------------------------------------------------------------------
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


    mainLayout->addWidget(
        mapCard,
        1);


    // =========================================================================
    // 4. Route Bottom Sheet
    //
    // 原来的：
    // 路线信息 / 当前地址 / 目的地 / 距离 / 出行方式
    //
    // 全部保留，但统一塞进地图下面的一张 Bottom Sheet。
    // =========================================================================
    auto *sheet =
        new QFrame(
            content);

    sheet->setObjectName(
        QStringLiteral(
            "navigationRouteSheet"));

    sheet->setAttribute(
        Qt::WA_StyledBackground,
        true);


    UiTheme::applyCardShadow(
        sheet,
        22,
        4);


    auto *sheetLayout =
        new QVBoxLayout(
            sheet);

    sheetLayout->setObjectName(
        QStringLiteral(
            "navigationSheetLayout"));

    sheetLayout->setContentsMargins(
        16,
        15,
        16,
        16);

    sheetLayout->setSpacing(
        11);


    // -------------------------------------------------------------------------
    // Sheet header
    // -------------------------------------------------------------------------
    auto *sheetHeader =
        new QHBoxLayout;

    sheetHeader->setSpacing(
        8);


    auto *routeTitle =
        new QLabel(
            QStringLiteral(
                "路线信息"),
            sheet);

    routeTitle->setObjectName(
        QStringLiteral(
            "navigationSectionTitle"));


    auto *routeDescription =
        new QLabel(
            QStringLiteral(
                "起终点与出行方式"),
            sheet);

    routeDescription->setObjectName(
        QStringLiteral(
            "navigationSheetDescription"));

    routeDescription->setAlignment(
        Qt::AlignRight |
        Qt::AlignVCenter);


    sheetHeader->addWidget(
        routeTitle);

    sheetHeader->addStretch();

    sheetHeader->addWidget(
        routeDescription);


    sheetLayout->addLayout(
        sheetHeader);


    // =========================================================================
    // Start
    // =========================================================================
    auto *startRow =
        new QFrame(
            sheet);

    startRow->setObjectName(
        QStringLiteral(
            "navigationPointRow"));

    startRow->setAttribute(
        Qt::WA_StyledBackground,
        true);


    auto *startLayout =
        new QHBoxLayout(
            startRow);

    startLayout->setContentsMargins(
        12,
        10,
        12,
        10);

    startLayout->setSpacing(
        10);


    auto *startIcon =
        new QLabel(
            startRow);

    startIcon->setObjectName(
        QStringLiteral(
            "navigationStartIcon"));

    startIcon->setAlignment(
        Qt::AlignCenter);


    auto *startText =
        new QVBoxLayout;

    startText->setSpacing(
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
            QStringLiteral(
                "--"),
            startRow);

    m_startLabel->setObjectName(
        QStringLiteral(
            "navigationCoordinate"));

    m_startLabel->setWordWrap(
        true);


    startText->addWidget(
        startCaption);

    startText->addWidget(
        m_startLabel);


    startLayout->addWidget(
        startIcon);

    startLayout->addLayout(
        startText,
        1);


    sheetLayout->addWidget(
        startRow);


    // -------------------------------------------------------------------------
    // Route connector
    // -------------------------------------------------------------------------
    auto *connector =
        new QFrame(
            sheet);

    connector->setObjectName(
        QStringLiteral(
            "navigationConnector"));


    sheetLayout->addWidget(
        connector,
        0,
        Qt::AlignLeft);


    // =========================================================================
    // Target
    // =========================================================================
    auto *targetRow =
        new QFrame(
            sheet);

    targetRow->setObjectName(
        QStringLiteral(
            "navigationPointRow"));

    targetRow->setAttribute(
        Qt::WA_StyledBackground,
        true);


    auto *targetLayout =
        new QHBoxLayout(
            targetRow);

    targetLayout->setContentsMargins(
        12,
        10,
        12,
        10);

    targetLayout->setSpacing(
        10);


    auto *targetIcon =
        new QLabel(
            targetRow);

    targetIcon->setObjectName(
        QStringLiteral(
            "navigationTargetIcon"));

    targetIcon->setAlignment(
        Qt::AlignCenter);


    auto *targetText =
        new QVBoxLayout;

    targetText->setSpacing(
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
            QStringLiteral(
                "--"),
            targetRow);

    m_targetLabel->setObjectName(
        QStringLiteral(
            "navigationCoordinate"));

    m_targetLabel->setWordWrap(
        true);


    targetText->addWidget(
        targetCaption);

    targetText->addWidget(
        m_targetLabel);


    targetLayout->addWidget(
        targetIcon);

    targetLayout->addLayout(
        targetText,
        1);


    sheetLayout->addWidget(
        targetRow);


    // =========================================================================
    // Distance
    // =========================================================================
    auto *distanceRow =
        new QFrame(
            sheet);

    distanceRow->setObjectName(
        QStringLiteral(
            "navigationDistanceRow"));

    distanceRow->setAttribute(
        Qt::WA_StyledBackground,
        true);


    auto *distanceLayout =
        new QHBoxLayout(
            distanceRow);

    distanceLayout->setContentsMargins(
        12,
        9,
        12,
        9);

    distanceLayout->setSpacing(
        8);


    auto *distanceIcon =
        new QLabel(
            distanceRow);

    distanceIcon->setObjectName(
        QStringLiteral(
            "navigationDistanceIcon"));

    distanceIcon->setAlignment(
        Qt::AlignCenter);


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
        distanceIcon);

    distanceLayout->addWidget(
        distanceCaption);

    distanceLayout->addStretch();

    distanceLayout->addWidget(
        m_distanceLabel);


    sheetLayout->addWidget(
        distanceRow);


    // =========================================================================
    // 出行方式
    // =========================================================================
    auto *modeDivider =
        new QFrame(
            sheet);

    modeDivider->setObjectName(
        QStringLiteral(
            "navigationSheetDivider"));

    modeDivider->setFrameShape(
        QFrame::HLine);


    sheetLayout->addWidget(
        modeDivider);


    auto *modeLayout =
        new QHBoxLayout;

    modeLayout->setObjectName(
        QStringLiteral(
            "navigationModeLayout"));

    modeLayout->setSpacing(
        8);


    auto *modeCaption =
        new QLabel(
            QStringLiteral(
                "出行方式"),
            sheet);

    modeCaption->setObjectName(
        QStringLiteral(
            "navigationModeTitle"));


    m_driveButton =
        new QPushButton(
            QStringLiteral(
                "驾车"),
            sheet);

    m_driveButton->setObjectName(
        QStringLiteral(
            "navigationModeButton"));

    m_driveButton->setCheckable(
        true);

    m_driveButton->setCursor(
        Qt::PointingHandCursor);

    m_driveButton->setIcon(
        QIcon(
            QStringLiteral(
                ":/icons/car.svg")));


    m_walkButton =
        new QPushButton(
            QStringLiteral(
                "步行"),
            sheet);

    m_walkButton->setObjectName(
        QStringLiteral(
            "navigationModeButton"));

    m_walkButton->setCheckable(
        true);

    m_walkButton->setCursor(
        Qt::PointingHandCursor);

    m_walkButton->setIcon(
        QIcon(
            QStringLiteral(
                ":/icons/walk.svg")));


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


    sheetLayout->addLayout(
        modeLayout);


    mainLayout->addWidget(
        sheet);


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


    // =========================================================================
    // Station
    // =========================================================================
    m_stationLabel->setText(
        request.toName
                .trimmed()
                .isEmpty()
            ? QStringLiteral(
                  "--")
            : request.toName);


    // =========================================================================
    // Start
    // =========================================================================
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


    // =========================================================================
    // Target
    // =========================================================================
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


    // =========================================================================
    // Straight distance
    // =========================================================================
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
// NO.11 Route mode
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
// Web Service Key
// ============================================================================
QString NavigationPage::webServiceKey() const
{
    return
        qEnvironmentVariable(
            "AMAP_WEB_SERVICE_KEY")
            .trimmed();
}


// ============================================================================
// Route Planning
// ============================================================================
void NavigationPage::loadRoute()
{
    if (!m_hasRouteRequest) {

        return;
    }


    // =========================================================================
    // 坐标校验
    // =========================================================================
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


    const bool walking =
        m_currentRequest.mode ==
        QStringLiteral(
            "walking");


    // =========================================================================
    // NO.11 Walking distance guard
    // =========================================================================
    if (walking) {

        const double straightKm =
            straightLineDistanceKm(
                m_currentRequest.fromLat,
                m_currentRequest.fromLng,
                m_currentRequest.toLat,
                m_currentRequest.toLng);


        if (straightKm >
            kMaxWalkingDistanceKm) {

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


    // =========================================================================
    // Key
    // =========================================================================
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


    // =========================================================================
    // New request
    // =========================================================================
    const quint64 requestId =
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


    setMapPlaceholder(
        QStringLiteral(
            "正在规划路线"),
        QStringLiteral(
            "正在连接高德地图服务…"));


    // =========================================================================
    // AMap V5
    // =========================================================================
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


    if (!walking) {

        query.addQueryItem(
            QStringLiteral(
                "strategy"),
            QStringLiteral(
                "32"));
    }


    url.setQuery(
        query);


    // =========================================================================
    // Loading UI
    // =========================================================================
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


    QNetworkRequest request(
        url);


    m_routeReply =
        m_networkManager->get(
            request);


    QNetworkReply *reply =
        m_routeReply;


    // =========================================================================
    // Timeout
    // =========================================================================
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


    // =========================================================================
    // Finished
    // =========================================================================
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


            if (requestId !=
                m_requestId) {

                reply->deleteLater();

                return;
            }


            const bool timedOut =
                reply->property(
                         "routeTimedOut")
                    .toBool();


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
                QStringLiteral(
                    "1")) {

                QString info =
                    root.value(
                            QStringLiteral(
                                "info"))
                        .toString();


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


            const QJsonObject path =
                paths.at(
                         0)
                    .toObject();


            const double distanceMeters =
                jsonDouble(
                    path.value(
                        QStringLiteral(
                            "distance")));


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


            requestStaticMap(
                points,
                distanceMeters,
                durationSeconds,
                requestId);
        });
}


// ============================================================================
// Static Map
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
                    QLatin1Char(
                        ';'))));


    query.addQueryItem(
        QStringLiteral(
            "traffic"),
        QStringLiteral(
            "0"));


    url.setQuery(
        query);


    // =========================================================================
    // Summary
    // =========================================================================
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
            "color:#151C24;"
            "margin-bottom:8px;\">"
            "%1"
            "</div>"
            "<div style=\""
            "font-size:13px;"
            "color:#7E8893;\">"
            "%2"
            "</div>"
            "</div>")
            .arg(
                title.toHtmlEscaped(),
                message.toHtmlEscaped()));
}


// ============================================================================
// Scale Map
// ============================================================================
void NavigationPage::rescaleMapPixmap()
{
    if (!m_mapLabel ||
        m_originalMapPixmap.isNull()) {

        return;
    }


    const QSize targetSize =
        m_mapLabel->size();


    if (targetSize.width() <=
            0 ||
        targetSize.height() <=
            0) {

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
// Responsive Style
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
            18);


    const int normalFont =
        scaledUi(
            scaleBase,
            13);


    const int smallFont =
        scaledUi(
            scaleBase,
            11);


    const int cardRadius =
        scaledUi(
            scaleBase,
            20);


    const int mapRadius =
        scaledUi(
            scaleBase,
            24);


    const int smallRadius =
        scaledUi(
            scaleBase,
            11);


    const int iconBox =
        scaledUi(
            scaleBase,
            34);


    const int iconSize =
        scaledUi(
            scaleBase,
            17);


    // =========================================================================
    // Page
    // =========================================================================
    QString pageStyle =
        QStringLiteral(

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

            "QScrollArea#navigationScrollArea > QWidget > QWidget{"
            "background:transparent;"
            "}"

            "QPushButton#navigationBackButton{"
            "background:%2;"
            "color:%1;"
            "border:1px solid %3;"
            "border-radius:%4px;"
            "padding:7px 11px;"
            "font-size:%5px;"
            "font-weight:700;"
            "}"

            "QPushButton#navigationBackButton:hover{"
            "background:#EAEFEC;"
            "}"

            "QLabel#navigationTitle{"
            "background:transparent;"
            "color:%1;"
            "font-size:%6px;"
            "font-weight:850;"
            "}"

            "QLabel#navigationSubtitle{"
            "background:transparent;"
            "color:%7;"
            "font-size:%8px;"
            "}");

    pageStyle =
        pageStyle
            .arg(
                UiTheme::textPrimary())
            .arg(
                UiTheme::surfaceSoft())
            .arg(
                UiTheme::border())
            .arg(
                smallRadius)
            .arg(
                normalFont)
            .arg(
                titleFont)
            .arg(
                UiTheme::textSecondary())
            .arg(
                smallFont);


    // =========================================================================
    // Destination
    // =========================================================================
    QString destinationStyle =
        QStringLiteral(

            "QFrame#navigationDestinationCard{"
            "background:%1;"
            "border:none;"
            "border-radius:%2px;"
            "}"

            "QLabel#navigationDestinationIcon{"
            "background:%3;"
            "border:none;"
            "border-radius:%4px;"
            "}"

            "QLabel#navigationCaption{"
            "background:transparent;"
            "color:%5;"
            "font-size:%6px;"
            "}"

            "QLabel#navigationStationName{"
            "background:transparent;"
            "color:#FFFFFF;"
            "font-size:%7px;"
            "font-weight:850;"
            "}"

            "QLabel#navigationBadge{"
            "background:%8;"
            "color:%1;"
            "border:none;"
            "border-radius:%9px;"
            "padding:5px 9px;"
            "font-size:%6px;"
            "font-weight:800;"
            "}");

    destinationStyle =
        destinationStyle
            .arg(
                UiTheme::dark())
            .arg(
                cardRadius)
            .arg(
                UiTheme::darkSoft())
            .arg(
                iconBox / 2)
            .arg(
                QStringLiteral(
                    "#9CA6B1"))
            .arg(
                smallFont)
            .arg(
                stationFont)
            .arg(
                UiTheme::lime())
            .arg(
                smallRadius);


    // =========================================================================
    // Map
    // =========================================================================
    QString mapStyle =
        QStringLiteral(

            "QFrame#navigationMapCard{"
            "background:#FFFFFF;"
            "border:1px solid %1;"
            "border-radius:%2px;"
            "}"

            "QLabel#navigationMapTitle{"
            "background:transparent;"
            "color:%3;"
            "font-size:%4px;"
            "font-weight:800;"
            "}"

            "QLabel#navigationMapCaption{"
            "background:transparent;"
            "color:%5;"
            "font-size:%6px;"
            "}"

            "QLabel#navigationLoadStatus{"
            "background:transparent;"
            "color:%5;"
            "font-size:%6px;"
            "}"

            "QLabel#navigationMapImage{"
            "background:#F2F5F3;"
            "color:%5;"
            "border:none;"
            "border-radius:%7px;"
            "padding:2px;"
            "}"

            "QLabel#navigationRouteSummary{"
            "background:%8;"
            "color:%3;"
            "border:none;"
            "border-radius:%9px;"
            "padding:9px 11px;"
            "font-size:%4px;"
            "font-weight:750;"
            "}");

    mapStyle =
        mapStyle
            .arg(
                UiTheme::border())
            .arg(
                mapRadius)
            .arg(
                UiTheme::textPrimary())
            .arg(
                normalFont)
            .arg(
                UiTheme::textSecondary())
            .arg(
                smallFont)
            .arg(
                cardRadius)
            .arg(
                UiTheme::limeSoft())
            .arg(
                smallRadius);


    QString progressStyle =
        QStringLiteral(

            "QProgressBar#navigationLoadProgress{"
            "background:#E2E7E4;"
            "border:none;"
            "border-radius:4px;"
            "min-height:8px;"
            "max-height:8px;"
            "}"

            "QProgressBar#navigationLoadProgress::chunk{"
            "background:%1;"
            "border-radius:4px;"
            "}");

    progressStyle =
        progressStyle.arg(
            UiTheme::limeStrong());


    // =========================================================================
    // Sheet
    // =========================================================================
    QString sheetStyle =
        QStringLiteral(

            "QFrame#navigationRouteSheet{"
            "background:#FFFFFF;"
            "border:1px solid %1;"
            "border-radius:%2px;"
            "}"

            "QLabel#navigationSectionTitle{"
            "background:transparent;"
            "color:%3;"
            "font-size:%4px;"
            "font-weight:800;"
            "}"

            "QLabel#navigationSheetDescription{"
            "background:transparent;"
            "color:%5;"
            "font-size:%6px;"
            "}"

            "QFrame#navigationPointRow{"
            "background:%7;"
            "border:none;"
            "border-radius:%8px;"
            "}"

            "QLabel#navigationCoordinate{"
            "background:transparent;"
            "color:%3;"
            "font-size:%4px;"
            "font-weight:650;"
            "}");

    sheetStyle =
        sheetStyle
            .arg(
                UiTheme::border())
            .arg(
                cardRadius)
            .arg(
                UiTheme::textPrimary())
            .arg(
                normalFont)
            .arg(
                UiTheme::textSecondary())
            .arg(
                smallFont)
            .arg(
                UiTheme::surfaceSoft())
            .arg(
                smallRadius);


    QString routeInfoStyle =
        QStringLiteral(

            "QLabel#navigationStartIcon{"
            "background:%1;"
            "border:none;"
            "border-radius:%2px;"
            "}"

            "QLabel#navigationTargetIcon{"
            "background:%3;"
            "border:none;"
            "border-radius:%2px;"
            "}"

            "QFrame#navigationConnector{"
            "background:%4;"
            "border:none;"
            "min-width:2px;"
            "max-width:2px;"
            "min-height:12px;"
            "max-height:12px;"
            "margin-left:%5px;"
            "}"

            "QFrame#navigationDistanceRow{"
            "background:%1;"
            "border:none;"
            "border-radius:%6px;"
            "}"

            "QLabel#navigationDistanceIcon{"
            "background:transparent;"
            "border:none;"
            "}"

            "QLabel#navigationDistance{"
            "background:transparent;"
            "color:%7;"
            "font-size:%8px;"
            "font-weight:800;"
            "}");

    routeInfoStyle =
        routeInfoStyle
            .arg(
                UiTheme::limeSoft())
            .arg(
                iconBox / 2)
            .arg(
                UiTheme::dark())
            .arg(
                UiTheme::borderStrong())
            .arg(
                iconBox / 2)
            .arg(
                smallRadius)
            .arg(
                UiTheme::textPrimary())
            .arg(
                normalFont);


    // =========================================================================
    // Mode
    // =========================================================================
    QString modeStyle =
        QStringLiteral(

            "QFrame#navigationSheetDivider{"
            "background:%1;"
            "border:none;"
            "max-height:1px;"
            "}"

            "QLabel#navigationModeTitle{"
            "background:transparent;"
            "color:%2;"
            "font-size:%3px;"
            "font-weight:750;"
            "}"

            "QPushButton#navigationModeButton{"
            "background:%4;"
            "color:%5;"
            "border:1px solid %1;"
            "border-radius:%6px;"
            "padding:8px 12px;"
            "font-size:%3px;"
            "font-weight:750;"
            "}"

            "QPushButton#navigationModeButton:hover{"
            "background:#EDF1EF;"
            "}"

            "QPushButton#navigationModeButton:checked{"
            "background:%7;"
            "color:%2;"
            "border-color:%7;"
            "}");

    modeStyle =
        modeStyle
            .arg(
                UiTheme::border())
            .arg(
                UiTheme::textPrimary())
            .arg(
                normalFont)
            .arg(
                UiTheme::surfaceSoft())
            .arg(
                UiTheme::textSecondary())
            .arg(
                smallRadius)
            .arg(
                UiTheme::lime());


    setStyleSheet(
        pageStyle +
        destinationStyle +
        mapStyle +
        progressStyle +
        sheetStyle +
        routeInfoStyle +
        modeStyle);


    // =========================================================================
    // Layouts
    // =========================================================================
    if (auto *mainLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "navigationMainLayout"))) {

        mainLayout->setContentsMargins(
            scaledUi(
                scaleBase,
                18),
            scaledUi(
                scaleBase,
                18),
            scaledUi(
                scaleBase,
                18),
            scaledUi(
                scaleBase,
                18));


        mainLayout->setSpacing(
            scaledUi(
                scaleBase,
                12));
    }


    if (auto *destinationLayout =
            findChild<QHBoxLayout *>(
                QStringLiteral(
                    "navigationDestinationLayout"))) {

        destinationLayout->setContentsMargins(
            scaledUi(
                scaleBase,
                15),
            scaledUi(
                scaleBase,
                13),
            scaledUi(
                scaleBase,
                15),
            scaledUi(
                scaleBase,
                13));


        destinationLayout->setSpacing(
            scaledUi(
                scaleBase,
                11));
    }


    if (auto *mapLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "navigationMapLayout"))) {

        mapLayout->setContentsMargins(
            scaledUi(
                scaleBase,
                10),
            scaledUi(
                scaleBase,
                10),
            scaledUi(
                scaleBase,
                10),
            scaledUi(
                scaleBase,
                10));


        mapLayout->setSpacing(
            scaledUi(
                scaleBase,
                8));
    }


    if (auto *sheetLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "navigationSheetLayout"))) {

        sheetLayout->setContentsMargins(
            scaledUi(
                scaleBase,
                16),
            scaledUi(
                scaleBase,
                15),
            scaledUi(
                scaleBase,
                16),
            scaledUi(
                scaleBase,
                16));


        sheetLayout->setSpacing(
            scaledUi(
                scaleBase,
                11));
    }


    // =========================================================================
    // Icons
    // =========================================================================
    if (auto *backButton =
            findChild<QPushButton *>(
                QStringLiteral(
                    "navigationBackButton"))) {

        backButton->setIconSize(
            QSize(
                iconSize,
                iconSize));
    }


    if (auto *destinationIcon =
            findChild<QLabel *>(
                QStringLiteral(
                    "navigationDestinationIcon"))) {

        destinationIcon->setFixedSize(
            iconBox,
            iconBox);


        destinationIcon->setPixmap(
            QIcon(
                QStringLiteral(
                    ":/icons/navigation-white.svg"))
                .pixmap(
                    QSize(
                        iconSize,
                        iconSize)));
    }


    if (auto *startIcon =
            findChild<QLabel *>(
                QStringLiteral(
                    "navigationStartIcon"))) {

        startIcon->setFixedSize(
            iconBox,
            iconBox);


        startIcon->setPixmap(
            QIcon(
                QStringLiteral(
                    ":/icons/location.svg"))
                .pixmap(
                    QSize(
                        iconSize,
                        iconSize)));
    }


    if (auto *targetIcon =
            findChild<QLabel *>(
                QStringLiteral(
                    "navigationTargetIcon"))) {

        targetIcon->setFixedSize(
            iconBox,
            iconBox);


        targetIcon->setPixmap(
            QIcon(
                QStringLiteral(
                    ":/icons/navigation-white.svg"))
                .pixmap(
                    QSize(
                        iconSize,
                        iconSize)));
    }


    if (auto *distanceIcon =
            findChild<QLabel *>(
                QStringLiteral(
                    "navigationDistanceIcon"))) {

        distanceIcon->setFixedSize(
            iconSize,
            iconSize);


        distanceIcon->setPixmap(
            QIcon(
                QStringLiteral(
                    ":/icons/location.svg"))
                .pixmap(
                    QSize(
                        iconSize,
                        iconSize)));
    }


    if (m_driveButton) {

        m_driveButton->setIconSize(
            QSize(
                iconSize,
                iconSize));
    }


    if (m_walkButton) {

        m_walkButton->setIconSize(
            QSize(
                iconSize,
                iconSize));
    }


    // =========================================================================
    // 地图真正作为主体
    // =========================================================================
    if (m_mapLabel) {

        m_mapLabel->setMinimumHeight(
            scaledUi(
                scaleBase,
                430));
    }
}
