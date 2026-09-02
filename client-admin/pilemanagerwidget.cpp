#include "pilemanagerwidget.h"
#include "netclient.h"
#include "protocol.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>

PileManagerWidget::PileManagerWidget(NetClient *netClient, QWidget *parent)
    : QWidget(parent), m_net(netClient)
{
    initUI();
    loadPiles();
}

void PileManagerWidget::initUI()
{
    auto *mainLayout = new QVBoxLayout(this);

    auto *topLayout = new QHBoxLayout();
    m_refreshBtn = new QPushButton("刷新电桩列表", this);
    topLayout->addWidget(m_refreshBtn);
    topLayout->addStretch();
    mainLayout->addLayout(topLayout);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels({"桩编号", "所属电站", "类型", "功率(kW)", "状态", "操作"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mainLayout->addWidget(m_table);

    connect(m_refreshBtn, &QPushButton::clicked, this, &PileManagerWidget::loadPiles);
}

void PileManagerWidget::loadPiles()
{
    if (!m_net) return;

    // 对应组长实现的 admin_pile_list
    QJsonObject req = Protocol::makeRequest(Protocol::MsgType::AdminPileList, QJsonObject());
    QJsonObject resp = m_net->request(req);

    if (resp.value("code").toInt() != Protocol::Ok) return;

    QJsonArray piles = resp.value("data").toArray();
    m_table->setRowCount(0);

    for (int i = 0; i < piles.size(); ++i) {
        QJsonObject p = piles[i].toObject();
        qint64 pileId = p.value("id").toInteger();
        m_table->insertRow(i);

        m_table->setItem(i, 0, new QTableWidgetItem(p.value("code").toString()));
        m_table->setItem(i, 1, new QTableWidgetItem(p.value("station_name").toString()));
        m_table->setItem(i, 2, new QTableWidgetItem(p.value("type").toString()));
        m_table->setItem(i, 3, new QTableWidgetItem(QString::number(p.value("power_kw").toDouble(), 'f', 1)));
        m_table->setItem(i, 4, new QTableWidgetItem(p.value("status").toString()));

        // 重启操作按钮
        QPushButton *restartBtn = new QPushButton("重启", this);
        restartBtn->setProperty("pileId", pileId);
        connect(restartBtn, &QPushButton::clicked, this, &PileManagerWidget::onRestartClicked);
        m_table->setCellWidget(i, 5, restartBtn);
    }
}

void PileManagerWidget::onRestartClicked()
{
    auto *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    qint64 pileId = btn->property("pileId").toLongLong();

    // 对应组长实现的 admin_pile_restart
    QJsonObject data;
    data["pile_id"] = pileId;
    QJsonObject req = Protocol::makeRequest(Protocol::MsgType::AdminPileRestart, data);
    QJsonObject resp = m_net->request(req);

    if (resp.value("code").toInt() == Protocol::Ok) {
        QMessageBox::information(this, "提示", "电桩重启成功，状态已恢复为 idle");
        loadPiles();
    } else {
        QMessageBox::warning(this, "错误", resp.value("msg").toString());
    }
}