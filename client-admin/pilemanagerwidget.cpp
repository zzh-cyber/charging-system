#include "pilemanagerwidget.h"
#include "netclient.h"
#include "protocol.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidgetItem>

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

    m_table->setHorizontalHeaderLabels({
        "桩编号",
        "所属电站",
        "类型",
        "功率(kW)",
        "状态",
        "操作"
    });

    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);

    mainLayout->addWidget(m_table);

    connect(
        m_refreshBtn,
        &QPushButton::clicked,
        this,
        &PileManagerWidget::loadPiles
    );
}

void PileManagerWidget::loadPiles()
{
    if (!m_net) {
        return;
    }

    // 请求：admin_pile_list
    QJsonObject req =
        Protocol::makeRequest(
            Protocol::MsgType::AdminPileList,
            QJsonObject()
        );

    QJsonObject resp = m_net->request(req);

    // 请求失败
    if (resp.value("code").toInt() != Protocol::Ok) {
        QString msg = resp.value("msg").toString();

        if (!msg.isEmpty()) {
            QMessageBox::warning(
                this,
                "获取电桩列表失败",
                msg
            );
        }

        return;
    }

    /*
     * 组长接口格式：
     *
     * data: {
     *     "list": [
     *         {
     *             "id": ...,
     *             "code": ...,
     *             "station": ...,
     *             "type": ...,
     *             "power_kw": ...,
     *             "status": ...
     *         }
     *     ]
     * }
     */
    QJsonObject data = resp.value("data").toObject();
    QJsonArray piles = data.value("list").toArray();

    // 清空旧数据
    m_table->setRowCount(0);

    for (int i = 0; i < piles.size(); ++i) {

        QJsonObject p = piles.at(i).toObject();

        qint64 pileId = p.value("id").toInteger();

        m_table->insertRow(i);

        // 桩编号
        m_table->setItem(
            i,
            0,
            new QTableWidgetItem(
                p.value("code").toString()
            )
        );

        // 所属电站
        // 注意：接口字段是 station，不是 station_name
        m_table->setItem(
            i,
            1,
            new QTableWidgetItem(
                p.value("station").toString()
            )
        );

        // 类型
        m_table->setItem(
            i,
            2,
            new QTableWidgetItem(
                p.value("type").toString()
            )
        );

        // 功率
        double powerKw = p.value("power_kw").toDouble();

        m_table->setItem(
            i,
            3,
            new QTableWidgetItem(
                QString::number(powerKw, 'f', 1)
            )
        );

        // 状态
        QString status = p.value("status").toString();

        m_table->setItem(
            i,
            4,
            new QTableWidgetItem(status)
        );

        // 重启按钮
            new QPushButton("重启", m_table);

        restartBtn->setProperty(
            "pileId",
            QVariant::fromValue<qlonglong>(pileId)
        );

        connect(
            restartBtn,
            &QPushButton::clicked,
            this,
            &PileManagerWidget::onRestartClicked
        );

        m_table->setCellWidget(
            i,
            5,
            restartBtn
        );
    }
}

void PileManagerWidget::onRestartClicked()
{
    if (!m_net) {
        return;
    }

    auto *btn =
        qobject_cast<QPushButton *>(sender());

    if (!btn) {
        return;
    }

    qint64 pileId =
        btn->property("pileId").toLongLong();

    /*
     *
     * admin_pile_restart
     *
     * 请求：
     * {
     *     "pile_id": xxx
     * }
     */
    QJsonObject data;
    data["pile_id"] = pileId;

    QJsonObject req =
        Protocol::makeRequest(
            Protocol::MsgType::AdminPileRestart,
            data
        );

    QJsonObject resp = m_net->request(req);

    if (resp.value("code").toInt() == Protocol::Ok) {

        QMessageBox::information(
            this,
            "提示",
            "电桩重启成功，状态已恢复为 idle"
        );

        // 重启成功后重新加载列表
        loadPiles();

    } else {

        QString msg = resp.value("msg").toString();

        if (msg.isEmpty()) {
            msg = "电桩重启失败";
        }

        QMessageBox::warning(
            this,
            "错误",
            msg
        );
    }
}
