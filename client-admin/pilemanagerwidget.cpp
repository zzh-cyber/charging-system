#include "pilemanagerwidget.h"
#include "netclient.h"
#include "protocol.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFont>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace {
constexpr int StatusColumn = 4;
constexpr int ActionColumn = 5;
} 

PileManagerWidget::PileManagerWidget(NetClient *netClient, QWidget *parent)
    : QWidget(parent), m_net(netClient), m_table(nullptr),
      m_refreshBtn(nullptr), m_statusFilter(nullptr)
{
    initUI();
    loadPiles();
}

void PileManagerWidget::initUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    auto *topLayout = new QHBoxLayout;
    m_refreshBtn = new QPushButton(QStringLiteral("刷新电桩列表"), this);
    m_statusFilter = new QComboBox(this);
    m_statusFilter->addItems({QStringLiteral("全部"), QStringLiteral("闲置"),
                              QStringLiteral("在用"), QStringLiteral("故障")});
    m_statusFilter->setToolTip(QStringLiteral("按电桩状态筛选"));
    topLayout->addWidget(m_refreshBtn);
    topLayout->addWidget(m_statusFilter);
    topLayout->addStretch();
    mainLayout->addLayout(topLayout);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("桩编号"), QStringLiteral("所属电站"),
        QStringLiteral("类型"), QStringLiteral("功率(kW)"),
        QStringLiteral("状态"), QStringLiteral("操作")
    });
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    mainLayout->addWidget(m_table);

    connect(m_refreshBtn, &QPushButton::clicked,
            this, &PileManagerWidget::loadPiles);
    connect(m_statusFilter, &QComboBox::currentTextChanged,
            this, &PileManagerWidget::applyStatusFilter);
}

QString PileManagerWidget::statusText(const QString &rawStatus)
{
    const QString status = rawStatus.trimmed().toLower();
    if (status == QStringLiteral("idle"))
        return QStringLiteral("闲置");
    if (status == QStringLiteral("busy") || status == QStringLiteral("charging"))
        return QStringLiteral("在用");
    if (status == QStringLiteral("fault"))
        return QStringLiteral("故障");
    return QStringLiteral("离线");
}

QColor PileManagerWidget::statusColor(const QString &rawStatus)
{
    const QString status = rawStatus.trimmed().toLower();
    if (status == QStringLiteral("idle"))
        return QColor(QStringLiteral("#2ecc71"));
    if (status == QStringLiteral("busy") || status == QStringLiteral("charging"))
        return QColor(QStringLiteral("#3498db"));
    if (status == QStringLiteral("fault"))
        return QColor(QStringLiteral("#e74c3c"));
    return QColor(QStringLiteral("#95a5a6"));
}

void PileManagerWidget::setCenteredItem(QTableWidgetItem *item)
{
    item->setTextAlignment(Qt::AlignCenter);
}

void PileManagerWidget::loadPiles()
{
    if (!m_net)
        return;

    const QJsonObject response = m_net->request(Protocol::makeRequest(
        Protocol::MsgType::AdminPileList, QJsonObject()));
    if (response.value(QStringLiteral("code")).toInt() != Protocol::Ok) {
        QString message = response.value(QStringLiteral("msg")).toString();
        if (message.isEmpty())
            message = QStringLiteral("无法获取电桩列表");
        QMessageBox::warning(this, QStringLiteral("获取电桩列表失败"), message);
        return;
    }

    const QJsonArray piles = response.value(QStringLiteral("data")).toObject()
                                 .value(QStringLiteral("list")).toArray();
    m_table->setRowCount(0);
    for (const QJsonValue &value : piles) {
        const QJsonObject pile = value.toObject();
        const int row = m_table->rowCount();
        const qint64 pileId = pile.value(QStringLiteral("id")).toVariant().toLongLong();
        const QString pileCode = pile.value(QStringLiteral("code")).toString();
        const QString rawStatus = pile.value(QStringLiteral("status")).toString()
                                      .trimmed().toLower();
        m_table->insertRow(row);

        const QStringList values = {
            pileCode,
            pile.value(QStringLiteral("station")).toString(),
            pile.value(QStringLiteral("type")).toString(),
            QString::number(pile.value(QStringLiteral("power_kw")).toDouble(), 'f', 1)
        };
        for (int column = 0; column < values.size(); ++column) {
            auto *item = new QTableWidgetItem(values.at(column));
            setCenteredItem(item);
            m_table->setItem(row, column, item);
        }

        auto *statusItem = new QTableWidgetItem(statusText(rawStatus));
        statusItem->setData(Qt::UserRole, rawStatus);
        statusItem->setForeground(statusColor(rawStatus));
        QFont font = statusItem->font();
        font.setBold(true);
        statusItem->setFont(font);
        setCenteredItem(statusItem);
        m_table->setItem(row, StatusColumn, statusItem);

        auto *button = new QPushButton(QStringLiteral("重启"), m_table);
        button->setProperty("pileId", QVariant::fromValue<qlonglong>(pileId));
        button->setProperty("pileCode", pileCode);
        button->setProperty("rawStatus", rawStatus);
        connect(button, &QPushButton::clicked,
                this, &PileManagerWidget::onRestartClicked);
        m_table->setCellWidget(row, ActionColumn, button);
    }
    applyStatusFilter();
}

void PileManagerWidget::applyStatusFilter()
{
    const QString selected = m_statusFilter->currentText();
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const QTableWidgetItem *item = m_table->item(row, StatusColumn);
        m_table->setRowHidden(row, selected != QStringLiteral("全部")
                                      && (!item || item->text() != selected));
    }
}

void PileManagerWidget::onRestartClicked()
{
    if (!m_net)
        return;
    auto *button = qobject_cast<QPushButton *>(sender());
    if (!button)
        return;

    const qint64 pileId = button->property("pileId").toLongLong();
    const QString pileCode = button->property("pileCode").toString();
    const QString rawStatus = button->property("rawStatus").toString()
                                  .trimmed().toLower();
    if (rawStatus == QStringLiteral("charging") || rawStatus == QStringLiteral("busy")) {
        QMessageBox::warning(this, QStringLiteral("禁止重启"),
                             QStringLiteral("当前电桩正在充电中，禁止远程重启！"));
        return;
    }

    const QString prompt =
        QStringLiteral("电桩编号：%1\n当前状态：%2\n\n重启将短暂中断服务，是否继续？")
            .arg(pileCode, statusText(rawStatus));
    if (QMessageBox::question(this, QStringLiteral("确认远程重启"), prompt,
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) != QMessageBox::Yes)
        return;

    button->setEnabled(false);
    button->setText(QStringLiteral("重启中..."));
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    QJsonObject data;
    data[QStringLiteral("pile_id")] = pileId;
    const QJsonObject response = m_net->request(Protocol::makeRequest(
        Protocol::MsgType::AdminPileRestart, data));
    button->setText(QStringLiteral("重启"));
    button->setEnabled(true);

    if (response.value(QStringLiteral("code")).toInt() != Protocol::Ok) {
        QString message = response.value(QStringLiteral("msg")).toString();
        if (message.isEmpty())
            message = QStringLiteral("电桩重启失败");
        QMessageBox::warning(this, QStringLiteral("重启失败"), message);
        return;
    }

    button->setProperty("rawStatus", QStringLiteral("idle"));
    for (int row = 0; row < m_table->rowCount(); ++row) {
        if (m_table->cellWidget(row, ActionColumn) != button)
            continue;
        QTableWidgetItem *item = m_table->item(row, StatusColumn);
        if (item) {
            item->setText(QStringLiteral("闲置"));
            item->setData(Qt::UserRole, QStringLiteral("idle"));
            item->setForeground(statusColor(QStringLiteral("idle")));
        }
        break;
    }
    applyStatusFilter();
    QMessageBox::information(this, QStringLiteral("重启成功"),
                             QStringLiteral("电桩重启成功，当前状态已恢复为“闲置”。"));
}
