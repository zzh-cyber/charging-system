#pragma once

// ============================================================================
// 充电用户端 - 首页（充电站列表）
// 展示附近充电站卡片；点击卡片跳转到该站的桩列表页。
// 数据来源：station_list 接口。
// NO.7：QStackedWidget 管理 loading/content/empty/error 四态，失败时展示缓存。
// 底部站点区仿高德：可上拉铺满、下拉收回，露出更多地图。
// ============================================================================

#include <QJsonArray>
#include <QWidget>
#include "routerequest.h"

class QLabel;
class QVBoxLayout;
class QStackedWidget;
class QComboBox;
class QFrame;
class QEvent;
class QVariantAnimation;
class NetClient;
class StationCardWidget;
class AmapWidget;
class QResizeEvent;
class QShowEvent;

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
        const RouteRequest &request);

protected:
    void showEvent(
        QShowEvent *event) override;

    void resizeEvent(
        QResizeEvent *event) override;

    bool eventFilter(
        QObject *watched,
        QEvent *event) override;

private:
    void applyResponsiveStyle();

    void loadStations();

    void clearList();

    void buildCards(
        const QJsonArray &list);

    // NO.4：按距离升序稳定排序 + 去重 + distance 兜底
    QJsonArray sortStations(
        const QJsonArray &raw) const;

    // NO.4：按当前 5/10 条限制，从排序结果截取并渲染
    void renderStations();

    int sheetMinHeight() const;

    int sheetPeekHeight() const;

    int sheetExpandedHeight() const;

    void setSheetHeight(
        int height);

    void applySheetState(
        bool animate);

    void applySheetFromRatio(
        bool animate);

    void startSheetDrag(
        int globalY);

    void updateSheetDrag(
        int globalY);

    void finishSheetDrag(
        int globalY);

    NetClient *m_net = nullptr;

    QStackedWidget *m_stack = nullptr;

    QVBoxLayout *m_listLayout = nullptr;

    AmapWidget *m_mapWidget = nullptr;

    QLabel *m_tip = nullptr;

    QComboBox *m_limitCombo = nullptr;

    QFrame *m_sheet = nullptr;

    QFrame *m_handle = nullptr;

    QVariantAnimation *m_sheetAnim = nullptr;

    bool m_loaded = false;

    // 展示条数上限
    int m_limit = 5;

    // 最近一次成功、已排序的完整列表缓存
    QJsonArray m_cachedList;

    // 底部面板高度占页面比例；松手后停在拖到的位置，窗口缩放时按比例保持
    double m_sheetRatio = 0.46;

    bool m_sheetExpanded = false;

    bool m_draggingSheet = false;

    int m_dragStartY = 0;

    int m_dragStartHeight = 0;

    // ------------------------------------------------------------------------
    // 用户当前位置
    // ------------------------------------------------------------------------
    double m_latitude = 0.0;
    double m_longitude = 0.0;

    bool m_hasLocation = false;
    quint64 m_nextRouteRequestId = 0;
};
