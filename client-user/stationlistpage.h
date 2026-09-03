#pragma once

// ============================================================================
// 充电用户端 - 首页（充电站列表）
// 展示附近充电站卡片；点击卡片跳转到该站的桩列表页。
// 数据来源：station_list 接口。
// ============================================================================

#include <QWidget>

class QLabel;
class QVBoxLayout;
class NetClient;

class StationListPage : public QWidget
{
    Q_OBJECT
public:
    explicit StationListPage(NetClient *net, QWidget *parent = nullptr);

signals:
    // 点击某个站点卡片时发出（携带站点 id 与名称）
    void stationSelected(qint64 stationId, const QString &name);

protected:
    // 首次显示时拉取一次站点列表
    void showEvent(QShowEvent *event) override;

private:
    void loadStations();
    void clearList();

    NetClient     *m_net;
    QVBoxLayout   *m_listLayout;
    QLabel        *m_tip;
    bool           m_loaded = false;
};
