#ifndef PILEMANAGERWIDGET_H
#define PILEMANAGERWIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>

class NetClient;

class PileManagerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PileManagerWidget(NetClient *netClient, QWidget *parent = nullptr);

public slots:
    void loadPiles();

private slots:
    void onRestartClicked();

private:
    void initUI();

    NetClient *m_net;
    QTableWidget *m_table;
    QPushButton *m_refreshBtn;
};

#endif // PILEMANAGERWIDGET_H