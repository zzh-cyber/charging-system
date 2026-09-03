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
class NetClient;

class StationListPage : public QWidget
{
    Q_OBJECT
public:
    explicit StationListPage(NetClient *net, QWidget *parent = nullptr);

signals:
    void stationSelected(qint64 stationId, const QString &name);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void loadStations();
    void clearList();
    void buildCards(const QJsonArray &list);

    NetClient      *m_net;
    QStackedWidget *m_stack;
    QVBoxLayout    *m_listLayout;
    QLabel         *m_tip;
    bool            m_loaded = false;
    QJsonArray      m_cachedList;   // 最近一次成功列表缓存（NO.7）
};
