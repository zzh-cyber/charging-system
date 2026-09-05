#include "pilestatuswidget.h"

#include "netclient.h"
#include "protocol.h"

#include <QTableView>
#include <QHeaderView>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QTimer>

#include <QVBoxLayout>
#include <QHBoxLayout>

#include <QJsonObject>
#include <QJsonArray>

#include <QMessageBox>

#include <QBrush>
#include <QColor>

#include <QRegularExpression>


// ============================================================
// PileStatusModel
// ============================================================

PileStatusModel::PileStatusModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}


int PileStatusModel::rowCount(
    const QModelIndex &parent
) const
{
    if (parent.isValid())
        return 0;

    return m_items.size();
}


int PileStatusModel::columnCount(
    const QModelIndex &parent
) const
{
    if (parent.isValid())
        return 0;

    return ColumnCount;
}


QVariant PileStatusModel::data(
    const QModelIndex &index,
    int role
) const
{
    if (!index.isValid())
        return QVariant();

    if (index.row() < 0 ||
        index.row() >= m_items.size())
    {
        return QVariant();
    }

    const PileStatusItem &item =
        m_items.at(index.row());


    // ====================
    // 显示内容
    // ====================

    if (role == Qt::DisplayRole)
    {
        switch (index.column())
        {
        case CodeColumn:
            return item.code;

        case StationColumn:
            return item.station;

        case TypeColumn:
            return item.type;

        case PowerColumn:
            return QString::number(
                item.powerKw,
                'f',
                1
            );

        case StatusColumn:
            return statusText(item.status);

        case UpdatedAtColumn:

            if (!item.updatedAt.isValid())
                return "-";

            return item.updatedAt.toString(
                "yyyy-MM-dd HH:mm:ss"
            );

        default:
            return QVariant();
        }
    }


    // ====================
    // 电桩 ID
    // ====================

    if (role == PileIdRole)
    {
        return item.id;
    }


    // ====================
    // 原始状态
    // ====================

    if (role == RawStatusRole)
    {
        return item.status;
    }


    // ====================
    // 排序值
    // ====================

    if (role == SortRole)
    {
        switch (index.column())
        {
        case CodeColumn:
            return item.code;

        case StationColumn:
            return item.station;

        case TypeColumn:
            return item.type;

        case PowerColumn:
            return item.powerKw;

        case StatusColumn:
            return item.status;

        case UpdatedAtColumn:
            return item.updatedAt;

        default:
            return QVariant();
        }
    }


    // ====================
    // 状态颜色
    // ====================

    if (role == Qt::ForegroundRole &&
        index.column() == StatusColumn)
    {
        if (item.status == "idle")
        {
            return QBrush(
                QColor("#2ecc71")
            );
        }

        if (item.status == "busy")
        {
            return QBrush(
                QColor("#3498db")
            );
        }

        if (item.status == "fault")
        {
            return QBrush(
                QColor("#e74c3c")
            );
        }
    }


    return QVariant();
}


QVariant PileStatusModel::headerData(
    int section,
    Qt::Orientation orientation,
    int role
) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation != Qt::Horizontal)
        return section + 1;

    switch (section)
    {
    case CodeColumn:
        return "电桩编号";

    case StationColumn:
        return "所属电站";

    case TypeColumn:
        return "类型";

    case PowerColumn:
        return "功率(kW)";

    case StatusColumn:
        return "状态";

    case UpdatedAtColumn:
        return "最近更新时间";

    default:
        return QVariant();
    }
}


void PileStatusModel::setItems(
    const QVector<PileStatusItem> &items
)
{
    if (m_items.size() == items.size()) {
        bool sameIds = true;
        for (int i = 0; i < items.size(); ++i)
            sameIds = sameIds && m_items.at(i).id == items.at(i).id;
        if (sameIds) {
            m_items = items;
            if (!m_items.isEmpty())
                emit dataChanged(index(0, 0), index(m_items.size() - 1, ColumnCount - 1));
            return;
        }
    }
    beginResetModel();
    m_items = items;

    endResetModel();
}


const PileStatusItem &
PileStatusModel::itemAt(int row) const
{
    return m_items.at(row);
}


QString PileStatusModel::statusText(
    const QString &status
) const
{
    if (status == "idle")
        return "闲置";

    if (status == "busy")
        return "在用";

    if (status == "fault")
        return "故障";

    // 未知状态直接显示原值
    return status;
}


// ============================================================
// PileStatusProxyModel
// ============================================================

PileStatusProxyModel::PileStatusProxyModel(
    QObject *parent
)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);

    // 使用模型自定义的 SortRole 排序
    setSortRole(
        PileStatusModel::SortRole
    );
}


void PileStatusProxyModel::setStatusFilter(
    const QString &status
)
{
    m_statusFilter = status;

    invalidateFilter();
}


bool PileStatusProxyModel::filterAcceptsRow(
    int sourceRow,
    const QModelIndex &sourceParent
) const
{
    // 空字符串 = 全部
    if (m_statusFilter.isEmpty())
    {
        return true;
    }

    QModelIndex index =
        sourceModel()->index(
            sourceRow,
            PileStatusModel::StatusColumn,
            sourceParent
        );

    QString status =
        sourceModel()
            ->data(
                index,
                PileStatusModel::RawStatusRole
            )
            .toString();

    return status == m_statusFilter;
}


// ============================================================
// PileStatusWidget
// ============================================================

PileStatusWidget::PileStatusWidget(
    NetClient *netClient,
    QWidget *parent
)
    : QWidget(parent),
      m_net(netClient)
{
    initUI();

    // 第一次进入立即加载
    loadStatus();

    // 10 秒自动刷新
    m_timer = new QTimer(this);

    m_timer->setInterval(10000);

    connect(
        m_timer,
        &QTimer::timeout,
        this,
        &PileStatusWidget::loadStatus
    );

    m_timer->start();
}


void PileStatusWidget::initUI()
{
    auto *mainLayout =
        new QVBoxLayout(this);


    // ========================================================
    // 顶部
    // ========================================================

    auto *topLayout =
        new QHBoxLayout();


    auto *titleLabel =
        new QLabel(
            "电桩状态",
            this
        );

    QFont titleFont =
        titleLabel->font();

    titleFont.setPointSize(16);
    titleFont.setBold(true);

    titleLabel->setFont(titleFont);


    topLayout->addWidget(
        titleLabel
    );


    topLayout->addSpacing(25);


    m_summaryLabel =
        new QLabel(
            "总数：0 | 闲置：0 | 在用：0 | 故障：0",
            this
        );

    topLayout->addWidget(
        m_summaryLabel
    );


    topLayout->addStretch();


    auto *filterLabel =
        new QLabel(
            "状态筛选：",
            this
        );

    topLayout->addWidget(
        filterLabel
    );


    m_statusCombo =
        new QComboBox(this);

    m_statusCombo->addItem(
        "全部"
    );

    m_statusCombo->addItem(
        "闲置"
    );

    m_statusCombo->addItem(
        "在用"
    );

    m_statusCombo->addItem(
        "故障"
    );

    topLayout->addWidget(
        m_statusCombo
    );


    m_refreshBtn =
        new QPushButton(
            "立即刷新",
            this
        );

    topLayout->addWidget(
        m_refreshBtn
    );


    mainLayout->addLayout(
        topLayout
    );


    // ========================================================
    // 最后更新时间
    // ========================================================

    m_lastUpdateLabel =
        new QLabel(
            "最后更新：-",
            this
        );

    mainLayout->addWidget(
        m_lastUpdateLabel
    );


    // ========================================================
    // Model
    // ========================================================

    m_model =
        new PileStatusModel(this);


    m_proxyModel =
        new PileStatusProxyModel(this);

    m_proxyModel->setSourceModel(
        m_model
    );


    // ========================================================
    // TableView
    // ========================================================

    m_table =
        new QTableView(this);

    m_table->setModel(
        m_proxyModel
    );


    m_table
        ->horizontalHeader()
        ->setSectionResizeMode(
            QHeaderView::Stretch
        );


    m_table->setSelectionBehavior(
        QAbstractItemView::SelectRows
    );

    m_table->setSelectionMode(
        QAbstractItemView::SingleSelection
    );

    m_table->setEditTriggers(
        QAbstractItemView::NoEditTriggers
    );

    // 允许点击表头排序
    m_table->setSortingEnabled(true);

    m_table->sortByColumn(
        PileStatusModel::UpdatedAtColumn,
        Qt::DescendingOrder
    );


    mainLayout->addWidget(
        m_table
    );


    // ========================================================
    // Signals
    // ========================================================

    connect(
        m_refreshBtn,
        &QPushButton::clicked,
        this,
        &PileStatusWidget::loadStatus
    );


    connect(
        m_statusCombo,
        QOverload<int>::of(
            &QComboBox::currentIndexChanged
        ),
        this,
        &PileStatusWidget::onFilterChanged
    );


    connect(
        m_table,
        &QTableView::doubleClicked,
        this,
        &PileStatusWidget::onTableDoubleClicked
    );
}


void PileStatusWidget::loadStatus()
{
    if (!m_net || m_requestInFlight)
    {
        return;
    }


    QJsonObject req =
        Protocol::makeRequest(
            Protocol::MsgType::AdminPileList,
            QJsonObject()
        );


    m_requestInFlight = true;
    QJsonObject resp = m_net->request(req);
    m_requestInFlight = false;


    int code =
        resp.value("code").toInt(-1);

    QString msg =
        resp.value("msg").toString();


    if (code != Protocol::Ok)
    {
        // 自动刷新失败时避免每 10 秒弹窗
        if (sender() == m_timer)
        {
            m_timer->stop();
            m_lastUpdateLabel->setText(
                QString(
                    "自动刷新失败：%1"
                ).arg(msg)
            );

            return;
        }


        QMessageBox::warning(
            this,
            "获取电桩状态失败",
            QString(
                "code=%1\n%2"
            )
                .arg(code)
                .arg(msg)
        );

        return;
    }


    /*
     * 当前 AdminPileList 格式：
     *
     * data : {
     *     "list" : [...]
     * }
     */

    QJsonObject data =
        resp.value("data").toObject();

    QJsonArray list = data.value("list").toArray();
    const QJsonValue statsValue = data.value(QStringLiteral("stats"));
    if (!statsValue.isObject()) {
        m_lastUpdateLabel->setText(QStringLiteral("协议响应异常：电桩统计格式错误"));
        return;
    }
    const QJsonObject stats = statsValue.toObject();
    const QJsonValue totalValue = stats.value(QStringLiteral("total"));
    const QJsonValue statTimeValue = stats.value(QStringLiteral("stat_time"));
    const auto validState = [&stats](const QString &name) {
        const QJsonValue value = stats.value(name);
        if (!value.isObject())
            return false;
        const QJsonObject state = value.toObject();
        return state.value(QStringLiteral("count")).isDouble()
            && state.value(QStringLiteral("rate")).isDouble();
    };
    if (!totalValue.isDouble()
        || !validState(QStringLiteral("idle"))
        || !validState(QStringLiteral("busy"))
        || !validState(QStringLiteral("fault"))
        || !statTimeValue.isString()
        || statTimeValue.toString().trimmed().isEmpty()) {
        m_lastUpdateLabel->setText(QStringLiteral("协议响应异常：电桩统计格式错误"));
        return;
    }
    const QDateTime statTime = parseDateTime(statTimeValue.toString());
    if (!statTime.isValid()) {
        m_lastUpdateLabel->setText(QStringLiteral("协议响应异常：电桩统计格式错误"));
        return;
    }


    QVector<PileStatusItem> items;

    items.reserve(
        list.size()
    );


    for (const QJsonValue &value : list)
    {
        QJsonObject p =
            value.toObject();


        PileStatusItem item;


        item.id =
            p.value("id").toInteger();


        item.code =
            p.value("code").toString();


        item.station =
            p.value("station").toString();


        item.type =
            p.value("type").toString();


        item.powerKw =
            p.value("power_kw").toDouble();


        item.status =
            p.value("status").toString();


        /*
         * 当前 admin_pile_list 没有最近更新时间，
         * 所以先兼容几个可能字段。
         *
         * 如果都没有，表格显示 "-"
         */

        QString timeText =
            p.value("updated_at").toString();

        if (timeText.isEmpty())
        {
            timeText =
                p.value("update_time").toString();
        }

        if (timeText.isEmpty())
        {
            timeText =
                p.value("status_updated_at").toString();
        }


        item.updatedAt =
            parseDateTime(timeText);


        items.append(item);
    }


    m_model->setItems(
        items
    );


    updateSummary(stats);
    m_lastUpdateLabel->setText(QStringLiteral("统计时间：%1").arg(statTime.toString("yyyy-MM-dd HH:mm:ss")));
}


void PileStatusWidget::updateSummary(const QJsonObject &stats)
{
    const int total = stats.value(QStringLiteral("total")).toInt();
    const QJsonObject idle = stats.value(QStringLiteral("idle")).toObject();
    const QJsonObject busy = stats.value(QStringLiteral("busy")).toObject();
    const QJsonObject fault = stats.value(QStringLiteral("fault")).toObject();
    const int idleCount = idle.value(QStringLiteral("count")).toInt();
    const int busyCount = busy.value(QStringLiteral("count")).toInt();
    const int faultCount = fault.value(QStringLiteral("count")).toInt();
    const double idleRate = idle.value(QStringLiteral("rate")).toDouble();
    const double busyRate = busy.value(QStringLiteral("rate")).toDouble();
    const double faultRate = fault.value(QStringLiteral("rate")).toDouble();


    m_summaryLabel->setText(
        QString(
            "总数：%1 | "
            "闲置：%2 (%3%) | "
            "在用：%4 (%5%) | "
            "故障：%6 (%7%)"
        )
            .arg(total)
            .arg(idleCount)
            .arg(
                QString::number(
                    idleRate,
                    'f',
                    1
                )
            )
            .arg(busyCount)
            .arg(
                QString::number(
                    busyRate,
                    'f',
                    1
                )
            )
            .arg(faultCount)
            .arg(
                QString::number(
                    faultRate,
                    'f',
                    1
                )
            )
    );
}


void PileStatusWidget::onFilterChanged(
    int index
)
{
    QString status;


    switch (index)
    {
    case 1:
        status = "idle";
        break;

    case 2:
        status = "busy";
        break;

    case 3:
        status = "fault";
        break;

    default:
        status.clear();
        break;
    }


    m_proxyModel->setStatusFilter(
        status
    );
}


void PileStatusWidget::onTableDoubleClicked(
    const QModelIndex &index
)
{
    if (!index.isValid())
        return;


    QModelIndex sourceIndex =
        m_proxyModel->mapToSource(
            index
        );


    if (!sourceIndex.isValid())
        return;


    const PileStatusItem &item =
        m_model->itemAt(
            sourceIndex.row()
        );


    emit openPileManageRequested(
        item.id
    );
}


QDateTime PileStatusWidget::parseDateTime(
    const QString &text
) const
{
    if (text.trimmed().isEmpty())
    {
        return QDateTime();
    }


    // ISO 格式
    QDateTime dt =
        QDateTime::fromString(
            text,
            Qt::ISODate
        );


    if (dt.isValid())
    {
        return dt;
    }


    // MySQL 常见格式
    dt =
        QDateTime::fromString(
            text,
            "yyyy-MM-dd HH:mm:ss"
        );


    return dt;
}
