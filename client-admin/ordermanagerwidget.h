#ifndef ORDERMANAGERWIDGET_H
#define ORDERMANAGERWIDGET_H

#include <QWidget>
#include <QJsonObject>

class NetClient;
class QLineEdit;
class QComboBox;
class QDateTimeEdit;
class QTableWidget;
class QLabel;
class QPushButton;

class OrderManagerWidget : public QWidget {
    Q_OBJECT
public:
    explicit OrderManagerWidget(NetClient *net, QWidget *parent = nullptr);
private slots:
    void query();
    void reset();
    void showDetail(int row, int column);
private:
    void buildUi();
    void loadFilterOptions();
    void loadPage(int page);
    QString statusText(const QString &status) const;
    QString formatDuration(qint64 seconds) const;
    QString valueText(const QJsonObject &o, const QString &key) const;
    NetClient *m_net;
    QLineEdit *m_orderNo, *m_phone;
    QComboBox *m_station, *m_pile, *m_status;
    QDateTimeEdit *m_start, *m_end;
    QTableWidget *m_table;
    QLabel *m_total, *m_pageLabel;
    QPushButton *m_prev, *m_next;
    int m_page = 1;
    int m_totalCount = 0;
};

#endif
