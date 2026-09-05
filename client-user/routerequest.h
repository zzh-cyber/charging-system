#pragma once

#include <QByteArray>
#include <QString>
#include <QtGlobal>
#include <QUrl>

#include <cmath>

struct RouteRequest
{
    quint64 requestId = 0;
    QString fromName;
    double fromLat = 0.0;
    double fromLng = 0.0;
    QString toName;
    double toLat = 0.0;
    double toLng = 0.0;
    double distance = -1.0;
    QString mode;

    bool isValid() const
    {
        return requestId != 0
            && !toName.trimmed().isEmpty()
            && !mode.trimmed().isEmpty()
            && validCoordinate(fromLat, fromLng)
            && validCoordinate(toLat, toLng);
    }

    QString fromLatText() const { return coordinateText(fromLat); }
    QString fromLngText() const { return coordinateText(fromLng); }
    QString toLatText() const { return coordinateText(toLat); }
    QString toLngText() const { return coordinateText(toLng); }

    QByteArray encodedFromName() const
    {
        return QUrl::toPercentEncoding(fromName);
    }

    QByteArray encodedToName() const
    {
        return QUrl::toPercentEncoding(toName);
    }

private:
    static bool validCoordinate(double lat, double lng)
    {
        return std::isfinite(lat)
            && std::isfinite(lng)
            && lat >= -90.0
            && lat <= 90.0
            && lng >= -180.0
            && lng <= 180.0;
    }

    static QString coordinateText(double value)
    {
        return QString::number(value, 'f', 6);
    }
};
