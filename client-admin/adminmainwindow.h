#ifndef ADMINMAINWINDOW_H
#define ADMINMAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QStackedWidget>

class NetClient;

class AdminMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit AdminMainWindow(NetClient *netClient = nullptr, QWidget *parent = nullptr);
    ~AdminMainWindow();

private slots:
    void onMenuSelected(int index);

private:
    void initUI();

    NetClient *m_net;
    QListWidget *sidebarList;
    QStackedWidget *contentStack;
};

#endif // ADMINMAINWINDOW_H