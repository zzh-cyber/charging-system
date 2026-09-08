#include "amapwidget.h"

#include "amapconfig.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebEngineSettings>
#include <QWebEngineView>

#include <cmath>

namespace
{

QString jsonStringLiteral(
    const QString &value)
{
    QJsonArray values;
    values.append(value);

    QByteArray json =
        QJsonDocument(values)
            .toJson(QJsonDocument::Compact);

    json.remove(0, 1);
    json.chop(1);

    return QString::fromUtf8(json);
}

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

} // namespace

AmapWidget::AmapWidget(
    QWidget *parent)
    : QWidget(parent)
    , m_view(new QWebEngineView(this))
{
    m_view->settings()->setAttribute(
        QWebEngineSettings::LocalContentCanAccessRemoteUrls,
        true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_view);

    connect(
        m_view,
        &QWebEngineView::titleChanged,
        this,
        [this](const QString &title) {
            if (title == QStringLiteral("map-ready")) {
                m_mapReady = true;
                applyPendingCenter();
                applyPendingUserLocation();
                applyPendingStations();
                applyPendingRoute();
                return;
            }

            if (title == QStringLiteral("map-unavailable")) {
                m_mapReady = false;
            }
        });

    loadMapHtml();
}

void AmapWidget::setCenter(
    double lat,
    double lng)
{
    if (!validCoordinate(lat, lng)) {
        return;
    }

    m_pendingLat = lat;
    m_pendingLng = lng;
    m_hasPendingCenter = true;

    applyPendingCenter();
}

void AmapWidget::setUserLocation(
    double lat,
    double lng)
{
    if (!validCoordinate(lat, lng)) {
        return;
    }

    m_pendingUserLat = lat;
    m_pendingUserLng = lng;
    m_hasPendingUserLocation = true;

    applyPendingUserLocation();
}

void AmapWidget::setStations(
    const QJsonArray &stations)
{
    m_pendingStations = stations;
    m_hasPendingStations = true;
    applyPendingStations();
}

void AmapWidget::setRoute(
    const QStringList &points)
{
    m_pendingRoutePoints = points;
    m_hasPendingRoute = true;
    applyPendingRoute();
}

void AmapWidget::loadMapHtml()
{
    QFile file(
        QStringLiteral(
            ":/maps/amap-map.html"));

    if (!file.open(QIODevice::ReadOnly)) {
        m_view->setHtml(
            QStringLiteral(
                "<!doctype html><html><head>"
                "<title>map-unavailable</title>"
                "</head><body>地图资源不可用</body></html>"),
            QUrl(QStringLiteral("qrc:/maps/")));
        return;
    }

    QString html =
        QString::fromUtf8(
            file.readAll());

    html.replace(
        QStringLiteral("**AMAP_KEY**"),
        jsonStringLiteral(
            AmapConfig::jsApiKey()));

    html.replace(
        QStringLiteral("**AMAP_SECRET**"),
        jsonStringLiteral(
            AmapConfig::jsApiSecret()));

    m_view->setHtml(
        html,
        QUrl(QStringLiteral("qrc:/maps/")));
}

void AmapWidget::applyPendingCenter()
{
    if (!m_mapReady ||
        !m_hasPendingCenter) {

        return;
    }

    const QString script =
        QStringLiteral(
            "window.setMapCenter(%1, %2);")
            .arg(
                m_pendingLat,
                0,
                'f',
                6)
            .arg(
                m_pendingLng,
                0,
                'f',
                6);

    m_hasPendingCenter = false;
    m_view->page()->runJavaScript(script);
}

void AmapWidget::applyPendingUserLocation()
{
    if (!m_mapReady ||
        !m_hasPendingUserLocation) {

        return;
    }

    const QString script =
        QStringLiteral(
            "window.setUserLocation(%1, %2);")
            .arg(
                m_pendingUserLat,
                0,
                'f',
                6)
            .arg(
                m_pendingUserLng,
                0,
                'f',
                6);

    m_hasPendingUserLocation = false;
    m_view->page()->runJavaScript(script);
}

void AmapWidget::applyPendingStations()
{
    if (!m_mapReady ||
        !m_hasPendingStations) {

        return;
    }

    const QString stationsJson =
        QString::fromUtf8(
            QJsonDocument(m_pendingStations)
                .toJson(QJsonDocument::Compact));

    const QString script =
        QStringLiteral(
            "window.setStations(%1);")
            .arg(stationsJson);

    m_hasPendingStations = false;
    m_view->page()->runJavaScript(script);
}

void AmapWidget::applyPendingRoute()
{
    if (!m_mapReady ||
        !m_hasPendingRoute) {

        return;
    }

    QJsonArray routePoints;

    for (const QString &pointText :
         m_pendingRoutePoints) {

        const QStringList coordinates =
            pointText.split(
                QLatin1Char(','));

        if (coordinates.size() != 2) {
            continue;
        }

        bool lngOk = false;
        bool latOk = false;

        const double lng =
            coordinates.at(0)
                .toDouble(&lngOk);

        const double lat =
            coordinates.at(1)
                .toDouble(&latOk);

        if (!lngOk ||
            !latOk ||
            !validCoordinate(lat, lng)) {

            continue;
        }

        QJsonArray coordinate;
        coordinate.append(lng);
        coordinate.append(lat);
        routePoints.append(coordinate);
    }

    const QString pointsJson =
        QString::fromUtf8(
            QJsonDocument(routePoints)
                .toJson(QJsonDocument::Compact));

    const QString script =
        QStringLiteral(
            "window.setRoute(%1);")
            .arg(pointsJson);

    m_hasPendingRoute = false;
    m_view->page()->runJavaScript(script);
}
