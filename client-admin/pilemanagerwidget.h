#pragma once

#include <QColor>
#include <QSet>
#include <QVector>
#include <QWidget>

class NetClient;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTableWidgetItem;

struct PileAssetItem
{
    qint64 id = 0;
    QString code;
    QString station;
    QString type;
    double powerKw = 0.0;
    QString status;
    int totalCount = 0;
    double totalHours = 0.0;
};

class PileManagerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PileManagerWidget(NetClient *netClient, QWidget *parent = nullptr);

public slots:
    void loadPiles();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void applyFilters();
    void resetFilters();

private:
    void initUI();
    void loadStationOptions();
    void populateTable(const QVector<PileAssetItem> &items);
    void showPileDetails(qint64 pileId);
    bool restartPile(qint64 pileId, QWidget *trigger = nullptr);
    PileAssetItem *findPile(qint64 pileId);
    static QString statusText(const QString &status);
    static QColor statusColor(const QString &status);
    static QString typeText(const QString &type);
    static QString powerText(double powerKw);
    static QTableWidgetItem *tableItem(const QString &text);

    NetClient *m_net = nullptr;
    QVector<PileAssetItem> m_items;
    QSet<qint64> m_restartingPileIds;
    QLineEdit *m_searchEdit = nullptr;
    QComboBox *m_stationFilter = nullptr;
    QComboBox *m_typeFilter = nullptr;
    QComboBox *m_statusFilter = nullptr;
    QPushButton *m_refreshBtn = nullptr;
    QTableWidget *m_table = nullptr;
    QLabel *m_totalValue = nullptr;
    QLabel *m_normalValue = nullptr;
    QLabel *m_abnormalValue = nullptr;
    QLabel *m_footerLabel = nullptr;
    QLabel *m_emptyLabel = nullptr;
};
