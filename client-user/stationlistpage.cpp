#include "stationlistpage.h"

#include "amapwidget.h"
#include "netclient.h"
#include "protocol.h"
#include "stationcardwidget.h"
#include "uitheme.h"
#include "windowhelper.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSet>
#include <QShowEvent>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QVector>

#include <algorithm>
#include <cmath>


StationListPage::StationListPage(
    NetClient *net,
    QWidget *parent)
    : QWidget(parent)
    , m_net(net)
    , m_mapWidget(new AmapWidget(this))
{
    setObjectName(
        QStringLiteral(
            "stationListPage"));


    // ========================================================================
    // 页头卡片
    // ========================================================================
    auto *headerCard =
        new QFrame(this);

    headerCard->setObjectName(
        QStringLiteral(
            "stationHeaderCard"));

    UiTheme::applyCardShadow(
        headerCard,
        18,
        4);


    auto *headerCardLayout =
        new QVBoxLayout(
            headerCard);

    headerCardLayout->setObjectName(
        QStringLiteral(
            "stationHeaderCardLayout"));

    headerCardLayout->setContentsMargins(
        14,
        7,
        14,
        7);

    headerCardLayout->setSpacing(
        3);


    auto *header =
        new QHBoxLayout;

    header->setObjectName(
        QStringLiteral(
            "stationHeaderLayout"));

    header->setSpacing(
        8);


    auto *title =
        new QLabel(
            QStringLiteral(
                "附近充电站"),
            headerCard);

    title->setObjectName(
        QStringLiteral(
            "stationTitle"));


    // ========================================================================
    // NO.4：显示最近 5 / 10 个
    // 业务逻辑保持最新 main 原样
    // ========================================================================
    m_limitCombo =
        new QComboBox(
            headerCard);

    m_limitCombo->setObjectName(
        QStringLiteral(
            "stationLimitCombo"));

    m_limitCombo->addItem(
        QStringLiteral(
            "最近 5 个"),
        5);

    m_limitCombo->addItem(
        QStringLiteral(
            "最近 10 个"),
        10);

    m_limitCombo->setCurrentIndex(
        0);

    m_limitCombo->setCursor(
        Qt::PointingHandCursor);


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

            if (!m_cachedList.isEmpty()) {

                renderStations();
            }
        });


    auto *refreshBtn =
        new QPushButton(
            QStringLiteral(
                "刷新"),
            headerCard);

    refreshBtn->setObjectName(
        QStringLiteral(
            "stationRefreshButton"));

    refreshBtn->setCursor(
        Qt::PointingHandCursor);


    connect(
        refreshBtn,
        &QPushButton::clicked,
        this,
        &StationListPage::loadStations);


    header->addWidget(
        title,
        1);

    header->addWidget(
        m_limitCombo);

    header->addWidget(
        refreshBtn);


    headerCardLayout->addLayout(
        header);


    // ========================================================================
    // 状态提示
    // ========================================================================
    m_tip =
        new QLabel(
            headerCard);

    m_tip->setObjectName(
        QStringLiteral(
            "stationTip"));

    m_tip->setWordWrap(
        true);


    headerCardLayout->addWidget(
        m_tip);


    // ========================================================================
    // 四态 Stack
    // ========================================================================
    m_stack =
        new QStackedWidget(
            this);

    m_stack->setObjectName(
        QStringLiteral(
            "stationStack"));


    // ========================================================================
    // 页0：加载中
    // ========================================================================
    auto *loadingPage =
        new QFrame;

    loadingPage->setObjectName(
        QStringLiteral(
            "stationStatePage"));


    {
        auto *layout =
            new QVBoxLayout(
                loadingPage);

        layout->setObjectName(
            QStringLiteral(
                "stationLoadingLayout"));

        layout->setAlignment(
            Qt::AlignCenter);

        layout->setSpacing(
            12);


        auto *label =
            new QLabel(
                QStringLiteral(
                    "加载中…"),
                loadingPage);

        label->setObjectName(
            QStringLiteral(
                "stationStateLabel"));

        label->setAlignment(
            Qt::AlignCenter);


        layout->addWidget(
            label);
    }


    m_stack->addWidget(
        loadingPage);


    // ========================================================================
    // 页1：内容
    // ========================================================================
    auto *contentPage =
        new QWidget;

    contentPage->setObjectName(
        QStringLiteral(
            "stationContentPage"));


    {
        auto *layout =
            new QVBoxLayout(
                contentPage);

        layout->setContentsMargins(
            0,
            0,
            0,
            0);


        auto *scroll =
            new QScrollArea(
                contentPage);

        scroll->setObjectName(
            QStringLiteral(
                "stationScrollArea"));

        scroll->setWidgetResizable(
            true);

        scroll->setFrameShape(
            QFrame::NoFrame);

        scroll->setHorizontalScrollBarPolicy(
            Qt::ScrollBarAlwaysOff);


        auto *container =
            new QWidget;

        container->setObjectName(
            QStringLiteral(
                "stationListContainer"));


        m_listLayout =
            new QVBoxLayout(
                container);

        m_listLayout->setContentsMargins(
            2,
            2,
            2,
            12);

        m_listLayout->setSpacing(
            12);


        scroll->setWidget(
            container);

        layout->addWidget(
            scroll);
    }


    m_stack->addWidget(
        contentPage);


    // ========================================================================
    // 页2：空数据
    // ========================================================================
    auto *emptyPage =
        new QFrame;

    emptyPage->setObjectName(
        QStringLiteral(
            "stationStatePage"));


    {
        auto *layout =
            new QVBoxLayout(
                emptyPage);

        layout->setObjectName(
            QStringLiteral(
                "stationEmptyLayout"));

        layout->setAlignment(
            Qt::AlignCenter);

        layout->setSpacing(
            12);


        auto *label =
            new QLabel(
                QStringLiteral(
                    "暂无充电站"),
                emptyPage);

        label->setObjectName(
            QStringLiteral(
                "stationStateLabel"));

        label->setAlignment(
            Qt::AlignCenter);


        auto *button =
            new QPushButton(
                QStringLiteral(
                    "刷新"),
                emptyPage);

        button->setObjectName(
            QStringLiteral(
                "stationStateButton"));

        button->setCursor(
            Qt::PointingHandCursor);


        connect(
            button,
            &QPushButton::clicked,
            this,
            &StationListPage::loadStations);


        layout->addWidget(
            label);

        layout->addWidget(
            button,
            0,
            Qt::AlignHCenter);
    }


    m_stack->addWidget(
        emptyPage);


    // ========================================================================
    // 页3：错误
    // ========================================================================
    auto *errorPage =
        new QFrame;

    errorPage->setObjectName(
        QStringLiteral(
            "stationStatePage"));


    {
        auto *layout =
            new QVBoxLayout(
                errorPage);

        layout->setObjectName(
            QStringLiteral(
                "stationErrorLayout"));

        layout->setAlignment(
            Qt::AlignCenter);

        layout->setSpacing(
            12);


        auto *label =
            new QLabel(
                QStringLiteral(
                    "加载失败，请检查网络后重试"),
                errorPage);

        label->setObjectName(
            QStringLiteral(
                "stationErrorLabel"));

        label->setAlignment(
            Qt::AlignCenter);

        label->setWordWrap(
            true);


        auto *button =
            new QPushButton(
                QStringLiteral(
                    "重试"),
                errorPage);

        button->setObjectName(
            QStringLiteral(
                "stationStateButton"));

        button->setCursor(
            Qt::PointingHandCursor);


        connect(
            button,
            &QPushButton::clicked,
            this,
            &StationListPage::loadStations);


        layout->addWidget(
            label);

        layout->addWidget(
            button,
            0,
            Qt::AlignHCenter);
    }


    m_stack->addWidget(
        errorPage);


    // ========================================================================
    // 地图卡片
    // 地图只负责展示，站点列表四态继续由下方 m_stack 管理。
    // ========================================================================
    auto *mapCard =
        new QFrame(this);

    mapCard->setObjectName(
        QStringLiteral(
            "stationMapCard"));

    auto *mapCardLayout =
        new QVBoxLayout(mapCard);

    mapCardLayout->setObjectName(
        QStringLiteral(
            "stationMapCardLayout"));

    mapCardLayout->setContentsMargins(
        0,
        0,
        0,
        0);

    mapCardLayout->setSpacing(0);


    auto *mapHeader =
        new QHBoxLayout;

    mapHeader->setObjectName(
        QStringLiteral(
            "stationMapHeaderLayout"));


    auto *mapTitle =
        new QLabel(
            QStringLiteral(
                "附近地图"),
            mapCard);

    mapTitle->setObjectName(
        QStringLiteral(
            "stationMapTitle"));
    mapTitle->hide();


    auto *mapHint =
        new QLabel(
            QStringLiteral(
                "当前位置与附近站点"),
            mapCard);

    mapHint->setObjectName(
        QStringLiteral(
            "stationMapHint"));
    mapHint->hide();


    mapHeader->addWidget(
        mapTitle);

    mapHeader->addStretch();

    mapHeader->addWidget(
        mapHint);


    mapCardLayout->addLayout(
        mapHeader);

    m_mapWidget->setObjectName(
        QStringLiteral(
            "stationMapWidget"));

    m_mapWidget->setMinimumHeight(
        220);

    m_mapWidget->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Expanding);

    mapCard->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Expanding);

    mapCardLayout->addWidget(
        m_mapWidget,
        1);


    // ========================================================================
    // 总布局
    // ========================================================================
    auto *layout =
        new QVBoxLayout(this);

    layout->setObjectName(
        QStringLiteral(
            "stationPageLayout"));

    layout->setContentsMargins(
        14,
        14,
        14,
        14);

    layout->setSpacing(
        12);


    layout->addWidget(
        headerCard);

    // 首页顶部只保留 MainWindow 的地址定位行；站点标题/筛选行下移，
    // 避免地图首屏被第二行控件挤压。
    headerCard->hide();

    layout->addWidget(
        mapCard,
        5);

    m_stack->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Expanding);

    m_stack->setMinimumHeight(
        320);

    layout->addWidget(
        mapCard,
        3);

    m_stack->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Expanding);

    m_stack->setMinimumHeight(
        320);

    layout->addWidget(
        m_stack,
        3);


    m_stack->setCurrentIndex(
        0);


    // ========================================================================
    // 最新 main：断线重连成功后自动刷新
    // ========================================================================
    connect(
        m_net,
        &NetClient::reconnected,
        this,
        &StationListPage::loadStations);


    applyResponsiveStyle();
}


// ============================================================================
// Resize
// ============================================================================
void StationListPage::resizeEvent(
    QResizeEvent *event)
{
    QWidget::resizeEvent(
        event);

    applyResponsiveStyle();
}


// ============================================================================
// UI
// ============================================================================
void StationListPage::applyResponsiveStyle()
{
    QWidget *scaleBase =
        window()
            ? window()
            : this;


    const int titleFont =
        scaledUi(
            scaleBase,
            18);

    const int normalFont =
        scaledUi(
            scaleBase,
            14);

    const int smallFont =
        scaledUi(
            scaleBase,
            12);

    const int stateFont =
        scaledUi(
            scaleBase,
            15);

    const int mapTitleFont =
        scaledUi(
            scaleBase,
            14);

    const int buttonFont =
        scaledUi(
            scaleBase,
            13);

    const int cardRadius =
        scaledUi(
            scaleBase,
            18);

    const int smallRadius =
        scaledUi(
            scaleBase,
            10);


    setStyleSheet(
        QStringLiteral(

            "QWidget#stationListPage{"
            "background:transparent;"
            "color:#202824;"
            "}"

            "QFrame#stationHeaderCard{"
            "background:#FFFFFF;"
            "border:1px solid #E7E3DA;"
            "border-radius:%1px;"
            "}"

            "QLabel#stationTitle{"
            "background:transparent;"
            "color:#202824;"
            "font-size:%2px;"
            "font-weight:800;"
            "}"

            "QLabel#stationTip{"
            "background:transparent;"
            "color:#7A837E;"
            "font-size:%3px;"
            "}"

            "QComboBox#stationLimitCombo{"
            "background:#FAF8F3;"
            "color:#315B4D;"
            "border:1px solid #E1DDD4;"
            "border-radius:%4px;"
            "padding:7px 10px;"
            "font-size:%5px;"
            "font-weight:600;"
            "}"

            "QComboBox#stationLimitCombo:hover{"
            "border-color:#C9D8CF;"
            "}"

            "QComboBox#stationLimitCombo:focus{"
            "border:1px solid #315B4D;"
            "}"

            "QComboBox#stationLimitCombo QAbstractItemView{"
            "background:#FFFFFF;"
            "color:#202824;"
            "border:1px solid #E7E3DA;"
            "selection-background-color:#E9F0EC;"
            "selection-color:#315B4D;"
            "outline:0;"
            "}"

            "QPushButton#stationRefreshButton{"
            "background:#315B4D;"
            "color:#FFFFFF;"
            "border:none;"
            "border-radius:%4px;"
            "font-size:%5px;"
            "font-weight:700;"
            "padding:8px 14px;"
            "}"

            "QPushButton#stationRefreshButton:hover{"
            "background:#284C41;"
            "}"

            "QFrame#stationMapCard{"
            "background:transparent;"
            "border:none;"
            "border-radius:%1px;"
            "}"

            "QLabel#stationMapTitle{"
            "background:transparent;"
            "color:#202824;"
            "font-size:%8px;"
            "font-weight:800;"
            "}"

            "QLabel#stationMapHint{"
            "background:transparent;"
            "color:#7A837E;"
            "font-size:%3px;"
            "}"

            "AmapWidget#stationMapWidget{"
            "background:#FAF8F3;"
            "border:none;"
            "}"

            "QStackedWidget#stationStack{"
            "background:transparent;"
            "border:none;"
            "}"

            "QWidget#stationContentPage{"
            "background:transparent;"
            "}"

            "QScrollArea#stationScrollArea{"
            "background:transparent;"
            "border:none;"
            "}"

            "QWidget#stationListContainer{"
            "background:transparent;"
            "}"

            "QFrame#stationStatePage{"
            "background:#FFFFFF;"
            "border:1px solid #E7E3DA;"
            "border-radius:%1px;"
            "}"

            "QLabel#stationStateLabel{"
            "background:transparent;"
            "color:#7A837E;"
            "font-size:%6px;"
            "}"

            "QLabel#stationErrorLabel{"
            "background:transparent;"
            "color:#C96C66;"
            "font-size:%6px;"
            "}"

            "QPushButton#stationStateButton{"
            "background:#E9F0EC;"
            "color:#315B4D;"
            "border:1px solid #D6E1DA;"
            "border-radius:%4px;"
            "font-size:%7px;"
            "font-weight:700;"
            "padding:8px 16px;"
            "}"

            "QPushButton#stationStateButton:hover{"
            "background:#DFE9E3;"
            "}")

        .arg(
            cardRadius)

        .arg(
            titleFont)

        .arg(
            smallFont)

        .arg(
            smallRadius)

        .arg(
            normalFont)

        .arg(
            stateFont)

        .arg(
            buttonFont)

        .arg(
            mapTitleFont));


    // ========================================================================
    // 页面边距
    // ========================================================================
    if (auto *pageLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "stationPageLayout"))) {

        pageLayout->setContentsMargins(
            scaledUi(scaleBase, 14),
            scaledUi(scaleBase, 14),
            scaledUi(scaleBase, 14),
            scaledUi(scaleBase, 14));

        pageLayout->setSpacing(
            scaledUi(
                scaleBase,
                8));
    }


    // ========================================================================
    // Header 卡
    // ========================================================================
    if (auto *headerCardLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "stationHeaderCardLayout"))) {

        headerCardLayout->setContentsMargins(
            scaledUi(scaleBase, 14),
            scaledUi(scaleBase, 7),
            scaledUi(scaleBase, 14),
            scaledUi(scaleBase, 7));

        headerCardLayout->setSpacing(
            scaledUi(
                scaleBase,
                3));
    }


    if (auto *header =
            findChild<QHBoxLayout *>(
                QStringLiteral(
                    "stationHeaderLayout"))) {

        header->setSpacing(
            scaledUi(
                scaleBase,
                8));
    }


    // ========================================================================
    // 地图卡
    // ========================================================================
    if (auto *mapCardLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "stationMapCardLayout"))) {

        mapCardLayout->setContentsMargins(
            0,
            0,
            0,
            0);

        mapCardLayout->setSpacing(0);
    }


    if (auto *mapHeader =
            findChild<QHBoxLayout *>(
                QStringLiteral(
                    "stationMapHeaderLayout"))) {

        mapHeader->setSpacing(
            scaledUi(
                scaleBase,
                8));
    }


    if (m_mapWidget) {
        m_mapWidget->setMinimumHeight(
            scaledUi(
                scaleBase,
                520));
    }


    if (m_stack) {
        m_stack->setMinimumHeight(
            scaledUi(
                scaleBase,
                320));
    }


    // ========================================================================
    // 列表区域
    // ========================================================================
    if (m_listLayout) {

        m_listLayout->setContentsMargins(
            scaledUi(scaleBase, 2),
            scaledUi(scaleBase, 2),
            scaledUi(scaleBase, 2),
            scaledUi(scaleBase, 12));

        m_listLayout->setSpacing(
            scaledUi(
                scaleBase,
                12));
    }


    // ========================================================================
    // 状态页
    // ========================================================================
    if (auto *loadingLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "stationLoadingLayout"))) {

        loadingLayout->setContentsMargins(
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 18));

        loadingLayout->setSpacing(
            scaledUi(
                scaleBase,
                12));
    }


    if (auto *emptyLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "stationEmptyLayout"))) {

        emptyLayout->setContentsMargins(
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 18));

        emptyLayout->setSpacing(
            scaledUi(
                scaleBase,
                12));
    }


    if (auto *errorLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "stationErrorLayout"))) {

        errorLayout->setContentsMargins(
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 18));

        errorLayout->setSpacing(
            scaledUi(
                scaleBase,
                12));
    }
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


    m_latitude =
        lat;

    m_longitude =
        lng;

    m_hasLocation =
        true;


    m_mapWidget->setStations(
        QJsonArray());


    m_mapWidget->setUserLocation(
        lat,
        lng);


    // 最新 main：
    // 定位位置发生变化后清除旧地址缓存
    m_cachedList =
        QJsonArray();


    if (isVisible()) {

        m_loaded =
            true;

        loadStations();

    } else {

        m_loaded =
            false;
    }
}


// =============================================================================
// 页面显示
// =============================================================================
void StationListPage::showEvent(
    QShowEvent *event)
{
    QWidget::showEvent(
        event);


    if (!m_loaded) {

        m_loaded =
            true;

        loadStations();
    }
}


// =============================================================================
// 清空站点卡片
// =============================================================================
void StationListPage::clearList()
{
    while (QLayoutItem *item =
               m_listLayout->takeAt(
                   0)) {

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
    m_stack->setCurrentIndex(
        0);

    m_tip->clear();


    // -------------------------------------------------------------------------
    // station_list：
    // data 中传 lat / lng
    // -------------------------------------------------------------------------
    QJsonObject data;


    data.insert(
        QStringLiteral(
            "lat"),
        m_latitude);


    data.insert(
        QStringLiteral(
            "lng"),
        m_longitude);


    const QJsonObject resp =
        m_net->request(
            Protocol::makeRequest(
                Protocol::MsgType::StationList,
                data));


    const int code =
        resp.value(
                QStringLiteral(
                    "code"))
            .toInt();


    const QJsonArray list =
        resp.value(
                QStringLiteral(
                    "data"))
            .toObject()
            .value(
                QStringLiteral(
                    "list"))
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


        m_mapWidget->setStations(
            QJsonArray());


        m_tip->setText(
            QStringLiteral(
                "附近暂无充电站"));


        m_stack->setCurrentIndex(
            2);

        return;
    }


    // -------------------------------------------------------------------------
    // 成功：排序并缓存
    // -------------------------------------------------------------------------
    m_cachedList =
        sortStations(
            list);


    renderStations();
}


// =============================================================================
// NO.4：按距离升序稳定排序 + 去重 + distance 兜底
// =============================================================================
QJsonArray StationListPage::sortStations(
    const QJsonArray &raw) const
{
    constexpr double kEarthRadiusKm =
        6371.0;


    const auto toRad =
        [](double deg) {

            return deg *
                   M_PI /
                   180.0;
        };


    QVector<QJsonObject> items;

    items.reserve(
        raw.size());


    QSet<qint64> seen;


    for (const QJsonValue &value :
         raw) {

        QJsonObject station =
            value.toObject();


        const qint64 id =
            station.value(
                       QStringLiteral(
                           "id"))
                .toVariant()
                .toLongLong();


        if (seen.contains(
                id)) {

            continue;
        }


        seen.insert(
            id);


        double distance =
            station.value(
                       QStringLiteral(
                           "distance"))
                .toDouble(
                    -1.0);


        const bool needCalc =
            !station.contains(
                QStringLiteral(
                    "distance")) ||
            distance < 0.0;


        if (needCalc &&
            m_hasLocation &&
            station.contains(
                QStringLiteral(
                    "latitude")) &&
            station.contains(
                QStringLiteral(
                    "longitude"))) {

            const double stationLat =
                station.value(
                           QStringLiteral(
                               "latitude"))
                    .toDouble();


            const double stationLng =
                station.value(
                           QStringLiteral(
                               "longitude"))
                    .toDouble();


            const double dLat =
                toRad(
                    stationLat -
                    m_latitude);


            const double dLng =
                toRad(
                    stationLng -
                    m_longitude);


            const double a =
                std::sin(
                    dLat / 2) *
                    std::sin(
                        dLat / 2) +
                std::cos(
                    toRad(
                        m_latitude)) *
                    std::cos(
                        toRad(
                            stationLat)) *
                    std::sin(
                        dLng / 2) *
                    std::sin(
                        dLng / 2);


            distance =
                2 *
                kEarthRadiusKm *
                std::atan2(
                    std::sqrt(
                        a),
                    std::sqrt(
                        1 - a));


            station[
                QStringLiteral(
                    "distance")] =
                distance;

        } else if (
            distance < 0.0) {

            distance =
                0.0;


            station[
                QStringLiteral(
                    "distance")] =
                distance;
        }


        items.append(
            station);
    }


    std::stable_sort(
        items.begin(),
        items.end(),
        [](const QJsonObject &a,
           const QJsonObject &b) {

            const double distanceA =
                a.value(
                     QStringLiteral(
                         "distance"))
                    .toDouble();


            const double distanceB =
                b.value(
                     QStringLiteral(
                         "distance"))
                    .toDouble();


            if (!qFuzzyCompare(
                    distanceA + 1.0,
                    distanceB + 1.0)) {

                return distanceA <
                       distanceB;
            }


            const int idleA =
                a.value(
                     QStringLiteral(
                         "idle"))
                    .toInt();


            const int idleB =
                b.value(
                     QStringLiteral(
                         "idle"))
                    .toInt();


            if (idleA !=
                idleB) {

                return idleA >
                       idleB;
            }


            return a.value(
                        QStringLiteral(
                            "name"))
                       .toString() <
                   b.value(
                        QStringLiteral(
                            "name"))
                       .toString();
        });


    QJsonArray sorted;


    for (const QJsonObject &object :
         items) {

        sorted.append(
            object);
    }


    return sorted;
}


// =============================================================================
// NO.4：按当前 5 / 10 限制渲染
// =============================================================================
void StationListPage::renderStations()
{
    const int total =
        m_cachedList.size();


    if (total == 0) {

        m_mapWidget->setStations(
            QJsonArray());

        m_tip->setText(
            QStringLiteral(
                "附近暂无充电站"));


        m_stack->setCurrentIndex(
            2);

        return;
    }


    const int shown =
        qMin(
            m_limit,
            total);


    QJsonArray view;


    for (int i = 0;
         i < shown;
         ++i) {

        view.append(
            m_cachedList.at(
                i));
    }


    m_mapWidget->setStations(
        view);


    buildCards(
        view);


    m_tip->setText(
        QStringLiteral(
            "附近共 %1 个，按距离显示最近 %2 个")
            .arg(
                total)
            .arg(
                shown));


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

        if (!value.isObject()) {

            continue;
        }


        auto *card =
            new StationCardWidget(
                value.toObject(),
                this);


        // ---------------------------------------------------------------------
        // 查看充电桩
        // ---------------------------------------------------------------------
        connect(
            card,
            &StationCardWidget::stationSelected,
            this,
            [this](
                qint64 stationId,
                const QString &name) {

                if (stationId <= 0) {

                    return;
                }


                emit stationSelected(
                    stationId,
                    name);
            });


        // ---------------------------------------------------------------------
        // 最新 main：
        // 使用 RouteRequest 进入导航
        // 这部分业务逻辑完整保留
        // ---------------------------------------------------------------------
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

                Q_UNUSED(
                    stationId);


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


                RouteRequest request;

                request.requestId =
                    ++m_nextRouteRequestId;

                request.fromLat =
                    m_latitude;

                request.fromLng =
                    m_longitude;

                request.toName =
                    name;

                request.toLat =
                    targetLat;

                request.toLng =
                    targetLng;

                request.distance =
                    distance;

                request.mode =
                    QStringLiteral(
                        "driving");


                if (!request.isValid()) {

                    return;
                }


                emit navigationRequested(
                    request);
            });


        m_listLayout->addWidget(
            card);
    }


    m_listLayout->addStretch();
}
