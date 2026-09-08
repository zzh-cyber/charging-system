#include "chargepage.h"

#include "uitheme.h"
#include "windowhelper.h"

#include <QDebug>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSize>
#include <QSizePolicy>
#include <QTimer>
#include <QVBoxLayout>


// ============================================================================
// NO.24：进程内充电计时恢复快照
// ============================================================================
namespace
{

class LiquidProgressBar final : public QProgressBar
{
public:
    explicit LiquidProgressBar(QWidget *parent = nullptr)
        : QProgressBar(parent)
    {
        setTextVisible(false);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const QRectF body = rect().adjusted(1, 1, -1, -1);
        QPainterPath clip;
        clip.addRoundedRect(body, 15, 15);
        painter.fillPath(clip, QColor("#E7ECE9"));

        const double ratio =
            maximum() > minimum()
                ? double(value() - minimum()) /
                      double(maximum() - minimum())
                : 0.0;

        const double surfaceY =
            body.bottom() - body.height() * ratio;

        QPainterPath liquid;
        liquid.moveTo(body.left(), body.bottom());
        liquid.lineTo(body.left(), surfaceY);
        liquid.cubicTo(
            body.left() + body.width() * 0.25, surfaceY - 4,
            body.left() + body.width() * 0.40, surfaceY + 4,
            body.left() + body.width() * 0.55, surfaceY);
        liquid.cubicTo(
            body.left() + body.width() * 0.72, surfaceY - 4,
            body.left() + body.width() * 0.86, surfaceY + 3,
            body.right(), surfaceY - 1);
        liquid.lineTo(body.right(), body.bottom());
        liquid.closeSubpath();

        painter.save();
        painter.setClipPath(clip);
        painter.fillPath(liquid, QColor("#70E889"));
        painter.restore();

        painter.setPen(QPen(QColor("#D9E1DC"), 1));
        painter.drawPath(clip);
    }
};

QString s_no24ResumeOrderNo;

qint64 s_no24ResumeAccumulatedMs =
    0;

bool s_no24ResumeValid =
    false;


void clearNo24ResumeSnapshot()
{
    s_no24ResumeOrderNo.clear();

    s_no24ResumeAccumulatedMs =
        0;

    s_no24ResumeValid =
        false;
}

} // namespace


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


    // =========================================================================
    // Root
    // =========================================================================
    auto *rootLayout =
        new QVBoxLayout(
            this);

    rootLayout->setContentsMargins(
        0,
        0,
        0,
        0);

    rootLayout->setSpacing(
        0);


    // =========================================================================
    // Scroll
    // =========================================================================
    auto *scrollArea =
        new QScrollArea(
            this);

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


    // =========================================================================
    // 页面标题
    // =========================================================================
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


    // =========================================================================
    // 订单状态区域
    //
    // 充电中时：
    // - 隐藏左侧重复的大标题“正在充电”
    // - 只显示订单号 + 右侧“充电中”状态
    //
    // 其它状态仍保留原状态标题。
    // =========================================================================
    auto *hero =
        new QFrame(
            content);

    hero->setObjectName(
        QStringLiteral(
            "chargeVehicleHero"));

    hero->setAttribute(
        Qt::WA_StyledBackground,
        true);


    auto *heroLayout =
        new QVBoxLayout(
            hero);

    heroLayout->setObjectName(
        QStringLiteral(
            "chargeHeroLayout"));

    heroLayout->setContentsMargins(
        2,
        4,
        2,
        0);

    heroLayout->setSpacing(
        4);


    auto *heroTop =
        new QHBoxLayout;

    heroTop->setObjectName(
        QStringLiteral(
            "chargeHeroTop"));

    heroTop->setSpacing(
        10);


    auto *stateBlock =
        new QVBoxLayout;

    stateBlock->setObjectName(
        QStringLiteral(
            "chargeStateBlock"));

    stateBlock->setSpacing(
        3);


    m_stateTitle =
        new QLabel(
            QStringLiteral(
                "暂无进行中的充电订单"),
            hero);

    m_stateTitle->setObjectName(
        QStringLiteral(
            "chargeStateTitle"));

    m_stateTitle->setWordWrap(
        true);


    m_orderLabel =
        new QLabel(
            QStringLiteral(
                "订单号：--"),
            hero);

    m_orderLabel->setObjectName(
        QStringLiteral(
            "chargeOrderNumber"));

    m_orderLabel->setWordWrap(
        true);


    stateBlock->addWidget(
        m_orderLabel);

    stateBlock->addWidget(
        m_stateTitle);


    m_statusLabel =
        new QLabel(
            QStringLiteral(
                "等待预约"),
            hero);

    m_statusLabel->setObjectName(
        QStringLiteral(
            "chargeStatusBadge"));

    m_statusLabel->setAlignment(
        Qt::AlignCenter);


    heroTop->addWidget(
        m_statusLabel,
        0,
        Qt::AlignTop);

    // 状态徽标放在左侧，把右上方的视觉空间留给车辆主体。
    // 标签对象和所有状态更新逻辑保持不变，仅调整布局顺序。
    heroTop->addLayout(
        stateBlock,
        1);


    heroLayout->addLayout(
        heroTop);


    layout->addWidget(
        hero);


    // =========================================================================
    // 汽车区域占位
    //
    // 注意：
    // 真正显示汽车的 chargeCarStage 不放进 layout，
    // 而是直接挂在 content 上。
    //
    // 这样只有汽车这一块可以突破 layout 右侧 18px 的页面 margin，
    // 其它所有区域仍然保持原来的正常左右留白。
    // =========================================================================
    auto *carStageAnchor =
        new QFrame(
            content);

    carStageAnchor->setObjectName(
        QStringLiteral(
            "chargeCarStageAnchor"));

    carStageAnchor->setAttribute(
        Qt::WA_TransparentForMouseEvents,
        true);

    carStageAnchor->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Fixed);


    layout->addWidget(
        carStageAnchor);


    // =========================================================================
    // 真正汽车舞台
    //
    // parent 是 content，而不是 anchor。
    // applyResponsiveStyle() 会让它：
    //
    // 左边仍然从普通内容边距开始，
    // 右边直接延伸到 content 边缘。
    // =========================================================================
    auto *carStage =
        new QFrame(
            content);

    carStage->setObjectName(
        QStringLiteral(
            "chargeCarStage"));

    carStage->setAttribute(
        Qt::WA_StyledBackground,
        true);

    carStage->setAttribute(
        Qt::WA_TransparentForMouseEvents,
        true);


    auto *carImage =
        new QLabel(
            carStage);

    carImage->setObjectName(
        QStringLiteral(
            "chargeCarImage"));

    carImage->setAlignment(
        Qt::AlignCenter);

    carImage->setAttribute(
        Qt::WA_TransparentForMouseEvents,
        true);


    // =========================================================================
    // 待支付停车提示
    //
    // 只作为待支付页面的视觉信息条，不引入停车或车牌业务。
    // =========================================================================
    auto *parkingCard =
        new QFrame(
            content);

    parkingCard->setObjectName(
        QStringLiteral(
            "chargeParkingCard"));

    parkingCard->setAttribute(
        Qt::WA_StyledBackground,
        true);


    auto *parkingLayout =
        new QHBoxLayout(
            parkingCard);

    parkingLayout->setContentsMargins(
        14,
        11,
        14,
        11);

    parkingLayout->setSpacing(
        10);


    auto *parkingIcon =
        new QLabel(
            QStringLiteral(
                "P"),
            parkingCard);

    parkingIcon->setObjectName(
        QStringLiteral(
            "chargeParkingIcon"));

    parkingIcon->setAlignment(
        Qt::AlignCenter);


    auto *parkingText =
        new QLabel(
            QStringLiteral(
                "限时免费停车"),
            parkingCard);

    parkingText->setObjectName(
        QStringLiteral(
            "chargeParkingText"));


    auto *parkingHint =
        new QLabel(
            QStringLiteral(
                "请及时驶离"),
            parkingCard);

    parkingHint->setObjectName(
        QStringLiteral(
            "chargeParkingHint"));


    parkingLayout->addWidget(
        parkingIcon);

    parkingLayout->addWidget(
        parkingText);

    parkingLayout->addStretch();

    parkingLayout->addWidget(
        parkingHint);


    parkingCard->hide();

    layout->addWidget(
        parkingCard);


    // =========================================================================
    // 待支付账单卡：参考移动端的集中数据面板，仅重排已有账单字段。
    // =========================================================================
    auto *pendingBillCard =
        new QFrame(
            content);

    pendingBillCard->setObjectName(
        QStringLiteral(
            "chargePendingBillCard"));

    pendingBillCard->setAttribute(
        Qt::WA_StyledBackground,
        true);


    auto *pendingBillLayout =
        new QVBoxLayout(
            pendingBillCard);

    pendingBillLayout->setContentsMargins(
        16,
        15,
        16,
        16);

    pendingBillLayout->setSpacing(
        14);


    auto *pendingBillTitle =
        new QLabel(
            QStringLiteral(
                "⚡ 本次充电账单"),
            pendingBillCard);

    pendingBillTitle->setObjectName(
        QStringLiteral(
            "chargePendingBillTitle"));

    pendingBillLayout->addWidget(
        pendingBillTitle);


    auto *pendingMetrics =
        new QGridLayout;

    pendingMetrics->setHorizontalSpacing(
        18);

    pendingMetrics->setVerticalSpacing(
        13);


    const auto addPendingMetric =
        [pendingBillCard,
         pendingMetrics](
            int column,
            const QString &caption,
            const QString &objectName) {

            auto *cell =
                new QWidget(
                    pendingBillCard);

            auto *cellLayout =
                new QVBoxLayout(
                    cell);

            cellLayout->setContentsMargins(
                0,
                0,
                0,
                0);

            cellLayout->setSpacing(
                5);

            auto *captionLabel =
                new QLabel(
                    caption,
                    cell);

            captionLabel->setObjectName(
                QStringLiteral(
                    "chargePendingMetricCaption"));

            auto *valueLabel =
                new QLabel(
                    QStringLiteral("--"),
                    cell);

            valueLabel->setObjectName(
                objectName);

            cellLayout->addWidget(
                captionLabel);

            cellLayout->addWidget(
                valueLabel);

            pendingMetrics->addWidget(
                cell,
                0,
                column);
        };


    addPendingMetric(
        0,
        QStringLiteral("充电时长"),
        QStringLiteral("chargePendingDuration"));

    addPendingMetric(
        1,
        QStringLiteral("充电电量"),
        QStringLiteral("chargePendingEnergy"));

    addPendingMetric(
        2,
        QStringLiteral("本次费用"),
        QStringLiteral("chargePendingAmount"));


    pendingBillLayout->addLayout(
        pendingMetrics);

    pendingBillCard->hide();

    layout->addWidget(
        pendingBillCard);


    // =========================================================================
    // Charging content
    // =========================================================================
    m_chargingRecordCard =
        new QFrame(
            content);

    m_chargingRecordCard->setObjectName(
        QStringLiteral(
            "chargingRecordCard"));

    m_chargingRecordCard->setAttribute(
        Qt::WA_StyledBackground,
        true);


    auto *recordLayout =
        new QVBoxLayout(
            m_chargingRecordCard);

    recordLayout->setObjectName(
        QStringLiteral(
            "chargingRecordLayout"));

    recordLayout->setContentsMargins(
        0,
        0,
        0,
        0);

    recordLayout->setSpacing(
        11);


    // =========================================================================
    // SOC + 当前功率
    // =========================================================================
    auto *primaryGrid =
        new QGridLayout;

    primaryGrid->setObjectName(
        QStringLiteral(
            "chargePrimaryGrid"));

    primaryGrid->setHorizontalSpacing(
        10);

    primaryGrid->setVerticalSpacing(
        10);


    // -------------------------------------------------------------------------
    // Battery
    // -------------------------------------------------------------------------
    auto *batteryCard =
        new QFrame(
            m_chargingRecordCard);

    batteryCard->setObjectName(
        QStringLiteral(
            "chargeBatteryCard"));

    batteryCard->setAttribute(
        Qt::WA_StyledBackground,
        true);


    auto *batteryLayout =
        new QVBoxLayout(
            batteryCard);

    batteryLayout->setObjectName(
        QStringLiteral(
            "chargeBatteryLayout"));

    batteryLayout->setContentsMargins(
        14,
        13,
        14,
        13);

    batteryLayout->setSpacing(
        7);


    auto *batteryTop =
        new QHBoxLayout;

    batteryTop->setSpacing(
        7);


    m_batteryIconLabel =
        new QLabel(
            batteryCard);

    m_batteryIconLabel->setObjectName(
        QStringLiteral(
            "chargeBatteryIcon"));

    m_batteryIconLabel->setAlignment(
        Qt::AlignCenter);


    m_batteryStateLabel =
        new QLabel(
            QStringLiteral(
                "充电中"),
            batteryCard);

    m_batteryStateLabel->setObjectName(
        QStringLiteral(
            "chargeBatteryState"));


    batteryTop->addWidget(
        m_batteryIconLabel);

    batteryTop->addWidget(
        m_batteryStateLabel);

    batteryTop->addStretch();


    batteryLayout->addLayout(
        batteryTop);


    m_batteryPercentLabel =
        new QLabel(
            QStringLiteral(
                "--%"),
            batteryCard);

    m_batteryPercentLabel->setObjectName(
        QStringLiteral(
            "chargeBatteryPercent"));


    batteryLayout->addWidget(
        m_batteryPercentLabel);


    m_batteryProgressBar =
        new LiquidProgressBar(
            batteryCard);

    m_batteryProgressBar->setObjectName(
        QStringLiteral(
            "chargeBatteryProgress"));

    m_batteryProgressBar->setRange(
        0,
        100);

    m_batteryProgressBar->setValue(
        0);

    m_batteryProgressBar->setTextVisible(
        false);


    batteryLayout->addWidget(
        m_batteryProgressBar);


    m_batteryRangeLabel =
        new QLabel(
            QStringLiteral(
                "初始电量 --%    ·    目标 100%"),
            batteryCard);

    m_batteryRangeLabel->setObjectName(
        QStringLiteral(
            "chargeBatteryRange"));

    m_batteryRangeLabel->setWordWrap(
        true);


    batteryLayout->addWidget(
        m_batteryRangeLabel);


    // -------------------------------------------------------------------------
    // Power
    // -------------------------------------------------------------------------
    auto *powerCard =
        new QFrame(
            m_chargingRecordCard);

    powerCard->setObjectName(
        QStringLiteral(
            "chargePowerCard"));

    powerCard->setAttribute(
        Qt::WA_StyledBackground,
        true);


    auto *powerLayout =
        new QVBoxLayout(
            powerCard);

    powerLayout->setObjectName(
        QStringLiteral(
            "chargePowerLayout"));

    powerLayout->setContentsMargins(
        14,
        13,
        14,
        13);

    powerLayout->setSpacing(
        7);


    auto *powerTop =
        new QHBoxLayout;

    powerTop->setSpacing(
        7);


    auto *powerIcon =
        new QLabel(
            powerCard);

    powerIcon->setObjectName(
        QStringLiteral(
            "chargePowerIcon"));

    powerIcon->setAlignment(
        Qt::AlignCenter);


    auto *powerCaption =
        new QLabel(
            QStringLiteral(
                "当前功率"),
            powerCard);

    powerCaption->setObjectName(
        QStringLiteral(
            "chargePowerCaption"));


    powerTop->addWidget(
        powerIcon);

    powerTop->addWidget(
        powerCaption);

    powerTop->addStretch();


    powerLayout->addLayout(
        powerTop);


    m_powerLabel =
        new QLabel(
            QStringLiteral(
                "-- kW"),
            powerCard);

    m_powerLabel->setObjectName(
        QStringLiteral(
            "chargePowerValue"));


    powerLayout->addWidget(
        m_powerLabel);

    powerLayout->addStretch();


    primaryGrid->addWidget(
        batteryCard,
        0,
        0);

    primaryGrid->addWidget(
        powerCard,
        0,
        1);


    primaryGrid->setColumnStretch(
        0,
        1);

    primaryGrid->setColumnStretch(
        1,
        1);


    recordLayout->addLayout(
        primaryGrid);


    // =========================================================================
    // 充电接口
    //
    // Type 当前暂时前端固定。
    //
    // TODO:
    // 后续服务端返回 connector_type 后，
    // 这里改为真实 connector_type。
    //
    // 功率不写死，继续使用真实 m_powerKw。
    // =========================================================================
    auto *portCard =
        new QFrame(
            m_chargingRecordCard);

    portCard->setObjectName(
        QStringLiteral(
            "chargePortCard"));

    portCard->setAttribute(
        Qt::WA_StyledBackground,
        true);


    auto *portLayout =
        new QHBoxLayout(
            portCard);

    portLayout->setObjectName(
        QStringLiteral(
            "chargePortLayout"));

    portLayout->setContentsMargins(
        15,
        14,
        12,
        14);

    portLayout->setSpacing(
        12);


    auto *portTextLayout =
        new QVBoxLayout;

    portTextLayout->setSpacing(
        4);


    auto *portCaption =
        new QLabel(
            QStringLiteral(
                "充电接口"),
            portCard);

    portCaption->setObjectName(
        QStringLiteral(
            "chargePortCaption"));


    auto *portType =
        new QLabel(
            QStringLiteral(
                "Type · GB/T 20234.3"),
            portCard);

    portType->setObjectName(
        QStringLiteral(
            "chargePortType"));

    portType->setWordWrap(
        true);


    auto *portTip =
        new QLabel(
            QStringLiteral(
                "充电接口示意 · 当前功率 -- kW"),
            portCard);

    portTip->setObjectName(
        QStringLiteral(
            "chargePortTip"));

    portTip->setWordWrap(
        true);


    portTextLayout->addWidget(
        portCaption);

    portTextLayout->addSpacing(
        2);

    portTextLayout->addWidget(
        portType);

    portTextLayout->addWidget(
        portTip);

    portTextLayout->addStretch();


    auto *portImage =
        new QLabel(
            portCard);

    portImage->setObjectName(
        QStringLiteral(
            "chargePortImage"));

    portImage->setAlignment(
        Qt::AlignRight |
        Qt::AlignVCenter);


    portLayout->addLayout(
        portTextLayout,
        1);

    portLayout->addWidget(
        portImage);


    recordLayout->addWidget(
        portCard);


    // =========================================================================
    // 实时记录 Header
    // =========================================================================
    auto *recordHeader =
        new QHBoxLayout;

    recordHeader->setObjectName(
        QStringLiteral(
            "chargeRecordHeader"));

    recordHeader->setSpacing(
        8);


    auto *recordIcon =
        new QLabel(
            m_chargingRecordCard);

    recordIcon->setObjectName(
        QStringLiteral(
            "chargeRecordIcon"));

    recordIcon->setAlignment(
        Qt::AlignCenter);


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
        recordIcon);

    recordHeader->addWidget(
        recordTitle);

    recordHeader->addStretch();

    recordHeader->addWidget(
        recordBadge);


    recordLayout->addLayout(
        recordHeader);


    // =========================================================================
    // 实时记录详情
    // =========================================================================
    auto *liveCard =
        new QFrame(
            m_chargingRecordCard);

    liveCard->setObjectName(
        QStringLiteral(
            "chargeLiveCard"));

    liveCard->setAttribute(
        Qt::WA_StyledBackground,
        true);


    auto *liveLayout =
        new QVBoxLayout(
            liveCard);

    liveLayout->setObjectName(
        QStringLiteral(
            "chargeLiveLayout"));

    liveLayout->setContentsMargins(
        14,
        5,
        14,
        5);

    liveLayout->setSpacing(
        0);


    const auto addLiveRow =
        [liveCard,
         liveLayout](
            const QString &caption,
            const QString &initialValue,
            const QString &role,
            QLabel *&valueLabel,
            bool addDivider) {

            auto *row =
                new QWidget(
                    liveCard);

            row->setObjectName(
                QStringLiteral(
                    "chargeLiveRow"));


            auto *rowLayout =
                new QHBoxLayout(
                    row);

            rowLayout->setContentsMargins(
                0,
                10,
                0,
                10);

            rowLayout->setSpacing(
                10);


            auto *captionLabel =
                new QLabel(
                    caption,
                    row);

            captionLabel->setObjectName(
                QStringLiteral(
                    "chargeLiveCaption"));


            valueLabel =
                new QLabel(
                    initialValue,
                    row);

            valueLabel->setObjectName(
                QStringLiteral(
                    "chargeLiveValue"));

            valueLabel->setProperty(
                "metricRole",
                role);

            valueLabel->setAlignment(
                Qt::AlignRight |
                Qt::AlignVCenter);


            rowLayout->addWidget(
                captionLabel);

            rowLayout->addStretch();

            rowLayout->addWidget(
                valueLabel);


            liveLayout->addWidget(
                row);


            if (addDivider) {

                auto *divider =
                    new QFrame(
                        liveCard);

                divider->setObjectName(
                    QStringLiteral(
                        "chargeLiveDivider"));

                divider->setFrameShape(
                    QFrame::HLine);


                liveLayout->addWidget(
                    divider);
            }
        };


    addLiveRow(
        QStringLiteral(
            "已充时间"),
        QStringLiteral(
            "00:00:00"),
        QStringLiteral(
            "time"),
        m_elapsedLabel,
        true);


    addLiveRow(
        QStringLiteral(
            "已充电量"),
        QStringLiteral(
            "-- kWh"),
        QStringLiteral(
            "energy"),
        m_currentKwhLabel,
        true);


    addLiveRow(
        QStringLiteral(
            "预估费用"),
        QStringLiteral(
            "￥--"),
        QStringLiteral(
            "fee"),
        m_estimatedFeeLabel,
        false);


    recordLayout->addWidget(
        liveCard);


    // 默认只在充电中显示
    m_chargingRecordCard->hide();


    layout->addWidget(
        m_chargingRecordCard);


    // =========================================================================
    // 最终账单
    // =========================================================================
    m_resultLabel =
        new QLabel(
            content);

    m_resultLabel->setObjectName(
        QStringLiteral(
            "chargeResultLabel"));

    m_resultLabel->setWordWrap(
        true);

    m_resultLabel->setAlignment(
        Qt::AlignLeft |
        Qt::AlignVCenter);

    m_resultLabel->hide();


    layout->addWidget(
        m_resultLabel);


    // =========================================================================
    // 当前提示
    // =========================================================================
    auto *guideCard =
        new QFrame(
            content);

    guideCard->setObjectName(
        QStringLiteral(
            "chargeGuideCard"));

    guideCard->setAttribute(
        Qt::WA_StyledBackground,
        true);


    auto *guideLayout =
        new QVBoxLayout(
            guideCard);

    guideLayout->setObjectName(
        QStringLiteral(
            "chargeGuideLayout"));

    guideLayout->setContentsMargins(
        15,
        14,
        15,
        14);

    guideLayout->setSpacing(
        8);


    auto *guideHeader =
        new QHBoxLayout;

    guideHeader->setSpacing(
        8);


    auto *guideIcon =
        new QLabel(
            guideCard);

    guideIcon->setObjectName(
        QStringLiteral(
            "chargeGuideIcon"));

    guideIcon->setAlignment(
        Qt::AlignCenter);


    auto *guideTitle =
        new QLabel(
            QStringLiteral(
                "当前提示"),
            guideCard);

    guideTitle->setObjectName(
        QStringLiteral(
            "chargeGuideTitle"));


    guideHeader->addWidget(
        guideIcon);

    guideHeader->addWidget(
        guideTitle);

    guideHeader->addStretch();


    guideLayout->addLayout(
        guideHeader);


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
        m_tipLabel);

    guideLayout->addWidget(
        billingNote);


    layout->addWidget(
        guideCard);


    // =========================================================================
    // 操作按钮
    // =========================================================================
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
            content);

    m_startButton->setObjectName(
        QStringLiteral(
            "chargeStartButton"));

    m_startButton->setCursor(
        Qt::PointingHandCursor);

    m_startButton->setIcon(
        QIcon(
            QStringLiteral(
                ":/icons/charge-active.svg")));


    m_settleButton =
        new QPushButton(
            QStringLiteral(
                "结束充电"),
            content);

    m_settleButton->setObjectName(
        QStringLiteral(
            "chargeFinishButton"));

    m_settleButton->setCursor(
        Qt::PointingHandCursor);

    m_settleButton->setIcon(
        QIcon(
            QStringLiteral(
                ":/icons/stop-white.svg")));


    m_payButton =
        new QPushButton(
            QStringLiteral(
                "确认支付"),
            content);

    m_payButton->setObjectName(
        QStringLiteral(
            "chargePayButton"));

    m_payButton->setCursor(
        Qt::PointingHandCursor);

    m_payButton->setIcon(
        QIcon(
            QStringLiteral(
                ":/icons/wallet-white.svg")));


    actionLayout->addWidget(
        m_startButton,
        1);

    actionLayout->addWidget(
        m_settleButton,
        1);

    actionLayout->addWidget(
        m_payButton,
        1);


    layout->addLayout(
        actionLayout);


    layout->addStretch();


    scrollArea->setWidget(
        content);

    rootLayout->addWidget(
        scrollArea);


    // =========================================================================
    // Timer
    // =========================================================================
    m_chargeTimer =
        new QTimer(
            this);

    m_chargeTimer->setInterval(
        1000);


    connect(
        m_chargeTimer,
        &QTimer::timeout,
        this,
        &ChargePage::updateChargingInfo);


    // =========================================================================
    // 开始充电
    // =========================================================================
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


    // =========================================================================
    // 结束充电
    // =========================================================================
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


    // =========================================================================
    // 支付
    // =========================================================================
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
    const QString newOrderNo =
        orderNo.trimmed();


    if (newOrderNo.isEmpty()) {

        reset();

        return;
    }


    if (s_no24ResumeValid &&
        s_no24ResumeOrderNo !=
            newOrderNo) {

        clearNo24ResumeSnapshot();
    }


    m_orderNo =
        newOrderNo;


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


    resetBatteryInfo();


    refreshUi();
}


// ============================================================================
// 开始充电成功
// ============================================================================
void ChargePage::setChargingState(
    const QString &startTime,
    double powerKw,
    double unitPrice,
    double startSoc,
    double batteryCapacityKwh,
    double targetSoc)
{
    qInfo().noquote()
        << "[NO24] setChargingState ENTER"
        << "order=" << m_orderNo
        << "elapsedOrder=" << m_elapsedOrderNo
        << "startTime=" << startTime
        << "network=" << m_networkAvailable
        << "accMs=" << m_accumulatedChargeMs
        << "timerValid=" << m_onlineChargeTimer.isValid()
        << "timerMs="
        << (m_onlineChargeTimer.isValid()
                ? m_onlineChargeTimer.elapsed()
                : -1);


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


    if (!m_chargeStartedAt.isValid()) {

        m_chargeStartedAt =
            QDateTime::currentDateTime();
    }


    // ========================================================================
    // NO.24
    // ========================================================================
    if (m_elapsedOrderNo !=
        m_orderNo) {

        m_elapsedOrderNo =
            m_orderNo;


        m_accumulatedChargeMs =
            0;


        m_onlineChargeTimer.invalidate();


        if (s_no24ResumeValid &&
            s_no24ResumeOrderNo ==
                m_orderNo) {

            m_accumulatedChargeMs =
                qMax<qint64>(
                    0,
                    s_no24ResumeAccumulatedMs);


            qInfo().noquote()
                << "[NO24] RESTORE SNAPSHOT"
                << "order="
                << m_orderNo
                << "accMs="
                << m_accumulatedChargeMs;

        } else if (
            m_chargeStartedAt.isValid()) {

            m_accumulatedChargeMs =
                qMax<qint64>(
                    0,
                    m_chargeStartedAt.msecsTo(
                        QDateTime::
                            currentDateTime()));
        }
    }


    if (m_networkAvailable &&
        !m_onlineChargeTimer.isValid()) {

        m_onlineChargeTimer.start();
    }


    // 保留当前原有重复判断逻辑
    if (m_networkAvailable &&
        !m_onlineChargeTimer.isValid()) {

        m_onlineChargeTimer.start();
    }


    if (powerKw > 0.0) {

        m_powerKw =
            powerKw;
    }


    if (unitPrice > 0.0) {

        m_unitPrice =
            unitPrice;
    }


    // ========================================================================
    // SOC
    // ========================================================================
    if (startSoc >= 0.0 &&
        startSoc <= 100.0 &&
        batteryCapacityKwh >
            0.0) {

        m_startSoc =
            startSoc;


        m_batteryCapacityKwh =
            batteryCapacityKwh;


        if (targetSoc > 0.0 &&
            targetSoc <= 100.0) {

            m_targetSoc =
                targetSoc;

        } else {

            m_targetSoc =
                100.0;
        }


        if (m_targetSoc <
            m_startSoc) {

            m_targetSoc =
                m_startSoc;
        }


        m_currentSoc =
            m_startSoc;


        m_hasBatteryInfo =
            true;

    } else {

        resetBatteryInfo();
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


    if (s_no24ResumeValid &&
        s_no24ResumeOrderNo ==
            m_orderNo) {

        clearNo24ResumeSnapshot();
    }


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


    if (s_no24ResumeValid &&
        s_no24ResumeOrderNo ==
            m_orderNo) {

        clearNo24ResumeSnapshot();
    }


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
// Reset
// ============================================================================
void ChargePage::reset()
{
    qInfo().noquote()
        << "[NO24] RESET CALLED"
        << "state="
        << static_cast<int>(
               m_state)
        << "order="
        << m_orderNo
        << "elapsedOrder="
        << m_elapsedOrderNo
        << "accMs="
        << m_accumulatedChargeMs
        << "timerValid="
        << m_onlineChargeTimer.isValid()
        << "timerMs="
        << (m_onlineChargeTimer.isValid()
                ? m_onlineChargeTimer.elapsed()
                : -1);


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


    m_accumulatedChargeMs =
        0;


    m_onlineChargeTimer.invalidate();


    m_elapsedOrderNo.clear();


    resetBatteryInfo();


    refreshUi();
}


// ============================================================================
// 状态刷新
// ============================================================================
void ChargePage::refreshUi()
{
    if (auto *subtitle =
            findChild<QLabel *>(
                QStringLiteral(
                    "chargeSubtitle"))) {

        // 进行中的订单以车辆和核心数据为主，减少重复说明文字。
        subtitle->setVisible(
            m_state == ChargeState::Empty ||
            m_state == ChargeState::Reserved);
    }


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


    auto *heroTop =
        findChild<QHBoxLayout *>(
            QStringLiteral(
                "chargeHeroTop"));

    auto *stateBlock =
        findChild<QVBoxLayout *>(
            QStringLiteral(
                "chargeStateBlock"));


    if (stateBlock) {

        stateBlock->removeWidget(
            m_stateTitle);

        stateBlock->removeWidget(
            m_orderLabel);


        // 待支付页沿用参考图的“订单号在上、状态标题在下”；
        // 其他页面恢复更自然的“状态标题在上、订单号在下”。
        if (m_state ==
            ChargeState::PendingPayment) {

            stateBlock->addWidget(
                m_orderLabel);

            stateBlock->addWidget(
                m_stateTitle);

        } else {

            stateBlock->addWidget(
                m_stateTitle);

            stateBlock->addWidget(
                m_orderLabel);
        }
    }


    if (heroTop &&
        stateBlock) {

        heroTop->removeWidget(
            m_statusLabel);

        heroTop->removeItem(
            stateBlock);


        if (m_state ==
            ChargeState::Charging) {

            // 充电中：徽标在左，给右上车辆留出视觉空间。
            heroTop->addWidget(
                m_statusLabel,
                0,
                Qt::AlignTop);

            heroTop->addLayout(
                stateBlock,
                1);

        } else {

            // 未预约等状态：主标题靠左，状态徽标靠右。
            heroTop->addLayout(
                stateBlock,
                1);

            heroTop->addWidget(
                m_statusLabel,
                0,
                Qt::AlignTop);
        }
    }


    // 停车提示只属于待支付状态，避免状态切换后残留。
    if (auto *parkingCard =
            findChild<QFrame *>(
                QStringLiteral(
                    "chargeParkingCard"))) {

        parkingCard->hide();
    }


    if (auto *pendingBillCard =
            findChild<QFrame *>(
                QStringLiteral(
                    "chargePendingBillCard"))) {

        pendingBillCard->hide();
    }


    if (auto *guideCard =
            findChild<QFrame *>(
                QStringLiteral(
                    "chargeGuideCard"))) {

        guideCard->show();
    }


    m_statusLabel->show();

    m_stateTitle->setAlignment(
        Qt::AlignLeft |
        Qt::AlignVCenter);

    m_orderLabel->setAlignment(
        Qt::AlignLeft |
        Qt::AlignVCenter);


    switch (m_state) {

    case ChargeState::Empty:

        m_stateTitle->show();

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


    case ChargeState::Reserved:

        m_stateTitle->show();

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


    case ChargeState::Charging:

        // --------------------------------------------------------------------
        // 用户要求：
        // 这里不再显示重复的大标题“正在充电”。
        //
        // 只保留：
        // 订单号 + 右侧“充电中”Badge。
        // --------------------------------------------------------------------
        m_stateTitle->hide();


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


    case ChargeState::PendingPayment:
    {
        if (auto *parkingCard =
                findChild<QFrame *>(
                    QStringLiteral(
                        "chargeParkingCard"))) {

            parkingCard->show();
        }


        if (auto *pendingBillCard =
                findChild<QFrame *>(
                    QStringLiteral(
                        "chargePendingBillCard"))) {

            pendingBillCard->show();
        }


        if (auto *guideCard =
                findChild<QFrame *>(
                    QStringLiteral(
                        "chargeGuideCard"))) {

            guideCard->hide();
        }

        m_stateTitle->show();

        m_stateTitle->setText(
            QStringLiteral(
                "充电已结束 · 等待支付"));


        m_stateTitle->setAlignment(
            Qt::AlignCenter);

        m_orderLabel->setAlignment(
            Qt::AlignCenter);


        m_orderLabel->setText(
            QStringLiteral(
                "订单号：%1")
                .arg(
                    m_orderNo));


        m_statusLabel->setText(
            QStringLiteral(
                "待支付"));

        m_statusLabel->hide();


        m_startButton->hide();

        m_settleButton->hide();


        m_chargingRecordCard->hide();


        const QString duration =
            formatDuration(
                m_finalDurationSeconds);


        if (auto *durationLabel =
                findChild<QLabel *>(
                    QStringLiteral(
                        "chargePendingDuration"))) {

            durationLabel->setText(
                duration);
        }


        if (auto *energyLabel =
                findChild<QLabel *>(
                    QStringLiteral(
                        "chargePendingEnergy"))) {

            energyLabel->setText(
                QStringLiteral("%1 kWh")
                    .arg(
                        m_settledKwh,
                        0,
                        'f',
                        2));
        }


        if (auto *amountLabel =
                findChild<QLabel *>(
                    QStringLiteral(
                        "chargePendingAmount"))) {

            amountLabel->setText(
                QStringLiteral("￥%1")
                    .arg(
                        m_settledAmount,
                        0,
                        'f',
                        2));
        }


        m_resultLabel->hide();


        m_payButton->show();

        m_payButton->setEnabled(
            true);


        m_tipLabel->setText(
            QStringLiteral(
                "请确认最终账单后完成支付"));

        break;
    }


    case ChargeState::Paid:
    {
        m_stateTitle->show();

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
// Responsive Style
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


    const int stateFont =
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


    const int bigValueFont =
        scaledUi(
            scaleBase,
            29);


    const int liveValueFont =
        scaledUi(
            scaleBase,
            16);


    const int portTypeFont =
        scaledUi(
            scaleBase,
            18);


    const int cardRadius =
        scaledUi(
            scaleBase,
            20);


    const int smallRadius =
        scaledUi(
            scaleBase,
            11);


    const int iconBox =
        scaledUi(
            scaleBase,
            32);


    const int iconSize =
        scaledUi(
            scaleBase,
            17);


    const int buttonHeight =
        scaledUi(
            scaleBase,
            48);


    const int buttonIconSize =
        scaledUi(
            scaleBase,
            18);


    // =========================================================================
    // Page
    // =========================================================================
    QString pageStyle =
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

            "QScrollArea#chargeScrollArea > QWidget > QWidget{"
            "background:transparent;"
            "}"

            "QLabel#chargeTitle{"
            "background:transparent;"
            "color:%1;"
            "font-size:%2px;"
            "font-weight:850;"
            "}"

            "QLabel#chargeSubtitle{"
            "background:transparent;"
            "color:%3;"
            "font-size:%4px;"
            "}");

    pageStyle =
        pageStyle
            .arg(
                UiTheme::textPrimary())
            .arg(
                titleFont)
            .arg(
                UiTheme::textSecondary())
            .arg(
                smallFont);


    // =========================================================================
    // Hero / Car
    // =========================================================================
    QString heroStyle =
        QStringLiteral(

            "QFrame#chargeVehicleHero{"
            "background:transparent;"
            "border:none;"
            "}"

            "QLabel#chargeStateTitle{"
            "background:transparent;"
            "border:none;"
            "color:%1;"
            "font-size:%2px;"
            "font-weight:850;"
            "}"

            "QLabel#chargeOrderNumber{"
            "background:transparent;"
            "border:none;"
            "color:%3;"
            "font-size:%4px;"
            "}"

            "QFrame#chargeCarStageAnchor{"
            "background:transparent;"
            "border:none;"
            "}"

            "QFrame#chargeCarStage{"
            "background:transparent;"
            "border:none;"
            "}"

            "QLabel#chargeCarImage{"
            "background:transparent;"
            "border:none;"
            "}");

    heroStyle =
        heroStyle
            .arg(
                UiTheme::textPrimary())
            .arg(
                stateFont)
            .arg(
                UiTheme::textSecondary())
            .arg(
                smallFont);


    // =========================================================================
    // Charging wrapper
    // =========================================================================
    const QString chargingWrapperStyle =
        QStringLiteral(

            "QFrame#chargingRecordCard{"
            "background:transparent;"
            "border:none;"
            "}");


    // =========================================================================
    // Pending payment parking banner
    // =========================================================================
    QString parkingStyle =
        QStringLiteral(

            "QFrame#chargeParkingCard{"
            "background:%1;"
            "border:1px solid %2;"
            "border-radius:%3px;"
            "}"

            "QLabel#chargeParkingIcon{"
            "background:#E6F4FF;"
            "color:#258BD2;"
            "border:none;"
            "border-radius:%4px;"
            "font-size:%5px;"
            "font-weight:900;"
            "min-width:%6px;"
            "max-width:%6px;"
            "min-height:%6px;"
            "max-height:%6px;"
            "}"

            "QLabel#chargeParkingText{"
            "background:transparent;"
            "color:%7;"
            "border:none;"
            "font-size:%8px;"
            "font-weight:800;"
            "}"

            "QLabel#chargeParkingHint{"
            "background:%9;"
            "color:%10;"
            "border:none;"
            "border-radius:%4px;"
            "padding:5px 9px;"
            "font-size:%5px;"
            "font-weight:700;"
            "}")
            .arg(
                UiTheme::surface())
            .arg(
                UiTheme::border())
            .arg(
                cardRadius)
            .arg(
                smallRadius)
            .arg(
                tinyFont)
            .arg(
                iconBox)
            .arg(
                UiTheme::textPrimary())
            .arg(
                normalFont)
            .arg(
                UiTheme::limeSoft())
            .arg(
                UiTheme::limeStrong());


    QString pendingBillStyle =
        QStringLiteral(

            "QFrame#chargePendingBillCard{"
            "background:%1;"
            "border:1px solid %2;"
            "border-radius:%3px;"
            "}"

            "QLabel#chargePendingBillTitle{"
            "background:transparent;"
            "color:%4;"
            "border:none;"
            "font-size:%5px;"
            "font-weight:850;"
            "}"

            "QLabel#chargePendingMetricCaption{"
            "background:transparent;"
            "color:%6;"
            "border:none;"
            "font-size:%7px;"
            "}"

            "QLabel#chargePendingDuration,"
            "QLabel#chargePendingEnergy,"
            "QLabel#chargePendingAmount{"
            "background:transparent;"
            "color:%4;"
            "border:none;"
            "font-size:%8px;"
            "font-weight:850;"
            "}"

            "QLabel#chargePendingAmount{"
            "color:%9;"
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
                normalFont)
            .arg(
                UiTheme::textSecondary())
            .arg(
                tinyFont)
            .arg(
                liveValueFont)
            .arg(
                UiTheme::limeStrong());


    // =========================================================================
    // Primary metrics
    // =========================================================================
    QString primaryMetricStyle =
        QStringLiteral(

            "QFrame#chargeBatteryCard,"
            "QFrame#chargePowerCard{"
            "background:%1;"
            "border:1px solid %2;"
            "border-radius:%3px;"
            "}"

            "QLabel#chargeBatteryIcon,"
            "QLabel#chargePowerIcon{"
            "background:%4;"
            "border:none;"
            "border-radius:%5px;"
            "}"

            "QLabel#chargeBatteryState,"
            "QLabel#chargePowerCaption{"
            "background:transparent;"
            "color:%6;"
            "font-size:%7px;"
            "font-weight:700;"
            "}"

            "QLabel#chargeBatteryPercent,"
            "QLabel#chargePowerValue{"
            "background:transparent;"
            "color:%8;"
            "font-size:%9px;"
            "font-weight:900;"
            "}");

    primaryMetricStyle =
        primaryMetricStyle
            .arg(
                UiTheme::surface())
            .arg(
                UiTheme::border())
            .arg(
                cardRadius)
            .arg(
                UiTheme::limeSoft())
            .arg(
                iconBox / 2)
            .arg(
                UiTheme::textSecondary())
            .arg(
                smallFont)
            .arg(
                UiTheme::textPrimary())
            .arg(
                bigValueFont);


    QString batteryStyle =
        QStringLiteral(

            "QLabel#chargeBatteryRange{"
            "background:transparent;"
            "color:%1;"
            "font-size:%2px;"
            "}"

            "QProgressBar#chargeBatteryProgress{"
            "background:transparent;"
            "border:none;"
            "min-height:82px;"
            "max-height:82px;"
            "}");

    batteryStyle =
        batteryStyle
            .arg(
                UiTheme::textSecondary())
            .arg(
                tinyFont);


    // =========================================================================
    // Port
    // =========================================================================
    QString portStyle =
        QStringLiteral(

            "QFrame#chargePortCard{"
            "background:%1;"
            "border:1px solid %2;"
            "border-radius:%3px;"
            "}"

            "QLabel#chargePortCaption{"
            "background:transparent;"
            "color:%4;"
            "font-size:%5px;"
            "font-weight:700;"
            "}"

            "QLabel#chargePortType{"
            "background:transparent;"
            "color:%6;"
            "font-size:%7px;"
            "font-weight:850;"
            "}"

            "QLabel#chargePortTip{"
            "background:transparent;"
            "color:%4;"
            "font-size:%8px;"
            "}"

            "QLabel#chargePortImage{"
            "background:transparent;"
            "border:none;"
            "}");

    portStyle =
        portStyle
            .arg(
                UiTheme::surface())
            .arg(
                UiTheme::border())
            .arg(
                cardRadius)
            .arg(
                UiTheme::textSecondary())
            .arg(
                tinyFont)
            .arg(
                UiTheme::textPrimary())
            .arg(
                portTypeFont)
            .arg(
                smallFont);


    // =========================================================================
    // Record header
    // =========================================================================
    QString recordHeaderStyle =
        QStringLiteral(

            "QLabel#chargeRecordIcon{"
            "background:%1;"
            "border:none;"
            "border-radius:%2px;"
            "}"

            "QLabel#chargeRecordTitle{"
            "background:transparent;"
            "color:%3;"
            "font-size:%4px;"
            "font-weight:800;"
            "}"

            "QLabel#chargeRecordBadge{"
            "background:%1;"
            "color:%5;"
            "border:none;"
            "border-radius:%6px;"
            "font-size:%7px;"
            "font-weight:750;"
            "padding:5px 9px;"
            "}");

    recordHeaderStyle =
        recordHeaderStyle
            .arg(
                UiTheme::limeSoft())
            .arg(
                iconBox / 2)
            .arg(
                UiTheme::textPrimary())
            .arg(
                normalFont)
            .arg(
                UiTheme::limeStrong())
            .arg(
                smallRadius)
            .arg(
                tinyFont);


    // =========================================================================
    // Live
    // =========================================================================
    QString liveStyle =
        QStringLiteral(

            "QFrame#chargeLiveCard{"
            "background:%1;"
            "border:1px solid %2;"
            "border-radius:%3px;"
            "}"

            "QWidget#chargeLiveRow{"
            "background:transparent;"
            "}"

            "QLabel#chargeLiveCaption{"
            "background:transparent;"
            "color:%4;"
            "font-size:%5px;"
            "}"

            "QLabel#chargeLiveValue{"
            "background:transparent;"
            "color:%6;"
            "font-size:%7px;"
            "font-weight:800;"
            "}"

            "QLabel#chargeLiveValue[metricRole=\"fee\"]{"
            "color:%8;"
            "}"

            "QFrame#chargeLiveDivider{"
            "background:%2;"
            "border:none;"
            "max-height:1px;"
            "}");

    liveStyle =
        liveStyle
            .arg(
                UiTheme::surface())
            .arg(
                UiTheme::border())
            .arg(
                cardRadius)
            .arg(
                UiTheme::textSecondary())
            .arg(
                smallFont)
            .arg(
                UiTheme::textPrimary())
            .arg(
                liveValueFont)
            .arg(
                UiTheme::limeStrong());


    // =========================================================================
    // Guide
    // =========================================================================
    QString guideStyle =
        QStringLiteral(

            "QFrame#chargeGuideCard{"
            "background:#FFFFFF;"
            "border:1px solid %1;"
            "border-radius:%2px;"
            "}"

            "QLabel#chargeGuideIcon{"
            "background:%3;"
            "border:none;"
            "border-radius:%4px;"
            "}"

            "QLabel#chargeGuideTitle{"
            "background:transparent;"
            "color:%5;"
            "font-size:%6px;"
            "font-weight:750;"
            "}"

            "QLabel#chargeTipLabel{"
            "background:transparent;"
            "color:%5;"
            "font-size:%7px;"
            "}"

            "QLabel#chargeBillingNote{"
            "background:transparent;"
            "color:%8;"
            "font-size:%9px;"
            "}");

    guideStyle =
        guideStyle
            .arg(
                UiTheme::border())
            .arg(
                cardRadius)
            .arg(
                UiTheme::limeSoft())
            .arg(
                iconBox / 2)
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


    // =========================================================================
    // Buttons
    // =========================================================================
    QString buttonStyle =
        QStringLiteral(

            "QPushButton#chargeStartButton{"
            "background:%1;"
            "color:#FFFFFF;"
            "border:none;"
            "border-radius:%2px;"
            "font-size:%3px;"
            "font-weight:800;"
            "padding:10px 18px;"
            "}"

            "QPushButton#chargeStartButton:hover{"
            "background:%4;"
            "}"

            "QPushButton#chargeStartButton:disabled{"
            "background:#DDE2DF;"
            "color:#9BA39F;"
            "}"

            "QPushButton#chargeFinishButton{"
            "background:%5;"
            "color:#FFFFFF;"
            "border:none;"
            "border-radius:%2px;"
            "font-size:%3px;"
            "font-weight:800;"
            "padding:10px 18px;"
            "}"

            "QPushButton#chargeFinishButton:hover{"
            "background:#C85858;"
            "}"

            "QPushButton#chargePayButton{"
            "background:%6;"
            "color:%1;"
            "border:none;"
            "border-radius:%2px;"
            "font-size:%3px;"
            "font-weight:850;"
            "padding:10px 18px;"
            "}"

            "QPushButton#chargePayButton:hover{"
            "background:#63E27C;"
            "}");

    buttonStyle =
        buttonStyle
            .arg(
                UiTheme::dark())
            .arg(
                smallRadius)
            .arg(
                normalFont)
            .arg(
                UiTheme::darkHover())
            .arg(
                UiTheme::danger())
            .arg(
                UiTheme::lime());


    setStyleSheet(
        pageStyle +
        heroStyle +
        chargingWrapperStyle +
        parkingStyle +
        pendingBillStyle +
        primaryMetricStyle +
        batteryStyle +
        portStyle +
        recordHeaderStyle +
        liveStyle +
        guideStyle +
        buttonStyle);


    // =========================================================================
    // Status badge
    // =========================================================================
    if (m_statusLabel) {

        QString background =
            UiTheme::surfaceSoft();

        QString color =
            UiTheme::textSecondary();


        switch (m_state) {

        case ChargeState::Empty:
            break;


        case ChargeState::Reserved:

            background =
                QStringLiteral(
                    "#FFF0CE");

            color =
                QStringLiteral(
                    "#93611A");

            break;


        case ChargeState::Charging:

            background =
                UiTheme::lime();

            color =
                UiTheme::dark();

            break;


        case ChargeState::PendingPayment:

            background =
                QStringLiteral(
                    "#FFF0CE");

            color =
                QStringLiteral(
                    "#93611A");

            break;


        case ChargeState::Paid:

            background =
                UiTheme::lime();

            color =
                UiTheme::dark();

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
                "font-weight:800;"
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


    // =========================================================================
    // Final bill
    // =========================================================================
    if (m_resultLabel) {

        QString background =
            UiTheme::surface();

        QString border =
            UiTheme::border();

        QString color =
            UiTheme::textPrimary();


        if (m_state ==
            ChargeState::PendingPayment) {

            background =
                QStringLiteral(
                    "#FFF7E6");

            border =
                QStringLiteral(
                    "#EEDCB4");

            color =
                QStringLiteral(
                    "#805B20");

        } else if (
            m_state ==
            ChargeState::Paid) {

            background =
                UiTheme::limeSoft();

            border =
                QStringLiteral(
                    "#BDEEC8");

            color =
                UiTheme::textPrimary();
        }


        m_resultLabel->setStyleSheet(
            QStringLiteral(
                "QLabel#chargeResultLabel{"
                "background:%1;"
                "color:%2;"
                "border:1px solid %3;"
                "border-radius:%4px;"
                "font-size:%5px;"
                "font-weight:750;"
                "padding:%6px;"
                "}")
                .arg(
                    background)
                .arg(
                    color)
                .arg(
                    border)
                .arg(
                    cardRadius)
                .arg(
                    normalFont)
                .arg(
                    scaledUi(
                        scaleBase,
                        17)));
    }


    // =========================================================================
    // Content
    // =========================================================================
    if (auto *contentLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "chargeContentLayout"))) {

        // 整个页面继续保持 18px 左右留白。
        // 只有汽车舞台会单独突破右边距。
        contentLayout->setContentsMargins(
            scaledUi(
                scaleBase,
                18),
            scaledUi(
                scaleBase,
                18),
            scaledUi(
                scaleBase,
                18),
            scaledUi(
                scaleBase,
                18));


        contentLayout->setSpacing(
            scaledUi(
                scaleBase,
                14));
    }


    // =========================================================================
    // Hero
    // =========================================================================
    if (auto *heroLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "chargeHeroLayout"))) {

        heroLayout->setContentsMargins(
            scaledUi(
                scaleBase,
                2),
            scaledUi(
                scaleBase,
                4),
            scaledUi(
                scaleBase,
                2),
            0);


        heroLayout->setSpacing(
            scaledUi(
                scaleBase,
                4));
    }


    // =========================================================================
    // CAR
    //
    // 目标：
    //
    // 1. 车比上一版略小；
    // 2. 车靠右；
    // 3. 车头能够完整露出来；
    // 4. 显示大部分车身；
    // 5. 右侧只有汽车区域突破页面 margin；
    // 6. 车身最右侧轻微裁掉，形成从右边开进来的感觉。
    // =========================================================================
    if (auto *anchor =
            findChild<QFrame *>(
                QStringLiteral(
                    "chargeCarStageAnchor"))) {

        const bool showFullCar =
            m_state == ChargeState::PendingPayment ||
            m_state == ChargeState::Paid;


        const int stageHeight =
            scaledUi(
                scaleBase,
                showFullCar
                    ? 220
                    : 190);


        anchor->setFixedHeight(
            stageHeight);


        auto *stage =
            findChild<QFrame *>(
                QStringLiteral(
                    "chargeCarStage"));


        auto *content =
            findChild<QWidget *>(
                QStringLiteral(
                    "chargeContent"));


        if (stage &&
            content) {

            const QPoint anchorPos =
                anchor->mapTo(
                    content,
                    QPoint(
                        0,
                        0));


            // 左边仍然从正常内容起点开始，
            // 但是右边直接顶到 content 边缘。
            const int stageX =
                anchorPos.x();


            const int stageWidth =
                qMax(
                    1,
                    content->width() -
                        stageX);


            stage->setGeometry(
                stageX,
                anchorPos.y(),
                stageWidth,
                stageHeight);


            stage->raise();


            if (auto *image =
                    findChild<QLabel *>(
                        QStringLiteral(
                            "chargeCarImage"))) {

                // ------------------------------------------------------------
                // 比上一版明显小一点：
                // 最大宽度大约是 stageWidth + 很少量溢出。
                //
                // 图片向右推出一点，因此尾部/右侧轻微被裁，
                // 车头仍能完整进入画面。
                // ------------------------------------------------------------
                const int overflowRight =
                    showFullCar
                        ? 0
                        : scaledUi(
                              scaleBase,
                              55);


                const int visualWidth =
                    showFullCar
                        ? stageWidth
                        : qMin(
                              stageWidth +
                                  scaledUi(
                                      scaleBase,
                                      16),

                              scaledUi(
                                  scaleBase,
                                  520));


                const int imageHeight =
                    stageHeight;


                const int imageX =
                    showFullCar
                        ? 0
                        : stageWidth -
                              visualWidth +
                              overflowRight;


                image->setGeometry(
                    imageX,
                    0,
                    visualWidth,
                    imageHeight);


                const QPixmap source(
                    QStringLiteral(
                        ":/images/car-main.png"));


                if (!source.isNull()) {

                    image->setPixmap(
                        source.scaled(
                            visualWidth,
                            scaledUi(
                                scaleBase,
                                showFullCar
                                    ? 206
                                    : 174),
                            Qt::KeepAspectRatio,
                            Qt::SmoothTransformation));
                }


                image->raise();
            }
        }
    }


    // =========================================================================
    // Primary Grid
    // =========================================================================
    if (auto *primaryGrid =
            findChild<QGridLayout *>(
                QStringLiteral(
                    "chargePrimaryGrid"))) {

        primaryGrid->setHorizontalSpacing(
            scaledUi(
                scaleBase,
                10));


        primaryGrid->setVerticalSpacing(
            scaledUi(
                scaleBase,
                10));
    }


    // =========================================================================
    // Battery
    // =========================================================================
    if (auto *batteryLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "chargeBatteryLayout"))) {

        batteryLayout->setContentsMargins(
            scaledUi(
                scaleBase,
                14),
            scaledUi(
                scaleBase,
                13),
            scaledUi(
                scaleBase,
                14),
            scaledUi(
                scaleBase,
                13));


        batteryLayout->setSpacing(
            scaledUi(
                scaleBase,
                7));
    }


    if (m_batteryProgressBar) {

        m_batteryProgressBar->setFixedHeight(
            scaledUi(
                scaleBase,
                82));
    }


    if (m_batteryIconLabel) {

        m_batteryIconLabel->setFixedSize(
            iconBox,
            iconBox);


        m_batteryIconLabel->setPixmap(
            QIcon(
                QStringLiteral(
                    ":/icons/battery.svg"))
                .pixmap(
                    QSize(
                        iconSize,
                        iconSize)));
    }


    // =========================================================================
    // Power
    // =========================================================================
    if (auto *powerLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "chargePowerLayout"))) {

        powerLayout->setContentsMargins(
            scaledUi(
                scaleBase,
                14),
            scaledUi(
                scaleBase,
                13),
            scaledUi(
                scaleBase,
                14),
            scaledUi(
                scaleBase,
                13));


        powerLayout->setSpacing(
            scaledUi(
                scaleBase,
                7));
    }


    if (auto *powerIcon =
            findChild<QLabel *>(
                QStringLiteral(
                    "chargePowerIcon"))) {

        powerIcon->setFixedSize(
            iconBox,
            iconBox);


        powerIcon->setPixmap(
            QIcon(
                QStringLiteral(
                    ":/icons/charge.svg"))
                .pixmap(
                    QSize(
                        iconSize,
                        iconSize)));
    }


    // =========================================================================
    // Charging Port
    // =========================================================================
    if (auto *portLayout =
            findChild<QHBoxLayout *>(
                QStringLiteral(
                    "chargePortLayout"))) {

        portLayout->setContentsMargins(
            scaledUi(
                scaleBase,
                15),
            scaledUi(
                scaleBase,
                14),
            scaledUi(
                scaleBase,
                12),
            scaledUi(
                scaleBase,
                14));


        portLayout->setSpacing(
            scaledUi(
                scaleBase,
                12));
    }


    if (auto *portImage =
            findChild<QLabel *>(
                QStringLiteral(
                    "chargePortImage"))) {

        const int imageWidth =
            scaledUi(
                scaleBase,
                145);


        const int imageHeight =
            scaledUi(
                scaleBase,
                105);


        portImage->setFixedSize(
            imageWidth,
            imageHeight);


        const QPixmap source(
            QStringLiteral(
                ":/images/plug-gbt.jpg"));


        if (!source.isNull()) {

            portImage->setPixmap(
                source.scaled(
                    imageWidth,
                    imageHeight,
                    Qt::KeepAspectRatio,
                    Qt::SmoothTransformation));
        }
    }


    // =========================================================================
    // Record
    // =========================================================================
    if (auto *recordIcon =
            findChild<QLabel *>(
                QStringLiteral(
                    "chargeRecordIcon"))) {

        recordIcon->setFixedSize(
            iconBox,
            iconBox);


        recordIcon->setPixmap(
            QIcon(
                QStringLiteral(
                    ":/icons/plug.svg"))
                .pixmap(
                    QSize(
                        iconSize,
                        iconSize)));
    }


    // =========================================================================
    // Live
    // =========================================================================
    if (auto *liveLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "chargeLiveLayout"))) {

        liveLayout->setContentsMargins(
            scaledUi(
                scaleBase,
                14),
            scaledUi(
                scaleBase,
                5),
            scaledUi(
                scaleBase,
                14),
            scaledUi(
                scaleBase,
                5));
    }


    // =========================================================================
    // Guide
    // =========================================================================
    if (auto *guideLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "chargeGuideLayout"))) {

        guideLayout->setContentsMargins(
            scaledUi(
                scaleBase,
                15),
            scaledUi(
                scaleBase,
                14),
            scaledUi(
                scaleBase,
                15),
            scaledUi(
                scaleBase,
                14));


        guideLayout->setSpacing(
            scaledUi(
                scaleBase,
                8));
    }


    if (auto *guideIcon =
            findChild<QLabel *>(
                QStringLiteral(
                    "chargeGuideIcon"))) {

        guideIcon->setFixedSize(
            iconBox,
            iconBox);


        guideIcon->setPixmap(
            QIcon(
                QStringLiteral(
                    ":/icons/network.svg"))
                .pixmap(
                    QSize(
                        iconSize,
                        iconSize)));
    }


    // =========================================================================
    // CTA
    // =========================================================================
    if (m_startButton) {

        m_startButton->setMinimumHeight(
            buttonHeight);


        m_startButton->setIconSize(
            QSize(
                buttonIconSize,
                buttonIconSize));
    }


    if (m_settleButton) {

        m_settleButton->setMinimumHeight(
            buttonHeight);


        m_settleButton->setIconSize(
            QSize(
                buttonIconSize,
                buttonIconSize));
    }


    if (m_payButton) {

        m_payButton->setMinimumHeight(
            buttonHeight);


        m_payButton->setIconSize(
            QSize(
                buttonIconSize,
                buttonIconSize));
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
// 实时数据
// ============================================================================
void ChargePage::updateChargingInfo()
{
    if (m_state !=
        ChargeState::Charging) {

        return;
    }


    if (!m_networkAvailable) {

        return;
    }


    if (!m_chargeStartedAt.isValid()) {

        return;
    }


    qint64 elapsedMs =
        m_accumulatedChargeMs;


    if (m_networkAvailable &&
        m_onlineChargeTimer.isValid()) {

        elapsedMs +=
            m_onlineChargeTimer.elapsed();
    }


    if (elapsedMs < 0) {

        elapsedMs =
            0;
    }


    const qint64 elapsedSeconds =
        elapsedMs /
        1000;


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
    // Power
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


        // 接口卡继续使用同一份真实功率
        if (auto *portTip =
                findChild<QLabel *>(
                    QStringLiteral(
                        "chargePortTip"))) {

            portTip->setText(
                QStringLiteral(
                    "充电接口示意 · 当前功率 %1 kW")
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


        // ====================================================================
        // SOC
        // ====================================================================
        if (m_hasBatteryInfo &&
            m_batteryCapacityKwh >
                0.0) {

            m_currentSoc =
                m_startSoc +
                (m_currentKwh /
                 m_batteryCapacityKwh) *
                    100.0;


            m_currentSoc =
                qBound(
                    m_startSoc,
                    m_currentSoc,
                    m_targetSoc);


            if (m_batteryPercentLabel) {

                m_batteryPercentLabel->setText(
                    QStringLiteral(
                        "%1%")
                        .arg(
                            qRound(
                                m_currentSoc)));
            }


            if (m_batteryProgressBar) {

                m_batteryProgressBar->setValue(
                    qRound(
                        m_currentSoc));
            }


            if (m_batteryRangeLabel) {

                m_batteryRangeLabel->setText(
                    QStringLiteral(
                        "初始电量 %1%    ·    目标 %2%")
                        .arg(
                            qRound(
                                m_startSoc))
                        .arg(
                            qRound(
                                m_targetSoc)));
            }

        } else {

            if (m_batteryPercentLabel) {

                m_batteryPercentLabel->setText(
                    QStringLiteral(
                        "--%"));
            }


            if (m_batteryProgressBar) {

                m_batteryProgressBar->setValue(
                    0);
            }
        }


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


        if (auto *portTip =
                findChild<QLabel *>(
                    QStringLiteral(
                        "chargePortTip"))) {

            portTip->setText(
                QStringLiteral(
                    "充电接口示意 · 当前功率 -- kW"));
        }
    }


    // ========================================================================
    // Fee
    // ========================================================================
    if (m_powerKw > 0.0 &&
        m_unitPrice > 0.0) {

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


// ============================================================================
// Reset Battery
// ============================================================================
void ChargePage::resetBatteryInfo()
{
    m_startSoc =
        -1.0;


    m_batteryCapacityKwh =
        0.0;


    m_targetSoc =
        100.0;


    m_currentSoc =
        -1.0;


    m_hasBatteryInfo =
        false;


    if (m_batteryPercentLabel) {

        m_batteryPercentLabel->setText(
            QStringLiteral(
                "--%"));
    }


    if (m_batteryProgressBar) {

        m_batteryProgressBar->setValue(
            0);
    }


    if (m_batteryRangeLabel) {

        m_batteryRangeLabel->setText(
            QStringLiteral(
                "初始电量 --%    ·    目标 100%"));
    }
}


// ============================================================================
// 网络断开
// ============================================================================
void ChargePage::handleNetworkDisconnected()
{
    if (!m_networkAvailable) {

        return;
    }


    qInfo().noquote()
        << "[NO24] DISCONNECTED ENTER"
        << "order=" << m_orderNo
        << "accMs=" << m_accumulatedChargeMs
        << "timerValid="
        << m_onlineChargeTimer.isValid()
        << "timerMs="
        << (m_onlineChargeTimer.isValid()
                ? m_onlineChargeTimer.elapsed()
                : -1);


    if (m_state ==
        ChargeState::Charging) {

        updateChargingInfo();


        if (m_onlineChargeTimer.isValid()) {

            m_accumulatedChargeMs +=
                m_onlineChargeTimer.elapsed();


            m_onlineChargeTimer.invalidate();
        }


        if (!m_orderNo.isEmpty()) {

            s_no24ResumeOrderNo =
                m_orderNo;


            s_no24ResumeAccumulatedMs =
                m_accumulatedChargeMs;


            s_no24ResumeValid =
                true;
        }


        qInfo().noquote()
            << "[NO24] SNAPSHOT SAVED"
            << "order="
            << s_no24ResumeOrderNo
            << "accMs="
            << s_no24ResumeAccumulatedMs;


        stopChargeTimer();
    }


    m_networkAvailable =
        false;


    if (m_state !=
        ChargeState::Charging) {

        return;
    }


    if (m_batteryStateLabel) {

        m_batteryStateLabel->setText(
            QStringLiteral(
                "等待重连"));
    }


    if (m_tipLabel) {

        m_tipLabel->setText(
            QStringLiteral(
                "网络已断开，正在重新连接，实时数据已暂停更新"));
    }
}


// ============================================================================
// 网络恢复
// ============================================================================
void ChargePage::handleNetworkReconnected()
{
    qInfo().noquote()
        << "[NO24] RECONNECTED ENTER"
        << "order=" << m_orderNo
        << "elapsedOrder="
        << m_elapsedOrderNo
        << "network="
        << m_networkAvailable
        << "accMs="
        << m_accumulatedChargeMs
        << "timerValid="
        << m_onlineChargeTimer.isValid()
        << "timerMs="
        << (m_onlineChargeTimer.isValid()
                ? m_onlineChargeTimer.elapsed()
                : -1);


    if (m_networkAvailable) {

        return;
    }


    m_networkAvailable =
        true;


    if (m_state !=
        ChargeState::Charging) {

        return;
    }


    if (!m_onlineChargeTimer.isValid()) {

        m_onlineChargeTimer.start();


        qInfo().noquote()
            << "[NO24] RECONNECTED STARTED"
            << "accMs="
            << m_accumulatedChargeMs
            << "timerMs="
            << m_onlineChargeTimer.elapsed();
    }


    if (m_batteryStateLabel) {

        m_batteryStateLabel->setText(
            QStringLiteral(
                "充电中"));
    }


    if (m_tipLabel) {

        m_tipLabel->setText(
            QStringLiteral(
                "网络已恢复，实时充电记录继续更新"));
    }


    startChargeTimer();
}
