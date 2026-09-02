#include "stationmanagerwidget.h"
#include "netclient.h"
#include "protocol.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QPushButton>
#include <QTableWidgetItem>
#include <QMessageBox>

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

    m_refreshBtn =
        new QPushButton("刷新电站列表", this);

    topLayout->addWidget(m_refreshBtn);
    topLayout->addStretch();

    mainLayout->addLayout(topLayout);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);

    m_table->setHorizontalHeaderLabels({
        "电站名称",
        "地址",
        "基础电费(元/度)",
        "总桩数",
        "在线率"
    });

    m_table->horizontalHeader()->setSectionResizeMode(
        QHeaderView::Stretch
    );

    m_table->setEditTriggers(
        QAbstractItemView::NoEditTriggers
    );

    m_table->setSelectionBehavior(
        QAbstractItemView::SelectRows
    );

    mainLayout->addWidget(m_table);

    connect(
        m_refreshBtn,
        &QPushButton::clicked,
        this,
        &StationManagerWidget::loadStations
    );
}

void StationManagerWidget::loadStations()
{
    if (!m_net) {
        return;
    }

    // 请求：admin_station_list
    QJsonObject req =
        Protocol::makeRequest(
            Protocol::MsgType::AdminStationList,
            QJsonObject()
        );

    QJsonObject resp = m_net->request(req);

    // 请求失败
    if (resp.value("code").toInt() != Protocol::Ok) {

        QString msg = resp.value("msg").toString();

        if (!msg.isEmpty()) {
            QMessageBox::warning(
                this,
                "获取电站列表失败",
                msg
            );
        }

        return;
    }

    /*
     * 
     * data: {
     *     "list": [
     *         {
     *             "id": ...,
     *             "name": ...,
     *             "address": ...,
     *             "longitude": ...,
     *             "latitude": ...,
     *             "total": ...,
     *             "online_rate": ...
     *         }
     *     ]
     * }
     */
    QJsonObject data =
        resp.value("data").toObject();

    QJsonArray stations =
        data.value("list").toArray();

    // 清空旧数据
    m_table->setRowCount(0);

    for (int i = 0; i < stations.size(); ++i) {

        QJsonObject s =
            stations.at(i).toObject();

        m_table->insertRow(i);

        // 电站名称
        m_table->setItem(
            i,
            0,
            new QTableWidgetItem(
                s.value("name").toString()
            )
        );

        // 地址
        m_table->setItem(
            i,
            1,
            new QTableWidgetItem(
                s.value("address").toString()
            )
        );

        /*
         * 当前接口契约中没有 price 字段，
         * 所以基础电费暂时显示 "-"
         */
        m_table->setItem(
            i,
            2,
            new QTableWidgetItem("-")
        );

        // 总桩数
        // 注意：接口字段叫 total，不是 pile_count
        int total =
            s.value("total").toInt();

        m_table->setItem(
            i,
            3,
            new QTableWidgetItem(
                QString::number(total)
            )
        );

        // 在线率
        double rate =
            s.value("online_rate").toDouble();

        /*
         * 按接口设计，
         * online_rate 暂按 0 ~ 1 之间的小数处理。
         *
         * 例如：
         * 0.8 -> 80.0%
         */
        rate *= 100.0;

        QString rateText =
            QString("%1%")
                .arg(
                    QString::number(
                        rate,
                        'f',
                        1
                    )
                );

        m_table->setItem(
            i,
            4,
            new QTableWidgetItem(rateText)
        );
    }
}
