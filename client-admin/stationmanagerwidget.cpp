#include "stationmanagerwidget.h"
#include "netclient.h"
#include "protocol.h"

#include <QAbstractItemView>
#include <QColor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleValidator>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace {
QLabel *makeValueLabel(QWidget *parent)
{
    auto *label = new QLabel(QStringLiteral("--"), parent);
    label->setObjectName(QStringLiteral("stationDetailValue"));
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setWordWrap(true);
    return label;
}

QTableWidgetItem *readOnlyItem(const QString &text)
{
    auto *item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}
}

StationManagerWidget::StationManagerWidget(NetClient *netClient, QWidget *parent)
    : QWidget(parent), m_net(netClient)
{
    setObjectName(QStringLiteral("stationManagerPage"));
    initUI();
    loadStations();
}

void StationManagerWidget::initUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(22, 20, 22, 20);
    mainLayout->setSpacing(14);

    auto *topLayout = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("电站管理"), this);
    title->setObjectName(QStringLiteral("stationPageHeading"));
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral("搜索电站名称或地址"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setMinimumWidth(260);
    m_refreshBtn = new QPushButton(QStringLiteral("刷新电站列表"), this);
    auto *addButton = new QPushButton(QStringLiteral("+ 新增电站"), this);
    topLayout->addWidget(title);
    topLayout->addStretch();
    topLayout->addWidget(m_searchEdit);
    topLayout->addWidget(m_refreshBtn);
    topLayout->addWidget(addButton);
    mainLayout->addLayout(topLayout);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setObjectName(QStringLiteral("stationSplitter"));

    auto *masterPanel = new QFrame(splitter);
    masterPanel->setObjectName(QStringLiteral("stationPanel"));
    auto *masterLayout = new QVBoxLayout(masterPanel);
    masterLayout->setContentsMargins(14, 14, 14, 14);
    auto *listTitle = new QLabel(QStringLiteral("电站列表"), masterPanel);
    listTitle->setObjectName(QStringLiteral("stationSectionTitle"));
    masterLayout->addWidget(listTitle);
    m_stationTable = new QTableWidget(masterPanel);
    m_stationTable->setColumnCount(4);
    m_stationTable->setHorizontalHeaderLabels({QStringLiteral("电站名称"), QStringLiteral("地址"),
                                                QStringLiteral("总桩数"), QStringLiteral("在线率")});
    m_stationTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_stationTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_stationTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_stationTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_stationTable->verticalHeader()->setVisible(false);
    m_stationTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_stationTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_stationTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_stationTable->setAlternatingRowColors(true);
    masterLayout->addWidget(m_stationTable);

    auto *detailPanel = new QFrame(splitter);
    detailPanel->setObjectName(QStringLiteral("stationPanel"));
    auto *detailLayout = new QVBoxLayout(detailPanel);
    detailLayout->setContentsMargins(18, 16, 18, 16);
    detailLayout->setSpacing(12);
    m_detailName = new QLabel(QStringLiteral("请选择电站"), detailPanel);
    m_detailName->setObjectName(QStringLiteral("stationDetailName"));
    detailLayout->addWidget(m_detailName);

    auto *infoCard = new QFrame(detailPanel);
    infoCard->setObjectName(QStringLiteral("stationInfoCard"));
    auto *infoLayout = new QFormLayout(infoCard);
    infoLayout->setContentsMargins(14, 12, 14, 12);
    infoLayout->setHorizontalSpacing(20);
    infoLayout->setVerticalSpacing(9);
    m_addressValue = makeValueLabel(infoCard);
    m_longitudeValue = makeValueLabel(infoCard);
    m_latitudeValue = makeValueLabel(infoCard);
    m_priceValue = makeValueLabel(infoCard);
    infoLayout->addRow(QStringLiteral("地址"), m_addressValue);
    infoLayout->addRow(QStringLiteral("经度"), m_longitudeValue);
    infoLayout->addRow(QStringLiteral("纬度"), m_latitudeValue);
    infoLayout->addRow(QStringLiteral("基础电价"), m_priceValue);
    detailLayout->addWidget(infoCard);

    auto *overviewCard = new QFrame(detailPanel);
    overviewCard->setObjectName(QStringLiteral("stationInfoCard"));
    auto *overviewLayout = new QGridLayout(overviewCard);
    overviewLayout->setContentsMargins(14, 12, 14, 12);
    overviewLayout->setSpacing(8);
    const QStringList overviewNames = {QStringLiteral("总桩数"), QStringLiteral("在线率"), QStringLiteral("闲置"),
                                       QStringLiteral("在用"), QStringLiteral("故障")};
    QLabel **overviewValues[] = {&m_totalValue, &m_onlineRateValue, &m_idleValue, &m_busyValue, &m_faultValue};
    for (int i = 0; i < overviewNames.size(); ++i) {
        auto *name = new QLabel(overviewNames.at(i), overviewCard);
        name->setObjectName(QStringLiteral("stationMetricName"));
        *overviewValues[i] = new QLabel(QStringLiteral("--"), overviewCard);
        (*overviewValues[i])->setObjectName(QStringLiteral("stationMetricValue"));
        overviewLayout->addWidget(name, 0, i);
        overviewLayout->addWidget(*overviewValues[i], 1, i);
        overviewLayout->setColumnStretch(i, 1);
    }
    detailLayout->addWidget(overviewCard);

    auto *pilesTitleRow = new QHBoxLayout;
    auto *pilesTitle = new QLabel(QStringLiteral("站内电桩"), detailPanel);
    pilesTitle->setObjectName(QStringLiteral("stationSectionTitle"));
    m_pileHint = new QLabel(QStringLiteral("请选择电站"), detailPanel);
    m_pileHint->setObjectName(QStringLiteral("stationPileHint"));
    pilesTitleRow->addWidget(pilesTitle);
    pilesTitleRow->addStretch();
    pilesTitleRow->addWidget(m_pileHint);
    detailLayout->addLayout(pilesTitleRow);
    m_pileTable = new QTableWidget(detailPanel);
    m_pileTable->setColumnCount(4);
    m_pileTable->setHorizontalHeaderLabels({QStringLiteral("桩编号"), QStringLiteral("类型"),
                                             QStringLiteral("额定功率"), QStringLiteral("状态")});
    m_pileTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_pileTable->verticalHeader()->setVisible(false);
    m_pileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_pileTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_pileTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    detailLayout->addWidget(m_pileTable, 1);

    splitter->addWidget(masterPanel);
    splitter->addWidget(detailPanel);
    splitter->setStretchFactor(0, 4);
    splitter->setStretchFactor(1, 6);
    splitter->setSizes({470, 700});
    mainLayout->addWidget(splitter, 1);

    connect(m_refreshBtn, &QPushButton::clicked, this, &StationManagerWidget::loadStations);
    connect(addButton, &QPushButton::clicked, this, &StationManagerWidget::showAddStationDialog);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &StationManagerWidget::filterStations);
    connect(m_stationTable, &QTableWidget::itemSelectionChanged, this, &StationManagerWidget::onStationSelectionChanged);
}

void StationManagerWidget::loadStations()
{
    if (!m_net) return;
    const qint64 previousId = m_selectedStationId;
    const QJsonObject response = m_net->request(Protocol::makeRequest(Protocol::MsgType::AdminStationList));
    if (response.value(QStringLiteral("code")).toInt(-1) != Protocol::Ok) {
        QMessageBox::warning(this, QStringLiteral("获取电站列表失败"), response.value(QStringLiteral("msg")).toString());
        return;
    }

    QVector<StationItem> stations;
    const QJsonArray list = response.value(QStringLiteral("data")).toObject().value(QStringLiteral("list")).toArray();
    stations.reserve(list.size());
    for (const QJsonValue &value : list) {
        const QJsonObject object = value.toObject();
        StationItem station;
        station.id = object.value(QStringLiteral("id")).toInteger();
        station.name = object.value(QStringLiteral("name")).toString();
        station.address = object.value(QStringLiteral("address")).toString();
        station.longitude = object.value(QStringLiteral("longitude")).toDouble();
        station.latitude = object.value(QStringLiteral("latitude")).toDouble();
        station.total = object.value(QStringLiteral("total")).toInt();
        station.onlineRate = object.value(QStringLiteral("online_rate")).toDouble();
        stations.append(station);
    }
    m_stations = stations;
    {
        const QSignalBlocker blocker(m_stationTable);
        m_stationTable->setRowCount(m_stations.size());
        for (int row = 0; row < m_stations.size(); ++row) {
            const StationItem &station = m_stations.at(row);
            auto *name = readOnlyItem(station.name);
            name->setData(Qt::UserRole, station.id);
            m_stationTable->setItem(row, 0, name);
            m_stationTable->setItem(row, 1, readOnlyItem(station.address));
            m_stationTable->setItem(row, 2, readOnlyItem(QString::number(station.total)));
            m_stationTable->setItem(row, 3, readOnlyItem(QStringLiteral("%1%").arg(QString::number(station.onlineRate * 100.0, 'f', 1))));
        }
    }
    filterStations(m_searchEdit->text());
    if (previousId > 0) selectStation(previousId);
    if (m_stationTable->selectedItems().isEmpty()) {
        for (int row = 0; row < m_stationTable->rowCount(); ++row) {
            if (!m_stationTable->isRowHidden(row)) {
                m_stationTable->selectRow(row);
                break;
            }
        }
    }
    if (m_stationTable->selectedItems().isEmpty()) {
        m_selectedStationId = 0;
        showStationDetails(nullptr);
    }
}

void StationManagerWidget::filterStations(const QString &text)
{
    const QString keyword = text.trimmed();
    int firstVisible = -1;
    for (int row = 0; row < m_stationTable->rowCount(); ++row) {
        const bool matches = keyword.isEmpty()
            || m_stationTable->item(row, 0)->text().contains(keyword, Qt::CaseInsensitive)
            || m_stationTable->item(row, 1)->text().contains(keyword, Qt::CaseInsensitive);
        m_stationTable->setRowHidden(row, !matches);
        if (matches && firstVisible < 0) firstVisible = row;
    }
    const int selectedRow = m_stationTable->currentRow();
    if (selectedRow >= 0 && !m_stationTable->isRowHidden(selectedRow)) return;
    if (firstVisible >= 0) m_stationTable->selectRow(firstVisible);
    else {
        m_stationTable->clearSelection();
        m_selectedStationId = 0;
        showStationDetails(nullptr);
    }
}

void StationManagerWidget::selectStation(qint64 stationId)
{
    for (int row = 0; row < m_stationTable->rowCount(); ++row) {
        if (!m_stationTable->isRowHidden(row) && m_stationTable->item(row, 0)->data(Qt::UserRole).toLongLong() == stationId) {
            m_stationTable->selectRow(row);
            return;
        }
    }
}

void StationManagerWidget::onStationSelectionChanged()
{
    const int row = m_stationTable->currentRow();
    if (row < 0 || m_stationTable->isRowHidden(row)) return;
    const qint64 stationId = m_stationTable->item(row, 0)->data(Qt::UserRole).toLongLong();
    for (const StationItem &station : m_stations) {
        if (station.id == stationId) {
            m_selectedStationId = stationId;
            showStationDetails(&station);
            loadStationPiles(stationId);
            return;
        }
    }
}

void StationManagerWidget::showStationDetails(const StationItem *station)
{
    if (!station) {
        m_detailName->setText(QStringLiteral("请选择电站"));
        for (QLabel *label : {m_addressValue, m_longitudeValue, m_latitudeValue, m_priceValue,
                              m_totalValue, m_onlineRateValue, m_idleValue, m_busyValue, m_faultValue})
            label->setText(QStringLiteral("--"));
        clearPileDetails(QStringLiteral("请选择电站"));
        return;
    }
    m_detailName->setText(station->name);
    m_addressValue->setText(station->address.isEmpty() ? QStringLiteral("--") : station->address);
    m_longitudeValue->setText(QString::number(station->longitude, 'f', 6));
    m_latitudeValue->setText(QString::number(station->latitude, 'f', 6));
    m_priceValue->setText(QStringLiteral("--"));
    m_totalValue->setText(QString::number(station->total));
    m_onlineRateValue->setText(QStringLiteral("%1%").arg(QString::number(station->onlineRate * 100.0, 'f', 1)));
    m_idleValue->setText(QStringLiteral("--"));
    m_busyValue->setText(QStringLiteral("--"));
    m_faultValue->setText(QStringLiteral("--"));
    clearPileDetails(QStringLiteral("正在加载…"));
}

void StationManagerWidget::loadStationPiles(qint64 stationId)
{
    if (!m_net || stationId <= 0) return;
    QJsonArray piles;
    int page = 1;
    int total = 0;
    do {
        const QJsonObject requestData{{QStringLiteral("station_id"), stationId},
                                      {QStringLiteral("page"), page}, {QStringLiteral("page_size"), 50}};
        const QJsonObject response = m_net->request(Protocol::makeRequest(Protocol::MsgType::AdminPileList, requestData));
        if (response.value(QStringLiteral("code")).toInt(-1) != Protocol::Ok) {
            clearPileDetails(QStringLiteral("站内电桩加载失败"));
            QMessageBox::warning(this, QStringLiteral("获取站内电桩失败"), response.value(QStringLiteral("msg")).toString());
            return;
        }
        const QJsonObject data = response.value(QStringLiteral("data")).toObject();
        const QJsonArray pageItems = data.value(QStringLiteral("list")).toArray();
        for (const QJsonValue &item : pageItems) piles.append(item);
        total = data.value(QStringLiteral("total")).toInt(piles.size());
        ++page;
        if (pageItems.isEmpty()) break;
    } while (piles.size() < total);
    if (stationId != m_selectedStationId) return;

    int idle = 0, busy = 0, fault = 0;
    m_pileTable->setRowCount(piles.size());
    for (int row = 0; row < piles.size(); ++row) {
        const QJsonObject pile = piles.at(row).toObject();
        const QString status = pile.value(QStringLiteral("status")).toString();
        if (status == QStringLiteral("idle")) ++idle;
        else if (status == QStringLiteral("busy")) ++busy;
        else if (status == QStringLiteral("fault")) ++fault;
        m_pileTable->setItem(row, 0, readOnlyItem(pile.value(QStringLiteral("code")).toString()));
        m_pileTable->setItem(row, 1, readOnlyItem(pile.value(QStringLiteral("type")).toString()));
        m_pileTable->setItem(row, 2, readOnlyItem(QStringLiteral("%1 kW").arg(QString::number(pile.value(QStringLiteral("power_kw")).toDouble(), 'f', 1))));
        auto *statusItem = readOnlyItem(statusText(status));
        if (status == QStringLiteral("idle")) statusItem->setForeground(QColor(QStringLiteral("#16A34A")));
        else if (status == QStringLiteral("busy")) statusItem->setForeground(QColor(QStringLiteral("#1677FF")));
        else if (status == QStringLiteral("fault")) statusItem->setForeground(QColor(QStringLiteral("#DC2626")));
        m_pileTable->setItem(row, 3, statusItem);
    }
    m_idleValue->setText(QString::number(idle));
    m_busyValue->setText(QString::number(busy));
    m_faultValue->setText(QString::number(fault));
    m_pileHint->setText(QStringLiteral("共 %1 台").arg(piles.size()));
}

void StationManagerWidget::clearPileDetails(const QString &message)
{
    m_pileTable->setRowCount(0);
    m_pileHint->setText(message);
}

QString StationManagerWidget::statusText(const QString &status) const
{
    if (status == QStringLiteral("idle")) return QStringLiteral("闲置");
    if (status == QStringLiteral("busy")) return QStringLiteral("在用");
    if (status == QStringLiteral("fault")) return QStringLiteral("故障");
    return status;
}

void StationManagerWidget::showAddStationDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("新增电站"));
    dialog.setMinimumWidth(430);
    auto *form = new QFormLayout(&dialog);
    form->setContentsMargins(22, 20, 22, 20);
    form->setSpacing(12);
    auto *name = new QLineEdit(&dialog);
    auto *address = new QLineEdit(&dialog);
    auto *longitude = new QLineEdit(&dialog);
    auto *latitude = new QLineEdit(&dialog);
    auto *price = new QLineEdit(QStringLiteral("1.00"), &dialog);
    auto *pileCount = new QSpinBox(&dialog);
    longitude->setValidator(new QDoubleValidator(-180, 180, 6, longitude));
    latitude->setValidator(new QDoubleValidator(-90, 90, 6, latitude));
    price->setValidator(new QDoubleValidator(0.01, 9999, 2, price));
    pileCount->setRange(0, 1000);
    pileCount->setValue(1);
    form->addRow(QStringLiteral("站名"), name);
    form->addRow(QStringLiteral("地址"), address);
    form->addRow(QStringLiteral("经度"), longitude);
    form->addRow(QStringLiteral("纬度"), latitude);
    form->addRow(QStringLiteral("电价"), price);
    form->addRow(QStringLiteral("电桩数量"), pileCount);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        if (name->text().trimmed().isEmpty() || address->text().trimmed().isEmpty()
            || longitude->text().isEmpty() || latitude->text().isEmpty() || price->text().isEmpty()) {
            QMessageBox::warning(&dialog, QStringLiteral("提示"), QStringLiteral("请完整填写电站信息"));
            return;
        }
        if (!m_net) return;
        const QJsonObject data{{QStringLiteral("name"), name->text().trimmed()},
                               {QStringLiteral("address"), address->text().trimmed()},
                               {QStringLiteral("longitude"), longitude->text().toDouble()},
                               {QStringLiteral("latitude"), latitude->text().toDouble()},
                               {QStringLiteral("price"), price->text().toDouble()},
                               {QStringLiteral("pile_count"), pileCount->value()}};
        const QJsonObject response = m_net->request(Protocol::makeRequest(Protocol::MsgType::AdminStationAdd, data));
        if (response.value(QStringLiteral("code")).toInt(-1) != Protocol::Ok) {
            QMessageBox::warning(&dialog, QStringLiteral("新增失败"), response.value(QStringLiteral("msg")).toString());
            return;
        }
        dialog.accept();
        m_selectedStationId = response.value(QStringLiteral("data")).toObject().value(QStringLiteral("id")).toInteger();
        loadStations();
    });
    dialog.exec();
}
