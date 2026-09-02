#include "stationlistpage.h"

#include "netclient.h"
#include "protocol.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QVBoxLayout>

StationListPage::StationListPage(NetClient *net, QWidget *parent)
    : QWidget(parent)
    , m_net(net)
{
    auto *title = new QLabel(QStringLiteral("附近充电站"), this);
    title->setStyleSheet("font-size:22px;font-weight:bold;padding:12px 16px 4px 16px;");

    m_tip = new QLabel(this);
    m_tip->setStyleSheet("color:#86909c;padding:0 16px 8px 16px;");

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *container = new QWidget;
    m_listLayout = new QVBoxLayout(container);
    m_listLayout->setContentsMargins(12, 4, 12, 12);
    m_listLayout->setSpacing(12);
    scroll->setWidget(container);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(title);
    layout->addWidget(m_tip);
    layout->addWidget(scroll, 1);
}

void StationListPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_loaded) {
        m_loaded = true;
        loadStations();
    }
}

void StationListPage::clearList()
{
    while (QLayoutItem *item = m_listLayout->takeAt(0)) {
        if (QWidget *w = item->widget())
            w->deleteLater();
        delete item;
    }
}

void StationListPage::loadStations()
{
    clearList();
    m_tip->setText(QStringLiteral("加载中…"));

    const QJsonObject resp = m_net->request(
        Protocol::makeRequest(Protocol::MsgType::StationList));

    const int code = resp.value("code").toInt();
    if (code != Protocol::Ok) {
        m_tip->setText(QStringLiteral("加载失败：%1").arg(resp.value("msg").toString()));
        return;
    }

    const QJsonArray list = resp.value("data").toObject().value("list").toArray();
    if (list.isEmpty()) {
        m_tip->setText(QStringLiteral("暂无充电站"));
        return;
    }
    m_tip->setText(QStringLiteral("共 %1 个充电站").arg(list.size()));

    for (const QJsonValue &v : list) {
        const QJsonObject s = v.toObject();
        const qint64  id      = s.value("id").toVariant().toLongLong();
        const QString name    = s.value("name").toString();
        const QString address = s.value("address").toString();
        const double  price   = s.value("price").toDouble();
        const int     total   = s.value("total").toInt();
        const int     idle    = s.value("idle").toInt();

        auto *card = new QPushButton(this);
        card->setCursor(Qt::PointingHandCursor);
        card->setMinimumHeight(112);
        card->setStyleSheet(
            "QPushButton{text-align:left;padding:14px;border:1px solid #d6e4ff;"
            "border-radius:10px;background:#ffffff;}"
            "QPushButton:hover{border-color:#1d4ed8;background:#eef4ff;}");

        // 卡片内容：名称+价格 / 地址 / 空闲
        auto *body = new QVBoxLayout(card);
        body->setContentsMargins(2, 2, 2, 2);
        body->setSpacing(4);

        auto *row1 = new QHBoxLayout;
        auto *nameLabel = new QLabel(name, card);
        nameLabel->setStyleSheet(
            "font-size:17px;font-weight:bold;border:none;background:transparent;");
        nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto *priceLabel = new QLabel(
            QStringLiteral("￥%1/度").arg(price, 0, 'f', 2), card);
        priceLabel->setStyleSheet(
            "color:#ff6a00;font-size:15px;font-weight:bold;border:none;background:transparent;");
        priceLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        row1->addWidget(nameLabel, 1);
        row1->addWidget(priceLabel);

        auto *addrLabel = new QLabel(address, card);
        addrLabel->setWordWrap(true);
        addrLabel->setStyleSheet(
            "color:#86909c;font-size:12px;border:none;background:transparent;");
        addrLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

        auto *idleLabel = new QLabel(
            QStringLiteral("空闲 %1/%2").arg(idle).arg(total), card);
        const QString idleColor =
            idle > 0 ? QStringLiteral("#16a34a") : QStringLiteral("#999999");
        idleLabel->setStyleSheet(
            QStringLiteral("color:%1;font-size:13px;font-weight:bold;"
                           "border:none;background:transparent;").arg(idleColor));
        idleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

        body->addLayout(row1);
        body->addWidget(addrLabel);
        body->addWidget(idleLabel);

        connect(card, &QPushButton::clicked, this, [this, id, name]() {
            emit stationSelected(id, name);
        });

        m_listLayout->addWidget(card);
    }
}
