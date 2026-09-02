#include "usermanagerwidget.h"
#include "netclient.h"
#include "protocol.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>

UserManagerWidget::UserManagerWidget(NetClient *netClient, QWidget *parent)
    : QWidget(parent), m_net(netClient)
{
    initUI();
    loadUsers();
}

void UserManagerWidget::initUI()
{
    auto *mainLayout = new QVBoxLayout(this);

    // 顶部工具栏
    auto *topLayout = new QHBoxLayout();
    m_refreshBtn = new QPushButton("刷新用户列表", this);
    topLayout->addWidget(m_refreshBtn);
    topLayout->addStretch();
    mainLayout->addLayout(topLayout);

    // 表格：手机号 / 昵称 / 余额 / 状态
    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({"手机号", "昵称", "余额 (元)", "状态"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    mainLayout->addWidget(m_table);

    connect(m_refreshBtn, &QPushButton::clicked, this, &UserManagerWidget::loadUsers);
}

void UserManagerWidget::loadUsers()
{
    if (!m_net) return;

    // 发起网络请求获取用户列表
    QJsonObject req = Protocol::makeRequest(Protocol::MsgType::AdminUserList, QJsonObject());
    QJsonObject resp = m_net->request(req);

    if (resp.value("code").toInt() != Protocol::Ok) {
        return;
    }

    QJsonArray users = resp.value("data").toArray();
    m_table->setRowCount(0);

    for (int i = 0; i < users.size(); ++i) {
        QJsonObject u = users[i].toObject();
        m_table->insertRow(i);

        m_table->setItem(i, 0, new QTableWidgetItem(u.value("phone").toString()));
        m_table->setItem(i, 1, new QTableWidgetItem(u.value("nickname").toString()));
        m_table->setItem(i, 2, new QTableWidgetItem(QString::number(u.value("balance").toDouble(), 'f', 2)));

        QString statusStr = u.value("status").toString();
        m_table->setItem(i, 3, new QTableWidgetItem(statusStr == "normal" ? "正常" : "冻结"));
    }
}