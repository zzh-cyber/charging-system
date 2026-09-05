#pragma once

#include <QFrame>
#include <QJsonObject>
#include <QString>

class StationCardWidget : public QFrame
{
    Q_OBJECT

public:
    explicit StationCardWidget(
        const QJsonObject &station,
        QWidget *parent = nullptr);

signals:
    void stationSelected(
        qint64 stationId,
        const QString &name);

    void navigationRequested(
        qint64 stationId,
        const QString &name,
        double latitude,
        double longitude,
        double distance);

private:
    qint64  m_stationId = 0;
    QString m_name;

    double m_latitude = 0.0;
    double m_longitude = 0.0;
    double m_distance = -1.0;

    bool m_hasCoordinate = false;
    bool m_hasDistance = false;
};
