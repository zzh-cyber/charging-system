#include "pilelistpage.h"

#include "netclient.h"
#include "protocol.h"
#include "uitheme.h"
#include "windowhelper.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
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


    // ========================================================================
    // 顶部信息卡
    // ========================================================================
    auto *headerCard =
        new QFrame(this);

    headerCard->setObjectName(
        QStringLiteral(
            "pileHeaderCard"));

    UiTheme::applyCardShadow(
        headerCard,
        16,
        4);


    auto *headerLayout =
        new QVBoxLayout(
            headerCard);

    headerLayout->setObjectName(
        QStringLiteral(
            "pileHeaderLayout"));

    headerLayout->setContentsMargins(
        18,
        16,
        18,
        16);

    headerLayout->setSpacing(
        8);


    // ========================================================================
    // 顶部第一行：返回 + 页面类型
    // ========================================================================
    auto *topRow =
        new QHBoxLayout;

    topRow->setSpacing(
        10);


    m_backBtn =
        new QPushButton(
            QStringLiteral("← 返回"),
            headerCard);

    m_backBtn->setObjectName(
        QStringLiteral(
            "pileBackButton"));

    m_backBtn->setCursor(
        Qt::PointingHandCursor);


    auto *pageLabel =
        new QLabel(
            QStringLiteral(
                "充电桩列表"),
            headerCard);

    pageLabel->setObjectName(
        QStringLiteral(
            "pilePageLabel"));


    topRow->addWidget(
        m_backBtn);

    topRow->addStretch();

    topRow->addWidget(
        pageLabel);


    headerLayout->addLayout(
        topRow);


    // ========================================================================
    // 当前站点名称
    // ========================================================================
    m_title =
        new QLabel(
            headerCard);

    m_title->setObjectName(
        QStringLiteral(
            "pileTitle"));

    m_title->setWordWrap(
        true);


    headerLayout->addWidget(
        m_title);


    // ========================================================================
    // 提示信息
    // ========================================================================
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


    // ========================================================================
    // 电桩列表滚动区域
    // ========================================================================
    auto *scroll =
        new QScrollArea(this);

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
        6,
        8,
        6,
        14);

    m_listLayout->setSpacing(
        14);


    scroll->setWidget(
        container);


    // ========================================================================
    // 页面总布局
    // ========================================================================
    auto *mainLayout =
        new QVBoxLayout(this);

    mainLayout->setObjectName(
        QStringLiteral(
            "pilePageLayout"));

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
        scroll,
        1);


    // ========================================================================
    // 返回
    // ========================================================================
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


    // ========================================================================
    // pile_list 请求
    // ========================================================================
    QJsonObject data;

    data["station_id"] =
        stationId;


    const QJsonObject resp =
        m_net->request(
            Protocol::makeRequest(
                Protocol::MsgType::PileList,
                data));


    const int code =
        resp.value(
                "code")
            .toInt();


    // ========================================================================
    // 请求失败
    // ========================================================================
    if (code != Protocol::Ok) {

        m_tip->setText(
            QStringLiteral(
                "加载失败：%1")
                .arg(
                    resp.value(
                            "msg")
                        .toString()));

        return;
    }


    // ========================================================================
    // 获取列表
    // ========================================================================
    const QJsonArray list =
        resp.value(
                "data")
            .toObject()
            .value(
                "list")
            .toArray();


    // ========================================================================
    // 空列表
    // ========================================================================
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


    // ========================================================================
    // 创建每个电桩卡片
    // ========================================================================
    for (const QJsonValue &value :
         list) {

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


        // ====================================================================
        // 类型中文映射
        // 保持原逻辑：slow -> 慢充，其余 -> 快充
        // ====================================================================
        const QString typeText =
            (type == "slow")
                ? QStringLiteral(
                      "慢充")
                : QStringLiteral(
                      "快充");


        // ====================================================================
        // 状态中文映射
        // ====================================================================
        QString statusText =
            status;


        if (status == "idle") {

            statusText =
                QStringLiteral(
                    "闲置");

        } else if (status == "busy") {

            statusText =
                QStringLiteral(
                    "在用");

        } else if (status == "fault") {

            statusText =
                QStringLiteral(
                    "故障");
        }


        // ====================================================================
        // 电桩卡片
        // ====================================================================
        auto *row =
            new QFrame(this);

        row->setObjectName(
            QStringLiteral(
                "pileRow"));

        UiTheme::applyCardShadow(
            row,
            14,
            3);


        auto *rowLayout =
            new QVBoxLayout(
                row);

        rowLayout->setObjectName(
            QStringLiteral(
                "pileRowLayout"));

        rowLayout->setContentsMargins(
            18,
            16,
            18,
            16);

        rowLayout->setSpacing(
            11);


        // ====================================================================
        // 第一行：电桩编号 + 状态
        // ====================================================================
        auto *titleRow =
            new QHBoxLayout;

        titleRow->setSpacing(
            12);


        auto *codeLabel =
            new QLabel(
                code,
                row);

        codeLabel->setObjectName(
            QStringLiteral(
                "pileCodeLabel"));


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


        rowLayout->addLayout(
            titleRow);


        // ====================================================================
        // 简短说明
        // ====================================================================
        auto *descriptionLabel =
            new QLabel(
                QStringLiteral(
                    "%1充电桩 · 额定功率 %2 kW")
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


        rowLayout->addWidget(
            descriptionLabel);


        // ====================================================================
        // 参数面板
        // ====================================================================
        auto *metricsPanel =
            new QFrame(row);

        metricsPanel->setObjectName(
            QStringLiteral(
                "pileMetricsPanel"));


        auto *metricsLayout =
            new QHBoxLayout(
                metricsPanel);

        metricsLayout->setObjectName(
            QStringLiteral(
                "pileMetricsLayout"));

        metricsLayout->setContentsMargins(
            14,
            11,
            14,
            11);

        metricsLayout->setSpacing(
            14);


        // --------------------------------------------------------------------
        // 充电类型
        // --------------------------------------------------------------------
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


        // --------------------------------------------------------------------
        // 中间分隔线
        // --------------------------------------------------------------------
        auto *divider =
            new QFrame(
                metricsPanel);

        divider->setObjectName(
            QStringLiteral(
                "pileMetricDivider"));

        divider->setFrameShape(
            QFrame::VLine);


        // --------------------------------------------------------------------
        // 额定功率
        // --------------------------------------------------------------------
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
                "pileMetricValue"));


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


        // ====================================================================
        // 底部：可预约提示 + 预约按钮
        // ====================================================================
        auto *actionRow =
            new QHBoxLayout;

        actionRow->setObjectName(
            QStringLiteral(
                "pileActionLayout"));

        actionRow->setSpacing(
            12);


        const bool canReserve =
            (status == "idle");


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


        // ====================================================================
        // 预约业务逻辑
        // 保持原逻辑不变
        // ====================================================================
        connect(
            reserveBtn,
            &QPushButton::clicked,
            this,
            [this, reserveBtn, pileId]() {

                // ------------------------------------------------------------
                // 用户信息校验
                // ------------------------------------------------------------
                if (m_userId <= 0) {

                    QMessageBox::warning(
                        this,
                        QStringLiteral(
                            "预约失败"),
                        QStringLiteral(
                            "用户信息无效，请重新登录"));

                    return;
                }


                // ------------------------------------------------------------
                // 电桩 ID 校验
                // ------------------------------------------------------------
                if (pileId <= 0) {

                    QMessageBox::warning(
                        this,
                        QStringLiteral(
                            "预约失败"),
                        QStringLiteral(
                            "电桩信息无效"));

                    return;
                }


                // ------------------------------------------------------------
                // 请求中
                // ------------------------------------------------------------
                reserveBtn->setEnabled(
                    false);

                reserveBtn->setText(
                    QStringLiteral(
                        "预约中…"));


                // ------------------------------------------------------------
                // 使用已有 reserve 协议
                // ------------------------------------------------------------
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


                // ------------------------------------------------------------
                // 预约失败
                // ------------------------------------------------------------
                if (resultCode !=
                    Protocol::Ok) {

                    reserveBtn->setEnabled(
                        true);

                    reserveBtn->setText(
                        QStringLiteral(
                            "预约"));


                    QMessageBox::warning(
                        this,
                        QStringLiteral(
                            "预约失败"),
                        message);

                    return;
                }


                // ------------------------------------------------------------
                // 读取订单号
                // ------------------------------------------------------------
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


                    QMessageBox::warning(
                        this,
                        QStringLiteral(
                            "预约失败"),
                        QStringLiteral(
                            "服务器未返回订单号"));

                    return;
                }


                // ------------------------------------------------------------
                // 成功
                // ------------------------------------------------------------
                reserveBtn->setText(
                    QStringLiteral(
                        "已预约"));


                QMessageBox::information(
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


    // 让卡片始终从顶部排列
    m_listLayout->addStretch();


    applyResponsiveStyle();
}


// ============================================================================
// 清空列表
// ============================================================================
void PileListPage::clearList()
{
    while (QLayoutItem *item =
               m_listLayout->takeAt(0)) {

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
            22);

    const int pageLabelFont =
        scaledUi(
            scaleBase,
            12);

    const int tipFont =
        scaledUi(
            scaleBase,
            12);

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
            17);

    const int statusFont =
        scaledUi(
            scaleBase,
            11);

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


    // ========================================================================
    // 页面基础样式
    // ========================================================================
    const QString pageStyle =
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
            "}")

            .arg(
                UiTheme::textPrimary());


    // ========================================================================
    // 顶部卡片
    // ========================================================================
    const QString headerStyle =
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
            "font-weight:600;"
            "padding:4px 9px;"
            "}"

            "QLabel#pileTitle{"
            "background:transparent;"
            "color:%8;"
            "font-size:%9px;"
            "font-weight:800;"
            "}"

            "QLabel#pileTip{"
            "background:transparent;"
            "color:%10;"
            "font-size:%11px;"
            "}")

            .arg(
                UiTheme::surface())

            .arg(
                UiTheme::border())

            .arg(
                cardRadius)

            .arg(
                UiTheme::primarySoft())

            .arg(
                UiTheme::primary())

            .arg(
                smallRadius)

            .arg(
                pageLabelFont)

            .arg(
                UiTheme::textPrimary())

            .arg(
                titleFont)

            .arg(
                UiTheme::textSecondary())

            .arg(
                tipFont);


    // ========================================================================
    // 返回按钮
    // ========================================================================
    const QString backButtonStyle =
        QStringLiteral(

            "QPushButton#pileBackButton{"
            "background:%1;"
            "color:%2;"
            "border:1px solid #D6E1DA;"
            "border-radius:%3px;"
            "font-size:%4px;"
            "font-weight:700;"
            "padding:7px 13px;"
            "}"

            "QPushButton#pileBackButton:hover{"
            "background:#DFE9E3;"
            "}")

            .arg(
                UiTheme::primarySoft())

            .arg(
                UiTheme::primary())

            .arg(
                smallRadius)

            .arg(
                buttonFont);


    // ========================================================================
    // 电桩卡片
    // ========================================================================
    const QString rowStyle =
        QStringLiteral(

            "QFrame#pileRow{"
            "background:%1;"
            "border:1px solid %2;"
            "border-radius:%3px;"
            "}"

            "QFrame#pileRow:hover{"
            "border-color:#CCD9D2;"
            "}"

            "QLabel#pileCodeLabel{"
            "background:transparent;"
            "border:none;"
            "color:%4;"
            "font-size:%5px;"
            "font-weight:800;"
            "}"

            "QLabel#pileDescriptionLabel{"
            "background:transparent;"
            "border:none;"
            "color:%6;"
            "font-size:%7px;"
            "}")

            .arg(
                UiTheme::surface())

            .arg(
                UiTheme::border())

            .arg(
                cardRadius)

            .arg(
                UiTheme::textPrimary())

            .arg(
                codeFont)

            .arg(
                UiTheme::textSecondary())

            .arg(
                descriptionFont);


    // ========================================================================
    // 状态标签
    // ========================================================================
    const QString statusStyle =
        QStringLiteral(

            "QLabel#pileStatusLabel{"
            "border:none;"
            "border-radius:%1px;"
            "font-size:%2px;"
            "font-weight:700;"
            "padding:5px 10px;"
            "}"

            // 闲置
            "QLabel#pileStatusLabel[pileState=\"idle\"]{"
            "background:#EAF3ED;"
            "color:#4F8668;"
            "}"

            // 在用
            "QLabel#pileStatusLabel[pileState=\"busy\"]{"
            "background:#EDF2F5;"
            "color:#55758A;"
            "}"

            // 故障
            "QLabel#pileStatusLabel[pileState=\"fault\"]{"
            "background:#F7ECEA;"
            "color:#C96C66;"
            "}")

            .arg(
                smallRadius)

            .arg(
                statusFont);


    // ========================================================================
    // 参数面板
    // ========================================================================
    const QString metricsStyle =
        QStringLiteral(

            "QFrame#pileMetricsPanel{"
            "background:%1;"
            "border:1px solid %2;"
            "border-radius:%3px;"
            "}"

            "QLabel#pileMetricCaption{"
            "background:transparent;"
            "border:none;"
            "color:%4;"
            "font-size:%5px;"
            "}"

            "QLabel#pileMetricValue{"
            "background:transparent;"
            "border:none;"
            "color:%6;"
            "font-size:%7px;"
            "font-weight:800;"
            "}"

            "QFrame#pileMetricDivider{"
            "background:%2;"
            "border:none;"
            "max-width:1px;"
            "}")

            .arg(
                UiTheme::surfaceSoft())

            .arg(
                UiTheme::border())

            .arg(
                smallRadius)

            .arg(
                UiTheme::textSecondary())

            .arg(
                metricCaptionFont)

            .arg(
                UiTheme::textPrimary())

            .arg(
                metricValueFont);


    // ========================================================================
    // 可预约提示
    // ========================================================================
    const QString availabilityStyle =
        QStringLiteral(

            "QLabel#pileAvailabilityLabel{"
            "background:transparent;"
            "border:none;"
            "font-size:%1px;"
            "font-weight:600;"
            "}"

            "QLabel#pileAvailabilityLabel[pileAvailable=\"true\"]{"
            "color:%2;"
            "}"

            "QLabel#pileAvailabilityLabel[pileAvailable=\"false\"]{"
            "color:%3;"
            "}")

            .arg(
                tipFont)

            .arg(
                UiTheme::success())

            .arg(
                UiTheme::textSecondary());


    // ========================================================================
    // 预约按钮
    // ========================================================================
    const QString reserveButtonStyle =
        QStringLiteral(

            "QPushButton#pileReserveButton{"
            "background:%1;"
            "color:#FFFFFF;"
            "border:none;"
            "border-radius:%2px;"
            "font-size:%3px;"
            "font-weight:700;"
            "padding:9px 22px;"
            "}"

            "QPushButton#pileReserveButton:hover{"
            "background:%4;"
            "}"

            "QPushButton#pileReserveButton:pressed{"
            "background:#203F36;"
            "}"

            "QPushButton#pileReserveButton:disabled{"
            "background:#E5E6E2;"
            "color:#A1A6A3;"
            "}")

            .arg(
                UiTheme::primary())

            .arg(
                smallRadius)

            .arg(
                buttonFont)

            .arg(
                UiTheme::primaryHover());


    setStyleSheet(
        pageStyle
        + headerStyle
        + backButtonStyle
        + rowStyle
        + statusStyle
        + metricsStyle
        + availabilityStyle
        + reserveButtonStyle);


    // ========================================================================
    // 顶部卡片响应式边距
    // ========================================================================
    if (auto *headerLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "pileHeaderLayout"))) {

        headerLayout->setContentsMargins(
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 16),
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 16));

        headerLayout->setSpacing(
            scaledUi(
                scaleBase,
                8));
    }


    // ========================================================================
    // 列表边距
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
    // 每张电桩卡片边距
    // ========================================================================
    const auto rowLayouts =
        findChildren<QVBoxLayout *>(
            QStringLiteral(
                "pileRowLayout"));


    for (QVBoxLayout *rowLayout :
         rowLayouts) {

        rowLayout->setContentsMargins(
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 16),
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 16));

        rowLayout->setSpacing(
            scaledUi(
                scaleBase,
                11));
    }


    // ========================================================================
    // 参数区边距
    // ========================================================================
    const auto metricLayouts =
        findChildren<QHBoxLayout *>(
            QStringLiteral(
                "pileMetricsLayout"));


    for (QHBoxLayout *metricLayout :
         metricLayouts) {

        metricLayout->setContentsMargins(
            scaledUi(scaleBase, 14),
            scaledUi(scaleBase, 11),
            scaledUi(scaleBase, 14),
            scaledUi(scaleBase, 11));

        metricLayout->setSpacing(
            scaledUi(
                scaleBase,
                14));
    }


    // ========================================================================
    // 底部操作区
    // ========================================================================
    const auto actionLayouts =
        findChildren<QHBoxLayout *>(
            QStringLiteral(
                "pileActionLayout"));


    for (QHBoxLayout *actionLayout :
         actionLayouts) {

        actionLayout->setSpacing(
            scaledUi(
                scaleBase,
                12));
    }


    // ========================================================================
    // 页面内部间距
    // ========================================================================
    if (auto *mainLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "pilePageLayout"))) {

        mainLayout->setSpacing(
            scaledUi(
                scaleBase,
                10));
    }
}
