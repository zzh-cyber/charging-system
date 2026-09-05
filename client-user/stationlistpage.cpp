#include "stationlistpage.h"

#include "netclient.h"
#include "protocol.h"

#include <QComboBox>
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

#include <algorithm>
#include <cmath>
#include <QSet>

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

    // -------------------------------------------------------------------------
    // NO.4：显示条数切换（最近 5 / 10 条）
    // 切换时从已缓存的排序结果即时截取，保留当前地址，无需重新定位/请求。
    // -------------------------------------------------------------------------
    m_limitCombo = new QComboBox(this);

    m_limitCombo->addItem(
        QStringLiteral("最近 5 个"),
        5);

    m_limitCombo->addItem(
        QStringLiteral("最近 10 个"),
        10);

    m_limitCombo->setCurrentIndex(0);

    m_limitCombo->setCursor(
        Qt::PointingHandCursor);

    m_limitCombo->setStyleSheet(
        "QComboBox{"
        "border:1px solid #d6e4ff;"
        "border-radius:6px;"
        "padding:4px 8px;"
        "font-size:13px;"
        "background:#ffffff;"
        "}");

    connect(
        m_limitCombo,
        QOverload<int>::of(
            &QComboBox::currentIndexChanged),
        this,
        [this](int) {
            m_limit =
                m_limitCombo
                    ->currentData()
                    .toInt();

            // 已有缓存则即时重渲染；无缓存时静默，等下次加载。
            if (!m_cachedList.isEmpty())
                renderStations();
        });

    header->addWidget(
        title,
        1);

    header->addWidget(
        m_limitCombo);

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

        if (QWidget *w =
                item->widget()) {

            w->deleteLater();
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

            renderStations();

            m_tip->setText(
                QStringLiteral(
                    "网络异常，当前为缓存数据"));

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
    // 成功：先按距离稳定排序并去重，缓存完整结果，再按 5/10 条截取展示（NO.4）
    // -------------------------------------------------------------------------
    m_cachedList = sortStations(list);

    renderStations();
}

// =============================================================================
// NO.4：按距离升序稳定排序 + 去重 + distance 兜底
// -----------------------------------------------------------------------------
// - 服务端已返回 distance，但客户端仍做一次防御：distance 缺失/为负时用
//   Haversine 依据站点经纬度与当前定位兜底计算，保证排序稳定可靠。
// - 以 station id 去重，避免重复卡片。
// - 同距离时：空闲桩多者靠前；再同则按站名升序，保证顺序稳定确定。
// =============================================================================
QJsonArray StationListPage::sortStations(
    const QJsonArray &raw) const
{
    constexpr double kEarthRadiusKm = 6371.0;

    const auto toRad = [](double deg) {
        return deg * M_PI / 180.0;
    };

    QVector<QJsonObject> items;
    items.reserve(raw.size());

    QSet<qint64> seen;

    for (const QJsonValue &v : raw) {

        QJsonObject s = v.toObject();

        const qint64 id =
            s.value(QStringLiteral("id"))
                .toVariant()
                .toLongLong();

        // 去重：同一站点只保留首个
        if (seen.contains(id))
            continue;

        seen.insert(id);

        // distance 兜底
        double distance =
            s.value(QStringLiteral("distance"))
                .toDouble(-1.0);

        const bool needCalc =
            !s.contains(QStringLiteral("distance")) ||
            distance < 0.0;

        if (needCalc && m_hasLocation &&
            s.contains(QStringLiteral("latitude")) &&
            s.contains(QStringLiteral("longitude"))) {

            const double sLat =
                s.value(QStringLiteral("latitude"))
                    .toDouble();

            const double sLng =
                s.value(QStringLiteral("longitude"))
                    .toDouble();

            const double dLat =
                toRad(sLat - m_latitude);

            const double dLng =
                toRad(sLng - m_longitude);

            const double a =
                std::sin(dLat / 2) *
                    std::sin(dLat / 2) +
                std::cos(toRad(m_latitude)) *
                    std::cos(toRad(sLat)) *
                    std::sin(dLng / 2) *
                    std::sin(dLng / 2);

            distance =
                2 * kEarthRadiusKm *
                std::atan2(
                    std::sqrt(a),
                    std::sqrt(1 - a));

            s[QStringLiteral("distance")] =
                distance;

        } else if (distance < 0.0) {

            // 无法兜底时置 0，避免负值污染排序
            distance = 0.0;
            s[QStringLiteral("distance")] =
                distance;
        }

        items.append(s);
    }

    std::stable_sort(
        items.begin(),
        items.end(),
        [](const QJsonObject &a,
           const QJsonObject &b) {

            const double da =
                a.value(QStringLiteral("distance"))
                    .toDouble();
            const double db =
                b.value(QStringLiteral("distance"))
                    .toDouble();

            if (!qFuzzyCompare(da + 1.0, db + 1.0))
                return da < db;

            const int ia =
                a.value(QStringLiteral("idle")).toInt();
            const int ib =
                b.value(QStringLiteral("idle")).toInt();

            if (ia != ib)
                return ia > ib;

            return a.value(QStringLiteral("name"))
                       .toString() <
                   b.value(QStringLiteral("name"))
                       .toString();
        });

    QJsonArray sorted;

    for (const QJsonObject &o : items)
        sorted.append(o);

    return sorted;
}

// =============================================================================
// NO.4：按当前 5/10 条限制，从已排序缓存截取并渲染
// =============================================================================
void StationListPage::renderStations()
{
    const int total = m_cachedList.size();

    if (total == 0) {

        m_tip->setText(
            QStringLiteral("附近暂无充电站"));

        m_stack->setCurrentIndex(2);

        return;
    }

    const int shown = qMin(m_limit, total);

    QJsonArray view;

    for (int i = 0; i < shown; ++i)
        view.append(m_cachedList.at(i));

    buildCards(view);

    m_tip->setText(
        QStringLiteral(
            "附近共 %1 个，按距离显示最近 %2 个")
            .arg(total)
            .arg(shown));

    m_stack->setCurrentIndex(1);
}

// =============================================================================
// 创建站点卡片
// =============================================================================
void StationListPage::buildCards(
    const QJsonArray &list)
{
    clearList();

    for (const QJsonValue &v : list) {

        const QJsonObject s =
            v.toObject();

        const qint64 id =
            s.value(
                 QStringLiteral("id"))
                .toVariant()
                .toLongLong();

        const QString name =
            s.value(
                 QStringLiteral("name"))
                .toString();

        const QString address =
            s.value(
                 QStringLiteral("address"))
                .toString();

        const double price =
            s.value(
                 QStringLiteral("price"))
                .toDouble();

        const int total =
            s.value(
                 QStringLiteral("total"))
                .toInt();

        const int idle =
            s.value(
                 QStringLiteral("idle"))
                .toInt();

        const bool hasDistance =
            s.contains(
                QStringLiteral("distance"));

        const double distance =
            s.value(
                 QStringLiteral("distance"))
                .toDouble();

        // ---------------------------------------------------------------------
        // 卡片
        // ---------------------------------------------------------------------
        auto *card =
            new QPushButton(this);

        card->setCursor(
            Qt::PointingHandCursor);

        card->setMinimumHeight(
            hasDistance
                ? 128
                : 112);

        card->setStyleSheet(
            "QPushButton{"
            "text-align:left;"
            "padding:14px;"
            "border:1px solid #d6e4ff;"
            "border-radius:10px;"
            "background:#ffffff;"
            "}"
            "QPushButton:hover{"
            "border-color:#1d4ed8;"
            "background:#eef4ff;"
            "}");

        auto *body =
            new QVBoxLayout(card);

        body->setContentsMargins(
            2, 2, 2, 2);

        body->setSpacing(4);

        // 第一行
        auto *row1 =
            new QHBoxLayout;

        auto *nameLabel =
            new QLabel(
                name,
                card);

        nameLabel->setStyleSheet(
            "font-size:17px;"
            "font-weight:bold;"
            "border:none;"
            "background:transparent;");

        nameLabel->setAttribute(
            Qt::WA_TransparentForMouseEvents);

        auto *priceLabel =
            new QLabel(
                QStringLiteral(
                    "￥%1/度")
                    .arg(
                        price,
                        0,
                        'f',
                        2),
                card);

        priceLabel->setStyleSheet(
            "color:#ff6a00;"
            "font-size:15px;"
            "font-weight:bold;"
            "border:none;"
            "background:transparent;");

        priceLabel->setAttribute(
            Qt::WA_TransparentForMouseEvents);

        row1->addWidget(
            nameLabel,
            1);

        row1->addWidget(
            priceLabel);

        // ---------------------------------------------------------------------
        // 地址
        // ---------------------------------------------------------------------
        auto *addrLabel =
            new QLabel(
                address,
                card);

        addrLabel->setWordWrap(
            true);

        addrLabel->setStyleSheet(
            "color:#86909c;"
            "font-size:12px;"
            "border:none;"
            "background:transparent;");

        addrLabel->setAttribute(
            Qt::WA_TransparentForMouseEvents);

        // ---------------------------------------------------------------------
        // 距离
        // ---------------------------------------------------------------------
        QLabel *distanceLabel =
            nullptr;

        if (hasDistance) {

            distanceLabel =
                new QLabel(
                    QStringLiteral(
                        "距您 %1 km")
                        .arg(
                            distance,
                            0,
                            'f',
                            1),
                    card);

            distanceLabel->setStyleSheet(
                "color:#4e5969;"
                "font-size:12px;"
                "border:none;"
                "background:transparent;");

            distanceLabel->setAttribute(
                Qt::WA_TransparentForMouseEvents);
        }

        // ---------------------------------------------------------------------
        // 空闲桩
        // ---------------------------------------------------------------------
        auto *idleLabel =
            new QLabel(
                QStringLiteral(
                    "空闲 %1/%2")
                    .arg(idle)
                    .arg(total),
                card);

        const QString idleColor =
            idle > 0
                ? QStringLiteral("#16a34a")
                : QStringLiteral("#999999");

        idleLabel->setStyleSheet(
            QStringLiteral(
                "color:%1;"
                "font-size:13px;"
                "font-weight:bold;"
                "border:none;"
                "background:transparent;")
                .arg(idleColor));

        idleLabel->setAttribute(
            Qt::WA_TransparentForMouseEvents);

        body->addLayout(
            row1);

        body->addWidget(
            addrLabel);

        if (distanceLabel) {
            body->addWidget(
                distanceLabel);
        }

        body->addWidget(
            idleLabel);

        connect(
            card,
            &QPushButton::clicked,
            this,
            [this, id, name]() {

                emit stationSelected(
                    id,
                    name);
            });

        m_listLayout->addWidget(
            card);
    }
}
