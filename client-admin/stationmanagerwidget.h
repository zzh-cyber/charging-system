#pragma once

#include <QVector>
#include <QWidget>

class NetClient;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

struct StationItem
{
    qint64 id = 0;
    QString name;
    QString address;
    double longitude = 0.0;
    double latitude = 0.0;
    int total = 0;
    double onlineRate = 0.0;
};

class StationManagerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit StationManagerWidget(NetClient *netClient, QWidget *parent = nullptr);

public slots:
    void loadStations();

private slots:
    void filterStations(const QString &text);
    void onStationSelectionChanged();
    void showAddStationDialog();

private:
    void initUI();
    void selectStation(qint64 stationId);
    void showStationDetails(const StationItem *station);
    void loadStationPiles(qint64 stationId);
    void clearPileDetails(const QString &message);
    QString statusText(const QString &status) const;

    NetClient *m_net = nullptr;
    QVector<StationItem> m_stations;
    QLineEdit *m_searchEdit = nullptr;
    QTableWidget *m_stationTable = nullptr;
    QTableWidget *m_pileTable = nullptr;
    QPushButton *m_refreshBtn = nullptr;
    QLabel *m_detailName = nullptr;
    QLabel *m_addressValue = nullptr;
    QLabel *m_longitudeValue = nullptr;
    QLabel *m_latitudeValue = nullptr;
    QLabel *m_priceValue = nullptr;
    QLabel *m_totalValue = nullptr;
    QLabel *m_onlineRateValue = nullptr;
    QLabel *m_idleValue = nullptr;
    QLabel *m_busyValue = nullptr;
    QLabel *m_faultValue = nullptr;
    QLabel *m_pileHint = nullptr;
    qint64 m_selectedStationId = 0;
};
