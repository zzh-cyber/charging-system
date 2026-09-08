#include "stationlistpage.h"

#include "netclient.h"
#include "protocol.h"
#include "stationcardwidget.h"
#include "uitheme.h"
#include "windowhelper.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSet>
#include <QShowEvent>
#include <QSize>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QVector>

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


    // =========================================================================
    // 页头卡片
    // =========================================================================
    auto *headerCard =
        new QFrame(this);

    headerCard->setObjectName(
        QStringLiteral(
            "stationHeaderCard"));

    headerCard->setAttribute(
        Qt::WA_StyledBackground,
        true);


    UiTheme::applyCardShadow(
        headerCard,
        22,
        5);


    auto *headerCardLayout =
        new QVBoxLayout(
            headerCard);

    headerCardLayout->setObjectName(
        QStringLiteral(
            "stationHeaderCardLayout"));

    headerCardLayout->setContentsMargins(
        16,
        15,
        16,
        14);

    headerCardLayout->setSpacing(
        10);


    // =========================================================================
    // 第一行：
    // 图标 + 标题 + 5/10 条选择 + 刷新
    // =========================================================================
    auto *header =
        new QHBoxLayout;

    header->setObjectName(
        QStringLiteral(
            "stationHeaderLayout"));

    header->setSpacing(
        8);


    // -------------------------------------------------------------------------
    // 标题图标
    // -------------------------------------------------------------------------
    auto *titleIcon =
        new QLabel(
            headerCard);

    titleIcon->setObjectName(
        QStringLiteral(
            "stationTitleIcon"));

    titleIcon->setAlignment(
        Qt::AlignCenter);


    // -------------------------------------------------------------------------
    // 标题
    // -------------------------------------------------------------------------
    auto *title =
        new QLabel(
            QStringLiteral(
                "附近充电站"),
            headerCard);

    title->setObjectName(
        QStringLiteral(
            "stationTitle"));


    // =========================================================================
    // NO.4：
    // 显示最近 5 / 10 个
    //
    // 原业务逻辑不变
    // =========================================================================
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


    // -------------------------------------------------------------------------
    // 刷新按钮
    // -------------------------------------------------------------------------
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


    refreshBtn->setIcon(
        QIcon(
            QStringLiteral(
                ":/icons/refresh.svg")));


    connect(
        refreshBtn,
        &QPushButton::clicked,
        this,
        &StationListPage::loadStations);


    header->addWidget(
        titleIcon);

    header->addWidget(
        title,
        1);

    header->addWidget(
        m_limitCombo);

    header->addWidget(
        refreshBtn);


    headerCardLayout->addLayout(
        header);


    // =========================================================================
    // 状态提示
    // =========================================================================
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


    // =========================================================================
    // 四态 Stack
    // =========================================================================
    m_stack =
        new QStackedWidget(
            this);

    m_stack->setObjectName(
        QStringLiteral(
            "stationStack"));


    // =========================================================================
    // 页0：加载中
    // =========================================================================
    auto *loadingPage =
        new QFrame;

    loadingPage->setObjectName(
        QStringLiteral(
            "stationStatePage"));

    loadingPage->setAttribute(
        Qt::WA_StyledBackground,
        true);


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


    // =========================================================================
    // 页1：站点内容
    // =========================================================================
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


    // =========================================================================
    // 页2：空数据
    // =========================================================================
    auto *emptyPage =
        new QFrame;

    emptyPage->setObjectName(
        QStringLiteral(
            "stationStatePage"));

    emptyPage->setAttribute(
        Qt::WA_StyledBackground,
        true);


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


    // =========================================================================
    // 页3：错误
    // =========================================================================
    auto *errorPage =
        new QFrame;

    errorPage->setObjectName(
        QStringLiteral(
            "stationStatePage"));

    errorPage->setAttribute(
        Qt::WA_StyledBackground,
        true);


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


    // =========================================================================
    // 页面总布局
    //
    // MainWindow 外层已经有 14px 左右留白，
    // 这里不再重复增加横向边距。
    // =========================================================================
    auto *layout =
        new QVBoxLayout(
            this);

    layout->setObjectName(
        QStringLiteral(
            "stationPageLayout"));


    layout->setContentsMargins(
        0,
        0,
        0,
        14);


    layout->setSpacing(
        12);


    layout->addWidget(
        headerCard);


    layout->addWidget(
        m_stack,
        1);


    m_stack->setCurrentIndex(
        0);


    // =========================================================================
    // 最新 main：
    // 断线重连成功后自动刷新
    // =========================================================================
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
// 响应式 UI
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
            21);


    const int normalFont =
        scaledUi(
            scaleBase,
            13);


    const int smallFont =
        scaledUi(
            scaleBase,
            11);


    const int stateFont =
        scaledUi(
            scaleBase,
            14);


    const int buttonFont =
        scaledUi(
            scaleBase,
            12);


    const int cardRadius =
        scaledUi(
            scaleBase,
            22);


    const int controlRadius =
        scaledUi(
            scaleBase,
            13);


    const int controlHeight =
        scaledUi(
            scaleBase,
            40);


    const int iconBoxSize =
        scaledUi(
            scaleBase,
            36);


    const int titleIconSize =
        scaledUi(
            scaleBase,
            19);


    const int refreshIconSize =
        scaledUi(
            scaleBase,
            16);


    // =========================================================================
    // 页面样式
    // =========================================================================
    setStyleSheet(
        QStringLiteral(

            // -----------------------------------------------------------------
            // Root
            // -----------------------------------------------------------------
            "QWidget#stationListPage{"
            "background:transparent;"
            "color:#151C24;"
            "}"


            // -----------------------------------------------------------------
            // Header
            // -----------------------------------------------------------------
            "QFrame#stationHeaderCard{"
            "background:#FFFFFF;"
            "border:1px solid #E7EBE9;"
            "border-radius:%1px;"
            "}"


            "QLabel#stationTitleIcon{"
            "background:#E2F9E7;"
            "border:none;"
            "border-radius:%2px;"
            "}"


            "QLabel#stationTitle{"
            "background:transparent;"
            "color:#151C24;"
            "font-size:%3px;"
            "font-weight:800;"
            "}"


            "QLabel#stationTip{"
            "background:transparent;"
            "color:#7E8893;"
            "font-size:%4px;"
            "}"


            // -----------------------------------------------------------------
            // 最近 5 / 10 个
            // -----------------------------------------------------------------
            "QComboBox#stationLimitCombo{"
            "background:#F7F9F8;"
            "color:#151C24;"
            "border:1px solid #E7EBE9;"
            "border-radius:%5px;"
            "padding:7px 10px;"
            "font-size:%6px;"
            "font-weight:650;"
            "}"


            "QComboBox#stationLimitCombo:hover{"
            "border-color:#D5DDD9;"
            "}"


            "QComboBox#stationLimitCombo:focus{"
            "background:#FFFFFF;"
            "border:1px solid #45D86B;"
            "}"


            "QComboBox#stationLimitCombo::drop-down{"
            "border:none;"
            "width:20px;"
            "}"


            "QComboBox#stationLimitCombo QAbstractItemView{"
            "background:#FFFFFF;"
            "color:#151C24;"
            "border:1px solid #E7EBE9;"
            "selection-background-color:#E2F9E7;"
            "selection-color:#151C24;"
            "outline:none;"
            "padding:4px;"
            "}"


            // -----------------------------------------------------------------
            // 刷新
            // 荧光绿作为首页操作高亮
            // -----------------------------------------------------------------
            "QPushButton#stationRefreshButton{"
            "background:#74EC8B;"
            "color:#171D27;"
            "border:none;"
            "border-radius:%5px;"
            "font-size:%6px;"
            "font-weight:750;"
            "padding:7px 13px;"
            "}"


            "QPushButton#stationRefreshButton:hover{"
            "background:#63E27C;"
            "}"


            "QPushButton#stationRefreshButton:pressed{"
            "background:#52D96E;"
            "}"


            // -----------------------------------------------------------------
            // Stack
            // -----------------------------------------------------------------
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


            "QScrollArea#stationScrollArea > QWidget > QWidget{"
            "background:transparent;"
            "}"


            "QWidget#stationListContainer{"
            "background:transparent;"
            "}"


            // -----------------------------------------------------------------
            // 四态卡
            // -----------------------------------------------------------------
            "QFrame#stationStatePage{"
            "background:#FFFFFF;"
            "border:1px solid #E7EBE9;"
            "border-radius:%1px;"
            "}"


            "QLabel#stationStateLabel{"
            "background:transparent;"
            "color:#7E8893;"
            "font-size:%7px;"
            "font-weight:600;"
            "}"


            "QLabel#stationErrorLabel{"
            "background:transparent;"
            "color:#E26868;"
            "font-size:%7px;"
            "font-weight:600;"
            "}"


            // -----------------------------------------------------------------
            // 空态 / 错误页按钮
            // -----------------------------------------------------------------
            "QPushButton#stationStateButton{"
            "background:#171D27;"
            "color:#FFFFFF;"
            "border:none;"
            "border-radius:%5px;"
            "font-size:%8px;"
            "font-weight:700;"
            "padding:9px 18px;"
            "}"


            "QPushButton#stationStateButton:hover{"
            "background:#252E3A;"
            "}"


            "QPushButton#stationStateButton:pressed{"
            "background:#10151C;"
            "}")

            .arg(
                cardRadius)          // %1

            .arg(
                iconBoxSize / 2)     // %2

            .arg(
                titleFont)           // %3

            .arg(
                smallFont)           // %4

            .arg(
                controlRadius)       // %5

            .arg(
                normalFont)          // %6

            .arg(
                stateFont)           // %7

            .arg(
                buttonFont));        // %8


    // =========================================================================
    // 页面布局
    // =========================================================================
    if (auto *pageLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "stationPageLayout"))) {

        pageLayout->setContentsMargins(
            0,
            0,
            0,
            scaledUi(
                scaleBase,
                14));


        pageLayout->setSpacing(
            scaledUi(
                scaleBase,
                12));
    }


    // =========================================================================
    // Header 卡布局
    // =========================================================================
    if (auto *headerCardLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "stationHeaderCardLayout"))) {

        headerCardLayout->setContentsMargins(
            scaledUi(
                scaleBase,
                16),

            scaledUi(
                scaleBase,
                15),

            scaledUi(
                scaleBase,
                16),

            scaledUi(
                scaleBase,
                14));


        headerCardLayout->setSpacing(
            scaledUi(
                scaleBase,
                10));
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


    // =========================================================================
    // 标题图标
    // =========================================================================
    if (auto *iconLabel =
            findChild<QLabel *>(
                QStringLiteral(
                    "stationTitleIcon"))) {

        iconLabel->setFixedSize(
            iconBoxSize,
            iconBoxSize);


        iconLabel->setPixmap(
            QIcon(
                QStringLiteral(
                    ":/icons/location.svg"))
                .pixmap(
                    QSize(
                        titleIconSize,
                        titleIconSize)));
    }


    // =========================================================================
    // 选择器
    // =========================================================================
    if (m_limitCombo) {

        m_limitCombo->setMinimumHeight(
            controlHeight);
    }


    // =========================================================================
    // 刷新按钮
    // =========================================================================
    if (auto *refreshButton =
            findChild<QPushButton *>(
                QStringLiteral(
                    "stationRefreshButton"))) {

        refreshButton->setMinimumHeight(
            controlHeight);


        refreshButton->setIconSize(
            QSize(
                refreshIconSize,
                refreshIconSize));
    }


    // =========================================================================
    // 列表区域
    // =========================================================================
    if (m_listLayout) {

        m_listLayout->setContentsMargins(
            scaledUi(
                scaleBase,
                2),

            scaledUi(
                scaleBase,
                2),

            scaledUi(
                scaleBase,
                2),

            scaledUi(
                scaleBase,
                12));


        m_listLayout->setSpacing(
            scaledUi(
                scaleBase,
                12));
    }


    // =========================================================================
    // 加载状态
    // =========================================================================
    if (auto *loadingLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "stationLoadingLayout"))) {

        loadingLayout->setContentsMargins(
            scaledUi(
                scaleBase,
                20),

            scaledUi(
                scaleBase,
                20),

            scaledUi(
                scaleBase,
                20),

            scaledUi(
                scaleBase,
                20));


        loadingLayout->setSpacing(
            scaledUi(
                scaleBase,
                12));
    }


    // =========================================================================
    // 空数据状态
    // =========================================================================
    if (auto *emptyLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "stationEmptyLayout"))) {

        emptyLayout->setContentsMargins(
            scaledUi(
                scaleBase,
                20),

            scaledUi(
                scaleBase,
                20),

            scaledUi(
                scaleBase,
                20),

            scaledUi(
                scaleBase,
                20));


        emptyLayout->setSpacing(
            scaledUi(
                scaleBase,
                12));
    }


    // =========================================================================
    // 错误状态
    // =========================================================================
    if (auto *errorLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "stationErrorLayout"))) {

        errorLayout->setContentsMargins(
            scaledUi(
                scaleBase,
                20),

            scaledUi(
                scaleBase,
                20),

            scaledUi(
                scaleBase,
                20),

            scaledUi(
                scaleBase,
                20));


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
               m_listLayout->takeAt(
                   0)) {

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


        m_tip->setText(
            QStringLiteral(
                "附近暂无充电站"));


        m_stack->setCurrentIndex(
            2);


        return;
    }


    // -------------------------------------------------------------------------
    // 成功：
    // 排序并缓存
    // -------------------------------------------------------------------------
    m_cachedList =
        sortStations(
            list);


    renderStations();
}


// ============================================================================
// NO.4：
// 按距离升序稳定排序 + 去重 + distance 兜底
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


// ============================================================================
// NO.4：
// 按当前 5 / 10 条限制渲染
// ============================================================================
void StationListPage::renderStations()
{
    const int total =
        m_cachedList.size();


    if (total == 0) {

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


// ============================================================================
// 创建站点卡片
// ============================================================================
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
        // RouteRequest 路线规划
        //
        // 原业务逻辑完整保留
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
