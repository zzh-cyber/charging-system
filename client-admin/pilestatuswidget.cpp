#include "pilestatuswidget.h"
#include "netclient.h"
#include "protocol.h"

#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMessageBox>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

namespace {
QWidget *createKpiCard(const QString &title, QLabel **valueLabel, QWidget *parent)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("monitorKpiCard"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 11, 16, 11);
    layout->setSpacing(2);
    auto *titleLabel = new QLabel(title, card);
    titleLabel->setObjectName(QStringLiteral("monitorKpiTitle"));
    *valueLabel = new QLabel(QStringLiteral("0"), card);
    (*valueLabel)->setObjectName(QStringLiteral("monitorKpiValue"));
    layout->addWidget(titleLabel);
    layout->addWidget(*valueLabel);
    return card;
}
}

PileStatusWidget::PileStatusWidget(NetClient *netClient, QWidget *parent)
    : QWidget(parent), m_net(netClient)
{
    setObjectName(QStringLiteral("pileStatusPage"));
    initUI();
    loadStatus();
    m_timer = new QTimer(this);
    m_timer->setInterval(10000);
    connect(m_timer, &QTimer::timeout, this, &PileStatusWidget::loadStatus);
    m_timer->start();
}

void PileStatusWidget::applyRefreshSettings(bool autoRefresh, int intervalMs, bool pauseWhenHidden, bool pageVisible)
{
    const bool becameVisible = pageVisible && !m_pageVisible;
    m_autoRefresh = autoRefresh;
    m_pauseWhenHidden = pauseWhenHidden;
    m_pageVisible = pageVisible;
    m_timer->setInterval(qMax(1000, intervalMs));
    if (becameVisible) loadStatus();
    if (m_autoRefresh && (!m_pauseWhenHidden || m_pageVisible)) m_timer->start();
    else m_timer->stop();
}

void PileStatusWidget::initUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(22, 20, 22, 20);
    mainLayout->setSpacing(14);
    auto *heading = new QLabel(QStringLiteral("电桩实时状态"), this);
    heading->setObjectName(QStringLiteral("monitorHeading"));
    mainLayout->addWidget(heading);

    auto *kpiLayout = new QHBoxLayout;
    kpiLayout->setSpacing(12);
    kpiLayout->addWidget(createKpiCard(QStringLiteral("总设备"), &m_totalValue, this));
    kpiLayout->addWidget(createKpiCard(QStringLiteral("闲置"), &m_idleValue, this));
    kpiLayout->addWidget(createKpiCard(QStringLiteral("在用"), &m_busyValue, this));
    kpiLayout->addWidget(createKpiCard(QStringLiteral("故障"), &m_faultValue, this));
    mainLayout->addLayout(kpiLayout);

    auto *toolbar = new QFrame(this);
    toolbar->setObjectName(QStringLiteral("monitorToolbar"));
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(12, 10, 12, 10);
    toolbarLayout->setSpacing(10);
    m_searchEdit = new QLineEdit(toolbar);
    m_searchEdit->setPlaceholderText(QStringLiteral("搜索桩编号"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setMinimumWidth(170);
    m_stationCombo = new QComboBox(toolbar);
    m_stationCombo->addItem(QStringLiteral("全部电站"), QString());
    m_stationCombo->setMinimumWidth(180);
    m_statusCombo = new QComboBox(toolbar);
    m_statusCombo->addItem(QStringLiteral("全部状态"), QString());
    m_statusCombo->addItem(QStringLiteral("闲置"), QStringLiteral("idle"));
    m_statusCombo->addItem(QStringLiteral("在用"), QStringLiteral("busy"));
    m_statusCombo->addItem(QStringLiteral("故障"), QStringLiteral("fault"));
    m_refreshBtn = new QPushButton(QStringLiteral("立即刷新"), toolbar);
    m_lastUpdateLabel = new QLabel(QStringLiteral("最后更新：-"), toolbar);
    m_lastUpdateLabel->setObjectName(QStringLiteral("monitorLastUpdate"));
    toolbarLayout->addWidget(m_searchEdit);
    toolbarLayout->addWidget(m_stationCombo);
    toolbarLayout->addWidget(m_statusCombo);
    toolbarLayout->addWidget(m_refreshBtn);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(m_lastUpdateLabel);
    mainLayout->addWidget(toolbar);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setObjectName(QStringLiteral("monitorScrollArea"));
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_cardsContainer = new QWidget(m_scrollArea);
    m_cardsContainer->setObjectName(QStringLiteral("monitorCardsContainer"));
    m_cardsLayout = new QVBoxLayout(m_cardsContainer);
    m_cardsLayout->setContentsMargins(0, 0, 8, 0);
    m_cardsLayout->setSpacing(16);
    m_cardsLayout->addStretch();
    m_scrollArea->setWidget(m_cardsContainer);
    mainLayout->addWidget(m_scrollArea, 1);

    connect(m_refreshBtn, &QPushButton::clicked, this, &PileStatusWidget::loadStatus);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &PileStatusWidget::applyFilters);
    connect(m_stationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PileStatusWidget::applyFilters);
    connect(m_statusCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PileStatusWidget::applyFilters);
}

void PileStatusWidget::loadStatus()
{
    if (!m_net || m_requestInFlight)
        return;
    m_requestInFlight = true;
    const QJsonObject response = m_net->request(Protocol::makeRequest(Protocol::MsgType::AdminPileList));
    m_requestInFlight = false;
    const int code = response.value(QStringLiteral("code")).toInt(-1);
    const QString message = response.value(QStringLiteral("msg")).toString();
    if (code != Protocol::Ok) {
        if (sender() == m_timer) {
            m_timer->stop();
            m_lastUpdateLabel->setText(QStringLiteral("自动刷新失败：%1").arg(message));
        } else {
            QMessageBox::warning(this, QStringLiteral("获取电桩状态失败"),
                                 QStringLiteral("code=%1\n%2").arg(code).arg(message));
        }
        return;
    }

    const QJsonObject data = response.value(QStringLiteral("data")).toObject();
    const QJsonArray list = data.value(QStringLiteral("list")).toArray();
    QVector<PileStatusItem> items;
    items.reserve(list.size());
    for (const QJsonValue &value : list) {
        const QJsonObject pile = value.toObject();
        PileStatusItem item;
        item.id = pile.value(QStringLiteral("id")).toInteger();
        item.code = pile.value(QStringLiteral("code")).toString();
        item.station = pile.value(QStringLiteral("station")).toString();
        item.type = pile.value(QStringLiteral("type")).toString();
        item.powerKw = pile.value(QStringLiteral("power_kw")).toDouble();
        item.status = pile.value(QStringLiteral("status")).toString();
        items.append(item);
    }
    m_items = items;
    updateStationFilter();
    rebuildCards();

    const QJsonObject stats = data.value(QStringLiteral("stats")).toObject();
    const QDateTime statTime = parseDateTime(stats.value(QStringLiteral("stat_time")).toString());
    const bool validStats = stats.value(QStringLiteral("total")).isDouble()
        && stats.value(QStringLiteral("idle")).isObject()
        && stats.value(QStringLiteral("busy")).isObject()
        && stats.value(QStringLiteral("fault")).isObject() && statTime.isValid();
    if (validStats) {
        updateSummary(stats);
        m_lastUpdateLabel->setText(QStringLiteral("最后更新：%1").arg(statTime.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));
    } else {
        m_lastUpdateLabel->setText(QStringLiteral("设备已更新，统计暂不可用"));
    }
}

void PileStatusWidget::applyFilters() { rebuildCards(); }

void PileStatusWidget::updateStationFilter()
{
    const QString selectedStation = m_stationCombo->currentData().toString();
    QStringList stations;
    for (const PileStatusItem &item : m_items)
        if (!item.station.isEmpty() && !stations.contains(item.station))
            stations.append(item.station);
    stations.sort(Qt::CaseInsensitive);
    const QSignalBlocker blocker(m_stationCombo);
    m_stationCombo->clear();
    m_stationCombo->addItem(QStringLiteral("全部电站"), QString());
    for (const QString &station : stations)
        m_stationCombo->addItem(station, station);
    m_stationCombo->setCurrentIndex(std::max(0, m_stationCombo->findData(selectedStation)));
}

void PileStatusWidget::rebuildCards(bool preserveScrollPosition)
{
    const int oldScroll = preserveScrollPosition ? m_scrollArea->verticalScrollBar()->value() : 0;
    while (QLayoutItem *child = m_cardsLayout->takeAt(0)) {
        delete child->widget();
        delete child;
    }
    const QString keyword = m_searchEdit->text().trimmed();
    const QString stationFilter = m_stationCombo->currentData().toString();
    const QString statusFilter = m_statusCombo->currentData().toString();
    QMap<QString, QVector<PileStatusItem>> groups;
    for (const PileStatusItem &item : m_items) {
        if (!keyword.isEmpty() && !item.code.contains(keyword, Qt::CaseInsensitive)) continue;
        if (!stationFilter.isEmpty() && item.station != stationFilter) continue;
        if (!statusFilter.isEmpty() && item.status != statusFilter) continue;
        groups[item.station.isEmpty() ? QStringLiteral("未分配电站") : item.station].append(item);
    }
    m_currentColumnCount = cardColumnCount();
    if (groups.isEmpty()) {
        auto *emptyLabel = new QLabel(QStringLiteral("没有符合筛选条件的设备"), m_cardsContainer);
        emptyLabel->setObjectName(QStringLiteral("monitorEmptyState"));
        emptyLabel->setAlignment(Qt::AlignCenter);
        m_cardsLayout->addWidget(emptyLabel, 1);
    } else {
        for (auto it = groups.cbegin(); it != groups.cend(); ++it) {
            auto *group = new QFrame(m_cardsContainer);
            group->setObjectName(QStringLiteral("monitorStationGroup"));
            auto *groupLayout = new QVBoxLayout(group);
            groupLayout->setContentsMargins(16, 14, 16, 16);
            groupLayout->setSpacing(12);
            auto *title = new QLabel(QStringLiteral("%1    %2 台").arg(it.key()).arg(it.value().size()), group);
            title->setObjectName(QStringLiteral("monitorStationTitle"));
            groupLayout->addWidget(title);
            auto *grid = new QGridLayout;
            grid->setContentsMargins(0, 0, 0, 0);
            grid->setSpacing(12);
            int index = 0;
            for (const PileStatusItem &item : it.value()) {
                grid->addWidget(createPileCard(item), index / m_currentColumnCount, index % m_currentColumnCount);
                ++index;
            }
            for (int column = 0; column < m_currentColumnCount; ++column) grid->setColumnStretch(column, 1);
            groupLayout->addLayout(grid);
            m_cardsLayout->addWidget(group);
        }
        m_cardsLayout->addStretch();
    }
    QTimer::singleShot(0, this, [this, oldScroll] { m_scrollArea->verticalScrollBar()->setValue(oldScroll); });
}

QWidget *PileStatusWidget::createPileCard(const PileStatusItem &item)
{
    auto *card = new QFrame(m_cardsContainer);
    card->setObjectName(QStringLiteral("monitorPileCard"));
    card->setMinimumWidth(170);
    card->setMinimumHeight(126);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(6);
    auto *code = new QLabel(item.code, card);
    code->setObjectName(QStringLiteral("monitorPileCode"));
    auto *status = new QLabel(QStringLiteral("●  %1").arg(statusText(item.status)), card);
    status->setObjectName(QStringLiteral("monitorPileStatus"));
    status->setProperty("pileStatus", item.status);
    auto *type = new QLabel(QStringLiteral("类型：%1").arg(item.type), card);
    type->setObjectName(QStringLiteral("monitorPileMeta"));
    const bool wholePower = qFuzzyCompare(item.powerKw + 1.0, qRound64(item.powerKw) + 1.0);
    auto *power = new QLabel(QStringLiteral("额定功率：%1 kW").arg(QString::number(item.powerKw, 'f', wholePower ? 0 : 1)), card);
    power->setObjectName(QStringLiteral("monitorPileMeta"));
    layout->addWidget(code);
    layout->addWidget(status);
    layout->addStretch();
    layout->addWidget(type);
    layout->addWidget(power);
    return card;
}

void PileStatusWidget::updateSummary(const QJsonObject &stats)
{
    m_totalValue->setText(QString::number(stats.value(QStringLiteral("total")).toInt()));
    const auto statusSummary = [](const QJsonObject &status) {
        return QStringLiteral("%1（%2%）")
            .arg(status.value(QStringLiteral("count")).toInt())
            .arg(QString::number(status.value(QStringLiteral("rate")).toDouble(), 'f', 1));
    };
    m_idleValue->setText(statusSummary(stats.value(QStringLiteral("idle")).toObject()));
    m_busyValue->setText(statusSummary(stats.value(QStringLiteral("busy")).toObject()));
    m_faultValue->setText(statusSummary(stats.value(QStringLiteral("fault")).toObject()));
}

QString PileStatusWidget::statusText(const QString &status) const
{
    if (status == QStringLiteral("idle")) return QStringLiteral("闲置");
    if (status == QStringLiteral("busy")) return QStringLiteral("在用");
    if (status == QStringLiteral("fault")) return QStringLiteral("故障");
    return status;
}

int PileStatusWidget::cardColumnCount() const
{
    const int width = m_scrollArea && m_scrollArea->viewport() ? m_scrollArea->viewport()->width() - 40 : this->width() - 40;
    return std::clamp(width / 215, 1, 5);
}

void PileStatusWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (!m_items.isEmpty() && cardColumnCount() != m_currentColumnCount) rebuildCards();
}

QDateTime PileStatusWidget::parseDateTime(const QString &text) const
{
    if (text.trimmed().isEmpty()) return QDateTime();
    QDateTime dateTime = QDateTime::fromString(text, Qt::ISODate);
    if (!dateTime.isValid()) dateTime = QDateTime::fromString(text, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    return dateTime;
}
