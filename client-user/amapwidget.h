#pragma once

#include <QWidget>
#include <QJsonArray>
#include <QStringList>

class QWebEngineView;

class AmapWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AmapWidget(
        QWidget *parent = nullptr);

    void setCenter(
        double lat,
        double lng);

    void setUserLocation(
        double lat,
        double lng);

    void setStations(
        const QJsonArray &stations);

    void setRoute(
        const QStringList &points);

private:
    void loadMapHtml();
    void applyPendingCenter();
    void applyPendingUserLocation();
    void applyPendingStations();
    void applyPendingRoute();

    QWebEngineView *m_view = nullptr;

    bool m_mapReady = false;
    bool m_hasPendingCenter = false;
    bool m_hasPendingUserLocation = false;
    bool m_hasPendingStations = false;
    bool m_hasPendingRoute = false;

    double m_pendingLat = 0.0;
    double m_pendingLng = 0.0;

    double m_pendingUserLat = 0.0;
    double m_pendingUserLng = 0.0;

    QJsonArray m_pendingStations;
    QStringList m_pendingRoutePoints;
};
