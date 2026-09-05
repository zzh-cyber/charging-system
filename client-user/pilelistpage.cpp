#include "pilelistpage.h"

#include "netclient.h"
#include "protocol.h"
#include "windowhelper.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QResizeEvent>


PileListPage::PileListPage(NetClient *net, QWidget *parent)
    : QWidget(parent)
    , m_net(net)
{
    auto *topBar = new QHBoxLayout;
    m_backBtn = new QPushButton(QStringLiteral("← 返回"), this);
    m_backBtn->setObjectName(
    QStringLiteral("pileBackButton"));

    m_backBtn->setCursor(Qt::PointingHandCursor);
    m_backBtn->setStyleSheet(
        "QPushButton{background:transparent;color:#1d4ed8;border:1px solid #1d4ed8;"
        "border-radius:8px;padding:6px 14px;font-weight:600;}"
        "QPushButton:hover{background:#1d4ed8;color:#ffffff;}");
    m_title = new QLabel(this);
    m_title->setObjectName(
    QStringLiteral("pileTitle"));

    m_title->setStyleSheet("font-size:20px;font-weight:bold;");
    topBar->addWidget(m_backBtn);
    topBar->addWidget(m_title, 1);

    m_tip = new QLabel(this);
    m_tip->setObjectName(
    QStringLiteral("pileTip"));

    m_tip->setStyleSheet("color:#86909c;padding:0 16px 8px 16px;");

    auto *container = new QWidget(this);
    m_listLayout = new QVBoxLayout(container);
    m_listLayout->setContentsMargins(12, 4, 12, 12);
    m_listLayout->setSpacing(10);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(topBar);
    layout->addWidget(m_tip);
    layout->addWidget(container, 1);

    connect(m_backBtn, &QPushButton::clicked, this, &PileListPage::back);
    applyResponsiveStyle();

}
void PileListPage::setUserId(qint64 userId)
{
    m_userId = userId;
}


void PileListPage::loadStation(qint64 stationId, const QString &name)
{
    clearList();
    m_title->setText(name);
    m_tip->setText(QStringLiteral("加载中…"));

    QJsonObject data;
    data["station_id"] = stationId;
    const QJsonObject resp = m_net->request(
        Protocol::makeRequest(Protocol::MsgType::PileList, data));

    const int code = resp.value("code").toInt();
    if (code != Protocol::Ok) {
        m_tip->setText(QStringLiteral("加载失败：%1").arg(resp.value("msg").toString()));
        return;
    }

    const QJsonArray list = resp.value("data").toObject().value("list").toArray();
    if (list.isEmpty()) {
        m_tip->setText(QStringLiteral("该站暂无电桩"));
        return;
    }
    m_tip->setText(QStringLiteral("共 %1 个电桩").arg(list.size()));

    for (const QJsonValue &v : list) {
        const QJsonObject p = v.toObject();
        const qint64 pileId =
        p.value("id")
            .toVariant()
            .toLongLong();

        const QString code   = p.value("code").toString();
        const QString type   = p.value("type").toString();
        const double  power  = p.value("power_kw").toDouble();
        const QString status = p.value("status").toString();

        // 类型/状态 → 中文映射（对应 api-contract.md 的字段取值）
        const QString typeText = (type == "slow") ? QStringLiteral("慢充")
                                                  : QStringLiteral("快充");
        QString statusText = status;
        if (status == "idle")
            statusText = QStringLiteral("闲置");
        else if (status == "busy")
            statusText = QStringLiteral("在用");
        else if (status == "fault")
            statusText = QStringLiteral("故障");

        // 状态配色：闲置绿 / 在用蓝 / 故障红
        QString color;
        if (status == "idle")
            color = QStringLiteral("#16a34a");
        else if (status == "busy")
            color = QStringLiteral("#2563eb");
        else
            color = QStringLiteral("#e11d48");

        auto *row = new QFrame(this);
        row->setObjectName(
    QStringLiteral("pileRow"));

        row->setStyleSheet(
            "QFrame{border:1px solid #d6e4ff;border-radius:8px;background:#ffffff;}");

        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(12, 10, 12, 10);

        auto *codeLabel = new QLabel(code, row);
        codeLabel->setObjectName(
    QStringLiteral("pileCodeLabel"));

        codeLabel->setStyleSheet("font-size:16px;font-weight:bold;border:none;");

        auto *infoLabel = new QLabel(
            QStringLiteral("%1 · %2 kW").arg(typeText).arg(power, 0, 'f', 1), row);
        infoLabel->setObjectName(
    QStringLiteral("pileInfoLabel"));

        infoLabel->setStyleSheet("color:#86909c;border:none;");

        auto *statusLabel = new QLabel(statusText, row);
        statusLabel->setObjectName(
            QStringLiteral("pileStatusLabel"));

        statusLabel->setProperty(
            "pileStatusColor",
            color);

        statusLabel->setStyleSheet(
            QStringLiteral("color:%1;font-weight:bold;border:none;").arg(color));

        // 「预约」按钮（UI 占位）：仅闲置桩可点，待 B 接 reserve 接口
        auto *reserveBtn = new QPushButton(QStringLiteral("预约"), row);
        reserveBtn->setObjectName(
    QStringLiteral("pileReserveButton"));

        reserveBtn->setCursor(Qt::PointingHandCursor);
        const bool canReserve = (status == "idle");
        reserveBtn->setEnabled(canReserve);
        if (canReserve) {
            reserveBtn->setStyleSheet(
                "QPushButton{padding:4px 14px;border:1px solid #1d4ed8;border-radius:6px;"
                "color:#1d4ed8;background:#ffffff;font-weight:bold;}"
                "QPushButton:hover{background:#1d4ed8;color:#ffffff;}");
        } else {
            reserveBtn->setStyleSheet(
                "QPushButton{padding:4px 14px;border:1px solid #ddd;border-radius:6px;"
                "color:#bbb;background:#f5f5f5;}");
            reserveBtn->setToolTip(QStringLiteral("该桩当前不可预约"));
        }
        connect(
            reserveBtn,
            &QPushButton::clicked,
            this,
            [this, reserveBtn, pileId]() {

                if (m_userId <= 0) {
                    QMessageBox::warning(
                        this,
                        QStringLiteral("预约失败"),
                        QStringLiteral("用户信息无效，请重新登录"));
                    return;
                }

                if (pileId <= 0) {
                    QMessageBox::warning(
                        this,
                        QStringLiteral("预约失败"),
                        QStringLiteral("电桩信息无效"));
                    return;
                }

                reserveBtn->setEnabled(false);
                reserveBtn->setText(
                    QStringLiteral("预约中…"));

                // 使用已有 reserve 协议
                QJsonObject data;
                data["pile_id"] =
                    pileId;

                const QJsonObject resp =
                    m_net->request(
                        Protocol::makeRequest(
                            Protocol::MsgType::Reserve,
                            data));

                const int resultCode =
                    resp.value("code").toInt();

                const QString message =
                    resp.value("msg").toString();

                if (resultCode != Protocol::Ok) {

                    reserveBtn->setEnabled(true);

                    reserveBtn->setText(
                        QStringLiteral("预约"));

                    QMessageBox::warning(
                        this,
                        QStringLiteral("预约失败"),
                        message);

                    return;
                }

                const QString orderNo =
                    resp.value("data")
                        .toObject()
                        .value("order_no")
                        .toString();

                if (orderNo.isEmpty()) {

                    reserveBtn->setEnabled(true);

                    reserveBtn->setText(
                        QStringLiteral("预约"));

                    QMessageBox::warning(
                        this,
                        QStringLiteral("预约失败"),
                        QStringLiteral(
                            "服务器未返回订单号"));

                    return;
                }

                reserveBtn->setText(
                    QStringLiteral("已预约"));

                QMessageBox::information(
                    this,
                    QStringLiteral("预约成功"),
                    QStringLiteral(
                        "预约成功，即将进入充电页面"));

                emit reservationSucceeded(
                    orderNo);
            });


        rowLayout->addWidget(codeLabel);
        rowLayout->addWidget(infoLabel, 1);
        rowLayout->addWidget(statusLabel);
        rowLayout->addWidget(reserveBtn);

        m_listLayout->addWidget(row);
    }
    applyResponsiveStyle();

}


void PileListPage::clearList()
{
    while (QLayoutItem *item = m_listLayout->takeAt(0)) {
        if (QWidget *w = item->widget())
            w->deleteLater();
        delete item;
    }
}
void PileListPage::resizeEvent(
    QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    applyResponsiveStyle();
}


void PileListPage::applyResponsiveStyle()
{
    QWidget *scaleBase =
        window()
            ? window()
            : this;

    const int titleFont =
        scaledUi(scaleBase, 20);

    const int normalFont =
        scaledUi(scaleBase, 14);

    const int codeFont =
        scaledUi(scaleBase, 16);

    const int buttonFont =
        scaledUi(scaleBase, 14);

    // ============================================================
    // 返回按钮
    // ============================================================
    if (m_backBtn) {

        m_backBtn->setStyleSheet(
            QStringLiteral(
                "QPushButton{"
                "background:transparent;"
                "color:#1d4ed8;"
                "border:1px solid #1d4ed8;"
                "border-radius:%1px;"
                "padding:%2px %3px;"
                "font-size:%4px;"
                "font-weight:600;"
                "}"
                "QPushButton:hover{"
                "background:#1d4ed8;"
                "color:#ffffff;"
                "}")
                .arg(
                    scaledUi(
                        scaleBase,
                        8))
                .arg(
                    scaledUi(
                        scaleBase,
                        6))
                .arg(
                    scaledUi(
                        scaleBase,
                        14))
                .arg(buttonFont));
    }

    // ============================================================
    // 标题
    // ============================================================
    if (m_title) {

        m_title->setStyleSheet(
            QStringLiteral(
                "font-size:%1px;"
                "font-weight:bold;")
                .arg(titleFont));
    }

    // ============================================================
    // 顶部提示
    // ============================================================
    if (m_tip) {

        m_tip->setStyleSheet(
            QStringLiteral(
                "color:#86909c;"
                "font-size:%1px;"
                "padding:0 %2px %3px %2px;")
                .arg(normalFont)
                .arg(
                    scaledUi(
                        scaleBase,
                        16))
                .arg(
                    scaledUi(
                        scaleBase,
                        8)));
    }

    // ============================================================
    // 列表整体间距
    // ============================================================
    if (m_listLayout) {

        m_listLayout->setContentsMargins(
            scaledUi(
                scaleBase,
                12),
            scaledUi(
                scaleBase,
                4),
            scaledUi(
                scaleBase,
                12),
            scaledUi(
                scaleBase,
                12));

        m_listLayout->setSpacing(
            scaledUi(
                scaleBase,
                10));
    }

    // ============================================================
    // 每一个桩卡片
    // ============================================================
    const auto rows =
        findChildren<QFrame *>(
            QStringLiteral(
                "pileRow"));

    for (QFrame *row : rows) {

        row->setStyleSheet(
            QStringLiteral(
                "QFrame#pileRow{"
                "border:1px solid #d6e4ff;"
                "border-radius:%1px;"
                "background:#ffffff;"
                "}")
                .arg(
                    scaledUi(
                        scaleBase,
                        8)));

        if (auto *rowLayout =
                qobject_cast<QHBoxLayout *>(
                    row->layout())) {

            rowLayout->setContentsMargins(
                scaledUi(
                    scaleBase,
                    12),
                scaledUi(
                    scaleBase,
                    10),
                scaledUi(
                    scaleBase,
                    12),
                scaledUi(
                    scaleBase,
                    10));

            rowLayout->setSpacing(
                scaledUi(
                    scaleBase,
                    8));
        }
    }

    // ============================================================
    // 桩编号
    // ============================================================
    const auto codeLabels =
        findChildren<QLabel *>(
            QStringLiteral(
                "pileCodeLabel"));

    for (QLabel *label : codeLabels) {

        label->setStyleSheet(
            QStringLiteral(
                "font-size:%1px;"
                "font-weight:bold;"
                "border:none;")
                .arg(codeFont));
    }

    // ============================================================
    // 类型 + 功率
    // ============================================================
    const auto infoLabels =
        findChildren<QLabel *>(
            QStringLiteral(
                "pileInfoLabel"));

    for (QLabel *label : infoLabels) {

        label->setStyleSheet(
            QStringLiteral(
                "font-size:%1px;"
                "color:#86909c;"
                "border:none;")
                .arg(normalFont));
    }

    // ============================================================
    // 状态
    // ============================================================
    const auto statusLabels =
        findChildren<QLabel *>(
            QStringLiteral(
                "pileStatusLabel"));

    for (QLabel *label : statusLabels) {

        const QString color =
            label->property(
                "pileStatusColor")
                .toString();

        label->setStyleSheet(
            QStringLiteral(
                "font-size:%1px;"
                "color:%2;"
                "font-weight:bold;"
                "border:none;")
                .arg(normalFont)
                .arg(color));
    }

    // ============================================================
    // 预约按钮
    // ============================================================
    const auto reserveButtons =
        findChildren<QPushButton *>(
            QStringLiteral(
                "pileReserveButton"));

    for (QPushButton *button :
         reserveButtons) {

        button->setStyleSheet(
            QStringLiteral(
                "QPushButton{"
                "padding:%1px %2px;"
                "border:1px solid #1d4ed8;"
                "border-radius:%3px;"
                "font-size:%4px;"
                "font-weight:bold;"
                "color:#1d4ed8;"
                "background:#ffffff;"
                "}"

                "QPushButton:hover{"
                "background:#1d4ed8;"
                "color:#ffffff;"
                "}"

                "QPushButton:disabled{"
                "border-color:#dddddd;"
                "color:#bbbbbb;"
                "background:#f5f5f5;"
                "}")
                .arg(
                    scaledUi(
                        scaleBase,
                        4))
                .arg(
                    scaledUi(
                        scaleBase,
                        14))
                .arg(
                    scaledUi(
                        scaleBase,
                        6))
                .arg(buttonFont));
    }
}
