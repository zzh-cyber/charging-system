#pragma once
#include <QJsonObject>
#include <QWidget>
class NetClient; class QLineEdit; class QComboBox; class QDateTimeEdit; class QTableWidget; class QLabel; class QPushButton;
class OrderManagerWidget : public QWidget
{
    Q_OBJECT
public: explicit OrderManagerWidget(NetClient*,QWidget* parent=nullptr);
private slots: void query(); void reset(); void showDetail(int,int);
private: void buildUi(); void loadFilterOptions(); void loadPage(int); void selectStatus(const QString&); void applyDateRange(int); void renderDetail(const QJsonObject&); QString statusText(const QString&)const; QString formatDuration(qint64)const; QString valueText(const QJsonObject&,const QString&)const;
 NetClient*m_net; QLineEdit*m_orderNo,*m_phone; QComboBox*m_station,*m_pile,*m_status,*m_dateRange; QDateTimeEdit*m_start,*m_end; QTableWidget*m_table; QLabel*m_total,*m_pageLabel,*m_summary,*m_detailContent; QPushButton*m_prev,*m_next; QWidget*m_morePanel,*m_detailPanel; QList<QPushButton*>m_tabs; int m_page=1,m_totalCount=0; };
