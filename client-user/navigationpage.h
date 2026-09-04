#pragma once

#include <QWidget>

class QLabel;

class NavigationPage : public QWidget
{
    Q_OBJECT

public:
    explicit NavigationPage(
        QWidget *parent = nullptr);

    void setNavigationData(
        const QString &stationName,
        double startLat,
        double startLng,
        double targetLat,
        double targetLng,
        double distance);

signals:
    void back();

private:
    QLabel *m_stationLabel;
    QLabel *m_startLabel;
    QLabel *m_targetLabel;
    QLabel *m_distanceLabel;
};
