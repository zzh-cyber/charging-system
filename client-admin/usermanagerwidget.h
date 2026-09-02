#ifndef USERMANAGERWIDGET_H
#define USERMANAGERWIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>

class NetClient;

class UserManagerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit UserManagerWidget(NetClient *netClient, QWidget *parent = nullptr);

public slots:
    void loadUsers();

private:
    void initUI();

    NetClient *m_net;
    QTableWidget *m_table;
    QPushButton *m_refreshBtn;
};

#endif // USERMANAGERWIDGET_H