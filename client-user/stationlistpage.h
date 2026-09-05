#pragma once

// ============================================================================
// 充电用户端 - 首页（充电站列表）
// 展示附近充电站卡片；点击卡片跳转到该站的桩列表页。
// 数据来源：station_list 接口。
// NO.7：QStackedWidget 管理 loading/content/empty/error 四态，失败时展示缓存。
// ============================================================================

#include <QJsonArray>
#include <QWidget>

class QLabel;
class QVBoxLayout;
class QStackedWidget;
class QComboBox;
class NetClient;
class StationCardWidget;


class StationListPage : public QWidget
{
    Q_OBJECT

public:
    explicit StationListPage(
        NetClient *net,
        QWidget *parent = nullptr);

    // MainWindow 地址解析成功后传入用户经纬度
    void setLocation(
        double lat,
        double lng);

signals:
    void stationSelected(
        qint64 stationId,
        const QString &name);
    
    void navigationRequested(
        qint64 stationId,
        const QString &name,
        double startLatitude,
        double startLongitude,
        double targetLatitude,
        double targetLongitude,
        double distance);

protected:
    void showEvent(
        QShowEvent *event) override;

private:
    void loadStations();
    void clearList();

    void buildCards(
        const QJsonArray &list);

    // NO.4：按距离升序稳定排序 + 去重 + distance 兜底
    QJsonArray sortStations(
        const QJsonArray &raw) const;

    // NO.4：按当前 5/10 条限制，从排序结果截取并渲染
    void renderStations();

    NetClient      *m_net;
    QStackedWidget *m_stack;
    QVBoxLayout    *m_listLayout;
    QLabel         *m_tip;
    QComboBox      *m_limitCombo;

    bool            m_loaded = false;

    // 展示条数上限（NO.4：5 或 10）
    int             m_limit = 5;

    // 最近一次成功、已排序的完整列表缓存（NO.7 离线展示 + NO.4 切换即时截取）
    QJsonArray      m_cachedList;

    // ------------------------------------------------------------------------
    // 用户当前位置
    // ------------------------------------------------------------------------
    double m_latitude = 0.0;
    double m_longitude = 0.0;
    bool m_hasLocation = false;
};
