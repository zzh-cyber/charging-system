#ifndef PILEMANAGERWIDGET_H
#define PILEMANAGERWIDGET_H

#include <QColor>
#include <QWidget>

class QComboBox;
class QPushButton;
class QTableWidget;
class QTableWidgetItem;
class NetClient;

class PileManagerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PileManagerWidget(NetClient *netClient, QWidget *parent = nullptr);

public slots:
    void loadPiles();

private slots:
    void applyStatusFilter();
    void onRestartClicked();

private:
    void initUI();
    static QString statusText(const QString &rawStatus);
    static QColor statusColor(const QString &rawStatus);
    static void setCenteredItem(QTableWidgetItem *item);

    NetClient *m_net;
    QTableWidget *m_table;
    QPushButton *m_refreshBtn;
    QComboBox *m_statusFilter;
};

#endif // PILEMANAGERWIDGET_H
