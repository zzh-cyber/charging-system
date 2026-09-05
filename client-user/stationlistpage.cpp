#include "stationlistpage.h"

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
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>


// ============================================================================
// 构造函数
// ============================================================================
StationListPage::StationListPage(
    NetClient *net,
    QWidget *parent)
    : QWidget(parent)
    , m_net(net)
{
    setObjectName(
        QStringLiteral(
            "stationListPage"));


    // ========================================================================
    // 顶部信息卡
    // ========================================================================
    auto *headerCard =
        new QFrame(this);

    headerCard->setObjectName(
        QStringLiteral(
            "stationHeaderCard"));

    UiTheme::applyCardShadow(
        headerCard,
        16,
        4);


    auto *headerLayout =
        new QVBoxLayout(
            headerCard);

    headerLayout->setObjectName(
        QStringLiteral(
            "stationHeaderLayout"));

    headerLayout->setContentsMargins(
        18,
        16,
        18,
        14);

    headerLayout->setSpacing(
        8);


    // ========================================================================
    // 标题行
    // ========================================================================
    auto *titleRow =
        new QHBoxLayout;

    titleRow->setSpacing(
        10);


    auto *title =
        new QLabel(
            QStringLiteral(
                "附近充电站"),
            headerCard);

    title->setObjectName(
        QStringLiteral(
            "stationTitle"));


    auto *sortBadge =
        new QLabel(
            QStringLiteral(
                "按距离优先"),
            headerCard);

    sortBadge->setObjectName(
        QStringLiteral(
            "stationSortBadge"));

    sortBadge->setAlignment(
        Qt::AlignCenter);


    titleRow->addWidget(
        title);

    titleRow->addWidget(
        sortBadge);

    titleRow->addStretch();


    headerLayout->addLayout(
        titleRow);


    // ========================================================================
    // 副标题
    // ========================================================================
    auto *subtitle =
        new QLabel(
            QStringLiteral(
                "查找当前位置附近的充电站，"
                "优先展示距离更近的站点"),
            headerCard);

    subtitle->setObjectName(
        QStringLiteral(
            "stationSubtitle"));

    subtitle->setWordWrap(
        true);


    headerLayout->addWidget(
        subtitle);


    // ========================================================================
    // 操作区
    // ========================================================================
    auto *controlRow =
        new QHBoxLayout;

    controlRow->setSpacing(
        8);


    // ------------------------------------------------------------------------
    // 动态提示
    // ------------------------------------------------------------------------
    m_tip =
        new QLabel(
            headerCard);

    m_tip->setObjectName(
        QStringLiteral(
            "stationTip"));

    m_tip->setWordWrap(
        true);


    // ------------------------------------------------------------------------
    // 最近 5 / 10 个
    // ------------------------------------------------------------------------
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

            // 已有缓存则即时重渲染
            if (!m_cachedList.isEmpty())
                renderStations();
        });


    // ------------------------------------------------------------------------
    // 刷新按钮
    // ------------------------------------------------------------------------
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


    controlRow->addWidget(
        m_tip,
        1);

    controlRow->addWidget(
        m_limitCombo);

    controlRow->addWidget(
        refreshBtn);


    headerLayout->addLayout(
        controlRow);


    // ========================================================================
    // 四态 Stack
    // ========================================================================
    m_stack =
        new QStackedWidget(this);

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


        auto *stateTitle =
            new QLabel(
                QStringLiteral(
                    "正在加载附近充电站"),
                loadingPage);

        stateTitle->setObjectName(
            QStringLiteral(
                "stationStateTitle"));

        stateTitle->setAlignment(
            Qt::AlignCenter);


        auto *stateDesc =
            new QLabel(
                QStringLiteral(
                    "请稍候，我们正在获取站点信息"),
                loadingPage);

        stateDesc->setObjectName(
            QStringLiteral(
                "stationStateDescription"));

        stateDesc->setAlignment(
            Qt::AlignCenter);


        layout->addWidget(
            stateTitle);

        layout->addWidget(
            stateDesc);
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
            6,
            8,
            6,
            14);

        m_listLayout->setSpacing(
            14);


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


        auto *stateTitle =
            new QLabel(
                QStringLiteral(
                    "附近暂无充电站"),
                emptyPage);

        stateTitle->setObjectName(
            QStringLiteral(
                "stationStateTitle"));

        stateTitle->setAlignment(
            Qt::AlignCenter);


        auto *stateDesc =
            new QLabel(
                QStringLiteral(
                    "可以重新定位，或稍后刷新站点列表"),
                emptyPage);

        stateDesc->setObjectName(
            QStringLiteral(
                "stationStateDescription"));

        stateDesc->setAlignment(
            Qt::AlignCenter);

        stateDesc->setWordWrap(
            true);


        auto *button =
            new QPushButton(
                QStringLiteral(
                    "重新刷新"),
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
            stateTitle);

        layout->addWidget(
            stateDesc);

        layout->addSpacing(
            4);

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


        auto *stateTitle =
            new QLabel(
                QStringLiteral(
                    "站点加载失败"),
                errorPage);

        stateTitle->setObjectName(
            QStringLiteral(
                "stationErrorTitle"));

        stateTitle->setAlignment(
            Qt::AlignCenter);


        auto *stateDesc =
            new QLabel(
                QStringLiteral(
                    "请检查网络连接后重新尝试"),
                errorPage);

        stateDesc->setObjectName(
            QStringLiteral(
                "stationStateDescription"));

        stateDesc->setAlignment(
            Qt::AlignCenter);


        auto *button =
            new QPushButton(
                QStringLiteral(
                    "重新加载"),
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
            stateTitle);

        layout->addWidget(
            stateDesc);

        layout->addSpacing(
            4);

        layout->addWidget(
            button,
            0,
            Qt::AlignHCenter);
    }

    m_stack->addWidget(
        errorPage);


    // ========================================================================
    // 总布局
    // ========================================================================
    auto *mainLayout =
        new QVBoxLayout(this);

    mainLayout->setObjectName(
        QStringLiteral(
            "stationPageLayout"));

    mainLayout->setContentsMargins(
        0,
        0,
        0,
        0);

    mainLayout->setSpacing(
        10);


    mainLayout->addWidget(
        headerCard);

    mainLayout->addWidget(
        m_stack,
        1);


    m_stack->setCurrentIndex(
        0);


    // ========================================================================
    // NetClient 断线重连后自动刷新
    // ========================================================================
    connect(
        m_net,
        &NetClient::reconnected,
        this,
        &StationListPage::loadStations);


    applyResponsiveStyle();
}


// ============================================================================
// 窗口变化
// ============================================================================
void StationListPage::resizeEvent(
    QResizeEvent *event)
{
    QWidget::resizeEvent(
        event);

    applyResponsiveStyle();
}


// ============================================================================
// 响应式样式
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
            22);

    const int subtitleFont =
        scaledUi(
            scaleBase,
            12);

    const int stateTitleFont =
        scaledUi(
            scaleBase,
            17);

    const int stateDescFont =
        scaledUi(
            scaleBase,
            12);

    const int controlFont =
        scaledUi(
            scaleBase,
            13);

    const int tipFont =
        scaledUi(
            scaleBase,
            11);

    const int cardRadius =
        scaledUi(
            scaleBase,
            18);

    const int controlRadius =
        scaledUi(
            scaleBase,
            10);

    const int badgeRadius =
        scaledUi(
            scaleBase,
            9);


    setStyleSheet(
        QStringLiteral(

            // ================================================================
            // 页面
            // ================================================================
            "QWidget#stationListPage{"
            "background:transparent;"
            "color:%1;"
            "}"

            "QWidget#stationContentPage,"
            "QWidget#stationListContainer{"
            "background:transparent;"
            "}"

            "QStackedWidget#stationStack{"
            "background:transparent;"
            "border:none;"
            "}"

            // ================================================================
            // 顶部信息卡
            // ================================================================
            "QFrame#stationHeaderCard{"
            "background:%2;"
            "border:1px solid %3;"
            "border-radius:%4px;"
            "}"

            "QLabel#stationTitle{"
            "background:transparent;"
            "color:%1;"
            "font-size:%5px;"
            "font-weight:800;"
            "}"

            "QLabel#stationSubtitle{"
            "background:transparent;"
            "color:%6;"
            "font-size:%7px;"
            "}"

            "QLabel#stationSortBadge{"
            "background:%8;"
            "color:%9;"
            "border:none;"
            "border-radius:%10px;"
            "font-size:%11px;"
            "font-weight:600;"
            "padding:4px 9px;"
            "}"

            "QLabel#stationTip{"
            "background:transparent;"
            "color:%6;"
            "font-size:%11px;"
            "}"

            // ================================================================
            // 数量选择器
            // ================================================================
            "QComboBox#stationLimitCombo{"
            "background:%12;"
            "color:%1;"
            "border:1px solid %3;"
            "border-radius:%13px;"
            "font-size:%14px;"
            "padding:7px 10px;"
            "}"

            "QComboBox#stationLimitCombo:hover{"
            "border-color:%9;"
            "}"

            "QComboBox#stationLimitCombo:focus{"
            "border-color:%9;"
            "}"

            // ================================================================
            // 刷新按钮
            // ================================================================
            "QPushButton#stationRefreshButton{"
            "background:%9;"
            "color:#FFFFFF;"
            "border:none;"
            "border-radius:%13px;"
            "font-size:%14px;"
            "font-weight:700;"
            "padding:8px 15px;"
            "}"

            "QPushButton#stationRefreshButton:hover{"
            "background:%15;"
            "}"

            // ================================================================
            // 滚动区
            // ================================================================
            "QScrollArea#stationScrollArea{"
            "background:transparent;"
            "border:none;"
            "}"

            // ================================================================
            // 状态页
            // ================================================================
            "QFrame#stationStatePage{"
            "background:%12;"
            "border:1px solid %3;"
            "border-radius:%4px;"
            "}"

            "QLabel#stationStateTitle{"
            "background:transparent;"
            "color:%1;"
            "font-size:%16px;"
            "font-weight:700;"
            "}"

            "QLabel#stationErrorTitle{"
            "background:transparent;"
            "color:%17;"
            "font-size:%16px;"
            "font-weight:700;"
            "}"

            "QLabel#stationStateDescription{"
            "background:transparent;"
            "color:%6;"
            "font-size:%18px;"
            "}"

            "QPushButton#stationStateButton{"
            "background:%8;"
            "color:%9;"
            "border:1px solid %3;"
            "border-radius:%13px;"
            "font-size:%14px;"
            "font-weight:600;"
            "padding:8px 18px;"
            "}"

            "QPushButton#stationStateButton:hover{"
            "background:#DFE9E3;"
            "}")

        .arg(
            UiTheme::textPrimary())       // %1

        .arg(
            UiTheme::surface())           // %2

        .arg(
            UiTheme::border())            // %3

        .arg(
            cardRadius)                   // %4

        .arg(
            titleFont)                    // %5

        .arg(
            UiTheme::textSecondary())     // %6

        .arg(
            subtitleFont)                 // %7

        .arg(
            UiTheme::primarySoft())       // %8

        .arg(
            UiTheme::primary())           // %9

        .arg(
            badgeRadius)                  // %10

        .arg(
            tipFont)                      // %11

        .arg(
            UiTheme::surfaceSoft())       // %12

        .arg(
            controlRadius)                // %13

        .arg(
            controlFont)                  // %14

        .arg(
            UiTheme::primaryHover())      // %15

        .arg(
            stateTitleFont)               // %16

        .arg(
            UiTheme::danger())            // %17

        .arg(
            stateDescFont));              // %18


    // ========================================================================
    // 顶部信息卡间距
    // ========================================================================
    if (auto *headerLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "stationHeaderLayout"))) {

        headerLayout->setContentsMargins(
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 16),
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 14));

        headerLayout->setSpacing(
            scaledUi(
                scaleBase,
                8));
    }


    // ========================================================================
    // 站点列表间距
    // ========================================================================
    if (m_listLayout) {

        m_listLayout->setContentsMargins(
            scaledUi(scaleBase, 6),
            scaledUi(scaleBase, 8),
            scaledUi(scaleBase, 6),
            scaledUi(scaleBase, 14));

        m_listLayout->setSpacing(
            scaledUi(
                scaleBase,
                14));
    }


    // ========================================================================
    // 页面间距
    // ========================================================================
    if (auto *mainLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "stationPageLayout"))) {

        mainLayout->setSpacing(
            scaledUi(
                scaleBase,
                10));
    }


    // ========================================================================
    // 空 / 错误状态间距
    // ========================================================================
    if (auto *emptyLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "stationEmptyLayout"))) {

        emptyLayout->setContentsMargins(
            scaledUi(scaleBase, 24),
            scaledUi(scaleBase, 24),
            scaledUi(scaleBase, 24),
            scaledUi(scaleBase, 24));

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
            scaledUi(scaleBase, 24),
            scaledUi(scaleBase, 24),
            scaledUi(scaleBase, 24),
            scaledUi(scaleBase, 24));

        errorLayout->setSpacing(
            scaledUi(
                scaleBase,
                12));
    }
}


// ============================================================================
// 设置用户当前位置
// ============================================================================
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


    // 定位变化后，旧地址缓存不能继续使用
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


// ============================================================================
// 页面显示
// ============================================================================
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


// ============================================================================
// 清空站点卡片
// ============================================================================
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


// ============================================================================
// 加载附近充电站
// ============================================================================
void StationListPage::loadStations()
{
    // ------------------------------------------------------------------------
    // 尚未定位
    // ------------------------------------------------------------------------
    if (!m_hasLocation) {

        m_tip->setText(
            QStringLiteral(
                "请先在上方输入地址并完成定位"));

        m_stack->setCurrentIndex(
            2);

        return;
    }


    // loading
    m_stack->setCurrentIndex(
        0);

    m_tip->setText(
        QStringLiteral(
            "正在获取附近站点…"));


    // ------------------------------------------------------------------------
    // station_list 协议保持不变
    // ------------------------------------------------------------------------
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


    // ------------------------------------------------------------------------
    // 请求失败
    // 有缓存 -> 展示缓存
    // 无缓存 -> error
    // ------------------------------------------------------------------------
    if (code != Protocol::Ok) {

        if (!m_cachedList.isEmpty()) {

            renderStations();

            m_tip->setText(
                QStringLiteral(
                    "网络异常，当前展示最近一次缓存数据"));

            return;
        }


        m_tip->setText(
            QStringLiteral(
                "站点数据获取失败"));

        m_stack->setCurrentIndex(
            3);

        return;
    }


    // ------------------------------------------------------------------------
    // 空数据
    // ------------------------------------------------------------------------
    if (list.isEmpty()) {

        m_cachedList =
            QJsonArray();

        m_tip->setText(
            QStringLiteral(
                "当前位置附近暂未找到充电站"));

        m_stack->setCurrentIndex(
            2);

        return;
    }


    // ------------------------------------------------------------------------
    // 成功
    // ------------------------------------------------------------------------
    m_cachedList =
        sortStations(
            list);

    renderStations();
}


// ============================================================================
// NO.4：按距离升序稳定排序 + 去重 + distance 兜底
// ============================================================================
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


        // 同一站点只保留首个
        if (seen.contains(id))
            continue;


        seen.insert(
            id);


        // --------------------------------------------------------------------
        // distance 兜底
        // --------------------------------------------------------------------
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
                    dLat / 2.0) *
                    std::sin(
                        dLat / 2.0)
                +
                std::cos(
                    toRad(
                        m_latitude))
                    *
                    std::cos(
                        toRad(
                            stationLat))
                    *
                    std::sin(
                        dLng / 2.0)
                    *
                    std::sin(
                        dLng / 2.0);


            distance =
                2.0 *
                kEarthRadiusKm *
                std::atan2(
                    std::sqrt(a),
                    std::sqrt(
                        1.0 - a));


            station[
                QStringLiteral(
                    "distance")] =
                distance;

        } else if (distance < 0.0) {

            // 无法兜底时置 0，避免负值污染排序
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
        [](
            const QJsonObject &a,
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


            if (idleA != idleB)
                return idleA > idleB;


            return
                a.value(
                     QStringLiteral(
                         "name"))
                    .toString()
                <
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


// ============================================================================
// 根据 5 / 10 条限制渲染
// ============================================================================
void StationListPage::renderStations()
{
    const int total =
        m_cachedList.size();


    if (total == 0) {

        m_tip->setText(
            QStringLiteral(
                "当前位置附近暂未找到充电站"));

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
            m_cachedList.at(i));
    }


    buildCards(
        view);


    m_tip->setText(
        QStringLiteral(
            "附近共 %1 个站点 · 当前显示最近 %2 个")
            .arg(total)
            .arg(shown));


    m_stack->setCurrentIndex(
        1);
}


// ============================================================================
// 创建站点卡片
// ============================================================================
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


        // --------------------------------------------------------------------
        // 查看该站充电桩
        // --------------------------------------------------------------------
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


        // --------------------------------------------------------------------
        // 一键导航
        // --------------------------------------------------------------------
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
