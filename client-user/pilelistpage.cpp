#include "pilelistpage.h"

#include "appmessagebox.h"
#include "netclient.h"
#include "protocol.h"
#include "uitheme.h"
#include "windowhelper.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSize>
#include <QSizePolicy>
#include <QVBoxLayout>


// ============================================================================
// 构造函数
// ============================================================================
PileListPage::PileListPage(
    NetClient *net,
    QWidget *parent)
    : QWidget(parent)
    , m_net(net)
{
    setObjectName(
        QStringLiteral(
            "pileListPage"));


    // =========================================================================
    // 顶部信息卡
    // =========================================================================
    auto *headerCard =
        new QFrame(this);

    headerCard->setObjectName(
        QStringLiteral(
            "pileHeaderCard"));

    headerCard->setAttribute(
        Qt::WA_StyledBackground,
        true);


    UiTheme::applyCardShadow(
        headerCard,
        22,
        5);


    auto *headerLayout =
        new QVBoxLayout(
            headerCard);

    headerLayout->setObjectName(
        QStringLiteral(
            "pileHeaderLayout"));

    headerLayout->setContentsMargins(
        16,
        15,
        16,
        15);

    headerLayout->setSpacing(
        10);


    // =========================================================================
    // 顶部第一行：
    // 返回 + 页面标签
    // =========================================================================
    auto *topRow =
        new QHBoxLayout;

    topRow->setObjectName(
        QStringLiteral(
            "pileTopRow"));

    topRow->setSpacing(
        10);


    m_backBtn =
        new QPushButton(
            QStringLiteral(
                "返回"),
            headerCard);

    m_backBtn->setObjectName(
        QStringLiteral(
            "pileBackButton"));

    m_backBtn->setCursor(
        Qt::PointingHandCursor);

    m_backBtn->setIcon(
        QIcon(
            QStringLiteral(
                ":/icons/back.svg")));


    auto *pageLabel =
        new QLabel(
            QStringLiteral(
                "充电桩列表"),
            headerCard);

    pageLabel->setObjectName(
        QStringLiteral(
            "pilePageLabel"));

    pageLabel->setAlignment(
        Qt::AlignCenter);


    topRow->addWidget(
        m_backBtn);

    topRow->addStretch();

    topRow->addWidget(
        pageLabel);


    headerLayout->addLayout(
        topRow);


    // =========================================================================
    // 当前站点标题
    // =========================================================================
    auto *titleRow =
        new QHBoxLayout;

    titleRow->setObjectName(
        QStringLiteral(
            "pileHeaderTitleRow"));

    titleRow->setSpacing(
        9);


    auto *titleIcon =
        new QLabel(
            headerCard);

    titleIcon->setObjectName(
        QStringLiteral(
            "pileHeaderTitleIcon"));

    titleIcon->setAlignment(
        Qt::AlignCenter);


    m_title =
        new QLabel(
            headerCard);

    m_title->setObjectName(
        QStringLiteral(
            "pileTitle"));

    m_title->setWordWrap(
        true);


    titleRow->addWidget(
        titleIcon);

    titleRow->addWidget(
        m_title,
        1);


    headerLayout->addLayout(
        titleRow);


    // =========================================================================
    // 提示信息
    // =========================================================================
    m_tip =
        new QLabel(
            QStringLiteral(
                "请选择空闲电桩进行预约"),
            headerCard);

    m_tip->setObjectName(
        QStringLiteral(
            "pileTip"));

    m_tip->setWordWrap(
        true);


    headerLayout->addWidget(
        m_tip);


    // =========================================================================
    // 电桩列表滚动区域
    // =========================================================================
    auto *scroll =
        new QScrollArea(
            this);

    scroll->setObjectName(
        QStringLiteral(
            "pileScrollArea"));

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
            "pileListContainer"));


    m_listLayout =
        new QVBoxLayout(
            container);

    m_listLayout->setContentsMargins(
        2,
        2,
        2,
        14);

    m_listLayout->setSpacing(
        12);


    scroll->setWidget(
        container);


    // =========================================================================
    // 页面总布局
    // =========================================================================
    auto *mainLayout =
        new QVBoxLayout(
            this);

    mainLayout->setObjectName(
        QStringLiteral(
            "pilePageLayout"));

    mainLayout->setContentsMargins(
        0,
        0,
        0,
        0);

    mainLayout->setSpacing(
        12);


    mainLayout->addWidget(
        headerCard);

    mainLayout->addWidget(
        scroll,
        1);


    // =========================================================================
    // 返回
    // =========================================================================
    connect(
        m_backBtn,
        &QPushButton::clicked,
        this,
        &PileListPage::back);


    applyResponsiveStyle();
}


// ============================================================================
// 设置当前用户 ID
// ============================================================================
void PileListPage::setUserId(
    qint64 userId)
{
    m_userId =
        userId;
}


// ============================================================================
// 加载指定站点的电桩
// ============================================================================
void PileListPage::loadStation(
    qint64 stationId,
    const QString &name)
{
    clearList();


    m_title->setText(
        name);


    m_tip->setText(
        QStringLiteral(
            "正在加载电桩信息…"));


    // =========================================================================
    // pile_list 请求
    // =========================================================================
    QJsonObject data;


    data["station_id"] =
        stationId;


    const QJsonObject resp =
        m_net->request(
            Protocol::makeRequest(
                Protocol::MsgType::PileList,
                data));


    const int resultCode =
        resp.value(
                "code")
            .toInt();


    // =========================================================================
    // 请求失败
    // =========================================================================
    if (resultCode !=
        Protocol::Ok) {

        m_tip->setText(
            QStringLiteral(
                "加载失败：%1")
                .arg(
                    resp.value(
                            "msg")
                        .toString()));


        return;
    }


    // =========================================================================
    // 获取列表
    // =========================================================================
    const QJsonArray list =
        resp.value(
                "data")
            .toObject()
            .value(
                "list")
            .toArray();


    // =========================================================================
    // 空列表
    // =========================================================================
    if (list.isEmpty()) {

        m_tip->setText(
            QStringLiteral(
                "该站暂无电桩"));


        return;
    }


    m_tip->setText(
        QStringLiteral(
            "共 %1 个电桩 · 闲置状态的电桩可进行预约")
            .arg(
                list.size()));


    // =========================================================================
    // 创建每个电桩卡片
    // =========================================================================
    for (const QJsonValue &value :
         list) {

        if (!value.isObject()) {

            continue;
        }


        const QJsonObject pile =
            value.toObject();


        const qint64 pileId =
            pile.value(
                    "id")
                .toVariant()
                .toLongLong();


        const QString code =
            pile.value(
                    "code")
                .toString();


        const QString type =
            pile.value(
                    "type")
                .toString();


        const double power =
            pile.value(
                    "power_kw")
                .toDouble();


        const QString status =
            pile.value(
                    "status")
                .toString();


        // =====================================================================
        // 类型中文映射
        //
        // 保留现有业务规则：
        // slow -> 慢充
        // 其它 -> 快充
        // =====================================================================
        const QString typeText =
            (type ==
             QStringLiteral("slow"))
                ? QStringLiteral(
                      "慢充")
                : QStringLiteral(
                      "快充");


        // =====================================================================
        // 状态中文映射
        // =====================================================================
        QString statusText =
            status;


        if (status ==
            QStringLiteral("idle")) {

            statusText =
                QStringLiteral(
                    "闲置");

        } else if (
            status ==
            QStringLiteral("busy")) {

            statusText =
                QStringLiteral(
                    "在用");

        } else if (
            status ==
            QStringLiteral("fault")) {

            statusText =
                QStringLiteral(
                    "故障");
        }


        // =====================================================================
        // 电桩卡片
        // =====================================================================
        auto *row =
            new QFrame(
                this);

        row->setObjectName(
            QStringLiteral(
                "pileRow"));

        row->setAttribute(
            Qt::WA_StyledBackground,
            true);

        row->setAttribute(
            Qt::WA_Hover,
            true);


        UiTheme::applyCardShadow(
            row,
            22,
            5);


        auto *rowLayout =
            new QVBoxLayout(
                row);

        rowLayout->setObjectName(
            QStringLiteral(
                "pileRowLayout"));

        rowLayout->setContentsMargins(
            16,
            16,
            16,
            16);

        rowLayout->setSpacing(
            11);


        // =====================================================================
        // Hero 区：
        // 充电桩图片 + 编号 / 类型 / 状态
        // =====================================================================
        auto *heroRow =
            new QHBoxLayout;

        heroRow->setObjectName(
            QStringLiteral(
                "pileHeroLayout"));

        heroRow->setSpacing(
            12);


        // ---------------------------------------------------------------------
        // 充电桩图片容器
        // ---------------------------------------------------------------------
        auto *imageFrame =
            new QFrame(
                row);

        imageFrame->setObjectName(
            QStringLiteral(
                "pileImageFrame"));

        imageFrame->setAttribute(
            Qt::WA_StyledBackground,
            true);


        auto *imageLayout =
            new QVBoxLayout(
                imageFrame);

        imageLayout->setContentsMargins(
            5,
            5,
            5,
            5);

        imageLayout->setAlignment(
            Qt::AlignCenter);


        auto *chargerImage =
            new QLabel(
                imageFrame);

        chargerImage->setObjectName(
            QStringLiteral(
                "pileChargerImage"));

        chargerImage->setAlignment(
            Qt::AlignCenter);


        imageLayout->addWidget(
            chargerImage);


        // ---------------------------------------------------------------------
        // 主要信息
        // ---------------------------------------------------------------------
        auto *heroInfo =
            new QVBoxLayout;

        heroInfo->setObjectName(
            QStringLiteral(
                "pileHeroInfoLayout"));

        heroInfo->setSpacing(
            7);


        // ---------------------------------------------------------------------
        // 编号 + 状态
        // ---------------------------------------------------------------------
        auto *titleRow =
            new QHBoxLayout;

        titleRow->setObjectName(
            QStringLiteral(
                "pileTitleRow"));

        titleRow->setSpacing(
            8);


        auto *codeLabel =
            new QLabel(
                code,
                row);

        codeLabel->setObjectName(
            QStringLiteral(
                "pileCodeLabel"));

        codeLabel->setWordWrap(
            true);


        auto *statusLabel =
            new QLabel(
                statusText,
                row);

        statusLabel->setObjectName(
            QStringLiteral(
                "pileStatusLabel"));

        statusLabel->setProperty(
            "pileState",
            status);

        statusLabel->setAlignment(
            Qt::AlignCenter);


        titleRow->addWidget(
            codeLabel,
            1);

        titleRow->addWidget(
            statusLabel);


        heroInfo->addLayout(
            titleRow);


        // ---------------------------------------------------------------------
        // 简短说明
        // ---------------------------------------------------------------------
        auto *descriptionLabel =
            new QLabel(
                QStringLiteral(
                    "%1 · %2 kW")
                    .arg(
                        typeText)
                    .arg(
                        power,
                        0,
                        'f',
                        1),
                row);

        descriptionLabel->setObjectName(
            QStringLiteral(
                "pileDescriptionLabel"));


        heroInfo->addWidget(
            descriptionLabel);


        // ---------------------------------------------------------------------
        // 小标签
        // ---------------------------------------------------------------------
        auto *typeBadge =
            new QLabel(
                typeText,
                row);

        typeBadge->setObjectName(
            QStringLiteral(
                "pileTypeBadge"));

        typeBadge->setProperty(
            "pileType",
            type);

        typeBadge->setAlignment(
            Qt::AlignCenter);


        heroInfo->addWidget(
            typeBadge,
            0,
            Qt::AlignLeft);


        heroRow->addWidget(
            imageFrame);

        heroRow->addLayout(
            heroInfo,
            1);


        rowLayout->addLayout(
            heroRow);


        // =====================================================================
        // 参数面板
        // =====================================================================
        auto *metricsPanel =
            new QFrame(
                row);

        metricsPanel->setObjectName(
            QStringLiteral(
                "pileMetricsPanel"));

        metricsPanel->setAttribute(
            Qt::WA_StyledBackground,
            true);


        auto *metricsLayout =
            new QHBoxLayout(
                metricsPanel);

        metricsLayout->setObjectName(
            QStringLiteral(
                "pileMetricsLayout"));

        metricsLayout->setContentsMargins(
            14,
            12,
            14,
            12);

        metricsLayout->setSpacing(
            14);


        // ---------------------------------------------------------------------
        // 充电类型
        // ---------------------------------------------------------------------
        auto *typeBlock =
            new QVBoxLayout;

        typeBlock->setSpacing(
            3);


        auto *typeCaption =
            new QLabel(
                QStringLiteral(
                    "充电类型"),
                metricsPanel);

        typeCaption->setObjectName(
            QStringLiteral(
                "pileMetricCaption"));


        auto *typeValue =
            new QLabel(
                typeText,
                metricsPanel);

        typeValue->setObjectName(
            QStringLiteral(
                "pileMetricValue"));


        typeBlock->addWidget(
            typeCaption);

        typeBlock->addWidget(
            typeValue);


        // ---------------------------------------------------------------------
        // 分隔线
        // ---------------------------------------------------------------------
        auto *divider =
            new QFrame(
                metricsPanel);

        divider->setObjectName(
            QStringLiteral(
                "pileMetricDivider"));

        divider->setFrameShape(
            QFrame::VLine);


        // ---------------------------------------------------------------------
        // 额定功率
        // ---------------------------------------------------------------------
        auto *powerBlock =
            new QVBoxLayout;

        powerBlock->setSpacing(
            3);


        auto *powerCaption =
            new QLabel(
                QStringLiteral(
                    "额定功率"),
                metricsPanel);

        powerCaption->setObjectName(
            QStringLiteral(
                "pileMetricCaption"));


        auto *powerValue =
            new QLabel(
                QStringLiteral(
                    "%1 kW")
                    .arg(
                        power,
                        0,
                        'f',
                        1),
                metricsPanel);

        powerValue->setObjectName(
            QStringLiteral(
                "pilePowerValue"));


        powerBlock->addWidget(
            powerCaption);

        powerBlock->addWidget(
            powerValue);


        metricsLayout->addLayout(
            typeBlock,
            1);

        metricsLayout->addWidget(
            divider);

        metricsLayout->addLayout(
            powerBlock,
            1);


        rowLayout->addWidget(
            metricsPanel);


        // =====================================================================
        // 底部：
        // 当前状态 + 预约按钮
        // =====================================================================
        auto *actionRow =
            new QHBoxLayout;

        actionRow->setObjectName(
            QStringLiteral(
                "pileActionLayout"));

        actionRow->setSpacing(
            10);


        const bool canReserve =
            status ==
            QStringLiteral(
                "idle");


        auto *availabilityLabel =
            new QLabel(
                canReserve
                    ? QStringLiteral(
                          "当前可预约")
                    : QStringLiteral(
                          "当前不可预约"),
                row);

        availabilityLabel->setObjectName(
            QStringLiteral(
                "pileAvailabilityLabel"));

        availabilityLabel->setProperty(
            "pileAvailable",
            canReserve);


        auto *reserveBtn =
            new QPushButton(
                QStringLiteral(
                    "预约"),
                row);

        reserveBtn->setObjectName(
            QStringLiteral(
                "pileReserveButton"));

        reserveBtn->setCursor(
            Qt::PointingHandCursor);

        reserveBtn->setEnabled(
            canReserve);

        reserveBtn->setIcon(
            QIcon(
                QStringLiteral(
                    ":/icons/calendar.svg")));


        if (!canReserve) {

            reserveBtn->setToolTip(
                QStringLiteral(
                    "该桩当前不可预约"));
        }


        actionRow->addWidget(
            availabilityLabel);

        actionRow->addStretch();

        actionRow->addWidget(
            reserveBtn);


        rowLayout->addLayout(
            actionRow);


        // =====================================================================
        // 预约业务逻辑
        //
        // 与现有版本保持一致
        // =====================================================================
        connect(
            reserveBtn,
            &QPushButton::clicked,
            this,
            [this, reserveBtn, pileId]() {

                // -------------------------------------------------------------
                // 用户信息校验
                // -------------------------------------------------------------
                if (m_userId <= 0) {

                    AppMessageBox::warning(
                        this,
                        QStringLiteral(
                            "预约失败"),
                        QStringLiteral(
                            "用户信息无效，请重新登录"));

                    return;
                }


                // -------------------------------------------------------------
                // 电桩 ID 校验
                // -------------------------------------------------------------
                if (pileId <= 0) {

                    AppMessageBox::warning(
                        this,
                        QStringLiteral(
                            "预约失败"),
                        QStringLiteral(
                            "电桩信息无效"));

                    return;
                }


                // -------------------------------------------------------------
                // 请求中
                // -------------------------------------------------------------
                reserveBtn->setEnabled(
                    false);

                reserveBtn->setText(
                    QStringLiteral(
                        "预约中…"));


                // -------------------------------------------------------------
                // 使用已有 Reserve 协议
                // -------------------------------------------------------------
                QJsonObject data;


                data["pile_id"] =
                    pileId;


                const QJsonObject resp =
                    m_net->request(
                        Protocol::makeRequest(
                            Protocol::MsgType::Reserve,
                            data));


                const int resultCode =
                    resp.value(
                            "code")
                        .toInt();


                const QString message =
                    resp.value(
                            "msg")
                        .toString();


                // -------------------------------------------------------------
                // 预约失败
                // -------------------------------------------------------------
                if (resultCode !=
                    Protocol::Ok) {

                    reserveBtn->setEnabled(
                        true);

                    reserveBtn->setText(
                        QStringLiteral(
                            "预约"));


                    AppMessageBox::warning(
                        this,
                        QStringLiteral(
                            "预约失败"),
                        message);


                    return;
                }


                // -------------------------------------------------------------
                // 读取订单号
                // -------------------------------------------------------------
                const QString orderNo =
                    resp.value(
                            "data")
                        .toObject()
                        .value(
                            "order_no")
                        .toString();


                if (orderNo.isEmpty()) {

                    reserveBtn->setEnabled(
                        true);

                    reserveBtn->setText(
                        QStringLiteral(
                            "预约"));


                    AppMessageBox::warning(
                        this,
                        QStringLiteral(
                            "预约失败"),
                        QStringLiteral(
                            "服务器未返回订单号"));


                    return;
                }


                // -------------------------------------------------------------
                // 成功
                // -------------------------------------------------------------
                reserveBtn->setText(
                    QStringLiteral(
                        "已预约"));


                AppMessageBox::information(
                    this,
                    QStringLiteral(
                        "预约成功"),
                    QStringLiteral(
                        "预约成功，即将进入充电页面"));


                emit reservationSucceeded(
                    orderNo);
            });


        m_listLayout->addWidget(
            row);
    }


    // 让卡片始终靠上
    m_listLayout->addStretch();


    applyResponsiveStyle();
}


// ============================================================================
// 清空列表
// ============================================================================
void PileListPage::clearList()
{
    while (QLayoutItem *item =
               m_listLayout->takeAt(
                   0)) {

        if (QWidget *widget =
                item->widget()) {

            widget->deleteLater();
        }


        delete item;
    }
}


// ============================================================================
// Resize
// ============================================================================
void PileListPage::resizeEvent(
    QResizeEvent *event)
{
    QWidget::resizeEvent(
        event);


    applyResponsiveStyle();
}


// ============================================================================
// 响应式样式
// ============================================================================
void PileListPage::applyResponsiveStyle()
{
    QWidget *scaleBase =
        window()
            ? window()
            : this;


    const int titleFont =
        scaledUi(
            scaleBase,
            21);


    const int pageLabelFont =
        scaledUi(
            scaleBase,
            11);


    const int tipFont =
        scaledUi(
            scaleBase,
            11);


    const int codeFont =
        scaledUi(
            scaleBase,
            17);


    const int descriptionFont =
        scaledUi(
            scaleBase,
            12);


    const int metricCaptionFont =
        scaledUi(
            scaleBase,
            11);


    const int metricValueFont =
        scaledUi(
            scaleBase,
            18);


    const int statusFont =
        scaledUi(
            scaleBase,
            11);


    const int buttonFont =
        scaledUi(
            scaleBase,
            12);


    const int cardRadius =
        scaledUi(
            scaleBase,
            22);


    const int smallRadius =
        scaledUi(
            scaleBase,
            11);


    const int imageRadius =
        scaledUi(
            scaleBase,
            17);


    const int imageWidth =
        scaledUi(
            scaleBase,
            78);


    const int imageHeight =
        scaledUi(
            scaleBase,
            86);


    const int chargerWidth =
        scaledUi(
            scaleBase,
            60);


    const int chargerHeight =
        scaledUi(
            scaleBase,
            72);


    const int titleIconBox =
        scaledUi(
            scaleBase,
            34);


    const int titleIconSize =
        scaledUi(
            scaleBase,
            18);


    const int backIconSize =
        scaledUi(
            scaleBase,
            16);


    const int buttonIconSize =
        scaledUi(
            scaleBase,
            16);


    const int buttonHeight =
        scaledUi(
            scaleBase,
            43);


    // =========================================================================
    // 页面
    // =========================================================================
    QString style =
        QStringLiteral(

            "QWidget#pileListPage{"
            "background:transparent;"
            "color:%1;"
            "}"

            "QWidget#pileListContainer{"
            "background:transparent;"
            "}"

            "QScrollArea#pileScrollArea{"
            "background:transparent;"
            "border:none;"
            "}"

            "QScrollArea#pileScrollArea > QWidget > QWidget{"
            "background:transparent;"
            "}");

    style =
        style.arg(
            UiTheme::textPrimary());


    // =========================================================================
    // Header
    // =========================================================================
    QString headerStyle =
        QStringLiteral(

            "QFrame#pileHeaderCard{"
            "background:%1;"
            "border:1px solid %2;"
            "border-radius:%3px;"
            "}"

            "QLabel#pilePageLabel{"
            "background:%4;"
            "color:%5;"
            "border:none;"
            "border-radius:%6px;"
            "font-size:%7px;"
            "font-weight:700;"
            "padding:5px 10px;"
            "}"

            "QLabel#pileHeaderTitleIcon{"
            "background:%4;"
            "border:none;"
            "border-radius:%8px;"
            "}"

            "QLabel#pileTitle{"
            "background:transparent;"
            "color:%9;"
            "font-size:%10px;"
            "font-weight:800;"
            "}"

            "QLabel#pileTip{"
            "background:transparent;"
            "color:%11;"
            "font-size:%12px;"
            "}");

    headerStyle =
        headerStyle
            .arg(
                UiTheme::surface())          // %1
            .arg(
                UiTheme::border())           // %2
            .arg(
                cardRadius)                  // %3
            .arg(
                UiTheme::limeSoft())         // %4
            .arg(
                UiTheme::limeStrong())       // %5
            .arg(
                smallRadius)                 // %6
            .arg(
                pageLabelFont)               // %7
            .arg(
                titleIconBox / 2)            // %8
            .arg(
                UiTheme::textPrimary())      // %9
            .arg(
                titleFont)                   // %10
            .arg(
                UiTheme::textSecondary())    // %11
            .arg(
                tipFont);                    // %12


    // =========================================================================
    // 返回按钮
    // =========================================================================
    QString backStyle =
        QStringLiteral(

            "QPushButton#pileBackButton{"
            "background:%1;"
            "color:%2;"
            "border:1px solid %3;"
            "border-radius:%4px;"
            "font-size:%5px;"
            "font-weight:700;"
            "padding:7px 11px;"
            "}"

            "QPushButton#pileBackButton:hover{"
            "background:#ECEFED;"
            "}"

            "QPushButton#pileBackButton:pressed{"
            "background:#E4E8E6;"
            "}");

    backStyle =
        backStyle
            .arg(
                UiTheme::surfaceSoft())
            .arg(
                UiTheme::textPrimary())
            .arg(
                UiTheme::border())
            .arg(
                smallRadius)
            .arg(
                buttonFont);


    // =========================================================================
    // 电桩卡片
    // =========================================================================
    QString rowStyle =
        QStringLiteral(

            "QFrame#pileRow{"
            "background:%1;"
            "border:1px solid %2;"
            "border-radius:%3px;"
            "}"

            "QFrame#pileRow:hover{"
            "border-color:%4;"
            "}"

            "QFrame#pileImageFrame{"
            "background:%5;"
            "border:none;"
            "border-radius:%6px;"
            "}"

            "QLabel#pileChargerImage{"
            "background:transparent;"
            "border:none;"
            "}"

            "QLabel#pileCodeLabel{"
            "background:transparent;"
            "border:none;"
            "color:%7;"
            "font-size:%8px;"
            "font-weight:800;"
            "}"

            "QLabel#pileDescriptionLabel{"
            "background:transparent;"
            "border:none;"
            "color:%9;"
            "font-size:%10px;"
            "}"

            "QLabel#pileTypeBadge{"
            "background:%11;"
            "color:%7;"
            "border:none;"
            "border-radius:%12px;"
            "font-size:%13px;"
            "font-weight:650;"
            "padding:4px 9px;"
            "}");

    rowStyle =
        rowStyle
            .arg(
                UiTheme::surface())          // %1
            .arg(
                UiTheme::border())           // %2
            .arg(
                cardRadius)                  // %3
            .arg(
                UiTheme::borderStrong())     // %4
            .arg(
                UiTheme::surfaceSoft())      // %5
            .arg(
                imageRadius)                 // %6
            .arg(
                UiTheme::textPrimary())      // %7
            .arg(
                codeFont)                    // %8
            .arg(
                UiTheme::textSecondary())    // %9
            .arg(
                descriptionFont)             // %10
            .arg(
                UiTheme::primarySoft())      // %11
            .arg(
                smallRadius)                 // %12
            .arg(
                tipFont);                    // %13


    // =========================================================================
    // 状态胶囊
    // =========================================================================
    QString statusStyle =
        QStringLiteral(

            "QLabel#pileStatusLabel{"
            "border:none;"
            "border-radius:%1px;"
            "font-size:%2px;"
            "font-weight:750;"
            "padding:5px 10px;"
            "}"

            "QLabel#pileStatusLabel[pileState=\"idle\"]{"
            "background:%3;"
            "color:%4;"
            "}"

            "QLabel#pileStatusLabel[pileState=\"busy\"]{"
            "background:#EEF2F5;"
            "color:#60798A;"
            "}"

            "QLabel#pileStatusLabel[pileState=\"fault\"]{"
            "background:#FCEBEB;"
            "color:%5;"
            "}");

    statusStyle =
        statusStyle
            .arg(
                smallRadius)
            .arg(
                statusFont)
            .arg(
                UiTheme::limeSoft())
            .arg(
                UiTheme::limeStrong())
            .arg(
                UiTheme::danger());


    // =========================================================================
    // 参数面板
    // =========================================================================
    QString metricsStyle =
        QStringLiteral(

            "QFrame#pileMetricsPanel{"
            "background:%1;"
            "border:none;"
            "border-radius:%2px;"
            "}"

            "QLabel#pileMetricCaption{"
            "background:transparent;"
            "border:none;"
            "color:%3;"
            "font-size:%4px;"
            "}"

            "QLabel#pileMetricValue{"
            "background:transparent;"
            "border:none;"
            "color:%5;"
            "font-size:%6px;"
            "font-weight:800;"
            "}"

            "QLabel#pilePowerValue{"
            "background:transparent;"
            "border:none;"
            "color:%7;"
            "font-size:%6px;"
            "font-weight:800;"
            "}"

            "QFrame#pileMetricDivider{"
            "background:%8;"
            "border:none;"
            "max-width:1px;"
            "}");

    metricsStyle =
        metricsStyle
            .arg(
                UiTheme::surfaceSoft())      // %1
            .arg(
                scaledUi(
                    scaleBase,
                    16))                     // %2
            .arg(
                UiTheme::textSecondary())    // %3
            .arg(
                metricCaptionFont)           // %4
            .arg(
                UiTheme::textPrimary())      // %5
            .arg(
                metricValueFont)             // %6
            .arg(
                UiTheme::limeStrong())       // %7
            .arg(
                UiTheme::border());          // %8


    // =========================================================================
    // 可预约状态
    // =========================================================================
    QString availabilityStyle =
        QStringLiteral(

            "QLabel#pileAvailabilityLabel{"
            "background:transparent;"
            "border:none;"
            "font-size:%1px;"
            "font-weight:650;"
            "}"

            "QLabel#pileAvailabilityLabel"
            "[pileAvailable=\"true\"]{"
            "color:%2;"
            "}"

            "QLabel#pileAvailabilityLabel"
            "[pileAvailable=\"false\"]{"
            "color:%3;"
            "}");

    availabilityStyle =
        availabilityStyle
            .arg(
                tipFont)
            .arg(
                UiTheme::limeStrong())
            .arg(
                UiTheme::textSecondary());


    // =========================================================================
    // 预约按钮
    // =========================================================================
    QString reserveStyle =
        QStringLiteral(

            "QPushButton#pileReserveButton{"
            "background:%1;"
            "color:%2;"
            "border:none;"
            "border-radius:%3px;"
            "font-size:%4px;"
            "font-weight:800;"
            "padding:9px 18px;"
            "}"

            "QPushButton#pileReserveButton:hover{"
            "background:#63E27C;"
            "}"

            "QPushButton#pileReserveButton:pressed{"
            "background:#52D96E;"
            "}"

            "QPushButton#pileReserveButton:disabled{"
            "background:#E3E7E5;"
            "color:#9CA39F;"
            "}");

    reserveStyle =
        reserveStyle
            .arg(
                UiTheme::lime())
            .arg(
                UiTheme::dark())
            .arg(
                smallRadius)
            .arg(
                buttonFont);


    setStyleSheet(
        style +
        headerStyle +
        backStyle +
        rowStyle +
        statusStyle +
        metricsStyle +
        availabilityStyle +
        reserveStyle);


    // =========================================================================
    // Header 响应式布局
    // =========================================================================
    if (auto *headerLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "pileHeaderLayout"))) {

        headerLayout->setContentsMargins(
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
                15));


        headerLayout->setSpacing(
            scaledUi(
                scaleBase,
                10));
    }


    if (auto *topRow =
            findChild<QHBoxLayout *>(
                QStringLiteral(
                    "pileTopRow"))) {

        topRow->setSpacing(
            scaledUi(
                scaleBase,
                10));
    }


    if (auto *headerTitleRow =
            findChild<QHBoxLayout *>(
                QStringLiteral(
                    "pileHeaderTitleRow"))) {

        headerTitleRow->setSpacing(
            scaledUi(
                scaleBase,
                9));
    }


    // =========================================================================
    // Header 图标
    // =========================================================================
    if (auto *titleIcon =
            findChild<QLabel *>(
                QStringLiteral(
                    "pileHeaderTitleIcon"))) {

        titleIcon->setFixedSize(
            titleIconBox,
            titleIconBox);


        titleIcon->setPixmap(
            QIcon(
                QStringLiteral(
                    ":/icons/plug.svg"))
                .pixmap(
                    QSize(
                        titleIconSize,
                        titleIconSize)));
    }


    if (m_backBtn) {

        m_backBtn->setIconSize(
            QSize(
                backIconSize,
                backIconSize));
    }


    // =========================================================================
    // 页面列表布局
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
                14));


        m_listLayout->setSpacing(
            scaledUi(
                scaleBase,
                12));
    }


    // =========================================================================
    // 电桩 Hero
    // =========================================================================
    const auto heroLayouts =
        findChildren<QHBoxLayout *>(
            QStringLiteral(
                "pileHeroLayout"));


    for (QHBoxLayout *heroLayout :
         heroLayouts) {

        heroLayout->setSpacing(
            scaledUi(
                scaleBase,
                12));
    }


    const auto heroInfoLayouts =
        findChildren<QVBoxLayout *>(
            QStringLiteral(
                "pileHeroInfoLayout"));


    for (QVBoxLayout *infoLayout :
         heroInfoLayouts) {

        infoLayout->setSpacing(
            scaledUi(
                scaleBase,
                7));
    }


    // =========================================================================
    // 每张电桩图
    // =========================================================================
    const auto imageFrames =
        findChildren<QFrame *>(
            QStringLiteral(
                "pileImageFrame"));


    for (QFrame *imageFrame :
         imageFrames) {

        imageFrame->setFixedSize(
            imageWidth,
            imageHeight);
    }


    const auto chargerImages =
        findChildren<QLabel *>(
            QStringLiteral(
                "pileChargerImage"));


    const QPixmap chargerSource(
        QStringLiteral(
            ":/images/charger-slim.jpg"));


    for (QLabel *chargerImage :
         chargerImages) {

        chargerImage->setFixedSize(
            chargerWidth,
            chargerHeight);


        if (!chargerSource.isNull()) {

            chargerImage->setPixmap(
                chargerSource.scaled(
                    chargerWidth,
                    chargerHeight,
                    Qt::KeepAspectRatio,
                    Qt::SmoothTransformation));

        } else {

            chargerImage->setPixmap(
                QIcon(
                    QStringLiteral(
                        ":/icons/plug.svg"))
                    .pixmap(
                        QSize(
                            titleIconSize * 2,
                            titleIconSize * 2)));
        }
    }


    // =========================================================================
    // 每张电桩卡片
    // =========================================================================
    const auto rowLayouts =
        findChildren<QVBoxLayout *>(
            QStringLiteral(
                "pileRowLayout"));


    for (QVBoxLayout *rowLayout :
         rowLayouts) {

        rowLayout->setContentsMargins(
            scaledUi(
                scaleBase,
                16),
            scaledUi(
                scaleBase,
                16),
            scaledUi(
                scaleBase,
                16),
            scaledUi(
                scaleBase,
                16));


        rowLayout->setSpacing(
            scaledUi(
                scaleBase,
                11));
    }


    // =========================================================================
    // 参数面板
    // =========================================================================
    const auto metricLayouts =
        findChildren<QHBoxLayout *>(
            QStringLiteral(
                "pileMetricsLayout"));


    for (QHBoxLayout *metricLayout :
         metricLayouts) {

        metricLayout->setContentsMargins(
            scaledUi(
                scaleBase,
                14),
            scaledUi(
                scaleBase,
                12),
            scaledUi(
                scaleBase,
                14),
            scaledUi(
                scaleBase,
                12));


        metricLayout->setSpacing(
            scaledUi(
                scaleBase,
                14));
    }


    // =========================================================================
    // 操作区
    // =========================================================================
    const auto actionLayouts =
        findChildren<QHBoxLayout *>(
            QStringLiteral(
                "pileActionLayout"));


    for (QHBoxLayout *actionLayout :
         actionLayouts) {

        actionLayout->setSpacing(
            scaledUi(
                scaleBase,
                10));
    }


    // =========================================================================
    // 预约按钮尺寸
    // =========================================================================
    const auto reserveButtons =
        findChildren<QPushButton *>(
            QStringLiteral(
                "pileReserveButton"));


    for (QPushButton *button :
         reserveButtons) {

        button->setMinimumHeight(
            buttonHeight);


        button->setIconSize(
            QSize(
                buttonIconSize,
                buttonIconSize));
    }


    // =========================================================================
    // 页面总间距
    // =========================================================================
    if (auto *mainLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "pilePageLayout"))) {

        mainLayout->setSpacing(
            scaledUi(
                scaleBase,
                12));
    }
}
