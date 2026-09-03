#pragma once

#include <QWidget>
#include <QAbstractTableModel>
#include <QSortFilterProxyModel>
#include <QVector>
#include <QDateTime>

class NetClient;
class QTableView;
class QComboBox;
class QPushButton;
class QLabel;
class QTimer;

// 单条电桩状态数据
struct PileStatusItem
{
    qint64 id = 0;

    QString code;
    QString station;
    QString type;

    double powerKw = 0.0;

    QString status;

    QDateTime updatedAt;
};


// ==================== 数据模型 ====================

class PileStatusModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column
    {
        CodeColumn = 0,
        StationColumn,
        TypeColumn,
        PowerColumn,
        StatusColumn,
        UpdatedAtColumn,
        ColumnCount
    };

    enum Role
    {
        PileIdRole = Qt::UserRole + 1,
        RawStatusRole,
        SortRole
    };

    explicit PileStatusModel(QObject *parent = nullptr);

    int rowCount(
        const QModelIndex &parent = QModelIndex()
    ) const override;

    int columnCount(
        const QModelIndex &parent = QModelIndex()
    ) const override;

    QVariant data(
        const QModelIndex &index,
        int role = Qt::DisplayRole
    ) const override;

    QVariant headerData(
        int section,
        Qt::Orientation orientation,
        int role = Qt::DisplayRole
    ) const override;

    void setItems(const QVector<PileStatusItem> &items);

    const PileStatusItem &itemAt(int row) const;

private:
    QVector<PileStatusItem> m_items;

    QString statusText(const QString &status) const;
};


// ==================== 筛选模型 ====================

class PileStatusProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit PileStatusProxyModel(QObject *parent = nullptr);

    void setStatusFilter(const QString &status);

protected:
    bool filterAcceptsRow(
        int sourceRow,
        const QModelIndex &sourceParent
    ) const override;

private:
    QString m_statusFilter;
};


// ==================== 页面 ====================

class PileStatusWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PileStatusWidget(
        NetClient *netClient,
        QWidget *parent = nullptr
    );

signals:

    // 双击一行后通知主窗口跳转到“电桩管理”
    void openPileManageRequested(qint64 pileId);

private slots:

    void loadStatus();

    void onFilterChanged(int index);

    void onTableDoubleClicked(
        const QModelIndex &index
    );

private:

    void initUI();

    void updateSummary(
        const QVector<PileStatusItem> &items
    );

    QDateTime parseDateTime(
        const QString &text
    ) const;


private:

    NetClient *m_net = nullptr;

    PileStatusModel *m_model = nullptr;

    PileStatusProxyModel *m_proxyModel = nullptr;

    QTableView *m_table = nullptr;

    QComboBox *m_statusCombo = nullptr;

    QPushButton *m_refreshBtn = nullptr;

    QLabel *m_summaryLabel = nullptr;

    QLabel *m_lastUpdateLabel = nullptr;

    QTimer *m_timer = nullptr;
};
