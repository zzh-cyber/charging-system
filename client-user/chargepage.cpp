#include "chargepage.h"
#include "windowhelper.h"

#include <QDoubleSpinBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QResizeEvent>


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
    // 充电量输入
    // ========================================================================
    auto *kwhRow =
        new QHBoxLayout;

    auto *kwhTitle =
        new QLabel(
            QStringLiteral("本次充电量"),
            orderCard);
    kwhTitle->setObjectName(
    QStringLiteral("chargeKwhTitle"));


    kwhTitle->setStyleSheet(
        "border:none;"
        "font-size:14px;"
        "font-weight:600;");

    m_kwhSpin =
        new QDoubleSpinBox(orderCard);

    m_kwhSpin->setRange(
        0.1,
        999.9);

    m_kwhSpin->setDecimals(1);
    m_kwhSpin->setSingleStep(1.0);
    m_kwhSpin->setValue(10.0);
    m_kwhSpin->setSuffix(
        QStringLiteral(" kWh"));

    m_kwhSpin->setMinimumWidth(150);

    kwhRow->addWidget(kwhTitle);
    kwhRow->addStretch();
    kwhRow->addWidget(m_kwhSpin);

    orderLayout->addLayout(kwhRow);

    // ========================================================================
    // 结束并结算
    // ========================================================================
    m_settleButton =
        new QPushButton(
            QStringLiteral("结束并结算"),
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

            const double kwh =
                m_kwhSpin->value();

            if (kwh <= 0.0) {
                m_tipLabel->setText(
                    QStringLiteral(
                        "请输入有效的充电量"));
                return;
            }

            emit settleRequested(
                m_orderNo,
                kwh);
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

    refreshUi();
}

void ChargePage::setChargingState()
{
    if (m_orderNo.isEmpty())
        return;

    m_state =
        ChargeState::Charging;

    refreshUi();
}

void ChargePage::setSettledResult(
    double kwh,
    double amount)
{
    if (m_orderNo.isEmpty())
        return;

    m_state =
        ChargeState::Settled;

    m_settledKwh = kwh;
    m_settledAmount = amount;

    refreshUi();
}

void ChargePage::reset()
{
    m_state =
        ChargeState::Empty;

    m_orderNo.clear();

    m_settledKwh = 0.0;
    m_settledAmount = 0.0;

    refreshUi();
}

void ChargePage::refreshUi()
{
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

        m_statusLabel->setStyleSheet(
            "border:none;"
            "color:#86909c;"
            "font-size:14px;");

        m_startButton->setEnabled(false);

        m_kwhSpin->setEnabled(false);

        m_settleButton->setEnabled(false);

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

        m_statusLabel->setStyleSheet(
            "border:none;"
            "color:#d97706;"
            "font-size:14px;"
            "font-weight:600;");

        m_startButton->setEnabled(true);

        m_kwhSpin->setEnabled(false);

        m_settleButton->setEnabled(false);

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

        m_statusLabel->setStyleSheet(
            "border:none;"
            "color:#16a34a;"
            "font-size:14px;"
            "font-weight:600;");

        m_startButton->setEnabled(false);

        m_kwhSpin->setEnabled(true);

        m_settleButton->setEnabled(true);

        m_resultLabel->hide();

        m_tipLabel->setText(
            QStringLiteral(
                "充电完成后请输入实际充电量并结算"));

        break;

    case ChargeState::Settled:

        m_stateTitle->setText(
            QStringLiteral(
                "充电完成"));

        m_orderLabel->setText(
            QStringLiteral(
                "订单号：%1")
                .arg(m_orderNo));

        m_statusLabel->setText(
            QStringLiteral(
                "状态：已结算"));

        m_statusLabel->setStyleSheet(
            "border:none;"
            "color:#1d4ed8;"
            "font-size:14px;"
            "font-weight:600;");

        m_startButton->setEnabled(false);

        m_kwhSpin->setEnabled(false);

        m_settleButton->setEnabled(false);

        m_resultLabel->setText(
            QStringLiteral(
                "本次充电 %1 kWh\n"
                "结算金额 ￥%2")
                .arg(
                    m_settledKwh,
                    0,
                    'f',
                    1)
                .arg(
                    m_settledAmount,
                    0,
                    'f',
                    2));

        m_resultLabel->show();

        m_tipLabel->setText(
            QStringLiteral(
                "本次充电订单已完成"));

        break;
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

        case ChargeState::Settled:
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
    // “本次充电量”
    // ============================================================
    if (auto *kwhTitle =
            findChild<QLabel *>(
                QStringLiteral(
                    "chargeKwhTitle"))) {

        kwhTitle->setStyleSheet(
            QStringLiteral(
                "border:none;"
                "font-size:%1px;"
                "font-weight:600;")
                .arg(normalFont));
    }

    // ============================================================
    // 充电量输入框
    // ============================================================
    if (m_kwhSpin) {

        m_kwhSpin->setMinimumWidth(
            scaledUi(
                scaleBase,
                150));

        m_kwhSpin->setStyleSheet(
            QStringLiteral(
                "QDoubleSpinBox{"
                "background:#ffffff;"
                "border:1px solid #d6e4ff;"
                "border-radius:%1px;"
                "padding:%2px %3px;"
                "font-size:%4px;"
                "}"
                "QDoubleSpinBox:focus{"
                "border-color:#1d4ed8;"
                "}")
                .arg(
                    scaledUi(
                        scaleBase,
                        8))
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
}
