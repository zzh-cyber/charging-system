#include "pilelistpage.h"

#include "netclient.h"
#include "protocol.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

PileListPage::PileListPage(NetClient *net, QWidget *parent)
    : QWidget(parent)
    , m_net(net)
{
    auto *topBar = new QHBoxLayout;
    m_backBtn = new QPushButton(QStringLiteral("← 返回"), this);
    m_backBtn->setCursor(Qt::PointingHandCursor);
    m_backBtn->setStyleSheet(
        "QPushButton{background:transparent;color:#1d4ed8;border:1px solid #1d4ed8;"
        "border-radius:8px;padding:6px 14px;font-weight:600;}"
        "QPushButton:hover{background:#1d4ed8;color:#ffffff;}");
    m_title = new QLabel(this);
    m_title->setStyleSheet("font-size:20px;font-weight:bold;");
    topBar->addWidget(m_backBtn);
    topBar->addWidget(m_title, 1);

    m_tip = new QLabel(this);
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
        row->setStyleSheet(
            "QFrame{border:1px solid #d6e4ff;border-radius:8px;background:#ffffff;}");

        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(12, 10, 12, 10);

        auto *codeLabel = new QLabel(code, row);
        codeLabel->setStyleSheet("font-size:16px;font-weight:bold;border:none;");

        auto *infoLabel = new QLabel(
            QStringLiteral("%1 · %2 kW").arg(typeText).arg(power, 0, 'f', 1), row);
        infoLabel->setStyleSheet("color:#86909c;border:none;");

        auto *statusLabel = new QLabel(statusText, row);
        statusLabel->setStyleSheet(
            QStringLiteral("color:%1;font-weight:bold;border:none;").arg(color));

        // 「预约」按钮（UI 占位）：仅闲置桩可点，待 B 接 reserve 接口
        auto *reserveBtn = new QPushButton(QStringLiteral("预约"), row);
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
        connect(reserveBtn, &QPushButton::clicked, this, [this, code]() {
            // TODO(B)：接入 reserve 接口并跳转预约流程。当前为 UI 占位。
            QMessageBox::information(this, QStringLiteral("预约"),
                QStringLiteral("桩 %1 的预约功能即将接入，敬请期待").arg(code));
        });

        rowLayout->addWidget(codeLabel);
        rowLayout->addWidget(infoLabel, 1);
        rowLayout->addWidget(statusLabel);
        rowLayout->addWidget(reserveBtn);

        m_listLayout->addWidget(row);
    }
}

void PileListPage::clearList()
{
    while (QLayoutItem *item = m_listLayout->takeAt(0)) {
        if (QWidget *w = item->widget())
            w->deleteLater();
        delete item;
    }
}
