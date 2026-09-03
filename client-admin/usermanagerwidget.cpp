#include "usermanagerwidget.h"
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

    // 用户表格
    m_table = new QTableWidget(this);

    m_table->setColumnCount(4);

    m_table->setHorizontalHeaderLabels({
        "手机号",
        "昵称",
        "余额 (元)",
        "状态"
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

    // 刷新按钮
    connect(
        m_refreshBtn,
        &QPushButton::clicked,
        this,
        &UserManagerWidget::loadUsers
    );
}

void UserManagerWidget::loadUsers()
{
    // 检查网络客户端
    if (!m_net) {
        QMessageBox::warning(
            this,
            "错误",
            "网络客户端为空"
        );
        return;
    }

    // 构造 admin_user_list 请求
    QJsonObject req =
        Protocol::makeRequest(
            Protocol::MsgType::AdminUserList,
            QJsonObject()
        );

    // 发送请求
    QJsonObject resp = m_net->request(req);

    // 获取返回状态
    int code = resp.value("code").toInt(-1);
    QString msg = resp.value("msg").toString();

    // 接口调用失败
    if (code != Protocol::Ok) {
        QMessageBox::warning(
            this,
            "获取用户列表失败",
            QString("code=%1\n%2")
                .arg(code)
                .arg(msg)
        );
        return;
    }

    QJsonObject data =
        resp.value("data").toObject();

    QJsonArray users =
        data.value("list").toArray();

    // 如果接口成功但没有用户
    if (users.isEmpty()) {
        m_table->setRowCount(0);

        QMessageBox::information(
            this,
            "提示",
            "接口请求成功，但用户列表为空"
        );

        return;
    }

    // 清空原来的表格数据
    m_table->setRowCount(0);

    // 填充用户数据
    for (int i = 0; i < users.size(); ++i) {

        QJsonObject u =
            users.at(i).toObject();

        m_table->insertRow(i);

        // 手机号
        m_table->setItem(
            i,
            0,
            new QTableWidgetItem(
                u.value("phone").toString()
            )
        );

        // 昵称
        m_table->setItem(
            i,
            1,
            new QTableWidgetItem(
                u.value("nickname").toString()
            )
        );

        // 余额
        double balance =
            u.value("balance").toDouble();

        m_table->setItem(
            i,
            2,
            new QTableWidgetItem(
                QString::number(
                    balance,
                    'f',
                    2
                )
            )
        );

        // 状态
        QString status =
            u.value("status").toString();

        QString statusText;

        if (status == "normal") {
            statusText = "正常";
        } else if (status == "frozen") {
            statusText = "冻结";
        } else {
            // 如果服务端返回其他状态，直接显示原始值
            statusText = status;
        }

        m_table->setItem(
            i,
            3,
            new QTableWidgetItem(
                statusText
            )
        );
    }
}
