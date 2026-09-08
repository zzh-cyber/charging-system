#include "routeplanner.h"

#include "amapconfig.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

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

QString coordinateText(
    double lng,
    double lat)
{
    return QStringLiteral("%1,%2")
        .arg(lng, 0, 'f', 6)
        .arg(lat, 0, 'f', 6);
}

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
            value.toString().toDouble(&ok);

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
            value.toString().toLongLong(&ok);

        if (ok) {
            return result;
        }
    }

    return fallback;
}

QJsonArray jsonArrayValue(
    const QJsonValue &value)
{
    if (value.isArray()) {
        return value.toArray();
    }

    QJsonArray result;

    if (value.isObject()) {
        result.append(value.toObject());
    }

    return result;
}

QStringList extractCoordinates(
    const QString &polyline)
{
    QStringList result;

    const QRegularExpression regex(
        QStringLiteral(
            "(-?\\d+(?:\\.\\d+)?),"
            "(-?\\d+(?:\\.\\d+)?)"));

    auto iterator =
        regex.globalMatch(polyline);

    QString lastPoint;

    while (iterator.hasNext()) {
        const QRegularExpressionMatch match =
            iterator.next();

        bool lngOk = false;
        bool latOk = false;
        const double lng =
            match.captured(1).toDouble(&lngOk);
        const double lat =
            match.captured(2).toDouble(&latOk);

        if (!lngOk ||
            !latOk ||
            !validCoordinate(lat, lng)) {

            continue;
        }

        const QString point =
            coordinateText(lng, lat);

        if (point != lastPoint) {
            result.append(point);
            lastPoint = point;
        }
    }

    return result;
}

QStringList extractRoutePoints(
    const QJsonObject &path)
{
    QStringList result =
        extractCoordinates(
            path.value(
                    QStringLiteral("polyline"))
                .toString());

    const QJsonArray steps =
        jsonArrayValue(
            path.value(
                QStringLiteral("steps")));

    QString lastPoint =
        result.isEmpty()
            ? QString()
            : result.last();

    for (const QJsonValue &stepValue : steps) {
        const QStringList points =
            extractCoordinates(
                stepValue.toObject()
                    .value(
                        QStringLiteral("polyline"))
                    .toString());

        for (const QString &point : points) {
            if (point == lastPoint) {
                continue;
            }

            result.append(point);
            lastPoint = point;
        }
    }

    return result;
}

qint64 stepDuration(
    const QJsonObject &path)
{
    qint64 total = 0;
    const QJsonArray steps =
        jsonArrayValue(
            path.value(
                QStringLiteral("steps")));

    for (const QJsonValue &stepValue : steps) {
        const QJsonObject step =
            stepValue.toObject();

        qint64 duration =
            jsonInt64(
                step.value(
                        QStringLiteral("cost"))
                    .toObject()
                    .value(
                        QStringLiteral("duration")));

        if (duration <= 0) {
            duration =
                jsonInt64(
                    step.value(
                        QStringLiteral("duration")));
        }

        if (duration > 0) {
            total += duration;
        }
    }

    return total;
}

} // namespace

RoutePlanner::RoutePlanner(
    QObject *parent)
    : QObject(parent)
    , m_networkManager(
          new QNetworkAccessManager(this))
{
}

quint64 RoutePlanner::planRoute(
    const RouteRequest &request)
{
    const quint64 requestId =
        ++m_requestId;

    if (m_reply) {
        m_reply->abort();
        m_reply = nullptr;
    }

    if (!validCoordinate(
            request.fromLat,
            request.fromLng) ||
        !validCoordinate(
            request.toLat,
            request.toLng)) {

        emit routeError(
            requestId,
            QStringLiteral(
                "起点或终点坐标无效"));
        return requestId;
    }

    const QString key =
        AmapConfig::webServiceKey();

    if (key.isEmpty()) {
        emit routeError(
            requestId,
            QStringLiteral(
                "未配置高德 Web 服务 API Key"));
        return requestId;
    }

    const bool walking =
        request.mode ==
        QStringLiteral("walking");

    QUrl url(
        walking
            ? QStringLiteral(
                  "https://restapi.amap.com/v5/direction/walking")
            : QStringLiteral(
                  "https://restapi.amap.com/v5/direction/driving"));

    QUrlQuery query;
    query.addQueryItem(
        QStringLiteral("key"),
        key);
    query.addQueryItem(
        QStringLiteral("origin"),
        coordinateText(
            request.fromLng,
            request.fromLat));
    query.addQueryItem(
        QStringLiteral("destination"),
        coordinateText(
            request.toLng,
            request.toLat));
    query.addQueryItem(
        QStringLiteral("show_fields"),
        QStringLiteral("cost,polyline"));
    query.addQueryItem(
        QStringLiteral("output"),
        QStringLiteral("json"));

    if (!walking) {
        query.addQueryItem(
            QStringLiteral("strategy"),
            QStringLiteral("32"));
    }

    url.setQuery(query);

    m_reply =
        m_networkManager->get(
            QNetworkRequest(url));

    QNetworkReply *reply =
        m_reply;

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

    connect(
        reply,
        &QNetworkReply::finished,
        this,
        [this, reply, request, requestId]() {
            if (m_reply == reply) {
                m_reply = nullptr;
            }

            if (requestId != m_requestId) {
                reply->deleteLater();
                return;
            }

            const bool timedOut =
                reply->property(
                         "routeTimedOut")
                    .toBool();

            if (reply->error() !=
                QNetworkReply::NoError) {

                const QString message =
                    timedOut
                        ? QStringLiteral(
                              "路线请求超时")
                        : reply->errorString();

                reply->deleteLater();
                emit routeError(
                    requestId,
                    message);
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

                emit routeError(
                    requestId,
                    QStringLiteral(
                        "高德返回的数据不是有效 JSON"));
                return;
            }

            const QJsonObject root =
                document.object();

            if (root.value(
                        QStringLiteral("status"))
                    .toString() !=
                QStringLiteral("1")) {

                QString info =
                    root.value(
                            QStringLiteral("info"))
                        .toString();

                const QJsonValue infoCodeValue =
                    root.value(
                        QStringLiteral("infocode"));

                QString infoCode =
                    infoCodeValue.toString();

                if (infoCode.isEmpty() &&
                    infoCodeValue.isDouble()) {

                    infoCode =
                        QString::number(
                            static_cast<qint64>(
                                infoCodeValue.toDouble()));
                }

                if (request.mode ==
                        QStringLiteral("walking") &&
                    (info ==
                         QStringLiteral(
                             "OVER_DIRECTION_RANGE") ||
                     infoCode ==
                         QStringLiteral("20803"))) {

                    emit routeError(
                        requestId,
                        QStringLiteral(
                            "OVER_DIRECTION_RANGE"));
                    return;
                }

                if (info.isEmpty()) {
                    info = QStringLiteral(
                        "高德地图服务返回错误");
                }

                emit routeError(
                    requestId,
                    info);
                return;
            }

            const QJsonArray paths =
                jsonArrayValue(
                    root.value(
                            QStringLiteral("route"))
                        .toObject()
                        .value(
                            QStringLiteral("paths")));

            if (paths.isEmpty()) {
                emit routeError(
                    requestId,
                    QStringLiteral(
                        "高德没有返回可用路线方案"));
                return;
            }

            QJsonObject path;
            QStringList routePoints;

            for (const QJsonValue &pathValue : paths) {
                if (!pathValue.isObject()) {
                    continue;
                }

                const QJsonObject candidate =
                    pathValue.toObject();
                const QStringList candidatePoints =
                    extractRoutePoints(candidate);

                if (!candidatePoints.isEmpty()) {
                    path = candidate;
                    routePoints = candidatePoints;
                    break;
                }
            }

            if (path.isEmpty()) {
                emit routeError(
                    requestId,
                    QStringLiteral(
                        "高德未返回 polyline 路线点"));
                return;
            }

            RouteResult result;
            result.distanceMeters =
                jsonDouble(
                    path.value(
                        QStringLiteral("distance")));

            result.durationSeconds =
                jsonInt64(
                    path.value(
                            QStringLiteral("cost"))
                        .toObject()
                        .value(
                            QStringLiteral("duration")));

            if (result.durationSeconds <= 0) {
                result.durationSeconds =
                    jsonInt64(
                        path.value(
                            QStringLiteral("duration")));
            }

            if (result.durationSeconds <= 0) {
                result.durationSeconds =
                    stepDuration(path);
            }

            result.points =
                routePoints;

            const QJsonArray steps =
                jsonArrayValue(
                    path.value(
                        QStringLiteral("steps")));

            for (const QJsonValue &stepValue : steps) {
                if (!stepValue.isObject()) {
                    continue;
                }

                const QJsonObject stepObject =
                    stepValue.toObject();

                RouteStep step;
                step.instruction =
                    stepObject.value(
                            QStringLiteral("instruction"))
                        .toString()
                        .trimmed();
                step.road =
                    stepObject.value(
                            QStringLiteral("road"))
                        .toString()
                        .trimmed();
                step.action =
                    stepObject.value(
                            QStringLiteral("action"))
                        .toString()
                        .trimmed();
                step.distanceMeters =
                    jsonDouble(
                        stepObject.value(
                            QStringLiteral("distance")));

                if (step.instruction.isEmpty() &&
                    step.road.isEmpty() &&
                    step.action.isEmpty() &&
                    step.distanceMeters <= 0.0) {

                    continue;
                }

                result.steps.append(step);
            }

            const QString start =
                coordinateText(
                    request.fromLng,
                    request.fromLat);
            const QString target =
                coordinateText(
                    request.toLng,
                    request.toLat);

            if (result.points.first() != start) {
                result.points.prepend(start);
            }

            if (result.points.last() != target) {
                result.points.append(target);
            }

            emit routeReady(
                requestId,
                result);
        });

    return requestId;
}

void RoutePlanner::cancelPending()
{
    ++m_requestId;

    if (m_reply) {
        m_reply->abort();
        m_reply = nullptr;
    }
}
