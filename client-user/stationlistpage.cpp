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
#include <QStackedWidget>
#include <QVBoxLayout>

StationListPage::StationListPage(NetClient *net, QWidget *parent)
    : QWidget(parent)
    , m_net(net)
{
    auto *header = new QHBoxLayout;
    header->setContentsMargins(16, 12, 16, 4);
    auto *title = new QLabel(QStringLiteral("附近充电站"), this);
    title->setStyleSheet("font-size:22px;font-weight:bold;");
    auto *refreshBtn = new QPushButton(QStringLiteral("刷新"), this);
    refreshBtn->setCursor(Qt::PointingHandCursor);
    refreshBtn->setStyleSheet(
        "QPushButton{background:#1d4ed8;color:#ffffff;border:none;"
        "border-radius:6px;padding:6px 14px;font-size:13px;}"
        "QPushButton:hover{background:#1e40af;}");
    connect(refreshBtn, &QPushButton::clicked, this, &StationListPage::loadStations);
    header->addWidget(title, 1);
    header->addWidget(refreshBtn);

    m_tip = new QLabel(this);
    m_tip->setStyleSheet("color:#86909c;padding:0 16px 8px 16px;");

    m_stack = new QStackedWidget(this);

    // ---- 页0：加载中 ----
    auto *loadingPage = new QWidget;
    {
        auto *l = new QVBoxLayout(loadingPage);
        l->setAlignment(Qt::AlignCenter);
        auto *lab = new QLabel(QStringLiteral("加载中…"), loadingPage);
        lab->setStyleSheet("color:#86909c;font-size:15px;");
        l->addWidget(lab);
    }
    m_stack->addWidget(loadingPage);

    // ---- 页1：内容（充电站列表）----
    auto *contentPage = new QWidget;
    {
        auto *l = new QVBoxLayout(contentPage);
        l->setContentsMargins(0, 0, 0, 0);
        auto *scroll = new QScrollArea(contentPage);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        auto *container = new QWidget;
        m_listLayout = new QVBoxLayout(container);
        m_listLayout->setContentsMargins(12, 4, 12, 12);
        m_listLayout->setSpacing(12);
        scroll->setWidget(container);
        l->addWidget(scroll);
    }
    m_stack->addWidget(contentPage);

    // ---- 页2：空数据 ----
    auto *emptyPage = new QWidget;
    {
        auto *l = new QVBoxLayout(emptyPage);
        l->setAlignment(Qt::AlignCenter);
        l->setSpacing(12);
        auto *lab = new QLabel(QStringLiteral("暂无充电站"), emptyPage);
        lab->setStyleSheet("color:#86909c;font-size:15px;");
        auto *btn = new QPushButton(QStringLiteral("刷新"), emptyPage);
        btn->setCursor(Qt::PointingHandCursor);
        connect(btn, &QPushButton::clicked, this, &StationListPage::loadStations);
        l->addWidget(lab);
        l->addWidget(btn, 0, Qt::AlignHCenter);
    }
    m_stack->addWidget(emptyPage);

    // ---- 页3：错误 ----
    auto *errorPage = new QWidget;
    {
        auto *l = new QVBoxLayout(errorPage);
        l->setAlignment(Qt::AlignCenter);
        l->setSpacing(12);
        auto *lab = new QLabel(QStringLiteral("加载失败，请检查网络后重试"), errorPage);
        lab->setStyleSheet("color:#e5484d;font-size:15px;");
        auto *btn = new QPushButton(QStringLiteral("重试"), errorPage);
        btn->setCursor(Qt::PointingHandCursor);
        connect(btn, &QPushButton::clicked, this, &StationListPage::loadStations);
        l->addWidget(lab);
        l->addWidget(btn, 0, Qt::AlignHCenter);
    }
    m_stack->addWidget(errorPage);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(header);
    layout->addWidget(m_tip);
    layout->addWidget(m_stack, 1);

    m_stack->setCurrentIndex(0);

    // 断线重连成功后自动刷新列表（NO.7）
    connect(m_net, &NetClient::reconnected, this, &StationListPage::loadStations);
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
    m_stack->setCurrentIndex(0);   // loading
    m_tip->clear();

    const QJsonObject resp = m_net->request(
        Protocol::makeRequest(Protocol::MsgType::StationList));

    const int code = resp.value("code").toInt();
    const QJsonArray list = resp.value("data").toObject().value("list").toArray();

    // 失败：有缓存则展示缓存（标注"缓存数据"），否则进错误态
    if (code != Protocol::Ok) {
        if (!m_cachedList.isEmpty()) {
            buildCards(m_cachedList);
            m_tip->setText(QStringLiteral("网络异常，当前为缓存数据"));
            m_stack->setCurrentIndex(1);
            return;
        }
        m_stack->setCurrentIndex(3);   // error
        return;
    }

    // 空数据
    if (list.isEmpty()) {
        m_stack->setCurrentIndex(2);   // empty
        return;
    }

    // 成功：更新缓存并展示
    m_cachedList = list;
    buildCards(list);
    m_tip->setText(QStringLiteral("共 %1 个充电站").arg(list.size()));
    m_stack->setCurrentIndex(1);       // content
}

void StationListPage::buildCards(const QJsonArray &list)
{
    clearList();

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
