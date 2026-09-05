#include "chargepage.h"
#include "windowhelper.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QTimer>



ChargePage::ChargePage(QWidget *parent)
    : QWidget(parent)
{
    // ========================================================================
    // 总布局
    // ========================================================================
    auto *layout = new QVBoxLayout(this);

    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(14);

    // ========================================================================
    // 页面标题
    // ========================================================================
    auto *title =
        new QLabel(QStringLiteral("充电"), this);
    title->setObjectName(
    QStringLiteral("chargeTitle"));


    title->setStyleSheet(
        "font-size:24px;"
        "font-weight:700;"
        "color:#1f2329;");

    auto *subtitle =
        new QLabel(
            QStringLiteral("查看当前订单并完成充电结算"),
            this);
    subtitle->setObjectName(
    QStringLiteral("chargeSubtitle"));


    subtitle->setStyleSheet(
        "color:#86909c;"
        "font-size:13px;");

    layout->addWidget(title);
    layout->addWidget(subtitle);

    // ========================================================================
    // 当前订单卡片
    // ========================================================================
    auto *orderCard = new QFrame(this);
    orderCard->setObjectName(
    QStringLiteral("chargeOrderCard"));


    orderCard->setStyleSheet(
        "QFrame{"
        "background:#ffffff;"
        "border:1px solid #d6e4ff;"
        "border-radius:14px;"
        "}");

    auto *orderLayout =
        new QVBoxLayout(orderCard);

    orderLayout->setContentsMargins(
        18, 18, 18, 18);

    orderLayout->setSpacing(12);

    m_stateTitle =
        new QLabel(
            QStringLiteral("暂无进行中的充电订单"),
            orderCard);

    m_stateTitle->setStyleSheet(
        "border:none;"
        "font-size:18px;"
        "font-weight:700;"
        "color:#1f2329;");

    m_orderLabel =
        new QLabel(
            QStringLiteral("订单号：--"),
            orderCard);

    m_orderLabel->setStyleSheet(
        "border:none;"
        "color:#4e5969;"
        "font-size:14px;");

    m_statusLabel =
        new QLabel(
            QStringLiteral("状态：等待预约"),
            orderCard);

    m_statusLabel->setStyleSheet(
        "border:none;"
        "color:#86909c;"
        "font-size:14px;");

    orderLayout->addWidget(m_stateTitle);
    orderLayout->addWidget(m_orderLabel);
    orderLayout->addWidget(m_statusLabel);
        // ========================================================================
    // 充电实时记录
    // ========================================================================
    m_chargingRecordCard =
        new QFrame(orderCard);

    m_chargingRecordCard->setObjectName(
        QStringLiteral("chargingRecordCard"));

    auto *recordLayout =
        new QVBoxLayout(
            m_chargingRecordCard);

    recordLayout->setContentsMargins(
        14, 12, 14, 12);

    recordLayout->setSpacing(8);


    // 已充时间
    auto *elapsedRow =
        new QHBoxLayout;

    auto *elapsedTitle =
        new QLabel(
            QStringLiteral("已充时间"),
            m_chargingRecordCard);

    m_elapsedLabel =
        new QLabel(
            QStringLiteral("00:00:00"),
            m_chargingRecordCard);

    elapsedRow->addWidget(elapsedTitle);
    elapsedRow->addStretch();
    elapsedRow->addWidget(m_elapsedLabel);

    recordLayout->addLayout(elapsedRow);


    // 当前功率
    auto *powerRow =
        new QHBoxLayout;

    auto *powerTitle =
        new QLabel(
            QStringLiteral("当前功率"),
            m_chargingRecordCard);

    m_powerLabel =
        new QLabel(
            QStringLiteral("-- kW"),
            m_chargingRecordCard);

    powerRow->addWidget(powerTitle);
    powerRow->addStretch();
    powerRow->addWidget(m_powerLabel);

    recordLayout->addLayout(powerRow);


    // 已充电量
    auto *currentKwhRow =
        new QHBoxLayout;

    auto *currentKwhTitle =
        new QLabel(
            QStringLiteral("已充电量"),
            m_chargingRecordCard);

    m_currentKwhLabel =
        new QLabel(
            QStringLiteral("-- kWh"),
            m_chargingRecordCard);

    currentKwhRow->addWidget(currentKwhTitle);
    currentKwhRow->addStretch();
    currentKwhRow->addWidget(m_currentKwhLabel);

    recordLayout->addLayout(currentKwhRow);


    // 预估费用
    auto *feeRow =
        new QHBoxLayout;

    auto *feeTitle =
        new QLabel(
            QStringLiteral("预估费用"),
            m_chargingRecordCard);

    m_estimatedFeeLabel =
        new QLabel(
            QStringLiteral("￥--"),
            m_chargingRecordCard);

    feeRow->addWidget(feeTitle);
    feeRow->addStretch();
    feeRow->addWidget(m_estimatedFeeLabel);

    recordLayout->addLayout(feeRow);


    // 默认隐藏，仅充电中显示
    m_chargingRecordCard->hide();

    orderLayout->addWidget(
        m_chargingRecordCard);


    // ========================================================================
    // 开始充电
    // ========================================================================
    m_startButton =
        new QPushButton(
            QStringLiteral("开始充电"),
            orderCard);

    m_startButton->setCursor(
        Qt::PointingHandCursor);

    orderLayout->addWidget(m_startButton);


    // ========================================================================
    // 结束并结算
    // ========================================================================
    m_settleButton =
    new QPushButton(
        QStringLiteral("结束充电"),
        orderCard);


    m_settleButton->setCursor(
        Qt::PointingHandCursor);

    orderLayout->addWidget(m_settleButton);

    // ========================================================================
    // 结算结果
    // ========================================================================
    m_resultLabel =
        new QLabel(orderCard);

    m_resultLabel->setWordWrap(true);

    m_resultLabel->setStyleSheet(
        "border:none;"
        "background:#f2f7ff;"
        "border-radius:8px;"
        "padding:10px;"
        "color:#1d4ed8;"
        "font-size:14px;"
        "font-weight:600;");

    orderLayout->addWidget(m_resultLabel);
    m_payButton =
    new QPushButton(
        QStringLiteral("确认支付"),
        orderCard);

m_payButton->setCursor(
    Qt::PointingHandCursor);

m_payButton->hide();

orderLayout->addWidget(
    m_payButton);

    layout->addWidget(orderCard);

    // ========================================================================
    // 提示区域
    // ========================================================================
    m_tipLabel =
        new QLabel(
            QStringLiteral(
                "请先在首页选择充电桩并完成预约"),
            this);

    m_tipLabel->setAlignment(
        Qt::AlignCenter);

    m_tipLabel->setWordWrap(true);

    m_tipLabel->setStyleSheet(
        "color:#86909c;"
        "font-size:13px;"
        "padding:8px;");

    layout->addWidget(m_tipLabel);

    layout->addStretch();
        // ========================================================================
    // 充电实时计时器
    // ========================================================================
    m_chargeTimer =
        new QTimer(this);

    m_chargeTimer->setInterval(1000);

    connect(
        m_chargeTimer,
        &QTimer::timeout,
        this,
        &ChargePage::updateChargingInfo);


    // ========================================================================
    // 按钮事件
    // ========================================================================
    connect(
        m_startButton,
        &QPushButton::clicked,
        this,
        [this]() {

            if (m_state != ChargeState::Reserved ||
                m_orderNo.isEmpty()) {
                return;
            }

            emit startChargeRequested(
                m_orderNo);
        });

connect(
    m_settleButton,
    &QPushButton::clicked,
    this,
    [this]() {

        if (m_state != ChargeState::Charging ||
            m_orderNo.isEmpty()) {
            return;
        }

        emit finishChargeRequested(
            m_orderNo);
    });

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

    m_settledKwh = 0.0;
    m_settledAmount = 0.0;
    m_finalDurationSeconds = 0;
m_finalBalance = 0.0;


    stopChargeTimer();

    m_chargeStartedAt =
        QDateTime();

    m_powerKw = 0.0;
    m_unitPrice = 0.0;

    m_currentKwh = 0.0;
    m_estimatedAmount = 0.0;

    refreshUi();

}

void ChargePage::setChargingState(
    const QString &startTime,
    double powerKw,
    double unitPrice)
{
    if (m_orderNo.isEmpty())
        return;

    m_state =
        ChargeState::Charging;

    // ------------------------------------------------------------
    // 优先使用服务端 start_time
    // ------------------------------------------------------------
    if (!startTime.trimmed().isEmpty()) {

        QDateTime parsed =
            QDateTime::fromString(
                startTime,
                Qt::ISODate);

        // 兼容 MySQL 常见 DATETIME 格式
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

    // 服务端暂时没返回 start_time 时才使用本机当前时间
    if (!m_chargeStartedAt.isValid()) {

        m_chargeStartedAt =
            QDateTime::currentDateTime();
    }

    // 当前服务端没有这两个字段时会得到 0，
    // 等服务端补接口后这里无需再次修改。
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



void ChargePage::reset()
{
    m_state =
        ChargeState::Empty;

    m_orderNo.clear();

    m_settledKwh = 0.0;
m_settledAmount = 0.0;

stopChargeTimer();

m_chargeStartedAt =
    QDateTime();

m_powerKw = 0.0;
m_unitPrice = 0.0;

m_currentKwh = 0.0;
m_estimatedAmount = 0.0;
m_finalDurationSeconds = 0;
m_finalBalance = 0.0;


refreshUi();

}

void ChargePage::refreshUi()
{
    const auto formatDuration =
        [](qint64 totalSeconds) {

            totalSeconds =
                qMax<qint64>(
                    0,
                    totalSeconds);

            const qint64 hours =
                totalSeconds / 3600;

            const qint64 minutes =
                (totalSeconds % 3600) / 60;

            const qint64 seconds =
                totalSeconds % 60;

            return QStringLiteral("%1:%2:%3")
                .arg(hours, 2, 10, QChar('0'))
                .arg(minutes, 2, 10, QChar('0'))
                .arg(seconds, 2, 10, QChar('0'));
        };

    switch (m_state) {

    case ChargeState::Empty:

        m_stateTitle->setText(
            QStringLiteral(
                "暂无进行中的充电订单"));

        m_orderLabel->setText(
            QStringLiteral(
                "订单号：--"));

        m_statusLabel->setText(
            QStringLiteral(
                "状态：等待预约"));

        m_startButton->show();
        m_startButton->setEnabled(false);

        m_settleButton->hide();
        m_payButton->hide();

        m_chargingRecordCard->hide();
        m_resultLabel->hide();

        m_tipLabel->setText(
            QStringLiteral(
                "请先在首页选择充电桩并完成预约"));

        break;


    case ChargeState::Reserved:

        m_stateTitle->setText(
            QStringLiteral(
                "充电订单已预约"));

        m_orderLabel->setText(
            QStringLiteral(
                "订单号：%1")
                .arg(m_orderNo));

        m_statusLabel->setText(
            QStringLiteral(
                "状态：已预约"));

        m_startButton->show();
        m_startButton->setEnabled(true);

        m_settleButton->hide();
        m_payButton->hide();

        m_chargingRecordCard->hide();
        m_resultLabel->hide();

        m_tipLabel->setText(
            QStringLiteral(
                "预约成功，可以开始充电"));

        break;


    case ChargeState::Charging:

        m_stateTitle->setText(
            QStringLiteral(
                "正在充电"));

        m_orderLabel->setText(
            QStringLiteral(
                "订单号：%1")
                .arg(m_orderNo));

        m_statusLabel->setText(
            QStringLiteral(
                "状态：充电中"));

        m_startButton->hide();

        m_settleButton->show();
        m_settleButton->setEnabled(true);

        m_payButton->hide();

        m_resultLabel->hide();

        m_chargingRecordCard->show();

        updateChargingInfo();

        m_tipLabel->setText(
            QStringLiteral(
                "正在充电，可实时查看充电记录"));

        break;


    case ChargeState::PendingPayment:
    {
        m_stateTitle->setText(
            QStringLiteral(
                "充电已结束"));

        m_orderLabel->setText(
            QStringLiteral(
                "订单号：%1")
                .arg(m_orderNo));

        m_statusLabel->setText(
            QStringLiteral(
                "状态：待支付"));

        m_startButton->hide();
        m_settleButton->hide();

        m_chargingRecordCard->hide();

        const QString duration =
            formatDuration(
                m_finalDurationSeconds);

        m_resultLabel->setText(
            QStringLiteral(
                "充电时长  %1\n"
                "充电电量  %2 kWh\n"
                "本次费用  ￥%3")
                .arg(duration)
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
        m_payButton->setEnabled(true);

        m_tipLabel->setText(
            QStringLiteral(
                "请确认账单后完成支付"));

        break;
    }


    case ChargeState::Paid:
    {
        m_stateTitle->setText(
            QStringLiteral(
                "支付成功"));

        m_orderLabel->setText(
            QStringLiteral(
                "订单号：%1")
                .arg(m_orderNo));

        m_statusLabel->setText(
            QStringLiteral(
                "状态：已支付"));

        m_startButton->hide();
        m_settleButton->hide();
        m_payButton->hide();

        m_chargingRecordCard->hide();

        const QString duration =
            formatDuration(
                m_finalDurationSeconds);

        m_resultLabel->setText(
            QStringLiteral(
                "充电时长  %1\n"
                "本次费用  ￥%2\n"
                "当前余额  ￥%3")
                .arg(duration)
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

void ChargePage::resizeEvent(
    QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    applyResponsiveStyle();
}


void ChargePage::applyResponsiveStyle()
{
    QWidget *scaleBase =
        window()
            ? window()
            : this;

    const int titleFont =
        scaledUi(scaleBase, 24);

    const int stateTitleFont =
        scaledUi(scaleBase, 18);

    const int normalFont =
        scaledUi(scaleBase, 14);

    const int smallFont =
        scaledUi(scaleBase, 13);

    // ============================================================
    // 页面整体边距
    // ============================================================
    if (auto *pageLayout =
            qobject_cast<QVBoxLayout *>(
                layout())) {

        const int margin =
            scaledUi(scaleBase, 18);

        pageLayout->setContentsMargins(
            margin,
            margin,
            margin,
            margin);

        pageLayout->setSpacing(
            scaledUi(scaleBase, 14));
    }

    // ============================================================
    // 页面标题
    // ============================================================
    if (auto *title =
            findChild<QLabel *>(
                QStringLiteral(
                    "chargeTitle"))) {

        title->setStyleSheet(
            QStringLiteral(
                "font-size:%1px;"
                "font-weight:700;"
                "color:#1f2329;")
                .arg(titleFont));
    }

    // ============================================================
    // 副标题
    // ============================================================
    if (auto *subtitle =
            findChild<QLabel *>(
                QStringLiteral(
                    "chargeSubtitle"))) {

        subtitle->setStyleSheet(
            QStringLiteral(
                "color:#86909c;"
                "font-size:%1px;")
                .arg(smallFont));
    }

    // ============================================================
    // 订单卡片
    // ============================================================
    if (auto *orderCard =
            findChild<QFrame *>(
                QStringLiteral(
                    "chargeOrderCard"))) {

        orderCard->setStyleSheet(
            QStringLiteral(
                "QFrame#chargeOrderCard{"
                "background:#ffffff;"
                "border:1px solid #d6e4ff;"
                "border-radius:%1px;"
                "}")
                .arg(
                    scaledUi(
                        scaleBase,
                        14)));

        if (auto *orderLayout =
                qobject_cast<QVBoxLayout *>(
                    orderCard->layout())) {

            const int margin =
                scaledUi(
                    scaleBase,
                    18);

            orderLayout->setContentsMargins(
                margin,
                margin,
                margin,
                margin);

            orderLayout->setSpacing(
                scaledUi(
                    scaleBase,
                    12));
        }
    }

    // ============================================================
    // 卡片主状态标题
    // ============================================================
    if (m_stateTitle) {

        m_stateTitle->setStyleSheet(
            QStringLiteral(
                "border:none;"
                "font-size:%1px;"
                "font-weight:700;"
                "color:#1f2329;")
                .arg(stateTitleFont));
    }

    // ============================================================
    // 订单号
    // ============================================================
    if (m_orderLabel) {

        m_orderLabel->setStyleSheet(
            QStringLiteral(
                "border:none;"
                "color:#4e5969;"
                "font-size:%1px;")
                .arg(normalFont));
    }

    // ============================================================
    // 当前状态
    // ============================================================
    if (m_statusLabel) {

        QString color =
            QStringLiteral("#86909c");

        QString weight =
            QStringLiteral("400");

        switch (m_state) {

        case ChargeState::Empty:
            break;

        case ChargeState::Reserved:
            color =
                QStringLiteral("#d97706");
            weight =
                QStringLiteral("600");
            break;

        case ChargeState::Charging:
            color =
                QStringLiteral("#16a34a");
            weight =
                QStringLiteral("600");
            break;

        case ChargeState::PendingPayment:
            color =
                QStringLiteral("#d97706");
            weight =
                QStringLiteral("600");
            break;

        case ChargeState::Paid:
            color =
                QStringLiteral("#1d4ed8");
            weight =
                QStringLiteral("600");
            break;

        }

        m_statusLabel->setStyleSheet(
            QStringLiteral(
                "border:none;"
                "color:%1;"
                "font-size:%2px;"
                "font-weight:%3;")
                .arg(color)
                .arg(normalFont)
                .arg(weight));
    }



    // ============================================================
    // 结算结果
    // ============================================================
    if (m_resultLabel) {

        m_resultLabel->setStyleSheet(
            QStringLiteral(
                "border:none;"
                "background:#f2f7ff;"
                "border-radius:%1px;"
                "padding:%2px;"
                "color:#1d4ed8;"
                "font-size:%3px;"
                "font-weight:600;")
                .arg(
                    scaledUi(
                        scaleBase,
                        8))
                .arg(
                    scaledUi(
                        scaleBase,
                        10))
                .arg(normalFont));
    }

    // ============================================================
    // 页面底部提示
    // ============================================================
    if (m_tipLabel) {

        m_tipLabel->setStyleSheet(
            QStringLiteral(
                "color:#86909c;"
                "font-size:%1px;"
                "padding:%2px;")
                .arg(smallFont)
                .arg(
                    scaledUi(
                        scaleBase,
                        8)));
    }
        // ============================================================
    // 充电实时记录卡
    // ============================================================
    if (m_chargingRecordCard) {

        m_chargingRecordCard->setStyleSheet(
            QStringLiteral(
                "QFrame#chargingRecordCard{"
                "background:#f7faff;"
                "border:1px solid #d6e4ff;"
                "border-radius:%1px;"
                "}"
                "QFrame#chargingRecordCard QLabel{"
                "border:none;"
                "font-size:%2px;"
                "}")
                .arg(
                    scaledUi(
                        scaleBase,
                        10))
                .arg(normalFont));

        if (auto *recordLayout =
                qobject_cast<QVBoxLayout *>(
                    m_chargingRecordCard->layout())) {

            recordLayout->setContentsMargins(
                scaledUi(scaleBase, 14),
                scaledUi(scaleBase, 12),
                scaledUi(scaleBase, 14),
                scaledUi(scaleBase, 12));

            recordLayout->setSpacing(
                scaledUi(
                    scaleBase,
                    8));
        }
    }

    if (m_elapsedLabel) {

        m_elapsedLabel->setStyleSheet(
            QStringLiteral(
                "border:none;"
                "font-size:%1px;"
                "font-weight:700;"
                "color:#1d4ed8;")
                .arg(normalFont));
    }

    if (m_powerLabel) {

        m_powerLabel->setStyleSheet(
            QStringLiteral(
                "border:none;"
                "font-size:%1px;"
                "font-weight:600;"
                "color:#1f2329;")
                .arg(normalFont));
    }

    if (m_currentKwhLabel) {

        m_currentKwhLabel->setStyleSheet(
            QStringLiteral(
                "border:none;"
                "font-size:%1px;"
                "font-weight:600;"
                "color:#1f2329;")
                .arg(normalFont));
    }

    if (m_estimatedFeeLabel) {

        m_estimatedFeeLabel->setStyleSheet(
            QStringLiteral(
                "border:none;"
                "font-size:%1px;"
                "font-weight:700;"
                "color:#ff6a00;")
                .arg(normalFont));
    }

}
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


void ChargePage::stopChargeTimer()
{
    if (!m_chargeTimer)
        return;

    if (m_chargeTimer->isActive()) {

        m_chargeTimer->stop();
    }
}


void ChargePage::updateChargingInfo()
{
    if (m_state != ChargeState::Charging)
        return;

    if (!m_chargeStartedAt.isValid())
        return;

    qint64 elapsedSeconds =
        m_chargeStartedAt.secsTo(
            QDateTime::currentDateTime());

    if (elapsedSeconds < 0)
        elapsedSeconds = 0;

    // 已充时间
    const qint64 hours =
        elapsedSeconds / 3600;

    const qint64 minutes =
        (elapsedSeconds % 3600) / 60;

    const qint64 seconds =
        elapsedSeconds % 60;

    if (m_elapsedLabel) {

        m_elapsedLabel->setText(
            QStringLiteral("%1:%2:%3")
                .arg(hours, 2, 10, QChar('0'))
                .arg(minutes, 2, 10, QChar('0'))
                .arg(seconds, 2, 10, QChar('0')));
    }

    // 当前功率 + 已充电量
    if (m_powerKw > 0.0) {

        if (m_powerLabel) {

            m_powerLabel->setText(
                QStringLiteral("%1 kW")
                    .arg(
                        m_powerKw,
                        0,
                        'f',
                        1));
        }

        m_currentKwh =
            m_powerKw *
            static_cast<double>(
                elapsedSeconds) /
            3600.0;

        if (m_currentKwhLabel) {

            m_currentKwhLabel->setText(
                QStringLiteral("%1 kWh")
                    .arg(
                        m_currentKwh,
                        0,
                        'f',
                        2));
        }

    } else {

        if (m_powerLabel)
            m_powerLabel->setText(
                QStringLiteral("-- kW"));

        if (m_currentKwhLabel)
            m_currentKwhLabel->setText(
                QStringLiteral("-- kWh"));
    }

    // 预估费用
    if (m_powerKw > 0.0 &&
        m_unitPrice > 0.0) {

        m_estimatedAmount =
            m_currentKwh *
            m_unitPrice;

        if (m_estimatedFeeLabel) {

            m_estimatedFeeLabel->setText(
                QStringLiteral("￥%1")
                    .arg(
                        m_estimatedAmount,
                        0,
                        'f',
                        2));
        }

    } else {

        if (m_estimatedFeeLabel) {

            m_estimatedFeeLabel->setText(
                QStringLiteral("￥--"));
        }
    }
}
