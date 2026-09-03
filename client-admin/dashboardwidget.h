#ifndef DASHBOARDWIDGET_H
#define DASHBOARDWIDGET_H

#include <QPointF>
#include <QWidget>

class QLabel;
class QPushButton;
class QRadioButton;
class NetClient;
class QChart;
class QChartView;
class QDateTimeAxis;
class QLineSeries;
class QValueAxis;

class DashboardWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DashboardWidget(NetClient *netClient, QWidget *parent = nullptr);

public slots:
    void refreshData();

private slots:
    void onRangeChanged();
    void onPointHovered(const QPointF &point, bool state);

private:
    void initUi();
    QWidget *createKpiCard(const QString &title, QLabel **valueLabel);
    void updateChart(const QJsonObject &data, int days);
    static double numberValue(const QJsonObject &object,
                              const QStringList &keys);
    static QString moneyText(double amount);

    NetClient *m_net;
    QLabel *m_todayRevenueLabel;
    QLabel *m_monthRevenueLabel;
    QLabel *m_totalRevenueLabel;
    QLabel *m_lastUpdateLabel;
    QLabel *m_loadingLabel;
    QPushButton *m_refreshButton;
    QRadioButton *m_sevenDaysButton;
    QRadioButton *m_thirtyDaysButton;
    QChart *m_chart;
    QChartView *m_chartView;
    QLineSeries *m_series;
    QDateTimeAxis *m_dateAxis;
    QValueAxis *m_valueAxis;
    bool m_loading;
};

#endif // DASHBOARDWIDGET_H
