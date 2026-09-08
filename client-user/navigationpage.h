#pragma once

#include <QString>
#include <QVector>
#include <QWidget>

#include "routerequest.h"
#include "routeresult.h"


class QLabel;
class QPushButton;
class QProgressBar;
class QResizeEvent;
class QTimer;
class AmapWidget;
class RoutePlanner;


class NavigationPage : public QWidget
{
    Q_OBJECT

public:
    explicit NavigationPage(
        QWidget *parent = nullptr);

    // 保留现有 RouteRequest 业务接口
    void setNavigationData(
        const RouteRequest &request);


protected:
    void resizeEvent(
        QResizeEvent *event) override;


signals:
    void back();


private:
    // ------------------------------------------------------------------------
    // UI
    // ------------------------------------------------------------------------
    void applyResponsiveStyle();

    void setMapPlaceholder(
        const QString &title,
        const QString &message);


    // ------------------------------------------------------------------------
    // NO.11
    // 驾车 / 步行切换
    // ------------------------------------------------------------------------
    void setRouteMode(
        const QString &mode);


    // ------------------------------------------------------------------------
    // NO.9
    // 高德 Web Service 路径规划
    // ------------------------------------------------------------------------
    void loadRoute();

    void showSimulationStep();
    void stopAndResetSimulation();

private:
    // ------------------------------------------------------------------------
    // 当前导航请求
    // ------------------------------------------------------------------------
    RouteRequest m_currentRequest;

    bool m_hasRouteRequest = false;


    // ------------------------------------------------------------------------
    // 路线基本信息
    // ------------------------------------------------------------------------
    QLabel *m_stationLabel = nullptr;
    QLabel *m_startLabel = nullptr;
    QLabel *m_targetLabel = nullptr;
    QLabel *m_distanceLabel = nullptr;

    QLabel *m_routeModeLabel = nullptr;

    QPushButton *m_driveButton = nullptr;
    QPushButton *m_walkButton = nullptr;


    // ------------------------------------------------------------------------
    // 地图
    // ------------------------------------------------------------------------
    QLabel *m_loadStatusLabel = nullptr;
    QLabel *m_routeSummaryLabel = nullptr;
    QLabel *m_routeDistanceValue = nullptr;
    QLabel *m_routeDurationValue = nullptr;
    QLabel *m_routeEtaValue = nullptr;

    QLabel *m_stepActionLabel = nullptr;
    QLabel *m_stepInstructionLabel = nullptr;
    QLabel *m_stepDetailLabel = nullptr;
    QLabel *m_simulationLabel = nullptr;

    QProgressBar *m_loadProgress = nullptr;

    QPushButton *m_continueButton = nullptr;
    QTimer *m_simulationTimer = nullptr;

    QVector<RouteStep> m_routeSteps;
    qsizetype m_currentStepIndex = -1;

    AmapWidget *m_mapWidget = nullptr;


    // ------------------------------------------------------------------------
    // 路线规划
    // ------------------------------------------------------------------------
    RoutePlanner *m_routePlanner = nullptr;
};
