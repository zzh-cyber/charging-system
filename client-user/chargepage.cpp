#include "chargepage.h"

#include "uitheme.h"
#include "windowhelper.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>


// ============================================================================
// 构造函数
// ============================================================================
ChargePage::ChargePage(
    QWidget *parent)
    : QWidget(parent)
{
    setObjectName(
        QStringLiteral(
            "chargePage"));


    // ========================================================================
    // 页面根布局
    // ========================================================================
    auto *rootLayout =
        new QVBoxLayout(this);

    rootLayout->setContentsMargins(
        0,
        0,
        0,
        0);

    rootLayout->setSpacing(
        0);


    // ========================================================================
    // 可滚动页面
    // ========================================================================
    auto *scrollArea =
        new QScrollArea(this);

    scrollArea->setObjectName(
        QStringLiteral(
            "chargeScrollArea"));

    scrollArea->setWidgetResizable(
        true);

    scrollArea->setFrameShape(
        QFrame::NoFrame);

    scrollArea->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAlwaysOff);


    auto *content =
        new QWidget;

    content->setObjectName(
        QStringLiteral(
            "chargeContent"));


    auto *layout =
        new QVBoxLayout(
            content);

    layout->setObjectName(
        QStringLiteral(
            "chargeContentLayout"));

    layout->setContentsMargins(
        18,
        18,
        18,
        18);

    layout->setSpacing(
        14);


    // ========================================================================
    // 页面标题
    // ========================================================================
    auto *title =
        new QLabel(
            QStringLiteral(
                "充电"),
            content);

    title->setObjectName(
        QStringLiteral(
            "chargeTitle"));


    auto *subtitle =
        new QLabel(
            QStringLiteral(
                "查看当前充电状态、实时记录与订单账单"),
            content);

    subtitle->setObjectName(
        QStringLiteral(
            "chargeSubtitle"));

    subtitle->setWordWrap(
        true);


    layout->addWidget(
        title);

    layout->addWidget(
        subtitle);


    // ========================================================================
    // 当前订单卡
    // ========================================================================
    auto *orderCard =
        new QFrame(content);

    orderCard->setObjectName(
        QStringLiteral(
            "chargeOrderCard"));

    UiTheme::applyCardShadow(
        orderCard,
        18,
        4);


    auto *orderLayout =
        new QVBoxLayout(
            orderCard);

    orderLayout->setObjectName(
        QStringLiteral(
            "chargeOrderLayout"));

    orderLayout->setContentsMargins(
        18,
        18,
        18,
        18);

    orderLayout->setSpacing(
        14);


    // ========================================================================
    // 订单顶部：状态标题 + 状态标签
    // ========================================================================
    auto *orderTopRow =
        new QHBoxLayout;

    orderTopRow->setSpacing(
        12);


    auto *stateBlock =
        new QVBoxLayout;

    stateBlock->setSpacing(
        5);


    m_stateTitle =
        new QLabel(
            QStringLiteral(
                "暂无进行中的充电订单"),
            orderCard);

    m_stateTitle->setObjectName(
        QStringLiteral(
            "chargeStateTitle"));

    m_stateTitle->setWordWrap(
        true);


    m_orderLabel =
        new QLabel(
            QStringLiteral(
                "订单号：--"),
            orderCard);

    m_orderLabel->setObjectName(
        QStringLiteral(
            "chargeOrderNumber"));

    m_orderLabel->setWordWrap(
        true);


    stateBlock->addWidget(
        m_stateTitle);

    stateBlock->addWidget(
        m_orderLabel);


    m_statusLabel =
        new QLabel(
            QStringLiteral(
                "状态：等待预约"),
            orderCard);

    m_statusLabel->setObjectName(
        QStringLiteral(
            "chargeStatusBadge"));

    m_statusLabel->setAlignment(
        Qt::AlignCenter);


    orderTopRow->addLayout(
        stateBlock,
        1);

    orderTopRow->addWidget(
        m_statusLabel,
        0,
        Qt::AlignTop);


    orderLayout->addLayout(
        orderTopRow);


    // ========================================================================
    // 充电实时记录卡
    // ========================================================================
    m_chargingRecordCard =
        new QFrame(
            orderCard);

    m_chargingRecordCard->setObjectName(
        QStringLiteral(
            "chargingRecordCard"));


    auto *recordLayout =
        new QVBoxLayout(
            m_chargingRecordCard);

    recordLayout->setObjectName(
        QStringLiteral(
            "chargingRecordLayout"));

    recordLayout->setContentsMargins(
        14,
        14,
        14,
        14);

    recordLayout->setSpacing(
        12);


    // ========================================================================
    // 实时记录标题
    // ========================================================================
    auto *recordHeader =
        new QHBoxLayout;


    auto *recordTitle =
        new QLabel(
            QStringLiteral(
                "实时充电记录"),
            m_chargingRecordCard);

    recordTitle->setObjectName(
        QStringLiteral(
            "chargeRecordTitle"));


    auto *recordBadge =
        new QLabel(
            QStringLiteral(
                "实时预估"),
            m_chargingRecordCard);

    recordBadge->setObjectName(
        QStringLiteral(
            "chargeRecordBadge"));

    recordBadge->setAlignment(
        Qt::AlignCenter);


    recordHeader->addWidget(
        recordTitle);

    recordHeader->addStretch();

    recordHeader->addWidget(
        recordBadge);


    recordLayout->addLayout(
        recordHeader);


    // ========================================================================
    // 四项实时数据
    // ========================================================================
    auto *metricsGrid =
        new QGridLayout;

    metricsGrid->setObjectName(
        QStringLiteral(
            "chargeMetricsGrid"));

    metricsGrid->setHorizontalSpacing(
        10);

    metricsGrid->setVerticalSpacing(
        10);


    const auto createMetricCard =
        [this](
            const QString &caption,
            const QString &initialValue,
            const QString &role,
            QLabel *&valueLabel) {

            auto *card =
                new QFrame(
                    m_chargingRecordCard);

            card->setObjectName(
                QStringLiteral(
                    "chargeMetricCard"));


            auto *itemLayout =
                new QVBoxLayout(
                    card);

            itemLayout->setObjectName(
                QStringLiteral(
                    "chargeMetricItemLayout"));

            itemLayout->setContentsMargins(
                12,
                10,
                12,
                10);

            itemLayout->setSpacing(
                4);


            auto *captionLabel =
                new QLabel(
                    caption,
                    card);

            captionLabel->setObjectName(
                QStringLiteral(
                    "chargeMetricCaption"));


            valueLabel =
                new QLabel(
                    initialValue,
                    card);

            valueLabel->setObjectName(
                QStringLiteral(
                    "chargeMetricValue"));

            valueLabel->setProperty(
                "metricRole",
                role);


            itemLayout->addWidget(
                captionLabel);

            itemLayout->addWidget(
                valueLabel);


            return card;
        };


    auto *elapsedCard =
        createMetricCard(
            QStringLiteral(
                "已充时间"),
            QStringLiteral(
                "00:00:00"),
            QStringLiteral(
                "time"),
            m_elapsedLabel);


    auto *powerCard =
        createMetricCard(
            QStringLiteral(
                "当前功率"),
            QStringLiteral(
                "-- kW"),
            QStringLiteral(
                "normal"),
            m_powerLabel);


    auto *kwhCard =
        createMetricCard(
            QStringLiteral(
                "已充电量"),
            QStringLiteral(
                "-- kWh"),
            QStringLiteral(
                "normal"),
            m_currentKwhLabel);


    auto *feeCard =
        createMetricCard(
            QStringLiteral(
                "预估费用"),
            QStringLiteral(
                "￥--"),
            QStringLiteral(
                "fee"),
            m_estimatedFeeLabel);


    metricsGrid->addWidget(
        elapsedCard,
        0,
        0);

    metricsGrid->addWidget(
        powerCard,
        0,
        1);

    metricsGrid->addWidget(
        kwhCard,
        1,
        0);

    metricsGrid->addWidget(
        feeCard,
        1,
        1);


    metricsGrid->setColumnStretch(
        0,
        1);

    metricsGrid->setColumnStretch(
        1,
        1);


    recordLayout->addLayout(
        metricsGrid);


    // 默认仅充电中显示
    m_chargingRecordCard->hide();


    orderLayout->addWidget(
        m_chargingRecordCard);


    // ========================================================================
    // 最终账单结果
    // ========================================================================
    m_resultLabel =
        new QLabel(
            orderCard);

    m_resultLabel->setObjectName(
        QStringLiteral(
            "chargeResultLabel"));

    m_resultLabel->setWordWrap(
        true);

    m_resultLabel->setAlignment(
        Qt::AlignLeft |
        Qt::AlignVCenter);

    m_resultLabel->hide();


    orderLayout->addWidget(
        m_resultLabel);


    // ========================================================================
    // 操作按钮
    // ========================================================================
    auto *actionLayout =
        new QHBoxLayout;

    actionLayout->setObjectName(
        QStringLiteral(
            "chargeActionLayout"));

    actionLayout->setSpacing(
        10);


    m_startButton =
        new QPushButton(
            QStringLiteral(
                "开始充电"),
            orderCard);

    m_startButton->setObjectName(
        QStringLiteral(
            "chargeStartButton"));

    m_startButton->setCursor(
        Qt::PointingHandCursor);


    m_settleButton =
        new QPushButton(
            QStringLiteral(
                "结束充电"),
            orderCard);

    m_settleButton->setObjectName(
        QStringLiteral(
            "chargeFinishButton"));

    m_settleButton->setCursor(
        Qt::PointingHandCursor);


    m_payButton =
        new QPushButton(
            QStringLiteral(
                "确认支付"),
            orderCard);

    m_payButton->setObjectName(
        QStringLiteral(
            "chargePayButton"));

    m_payButton->setCursor(
        Qt::PointingHandCursor);


    actionLayout->addWidget(
        m_startButton,
        1);

    actionLayout->addWidget(
        m_settleButton,
        1);

    actionLayout->addWidget(
        m_payButton,
        1);


    orderLayout->addLayout(
        actionLayout);


    layout->addWidget(
        orderCard);


    // ========================================================================
    // 当前提示 / 计费说明
    // ========================================================================
    auto *guideCard =
        new QFrame(
            content);

    guideCard->setObjectName(
        QStringLiteral(
            "chargeGuideCard"));


    auto *guideLayout =
        new QVBoxLayout(
            guideCard);

    guideLayout->setObjectName(
        QStringLiteral(
            "chargeGuideLayout"));

    guideLayout->setContentsMargins(
        16,
        14,
        16,
        14);

    guideLayout->setSpacing(
        7);


    auto *guideTitle =
        new QLabel(
            QStringLiteral(
                "当前提示"),
            guideCard);

    guideTitle->setObjectName(
        QStringLiteral(
            "chargeGuideTitle"));


    m_tipLabel =
        new QLabel(
            QStringLiteral(
                "请先在首页选择充电桩并完成预约"),
            guideCard);

    m_tipLabel->setObjectName(
        QStringLiteral(
            "chargeTipLabel"));

    m_tipLabel->setWordWrap(
        true);

    m_tipLabel->setAlignment(
        Qt::AlignLeft |
        Qt::AlignVCenter);


    auto *billingNote =
        new QLabel(
            QStringLiteral(
                "计费说明：充电中的电量与费用为实时预估，"
                "最终以充电结束后的服务端账单为准。"),
            guideCard);

    billingNote->setObjectName(
        QStringLiteral(
            "chargeBillingNote"));

    billingNote->setWordWrap(
        true);


    guideLayout->addWidget(
        guideTitle);

    guideLayout->addWidget(
        m_tipLabel);

    guideLayout->addWidget(
        billingNote);


    layout->addWidget(
        guideCard);

    layout->addStretch();


    scrollArea->setWidget(
        content);

    rootLayout->addWidget(
        scrollArea);


    // ========================================================================
    // 充电实时计时器
    // ========================================================================
    m_chargeTimer =
        new QTimer(this);

    m_chargeTimer->setInterval(
        1000);


    connect(
        m_chargeTimer,
        &QTimer::timeout,
        this,
        &ChargePage::updateChargingInfo);


    // ========================================================================
    // 开始充电
    // 业务逻辑保持不变
    // ========================================================================
    connect(
        m_startButton,
        &QPushButton::clicked,
        this,
        [this]() {

            if (m_state !=
                    ChargeState::Reserved ||
                m_orderNo.isEmpty()) {

                return;
            }


            emit startChargeRequested(
                m_orderNo);
        });


    // ========================================================================
    // 结束充电
    // 业务逻辑保持不变
    // ========================================================================
    connect(
        m_settleButton,
        &QPushButton::clicked,
        this,
        [this]() {

            if (m_state !=
                    ChargeState::Charging ||
                m_orderNo.isEmpty()) {

                return;
            }


            emit finishChargeRequested(
                m_orderNo);
        });


    // ========================================================================
    // 确认支付
    // 业务逻辑保持不变
    // ========================================================================
    connect(
        m_payButton,
        &QPushButton::clicked,
        this,
        [this]() {

            if (m_state !=
                    ChargeState::PendingPayment ||
                m_orderNo.isEmpty()) {

                return;
            }


            emit payChargeRequested(
                m_orderNo);
        });


    refreshUi();
}


// ============================================================================
// 预约成功
// ============================================================================
void ChargePage::setReservedOrder(
    const QString &orderNo)
{
    m_orderNo =
        orderNo.trimmed();


    if (m_orderNo.isEmpty()) {

        reset();

        return;
    }


    m_state =
        ChargeState::Reserved;


    m_settledKwh =
        0.0;

    m_settledAmount =
        0.0;

    m_finalDurationSeconds =
        0;

    m_finalBalance =
        0.0;


    stopChargeTimer();


    m_chargeStartedAt =
        QDateTime();


    m_powerKw =
        0.0;

    m_unitPrice =
        0.0;


    m_currentKwh =
        0.0;

    m_estimatedAmount =
        0.0;


    refreshUi();
}


// ============================================================================
// 开始充电成功
// ============================================================================
void ChargePage::setChargingState(
    const QString &startTime,
    double powerKw,
    double unitPrice)
{
    if (m_orderNo.isEmpty())
        return;


    m_state =
        ChargeState::Charging;


    // ========================================================================
    // 优先使用服务端 start_time
    // ========================================================================
    if (!startTime
             .trimmed()
             .isEmpty()) {

        QDateTime parsed =
            QDateTime::fromString(
                startTime,
                Qt::ISODate);


        // 兼容 MySQL DATETIME
        if (!parsed.isValid()) {

            parsed =
                QDateTime::fromString(
                    startTime,
                    QStringLiteral(
                        "yyyy-MM-dd HH:mm:ss"));
        }


        if (parsed.isValid()) {

            m_chargeStartedAt =
                parsed;
        }
    }


    // 服务端暂时没返回 start_time
    // 时才使用本机当前时间
    if (!m_chargeStartedAt.isValid()) {

        m_chargeStartedAt =
            QDateTime::currentDateTime();
    }


    // 当前服务端没有这两个字段时会得到 0
    // 等服务端补接口后这里无需再次修改
    if (powerKw > 0.0) {

        m_powerKw =
            powerKw;
    }


    if (unitPrice > 0.0) {

        m_unitPrice =
            unitPrice;
    }


    startChargeTimer();

    refreshUi();
}


// ============================================================================
// 结束充电成功 -> 待支付
// ============================================================================
void ChargePage::setPendingPaymentResult(
    qint64 durationSeconds,
    double kwh,
    double amount)
{
    if (m_orderNo.isEmpty())
        return;


    stopChargeTimer();


    m_state =
        ChargeState::PendingPayment;


    m_finalDurationSeconds =
        qMax<qint64>(
            0,
            durationSeconds);


    m_settledKwh =
        qMax(
            0.0,
            kwh);


    m_settledAmount =
        qMax(
            0.0,
            amount);


    refreshUi();
}


// ============================================================================
// 支付成功
// ============================================================================
void ChargePage::setPaidResult(
    qint64 durationSeconds,
    double kwh,
    double amount,
    double balance)
{
    if (m_orderNo.isEmpty())
        return;


    stopChargeTimer();


    m_state =
        ChargeState::Paid;


    m_finalDurationSeconds =
        qMax<qint64>(
            0,
            durationSeconds);


    m_settledKwh =
        qMax(
            0.0,
            kwh);


    m_settledAmount =
        qMax(
            0.0,
            amount);


    m_finalBalance =
        qMax(
            0.0,
            balance);


    refreshUi();
}


// ============================================================================
// 清空订单
// ============================================================================
void ChargePage::reset()
{
    m_state =
        ChargeState::Empty;


    m_orderNo.clear();


    m_settledKwh =
        0.0;

    m_settledAmount =
        0.0;


    stopChargeTimer();


    m_chargeStartedAt =
        QDateTime();


    m_powerKw =
        0.0;

    m_unitPrice =
        0.0;


    m_currentKwh =
        0.0;

    m_estimatedAmount =
        0.0;


    m_finalDurationSeconds =
        0;

    m_finalBalance =
        0.0;


    refreshUi();
}


// ============================================================================
// 根据订单状态刷新 UI
// ============================================================================
void ChargePage::refreshUi()
{
    const auto formatDuration =
        [](qint64 totalSeconds) {

            totalSeconds =
                qMax<qint64>(
                    0,
                    totalSeconds);


            const qint64 hours =
                totalSeconds /
                3600;


            const qint64 minutes =
                (totalSeconds %
                 3600) /
                60;


            const qint64 seconds =
                totalSeconds %
                60;


            return
                QStringLiteral(
                    "%1:%2:%3")
                    .arg(
                        hours,
                        2,
                        10,
                        QChar('0'))
                    .arg(
                        minutes,
                        2,
                        10,
                        QChar('0'))
                    .arg(
                        seconds,
                        2,
                        10,
                        QChar('0'));
        };


    switch (m_state) {

    // ========================================================================
    // 无订单
    // ========================================================================
    case ChargeState::Empty:

        m_stateTitle->setText(
            QStringLiteral(
                "暂无进行中的充电订单"));


        m_orderLabel->setText(
            QStringLiteral(
                "订单号：--"));


        m_statusLabel->setText(
            QStringLiteral(
                "等待预约"));


        m_startButton->show();

        m_startButton->setEnabled(
            false);


        m_settleButton->hide();

        m_payButton->hide();


        m_chargingRecordCard->hide();

        m_resultLabel->hide();


        m_tipLabel->setText(
            QStringLiteral(
                "请先在首页选择充电桩并完成预约"));

        break;


    // ========================================================================
    // 已预约
    // ========================================================================
    case ChargeState::Reserved:

        m_stateTitle->setText(
            QStringLiteral(
                "充电订单已预约"));


        m_orderLabel->setText(
            QStringLiteral(
                "订单号：%1")
                .arg(
                    m_orderNo));


        m_statusLabel->setText(
            QStringLiteral(
                "已预约"));


        m_startButton->show();

        m_startButton->setEnabled(
            true);


        m_settleButton->hide();

        m_payButton->hide();


        m_chargingRecordCard->hide();

        m_resultLabel->hide();


        m_tipLabel->setText(
            QStringLiteral(
                "预约成功，可以开始充电"));

        break;


    // ========================================================================
    // 充电中
    // ========================================================================
    case ChargeState::Charging:

        m_stateTitle->setText(
            QStringLiteral(
                "正在充电"));


        m_orderLabel->setText(
            QStringLiteral(
                "订单号：%1")
                .arg(
                    m_orderNo));


        m_statusLabel->setText(
            QStringLiteral(
                "充电中"));


        m_startButton->hide();


        m_settleButton->show();

        m_settleButton->setEnabled(
            true);


        m_payButton->hide();


        m_resultLabel->hide();


        m_chargingRecordCard->show();


        updateChargingInfo();


        m_tipLabel->setText(
            QStringLiteral(
                "正在充电，可实时查看本次充电记录"));

        break;


    // ========================================================================
    // 待支付
    // ========================================================================
    case ChargeState::PendingPayment:
    {
        m_stateTitle->setText(
            QStringLiteral(
                "充电已结束"));


        m_orderLabel->setText(
            QStringLiteral(
                "订单号：%1")
                .arg(
                    m_orderNo));


        m_statusLabel->setText(
            QStringLiteral(
                "待支付"));


        m_startButton->hide();

        m_settleButton->hide();


        m_chargingRecordCard->hide();


        const QString duration =
            formatDuration(
                m_finalDurationSeconds);


        m_resultLabel->setText(
            QStringLiteral(
                "充电时长    %1\n"
                "充电电量    %2 kWh\n"
                "本次费用    ￥%3")
                .arg(
                    duration)
                .arg(
                    m_settledKwh,
                    0,
                    'f',
                    2)
                .arg(
                    m_settledAmount,
                    0,
                    'f',
                    2));


        m_resultLabel->show();


        m_payButton->show();

        m_payButton->setEnabled(
            true);


        m_tipLabel->setText(
            QStringLiteral(
                "请确认最终账单后完成支付"));

        break;
    }


    // ========================================================================
    // 已支付
    // ========================================================================
    case ChargeState::Paid:
    {
        m_stateTitle->setText(
            QStringLiteral(
                "支付成功"));


        m_orderLabel->setText(
            QStringLiteral(
                "订单号：%1")
                .arg(
                    m_orderNo));


        m_statusLabel->setText(
            QStringLiteral(
                "已支付"));


        m_startButton->hide();

        m_settleButton->hide();

        m_payButton->hide();


        m_chargingRecordCard->hide();


        const QString duration =
            formatDuration(
                m_finalDurationSeconds);


        // 必须保留：
        // 支付成功
        // 充电时长
        // 本次费用
        // 当前余额
        m_resultLabel->setText(
            QStringLiteral(
                "充电时长    %1\n"
                "本次费用    ￥%2\n"
                "当前余额    ￥%3")
                .arg(
                    duration)
                .arg(
                    m_settledAmount,
                    0,
                    'f',
                    2)
                .arg(
                    m_finalBalance,
                    0,
                    'f',
                    2));


        m_resultLabel->show();


        m_tipLabel->setText(
            QStringLiteral(
                "本次充电订单已完成"));

        break;
    }
    }


    applyResponsiveStyle();
}


// ============================================================================
// Resize
// ============================================================================
void ChargePage::resizeEvent(
    QResizeEvent *event)
{
    QWidget::resizeEvent(
        event);

    applyResponsiveStyle();
}


// ============================================================================
// 响应式样式
// ============================================================================
void ChargePage::applyResponsiveStyle()
{
    QWidget *scaleBase =
        window()
            ? window()
            : this;


    const int titleFont =
        scaledUi(
            scaleBase,
            24);

    const int stateTitleFont =
        scaledUi(
            scaleBase,
            20);

    const int normalFont =
        scaledUi(
            scaleBase,
            14);

    const int smallFont =
        scaledUi(
            scaleBase,
            12);

    const int tinyFont =
        scaledUi(
            scaleBase,
            11);

    const int metricValueFont =
        scaledUi(
            scaleBase,
            17);

    const int buttonFont =
        scaledUi(
            scaleBase,
            14);

    const int cardRadius =
        scaledUi(
            scaleBase,
            18);

    const int smallRadius =
        scaledUi(
            scaleBase,
            10);


    // ========================================================================
    // 页面基础
    // ========================================================================
    const QString pageStyle =
        QStringLiteral(

            "QWidget#chargePage{"
            "background:transparent;"
            "color:%1;"
            "}"

            "QWidget#chargeContent{"
            "background:transparent;"
            "}"

            "QScrollArea#chargeScrollArea{"
            "background:transparent;"
            "border:none;"
            "}"

            "QLabel#chargeTitle{"
            "background:transparent;"
            "color:%1;"
            "font-size:%2px;"
            "font-weight:800;"
            "}"

            "QLabel#chargeSubtitle{"
            "background:transparent;"
            "color:%3;"
            "font-size:%4px;"
            "}")

            .arg(
                UiTheme::textPrimary())

            .arg(
                titleFont)

            .arg(
                UiTheme::textSecondary())

            .arg(
                smallFont);


    // ========================================================================
    // 订单卡
    // ========================================================================
    const QString orderStyle =
        QStringLiteral(

            "QFrame#chargeOrderCard{"
            "background:%1;"
            "border:1px solid %2;"
            "border-radius:%3px;"
            "}"

            "QLabel#chargeStateTitle{"
            "background:transparent;"
            "border:none;"
            "color:%4;"
            "font-size:%5px;"
            "font-weight:800;"
            "}"

            "QLabel#chargeOrderNumber{"
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
                stateTitleFont)

            .arg(
                UiTheme::textSecondary())

            .arg(
                smallFont);


    // ========================================================================
    // 实时记录
    // ========================================================================
    const QString recordStyle =
        QStringLiteral(

            "QFrame#chargingRecordCard{"
            "background:%1;"
            "border:1px solid %2;"
            "border-radius:%3px;"
            "}"

            "QLabel#chargeRecordTitle{"
            "background:transparent;"
            "border:none;"
            "color:%4;"
            "font-size:%5px;"
            "font-weight:700;"
            "}"

            "QLabel#chargeRecordBadge{"
            "background:%6;"
            "border:none;"
            "border-radius:%7px;"
            "color:%8;"
            "font-size:%9px;"
            "font-weight:700;"
            "padding:4px 8px;"
            "}"

            "QFrame#chargeMetricCard{"
            "background:%10;"
            "border:1px solid %2;"
            "border-radius:%7px;"
            "}"

            "QLabel#chargeMetricCaption{"
            "background:transparent;"
            "border:none;"
            "color:%11;"
            "font-size:%9px;"
            "}"

            "QLabel#chargeMetricValue{"
            "background:transparent;"
            "border:none;"
            "color:%4;"
            "font-size:%12px;"
            "font-weight:800;"
            "}"

            "QLabel#chargeMetricValue[metricRole=\"time\"]{"
            "color:%8;"
            "}"

            "QLabel#chargeMetricValue[metricRole=\"fee\"]{"
            "color:%13;"
            "}")

            .arg(
                UiTheme::surfaceSoft())       // %1

            .arg(
                UiTheme::border())            // %2

            .arg(
                cardRadius)                   // %3

            .arg(
                UiTheme::textPrimary())       // %4

            .arg(
                normalFont)                   // %5

            .arg(
                UiTheme::primarySoft())       // %6

            .arg(
                smallRadius)                  // %7

            .arg(
                UiTheme::primary())           // %8

            .arg(
                tinyFont)                     // %9

            .arg(
                UiTheme::surface())           // %10

            .arg(
                UiTheme::textSecondary())     // %11

            .arg(
                metricValueFont)              // %12

            .arg(
                UiTheme::accent());           // %13


    // ========================================================================
    // 提示说明卡
    // ========================================================================
    const QString guideStyle =
        QStringLiteral(

            "QFrame#chargeGuideCard{"
            "background:%1;"
            "border:1px solid %2;"
            "border-radius:%3px;"
            "}"

            "QLabel#chargeGuideTitle{"
            "background:transparent;"
            "color:%4;"
            "font-size:%5px;"
            "font-weight:700;"
            "}"

            "QLabel#chargeTipLabel{"
            "background:transparent;"
            "color:%4;"
            "font-size:%6px;"
            "}"

            "QLabel#chargeBillingNote{"
            "background:transparent;"
            "color:%7;"
            "font-size:%8px;"
            "}")

            .arg(
                UiTheme::surfaceSoft())

            .arg(
                UiTheme::border())

            .arg(
                cardRadius)

            .arg(
                UiTheme::textPrimary())

            .arg(
                normalFont)

            .arg(
                smallFont)

            .arg(
                UiTheme::textSecondary())

            .arg(
                tinyFont);


    // ========================================================================
    // 操作按钮
    // ========================================================================
    const QString buttonStyle =
        QStringLiteral(

            // 开始充电
            "QPushButton#chargeStartButton{"
            "background:%1;"
            "color:#FFFFFF;"
            "border:none;"
            "border-radius:%2px;"
            "font-size:%3px;"
            "font-weight:700;"
            "padding:11px 18px;"
            "}"

            "QPushButton#chargeStartButton:hover{"
            "background:%4;"
            "}"

            "QPushButton#chargeStartButton:disabled{"
            "background:#E3E5E2;"
            "color:#A0A5A2;"
            "}"

            // 结束充电
            "QPushButton#chargeFinishButton{"
            "background:#F8EFEC;"
            "color:#B65F59;"
            "border:1px solid #E9CFCA;"
            "border-radius:%2px;"
            "font-size:%3px;"
            "font-weight:700;"
            "padding:11px 18px;"
            "}"

            "QPushButton#chargeFinishButton:hover{"
            "background:#F2E2DE;"
            "}"

            "QPushButton#chargeFinishButton:disabled{"
            "background:#F1F1EE;"
            "color:#A5AAA6;"
            "border-color:#E3E3DE;"
            "}"

            // 确认支付
            "QPushButton#chargePayButton{"
            "background:%1;"
            "color:#FFFFFF;"
            "border:none;"
            "border-radius:%2px;"
            "font-size:%3px;"
            "font-weight:700;"
            "padding:11px 18px;"
            "}"

            "QPushButton#chargePayButton:hover{"
            "background:%4;"
            "}"

            "QPushButton#chargePayButton:disabled{"
            "background:#E3E5E2;"
            "color:#A0A5A2;"
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
        + orderStyle
        + recordStyle
        + guideStyle
        + buttonStyle);


    // ========================================================================
    // 状态 Badge
    // ========================================================================
    if (m_statusLabel) {

        QString background =
            QStringLiteral(
                "#F1F0EC");

        QString color =
            UiTheme::textSecondary();


        switch (m_state) {

        case ChargeState::Empty:

            background =
                QStringLiteral(
                    "#F1F0EC");

            color =
                UiTheme::textSecondary();

            break;


        case ChargeState::Reserved:

            background =
                QStringLiteral(
                    "#FFF3DF");

            color =
                QStringLiteral(
                    "#A86D1E");

            break;


        case ChargeState::Charging:

            background =
                QStringLiteral(
                    "#EAF3ED");

            color =
                UiTheme::success();

            break;


        case ChargeState::PendingPayment:

            background =
                QStringLiteral(
                    "#FFF3DF");

            color =
                QStringLiteral(
                    "#A86D1E");

            break;


        case ChargeState::Paid:

            background =
                QStringLiteral(
                    "#EAF3ED");

            color =
                UiTheme::primary();

            break;
        }


        m_statusLabel->setStyleSheet(
            QStringLiteral(
                "QLabel#chargeStatusBadge{"
                "background:%1;"
                "color:%2;"
                "border:none;"
                "border-radius:%3px;"
                "font-size:%4px;"
                "font-weight:700;"
                "padding:6px 10px;"
                "}")
                .arg(
                    background)
                .arg(
                    color)
                .arg(
                    smallRadius)
                .arg(
                    tinyFont));
    }


    // ========================================================================
    // 最终账单区域
    // ========================================================================
    if (m_resultLabel) {

        QString background =
            UiTheme::surfaceSoft();

        QString border =
            UiTheme::border();

        QString color =
            UiTheme::textPrimary();


        if (m_state ==
            ChargeState::PendingPayment) {

            background =
                QStringLiteral(
                    "#FFF8EA");

            border =
                QStringLiteral(
                    "#F0DDBA");

            color =
                QStringLiteral(
                    "#8E6222");

        } else if (
            m_state ==
            ChargeState::Paid) {

            background =
                QStringLiteral(
                    "#EDF5F0");

            border =
                QStringLiteral(
                    "#D5E4DB");

            color =
                UiTheme::primary();
        }


        m_resultLabel->setStyleSheet(
            QStringLiteral(
                "QLabel#chargeResultLabel{"
                "background:%1;"
                "color:%2;"
                "border:1px solid %3;"
                "border-radius:%4px;"
                "font-size:%5px;"
                "font-weight:650;"
                "padding:%6px;"
                "}")
                .arg(
                    background)
                .arg(
                    color)
                .arg(
                    border)
                .arg(
                    smallRadius)
                .arg(
                    normalFont)
                .arg(
                    scaledUi(
                        scaleBase,
                        14)));
    }


    // ========================================================================
    // 页面内容边距
    // ========================================================================
    if (auto *contentLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "chargeContentLayout"))) {

        contentLayout->setContentsMargins(
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 18));

        contentLayout->setSpacing(
            scaledUi(
                scaleBase,
                14));
    }


    // ========================================================================
    // 订单卡边距
    // ========================================================================
    if (auto *orderLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "chargeOrderLayout"))) {

        orderLayout->setContentsMargins(
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 18));

        orderLayout->setSpacing(
            scaledUi(
                scaleBase,
                14));
    }


    // ========================================================================
    // 实时记录边距
    // ========================================================================
    if (auto *recordLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "chargingRecordLayout"))) {

        recordLayout->setContentsMargins(
            scaledUi(scaleBase, 14),
            scaledUi(scaleBase, 14),
            scaledUi(scaleBase, 14),
            scaledUi(scaleBase, 14));

        recordLayout->setSpacing(
            scaledUi(
                scaleBase,
                12));
    }


    // ========================================================================
    // 实时指标卡边距
    // ========================================================================
    const auto metricLayouts =
        findChildren<QVBoxLayout *>(
            QStringLiteral(
                "chargeMetricItemLayout"));


    for (QVBoxLayout *metricLayout :
         metricLayouts) {

        metricLayout->setContentsMargins(
            scaledUi(scaleBase, 12),
            scaledUi(scaleBase, 10),
            scaledUi(scaleBase, 12),
            scaledUi(scaleBase, 10));

        metricLayout->setSpacing(
            scaledUi(
                scaleBase,
                4));
    }


    // ========================================================================
    // 按钮区域
    // ========================================================================
    if (auto *actionLayout =
            findChild<QHBoxLayout *>(
                QStringLiteral(
                    "chargeActionLayout"))) {

        actionLayout->setSpacing(
            scaledUi(
                scaleBase,
                10));
    }


    // ========================================================================
    // 提示卡
    // ========================================================================
    if (auto *guideLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "chargeGuideLayout"))) {

        guideLayout->setContentsMargins(
            scaledUi(scaleBase, 16),
            scaledUi(scaleBase, 14),
            scaledUi(scaleBase, 16),
            scaledUi(scaleBase, 14));

        guideLayout->setSpacing(
            scaledUi(
                scaleBase,
                7));
    }
}


// ============================================================================
// 开始计时
// ============================================================================
void ChargePage::startChargeTimer()
{
    if (!m_chargeTimer)
        return;


    if (!m_chargeStartedAt.isValid()) {

        m_chargeStartedAt =
            QDateTime::currentDateTime();
    }


    updateChargingInfo();


    if (!m_chargeTimer->isActive()) {

        m_chargeTimer->start();
    }
}


// ============================================================================
// 停止计时
// ============================================================================
void ChargePage::stopChargeTimer()
{
    if (!m_chargeTimer)
        return;


    if (m_chargeTimer->isActive()) {

        m_chargeTimer->stop();
    }
}


// ============================================================================
// 更新实时充电记录
// ============================================================================
void ChargePage::updateChargingInfo()
{
    if (m_state !=
        ChargeState::Charging) {

        return;
    }


    if (!m_chargeStartedAt.isValid())
        return;


    qint64 elapsedSeconds =
        m_chargeStartedAt.secsTo(
            QDateTime::currentDateTime());


    if (elapsedSeconds < 0) {

        elapsedSeconds =
            0;
    }


    // ========================================================================
    // 已充时间
    // ========================================================================
    const qint64 hours =
        elapsedSeconds /
        3600;


    const qint64 minutes =
        (elapsedSeconds %
         3600) /
        60;


    const qint64 seconds =
        elapsedSeconds %
        60;


    if (m_elapsedLabel) {

        m_elapsedLabel->setText(
            QStringLiteral(
                "%1:%2:%3")
                .arg(
                    hours,
                    2,
                    10,
                    QChar('0'))
                .arg(
                    minutes,
                    2,
                    10,
                    QChar('0'))
                .arg(
                    seconds,
                    2,
                    10,
                    QChar('0')));
    }


    // ========================================================================
    // 当前功率 + 已充电量
    // ========================================================================
    if (m_powerKw > 0.0) {

        if (m_powerLabel) {

            m_powerLabel->setText(
                QStringLiteral(
                    "%1 kW")
                    .arg(
                        m_powerKw,
                        0,
                        'f',
                        1));
        }


        // 原有公式保持不变
        m_currentKwh =
            m_powerKw *
            static_cast<double>(
                elapsedSeconds) /
            3600.0;


        if (m_currentKwhLabel) {

            m_currentKwhLabel->setText(
                QStringLiteral(
                    "%1 kWh")
                    .arg(
                        m_currentKwh,
                        0,
                        'f',
                        2));
        }

    } else {

        if (m_powerLabel) {

            m_powerLabel->setText(
                QStringLiteral(
                    "-- kW"));
        }


        if (m_currentKwhLabel) {

            m_currentKwhLabel->setText(
                QStringLiteral(
                    "-- kWh"));
        }
    }


    // ========================================================================
    // 预估费用
    // ========================================================================
    if (m_powerKw > 0.0 &&
        m_unitPrice > 0.0) {

        // 原有公式保持不变
        m_estimatedAmount =
            m_currentKwh *
            m_unitPrice;


        if (m_estimatedFeeLabel) {

            m_estimatedFeeLabel->setText(
                QStringLiteral(
                    "￥%1")
                    .arg(
                        m_estimatedAmount,
                        0,
                        'f',
                        2));
        }

    } else {

        if (m_estimatedFeeLabel) {

            m_estimatedFeeLabel->setText(
                QStringLiteral(
                    "￥--"));
        }
    }
}
