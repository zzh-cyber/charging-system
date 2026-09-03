#ifndef STATIONMANAGERWIDGET_H
#define STATIONMANAGERWIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>

class NetClient;

class StationManagerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit StationManagerWidget(NetClient *netClient, QWidget *parent = nullptr);

public slots:
    void loadStations();

private:
    void initUI();

    NetClient *m_net;
    QTableWidget *m_table;
    QPushButton *m_refreshBtn;
};

#endif // STATIONMANAGERWIDGET_H