#pragma once

// ============================================================================
// 充电用户端 - 桩列表页
// 展示某个充电站的电桩：编号 / 类型 / 功率 / 状态。
// 数据来源：pile_list 接口。
// ============================================================================

#include <QWidget>

class QLabel;
class QPushButton;
class QVBoxLayout;
class NetClient;

class PileListPage : public QWidget
{
    Q_OBJECT
public:
    explicit PileListPage(NetClient *net, QWidget *parent = nullptr);

    // 加载指定充电站的桩列表
    void loadStation(qint64 stationId, const QString &name);

signals:
    void back();

private:
    void clearList();

    NetClient     *m_net;
    QLabel        *m_title;
    QLabel        *m_tip;
    QPushButton   *m_backBtn;
    QVBoxLayout   *m_listLayout;
};
