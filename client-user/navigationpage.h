#pragma once

#include <QPixmap>
#include <QString>
#include <QWidget>

#include "routerequest.h"


class QLabel;
class QPushButton;
class QProgressBar;
class QResizeEvent;
class QNetworkAccessManager;
class QNetworkReply;


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

    void rescaleMapPixmap();


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

    void requestStaticMap(
        const QStringList &points,
        double routeDistanceMeters,
        qint64 routeDurationSeconds,
        quint64 requestId);


    QString webServiceKey() const;


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

    QProgressBar *m_loadProgress = nullptr;

    // 不再使用 QWebEngineView
    QLabel *m_mapLabel = nullptr;

    // 保存原始地图，窗口缩放时重新按比例显示
    QPixmap m_originalMapPixmap;


    // ------------------------------------------------------------------------
    // 网络
    // ------------------------------------------------------------------------
    QNetworkAccessManager *m_networkManager = nullptr;

    QNetworkReply *m_routeReply = nullptr;
    QNetworkReply *m_mapReply = nullptr;


    // 快速切换驾车 / 步行时，
    // 防止旧请求结果覆盖最新请求。
    quint64 m_requestId = 0;
};
