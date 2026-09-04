#include "stationlistpage.h"

#include "netclient.h"
#include "protocol.h"
#include "stationcardwidget.h"


#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QStackedWidget>
#include <QVBoxLayout>

StationListPage::StationListPage(
    NetClient *net,
    QWidget *parent)
    : QWidget(parent)
    , m_net(net)
{
    // =========================================================================
    // 标题栏
    // =========================================================================
    auto *header = new QHBoxLayout;

    header->setContentsMargins(
        16, 12, 16, 4);

    auto *title =
        new QLabel(
            QStringLiteral(
                "附近充电站"),
            this);

    title->setStyleSheet(
        "font-size:22px;"
        "font-weight:bold;");

    auto *refreshBtn =
        new QPushButton(
            QStringLiteral("刷新"),
            this);

    refreshBtn->setCursor(
        Qt::PointingHandCursor);

    refreshBtn->setStyleSheet(
        "QPushButton{"
        "background:#1d4ed8;"
        "color:#ffffff;"
        "border:none;"
        "border-radius:6px;"
        "padding:6px 14px;"
        "font-size:13px;"
        "}"
        "QPushButton:hover{"
        "background:#1e40af;"
        "}");

    connect(
        refreshBtn,
        &QPushButton::clicked,
        this,
        &StationListPage::loadStations);

    header->addWidget(
        title,
        1);

    header->addWidget(
        refreshBtn);

    // =========================================================================
    // 状态提示
    // =========================================================================
    m_tip = new QLabel(this);

    m_tip->setStyleSheet(
        "color:#86909c;"
        "padding:0 16px 8px 16px;");

    // =========================================================================
    // 四态 Stack
    // =========================================================================
    m_stack =
        new QStackedWidget(this);

    // -------------------------------------------------------------------------
    // 页0：加载中
    // -------------------------------------------------------------------------
    auto *loadingPage =
        new QWidget;

    {
        auto *l =
            new QVBoxLayout(
                loadingPage);

        l->setAlignment(
            Qt::AlignCenter);

        auto *lab =
            new QLabel(
                QStringLiteral(
                    "加载中…"),
                loadingPage);

        lab->setStyleSheet(
            "color:#86909c;"
            "font-size:15px;");

        l->addWidget(lab);
    }

    m_stack->addWidget(
        loadingPage);

    // -------------------------------------------------------------------------
    // 页1：内容
    // -------------------------------------------------------------------------
    auto *contentPage =
        new QWidget;

    {
        auto *l =
            new QVBoxLayout(
                contentPage);

        l->setContentsMargins(
            0, 0, 0, 0);

        auto *scroll =
            new QScrollArea(
                contentPage);

        scroll->setWidgetResizable(
            true);

        scroll->setFrameShape(
            QFrame::NoFrame);

        auto *container =
            new QWidget;

        m_listLayout =
            new QVBoxLayout(
                container);

        m_listLayout->setContentsMargins(
            12, 4, 12, 12);

        m_listLayout->setSpacing(12);

        scroll->setWidget(
            container);

        l->addWidget(scroll);
    }

    m_stack->addWidget(
        contentPage);

    // -------------------------------------------------------------------------
    // 页2：空数据
    // -------------------------------------------------------------------------
    auto *emptyPage =
        new QWidget;

    {
        auto *l =
            new QVBoxLayout(
                emptyPage);

        l->setAlignment(
            Qt::AlignCenter);

        l->setSpacing(12);

        auto *lab =
            new QLabel(
                QStringLiteral(
                    "暂无充电站"),
                emptyPage);

        lab->setStyleSheet(
            "color:#86909c;"
            "font-size:15px;");

        auto *btn =
            new QPushButton(
                QStringLiteral("刷新"),
                emptyPage);

        btn->setCursor(
            Qt::PointingHandCursor);

        connect(
            btn,
            &QPushButton::clicked,
            this,
            &StationListPage::loadStations);

        l->addWidget(lab);

        l->addWidget(
            btn,
            0,
            Qt::AlignHCenter);
    }

    m_stack->addWidget(
        emptyPage);

    // -------------------------------------------------------------------------
    // 页3：错误
    // -------------------------------------------------------------------------
    auto *errorPage =
        new QWidget;

    {
        auto *l =
            new QVBoxLayout(
                errorPage);

        l->setAlignment(
            Qt::AlignCenter);

        l->setSpacing(12);

        auto *lab =
            new QLabel(
                QStringLiteral(
                    "加载失败，请检查网络后重试"),
                errorPage);

        lab->setStyleSheet(
            "color:#e5484d;"
            "font-size:15px;");

        auto *btn =
            new QPushButton(
                QStringLiteral("重试"),
                errorPage);

        btn->setCursor(
            Qt::PointingHandCursor);

        connect(
            btn,
            &QPushButton::clicked,
            this,
            &StationListPage::loadStations);

        l->addWidget(lab);

        l->addWidget(
            btn,
            0,
            Qt::AlignHCenter);
    }

    m_stack->addWidget(
        errorPage);

    // =========================================================================
    // 总布局
    // =========================================================================
    auto *layout =
        new QVBoxLayout(this);

    layout->setContentsMargins(
        0, 0, 0, 0);

    layout->setSpacing(0);

    layout->addLayout(
        header);

    layout->addWidget(
        m_tip);

    layout->addWidget(
        m_stack,
        1);

    m_stack->setCurrentIndex(0);

    // =========================================================================
    // 最新 main 已有逻辑：
    // NetClient 断线重连成功后自动刷新
    // =========================================================================
    connect(
        m_net,
        &NetClient::reconnected,
        this,
        &StationListPage::loadStations);
}

// =============================================================================
// 设置用户当前位置
// =============================================================================
void StationListPage::setLocation(
    double lat,
    double lng)
{
    if (lat < -90.0 ||
        lat > 90.0 ||
        lng < -180.0 ||
        lng > 180.0) {

        m_tip->setText(
            QStringLiteral(
                "定位坐标无效"));

        return;
    }

    m_latitude = lat;
    m_longitude = lng;
    m_hasLocation = true;

    // 定位位置发生变化后，
    // 旧地址的缓存数据不能继续使用
    m_cachedList =
        QJsonArray();

    if (isVisible()) {

        m_loaded = true;

        loadStations();

    } else {

        m_loaded = false;
    }
}

// =============================================================================
// 页面显示
// =============================================================================
void StationListPage::showEvent(
    QShowEvent *event)
{
    QWidget::showEvent(event);

    if (!m_loaded) {

        m_loaded = true;

        loadStations();
    }
}

// =============================================================================
// 清空站点卡片
// =============================================================================
void StationListPage::clearList()
{
    while (QLayoutItem *item =
               m_listLayout->takeAt(0)) {

        if (QWidget *widget =
                item->widget()) {

            delete widget;
        }

        delete item;
    }
}


// =============================================================================
// 加载附近充电站
// =============================================================================
void StationListPage::loadStations()
{
    // -------------------------------------------------------------------------
    // 尚未定位
    // -------------------------------------------------------------------------
    if (!m_hasLocation) {

        m_tip->setText(
            QStringLiteral(
                "请先在上方输入地址并定位"));

        m_stack->setCurrentIndex(
            2);

        return;
    }

    // loading
    m_stack->setCurrentIndex(0);

    m_tip->clear();

    // -------------------------------------------------------------------------
    // station_list 协议保持不变
    // 只在 data 中增加 lat/lng
    // -------------------------------------------------------------------------
    QJsonObject data;

    data.insert(
        QStringLiteral("lat"),
        m_latitude);

    data.insert(
        QStringLiteral("lng"),
        m_longitude);

    const QJsonObject resp =
        m_net->request(
            Protocol::makeRequest(
                Protocol::MsgType::StationList,
                data));

    const int code =
        resp.value(
                QStringLiteral("code"))
            .toInt();

    const QJsonArray list =
        resp.value(
                QStringLiteral("data"))
            .toObject()
            .value(
                QStringLiteral("list"))
            .toArray();

    // -------------------------------------------------------------------------
    // 请求失败
    // 有缓存 → 展示缓存
    // 无缓存 → error
    // -------------------------------------------------------------------------
    if (code != Protocol::Ok) {

        if (!m_cachedList.isEmpty()) {

            buildCards(
                m_cachedList);

            m_tip->setText(
                QStringLiteral(
                    "网络异常，当前为缓存数据"));

            m_stack->setCurrentIndex(
                1);

            return;
        }

        m_stack->setCurrentIndex(
            3);

        return;
    }

    // -------------------------------------------------------------------------
    // 空数据
    // -------------------------------------------------------------------------
    if (list.isEmpty()) {

        m_cachedList =
            QJsonArray();

        m_tip->setText(
            QStringLiteral(
                "附近暂无充电站"));

        m_stack->setCurrentIndex(
            2);

        return;
    }

    // -------------------------------------------------------------------------
    // 成功
    // -------------------------------------------------------------------------
    m_cachedList = list;

    buildCards(list);

    m_tip->setText(
        QStringLiteral(
            "附近共 %1 个充电站")
            .arg(
                list.size()));

    m_stack->setCurrentIndex(
        1);
}

// =============================================================================
// 创建站点卡片
// =============================================================================
void StationListPage::buildCards(
    const QJsonArray &list)
{
    clearList();

    for (const QJsonValue &value :
         list) {

        if (!value.isObject())
            continue;

        auto *card =
            new StationCardWidget(
                value.toObject(),
                this);

        // -------------------------------------------------------------
        // 查看该站充电桩
        // -------------------------------------------------------------
        connect(
            card,
            &StationCardWidget::stationSelected,
            this,
            [this](
                qint64 stationId,
                const QString &name) {

                if (stationId <= 0)
                    return;

                emit stationSelected(
                    stationId,
                    name);
            });

        // -------------------------------------------------------------
        // 一键导航
        // -------------------------------------------------------------
        connect(
            card,
            &StationCardWidget::navigationRequested,
            this,
            [this](
                qint64 stationId,
                const QString &name,
                double targetLat,
                double targetLng,
                double distance) {

                // 起点必须完整
                if (!m_hasLocation ||
                    m_latitude < -90.0 ||
                    m_latitude > 90.0 ||
                    m_longitude < -180.0 ||
                    m_longitude > 180.0) {

                    m_tip->setText(
                        QStringLiteral(
                            "当前位置无效，请重新定位"));

                    return;
                }

                // 终点必须完整
                if (targetLat < -90.0 ||
                    targetLat > 90.0 ||
                    targetLng < -180.0 ||
                    targetLng > 180.0) {

                    m_tip->setText(
                        QStringLiteral(
                            "该充电站缺少有效坐标"));

                    return;
                }

                emit navigationRequested(
                    stationId,
                    name,
                    m_latitude,
                    m_longitude,
                    targetLat,
                    targetLng,
                    distance);
            });

        m_listLayout->addWidget(
            card);
    }

    m_listLayout->addStretch();
}
