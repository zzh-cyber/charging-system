#ifndef DASHBOARDWIDGET_H
#define DASHBOARDWIDGET_H
#include <QJsonObject>
#include <QPointF>
#include <QWidget>
class QLabel; class QPushButton; class QRadioButton; class NetClient; class QNetworkAccessManager; class QNetworkReply;
class QChart; class QChartView; class QLineSeries; class QValueAxis;
class DashboardWidget : public QWidget {
 Q_OBJECT
public: explicit DashboardWidget(NetClient*,QWidget*parent=nullptr); void setDarkTheme(bool);
public slots: void refreshData();
private slots: void onRangeChanged(); void onReplyFinished(QNetworkReply*); void onPointHovered(const QPointF&,bool);
private:
 void initUi(); QWidget*createKpiCard(const QString&,const QString&,QLabel**,const char*); QWidget*createPanel(const QString&,QLabel**);
 void applyDashboard(const QJsonObject&); void updateLoadChart(const QJsonObject&); void setLoading(bool);
 static double numberValue(const QJsonObject&,const QString&,double fallback=0); static QString numberText(double,const QString&unit=QString()); static QString moneyText(double); static QString comparisonText(const QJsonObject&,const QString&,const QString&);
 NetClient*m_net; QNetworkAccessManager*m_http; QLabel*m_kpiLabels[6]; QLabel*m_targetLabel; QLabel*m_stationLabel; QLabel*m_dispatchLabel; QLabel*m_alertLabel; QLabel*m_faultLabel; QLabel*m_structureLabel; QLabel*m_qualityLabel; QLabel*m_lastUpdateLabel; QLabel*m_loadingLabel; QPushButton*m_refreshButton; QRadioButton*m_todayButton; QRadioButton*m_sevenDaysButton; QRadioButton*m_thirtyDaysButton; QChart*m_chart; QChartView*m_chartView; QLineSeries*m_currentSeries; QLineSeries*m_previousSeries; QLineSeries*m_forecastSeries; QLineSeries*m_capacitySeries; QLineSeries*m_thresholdSeries; QValueAxis*m_hourAxis; QValueAxis*m_valueAxis; bool m_loading; int m_currentDays;
};
#endif
