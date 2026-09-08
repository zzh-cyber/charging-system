#include "pilemanagerwidget.h"
#include "netclient.h"
#include "protocol.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QEventLoop>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
constexpr int CodeColumn = 0;
constexpr int StationColumn = 1;
constexpr int TypeColumn = 2;
constexpr int PowerColumn = 3;
constexpr int StatusColumn = 4;
constexpr int ActionColumn = 5;

QWidget *createAssetSummary(const QString &title, QLabel **value, QWidget *parent)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("pileAssetSummary"));
    auto *layout = new QHBoxLayout(card);
    layout->setContentsMargins(14, 9, 14, 9);
    layout->setSpacing(10);
    auto *label = new QLabel(title, card);
    label->setObjectName(QStringLiteral("pileAssetSummaryTitle"));
    *value = new QLabel(QStringLiteral("0"), card);
    (*value)->setObjectName(QStringLiteral("pileAssetSummaryValue"));
    layout->addWidget(label);
    layout->addWidget(*value);
    return card;
}
}

PileManagerWidget::PileManagerWidget(NetClient *netClient, QWidget *parent)
    : QWidget(parent), m_net(netClient)
{
    setObjectName(QStringLiteral("pileManagerPage"));
    initUI();
    loadPiles();
}

void PileManagerWidget::initUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(22, 20, 22, 20);
    mainLayout->setSpacing(14);

    auto *headingRow = new QHBoxLayout;
    auto *heading = new QLabel(QStringLiteral("设备台账"), this);
    heading->setObjectName(QStringLiteral("pileAssetHeading"));
    headingRow->addWidget(heading);
    headingRow->addStretch();
    headingRow->addWidget(createAssetSummary(QStringLiteral("设备总数"), &m_totalValue, this));
    headingRow->addWidget(createAssetSummary(QStringLiteral("正常设备"), &m_normalValue, this));
    headingRow->addWidget(createAssetSummary(QStringLiteral("异常设备"), &m_abnormalValue, this));
    mainLayout->addLayout(headingRow);

    auto *toolbar = new QFrame(this);
    toolbar->setObjectName(QStringLiteral("pileAssetToolbar"));
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(12, 10, 12, 10);
    toolbarLayout->setSpacing(10);
    m_searchEdit = new QLineEdit(toolbar);
    m_searchEdit->setPlaceholderText(QStringLiteral("搜索桩编号"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setMinimumWidth(170);
    m_stationFilter = new QComboBox(toolbar);
    m_stationFilter->addItem(QStringLiteral("全部电站"), QString());
    m_stationFilter->setMinimumWidth(180);
    m_typeFilter = new QComboBox(toolbar);
    m_typeFilter->addItem(QStringLiteral("全部类型"), QString());
    m_typeFilter->addItem(QStringLiteral("快充"), QStringLiteral("fast"));
    m_typeFilter->addItem(QStringLiteral("慢充"), QStringLiteral("slow"));
    m_statusFilter = new QComboBox(toolbar);
    m_statusFilter->addItem(QStringLiteral("全部状态"), QString());
    m_statusFilter->addItem(QStringLiteral("闲置"), QStringLiteral("idle"));
    m_statusFilter->addItem(QStringLiteral("在用"), QStringLiteral("busy"));
    m_statusFilter->addItem(QStringLiteral("故障"), QStringLiteral("fault"));
    auto *resetButton = new QToolButton(toolbar);
    resetButton->setObjectName(QStringLiteral("pileAssetTextAction"));
    resetButton->setText(QStringLiteral("重置"));
    m_refreshBtn = new QPushButton(QStringLiteral("刷新"), toolbar);
    toolbarLayout->addWidget(m_searchEdit);
    toolbarLayout->addWidget(m_stationFilter);
    toolbarLayout->addWidget(m_typeFilter);
    toolbarLayout->addWidget(m_statusFilter);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(resetButton);
    toolbarLayout->addWidget(m_refreshBtn);
    mainLayout->addWidget(toolbar);

    m_table = new QTableWidget(this);
    m_table->setObjectName(QStringLiteral("pileAssetTable"));
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels({QStringLiteral("桩编号"), QStringLiteral("所属电站"),
                                        QStringLiteral("类型"), QStringLiteral("额定功率"),
                                        QStringLiteral("当前状态"), QStringLiteral("操作")});
    m_table->horizontalHeader()->setSectionResizeMode(CodeColumn, QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(StationColumn, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(TypeColumn, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(PowerColumn, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(StatusColumn, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(ActionColumn, QHeaderView::Fixed);
    m_table->setColumnWidth(CodeColumn, 145);
    m_table->setColumnWidth(TypeColumn, 90);
    m_table->setColumnWidth(PowerColumn, 115);
    m_table->setColumnWidth(StatusColumn, 100);
    m_table->setColumnWidth(ActionColumn, 80);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(48);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    mainLayout->addWidget(m_table, 1);

    m_emptyLabel = new QLabel(m_table->viewport());
    m_emptyLabel->setObjectName(QStringLiteral("pileAssetEmpty"));
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_emptyLabel->hide();
    m_table->viewport()->installEventFilter(this);
    m_footerLabel = new QLabel(QStringLiteral("共 0 台设备"), this);
    m_footerLabel->setObjectName(QStringLiteral("pileAssetFooter"));
    mainLayout->addWidget(m_footerLabel);

    connect(m_refreshBtn, &QPushButton::clicked, this, &PileManagerWidget::loadPiles);
    connect(resetButton, &QToolButton::clicked, this, &PileManagerWidget::resetFilters);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &PileManagerWidget::applyFilters);
    connect(m_stationFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PileManagerWidget::applyFilters);
    connect(m_typeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PileManagerWidget::applyFilters);
    connect(m_statusFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PileManagerWidget::applyFilters);
}

void PileManagerWidget::loadStationOptions()
{
    const QString selected = m_stationFilter->currentData().toString();
    const QJsonObject response = m_net->request(Protocol::makeRequest(Protocol::MsgType::AdminStationList));
    if (response.value(QStringLiteral("code")).toInt(-1) != Protocol::Ok)
        return;
    QStringList stations;
    const QJsonArray list = response.value(QStringLiteral("data")).toObject().value(QStringLiteral("list")).toArray();
    for (const QJsonValue &value : list) {
        const QString name = value.toObject().value(QStringLiteral("name")).toString();
        if (!name.isEmpty() && !stations.contains(name)) stations.append(name);
    }
    stations.sort(Qt::CaseInsensitive);
    const QSignalBlocker blocker(m_stationFilter);
    m_stationFilter->clear();
    m_stationFilter->addItem(QStringLiteral("全部电站"), QString());
    for (const QString &station : stations) m_stationFilter->addItem(station, station);
    const int index = m_stationFilter->findData(selected);
    m_stationFilter->setCurrentIndex(index >= 0 ? index : 0);
}

void PileManagerWidget::loadPiles()
{
    if (!m_net) return;
    m_refreshBtn->setEnabled(false);
    loadStationOptions();
    QJsonArray allPiles;
    int page = 1;
    int total = 0;
    do {
        const QJsonObject data{{QStringLiteral("page"), page}, {QStringLiteral("page_size"), 50}};
        const QJsonObject response = m_net->request(Protocol::makeRequest(Protocol::MsgType::AdminPileList, data));
        if (response.value(QStringLiteral("code")).toInt(-1) != Protocol::Ok) {
            m_refreshBtn->setEnabled(true);
            QString message = response.value(QStringLiteral("msg")).toString();
            if (message.isEmpty()) message = QStringLiteral("设备列表加载失败，请稍后重试");
            QMessageBox::warning(this, QStringLiteral("获取电桩列表失败"), message);
            return;
        }
        const QJsonObject responseData = response.value(QStringLiteral("data")).toObject();
        const QJsonArray pageItems = responseData.value(QStringLiteral("list")).toArray();
        for (const QJsonValue &item : pageItems) allPiles.append(item);
        total = responseData.value(QStringLiteral("total")).toInt(allPiles.size());
        ++page;
        if (pageItems.isEmpty()) break;
    } while (allPiles.size() < total);

    QVector<PileAssetItem> items;
    items.reserve(allPiles.size());
    int abnormal = 0;
    for (const QJsonValue &value : allPiles) {
        const QJsonObject object = value.toObject();
        PileAssetItem item;
        item.id = object.value(QStringLiteral("id")).toInteger();
        item.code = object.value(QStringLiteral("code")).toString();
        item.station = object.value(QStringLiteral("station")).toString();
        item.type = object.value(QStringLiteral("type")).toString();
        item.powerKw = object.value(QStringLiteral("power_kw")).toDouble();
        item.status = object.value(QStringLiteral("status")).toString().trimmed().toLower();
        item.totalCount = object.value(QStringLiteral("total_count")).toInt();
        item.totalHours = object.value(QStringLiteral("total_hours")).toDouble();
        if (item.status == QStringLiteral("fault")) ++abnormal;
        items.append(item);
    }
    m_items = items;
    m_totalValue->setText(QString::number(m_items.size()));
    m_normalValue->setText(QString::number(m_items.size() - abnormal));
    m_abnormalValue->setText(QString::number(abnormal));
    m_refreshBtn->setEnabled(true);
    applyFilters();
}

void PileManagerWidget::applyFilters()
{
    const QString keyword = m_searchEdit->text().trimmed();
    const QString station = m_stationFilter->currentData().toString();
    const QString type = m_typeFilter->currentData().toString();
    const QString status = m_statusFilter->currentData().toString();
    QVector<PileAssetItem> filtered;
    for (const PileAssetItem &item : m_items) {
        if (!keyword.isEmpty() && !item.code.contains(keyword, Qt::CaseInsensitive)) continue;
        if (!station.isEmpty() && item.station != station) continue;
        if (!type.isEmpty() && item.type != type) continue;
        if (!status.isEmpty() && item.status != status) continue;
        filtered.append(item);
    }
    populateTable(filtered);
    const bool noItems = filtered.isEmpty();
    m_emptyLabel->setText(m_items.isEmpty() ? QStringLiteral("暂无电桩设备")
                                            : QStringLiteral("未找到符合条件的设备"));
    m_emptyLabel->setVisible(noItems);
    if (noItems) {
        m_emptyLabel->setGeometry(m_table->viewport()->rect());
        m_emptyLabel->raise();
    }
    m_footerLabel->setText(filtered.size() == m_items.size()
        ? QStringLiteral("共 %1 台设备").arg(m_items.size())
        : QStringLiteral("显示 %1 台，共 %2 台设备").arg(filtered.size()).arg(m_items.size()));
}

void PileManagerWidget::resetFilters()
{
    m_searchEdit->clear();
    m_stationFilter->setCurrentIndex(0);
    m_typeFilter->setCurrentIndex(0);
    m_statusFilter->setCurrentIndex(0);
    applyFilters();
}

void PileManagerWidget::populateTable(const QVector<PileAssetItem> &items)
{
    m_table->setRowCount(0);
    for (const PileAssetItem &item : items) {
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        auto *code = tableItem(item.code);
        code->setData(Qt::UserRole, item.id);
        m_table->setItem(row, CodeColumn, code);
        m_table->setItem(row, StationColumn, tableItem(item.station));
        m_table->setItem(row, TypeColumn, tableItem(typeText(item.type)));
        m_table->setItem(row, PowerColumn, tableItem(powerText(item.powerKw)));
        auto *status = tableItem(statusText(item.status));
        status->setForeground(statusColor(item.status));
        QFont statusFont = status->font();
        statusFont.setBold(true);
        status->setFont(statusFont);
        m_table->setItem(row, StatusColumn, status);

        auto *actions = new QWidget(m_table);
        actions->setObjectName(QStringLiteral("pileAssetActions"));
        auto *layout = new QHBoxLayout(actions);
        layout->setContentsMargins(4, 2, 4, 2);
        layout->setSpacing(3);
        auto *details = new QToolButton(actions);
        details->setObjectName(QStringLiteral("pileAssetTextAction"));
        details->setText(QStringLiteral("详情"));
        connect(details, &QToolButton::clicked, this, [this, pileId = item.id] { showPileDetails(pileId); });
        layout->addWidget(details);
        m_table->setCellWidget(row, ActionColumn, actions);
    }
}

void PileManagerWidget::showPileDetails(qint64 pileId)
{
    PileAssetItem *item = findPile(pileId);
    if (!item) return;
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("设备详情"));
    dialog.setMinimumWidth(440);
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(22, 20, 22, 20);
    layout->setSpacing(14);
    auto *title = new QLabel(item->code, &dialog);
    title->setObjectName(QStringLiteral("pileDetailHeading"));
    layout->addWidget(title);
    auto *form = new QFormLayout;
    form->setHorizontalSpacing(24);
    form->setVerticalSpacing(10);
    form->addRow(QStringLiteral("设备编号"), new QLabel(item->code, &dialog));
    form->addRow(QStringLiteral("所属电站"), new QLabel(item->station, &dialog));
    form->addRow(QStringLiteral("类型"), new QLabel(typeText(item->type), &dialog));
    form->addRow(QStringLiteral("额定功率"), new QLabel(powerText(item->powerKw), &dialog));
    auto *status = new QLabel(statusText(item->status), &dialog);
    status->setStyleSheet(QStringLiteral("color: %1; font-weight: 600;").arg(statusColor(item->status).name()));
    form->addRow(QStringLiteral("当前状态"), status);
    form->addRow(QStringLiteral("累计充电次数"), new QLabel(QString::number(item->totalCount), &dialog));
    form->addRow(QStringLiteral("累计充电时长"), new QLabel(QStringLiteral("%1 小时").arg(QString::number(item->totalHours, 'f', 1)), &dialog));
    layout->addLayout(form);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    auto *restart = buttons->addButton(QStringLiteral("远程重启"), QDialogButtonBox::ActionRole);
    restart->setEnabled(!m_restartingPileIds.contains(pileId));
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(restart, &QPushButton::clicked, &dialog, [this, pileId, restart, &dialog] {
        if (restartPile(pileId, restart)) dialog.accept();
    });
    layout->addWidget(buttons);
    dialog.exec();
}

bool PileManagerWidget::restartPile(qint64 pileId, QWidget *trigger)
{
    PileAssetItem *item = findPile(pileId);
    if (!m_net || !item || m_restartingPileIds.contains(pileId)) return false;
    const QString prompt = QStringLiteral("确认重启设备？\n\n设备：%1\n所属电站：%2\n\n远程重启可能导致设备短暂离线。")
                               .arg(item->code, item->station);
    if (QMessageBox::question(this, QStringLiteral("确认远程重启"), prompt,
                              QMessageBox::Yes | QMessageBox::Cancel,
                              QMessageBox::Cancel) != QMessageBox::Yes)
        return false;

    m_restartingPileIds.insert(pileId);
    if (trigger) trigger->setEnabled(false);
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    const QJsonObject data{{QStringLiteral("pile_id"), pileId}};
    const QJsonObject response = m_net->request(Protocol::makeRequest(Protocol::MsgType::AdminPileRestart, data));
    m_restartingPileIds.remove(pileId);
    if (trigger) trigger->setEnabled(true);
    if (response.value(QStringLiteral("code")).toInt(-1) != Protocol::Ok) {
        QString message = response.value(QStringLiteral("msg")).toString();
        if (message.isEmpty()) message = QStringLiteral("电桩重启失败");
        QMessageBox::warning(this, QStringLiteral("重启失败"), message);
        return false;
    }
    item = findPile(pileId);
    if (item) item->status = response.value(QStringLiteral("data")).toObject()
                                 .value(QStringLiteral("status")).toString(QStringLiteral("idle"));
    int abnormal = 0;
    for (const PileAssetItem &asset : m_items)
        if (asset.status == QStringLiteral("fault")) ++abnormal;
    m_normalValue->setText(QString::number(m_items.size() - abnormal));
    m_abnormalValue->setText(QString::number(abnormal));
    applyFilters();
    QMessageBox::information(this, QStringLiteral("重启成功"), QStringLiteral("设备重启指令执行成功。"));
    return true;
}

PileAssetItem *PileManagerWidget::findPile(qint64 pileId)
{
    for (PileAssetItem &item : m_items)
        if (item.id == pileId) return &item;
    return nullptr;
}

QString PileManagerWidget::statusText(const QString &status)
{
    if (status == QStringLiteral("idle")) return QStringLiteral("闲置");
    if (status == QStringLiteral("busy")) return QStringLiteral("在用");
    if (status == QStringLiteral("fault")) return QStringLiteral("故障");
    return status;
}

QColor PileManagerWidget::statusColor(const QString &status)
{
    if (status == QStringLiteral("idle")) return QColor(QStringLiteral("#16A34A"));
    if (status == QStringLiteral("busy")) return QColor(QStringLiteral("#1677FF"));
    if (status == QStringLiteral("fault")) return QColor(QStringLiteral("#DC2626"));
    return QColor(QStringLiteral("#6B7280"));
}

QString PileManagerWidget::typeText(const QString &type)
{
    if (type == QStringLiteral("fast")) return QStringLiteral("快充");
    if (type == QStringLiteral("slow")) return QStringLiteral("慢充");
    return type;
}

QString PileManagerWidget::powerText(double powerKw)
{
    const bool whole = qFuzzyCompare(powerKw + 1.0, qRound64(powerKw) + 1.0);
    return QStringLiteral("%1 kW").arg(QString::number(powerKw, 'f', whole ? 0 : 1));
}

QTableWidgetItem *PileManagerWidget::tableItem(const QString &text)
{
    auto *item = new QTableWidgetItem(text);
    item->setTextAlignment(Qt::AlignCenter);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

bool PileManagerWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_table->viewport() && event->type() == QEvent::Resize && m_emptyLabel)
        m_emptyLabel->setGeometry(m_table->viewport()->rect());
    return QWidget::eventFilter(watched, event);
}
