#include "stationmanagerwidget.h"
#include "netclient.h"
#include "protocol.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>

StationManagerWidget::StationManagerWidget(NetClient *netClient, QWidget *parent)
    : QWidget(parent), m_net(netClient)
{
    initUI();
    loadStations();
}

void StationManagerWidget::initUI()
{
    auto *mainLayout = new QVBoxLayout(this);

    auto *topLayout = new QHBoxLayout();
    m_refreshBtn = new QPushButton("刷新电站列表", this);
    topLayout->addWidget(m_refreshBtn);
    topLayout->addStretch();
    mainLayout->addLayout(topLayout);

    // 统计：桩数与在线率
    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({"电站名称", "地址", "基础电费(元/度)", "总桩数", "在线率"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mainLayout->addWidget(m_table);

    connect(m_refreshBtn, &QPushButton::clicked, this, &StationManagerWidget::loadStations);
}

void StationManagerWidget::loadStations()
{
    if (!m_net) return;

    // 对应组长实现的 admin_station_list
    QJsonObject req = Protocol::makeRequest(Protocol::MsgType::AdminStationList, QJsonObject());
    QJsonObject resp = m_net->request(req);

    if (resp.value("code").toInt() != Protocol::Ok) return;

    QJsonArray stations = resp.value("data").toArray();
    m_table->setRowCount(0);

    for (int i = 0; i < stations.size(); ++i) {
        QJsonObject s = stations[i].toObject();
        m_table->insertRow(i);

        m_table->setItem(i, 0, new QTableWidgetItem(s.value("name").toString()));
        m_table->setItem(i, 1, new QTableWidgetItem(s.value("address").toString()));
        m_table->setItem(i, 2, new QTableWidgetItem(QString::number(s.value("price").toDouble(), 'f', 2)));
        m_table->setItem(i, 3, new QTableWidgetItem(QString::number(s.value("pile_count").toInt())));

        double rate = s.value("online_rate").toDouble() * 100.0;
        m_table->setItem(i, 4, new QTableWidgetItem(QString("%1%").arg(QString::number(rate, 'f', 1))));
    }
}