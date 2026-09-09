#pragma once
#include <QWidget>
class NetClient; class QLabel;
class WorkbenchWidget : public QWidget
{
    Q_OBJECT
public: explicit WorkbenchWidget(NetClient*, QWidget* parent=nullptr);
signals: void openRealtime(const QString& status); void openOrders(const QString& status); void openUsers(const QString& status);
private:
    void load();
    NetClient *m_net;
    QLabel *m_cards[4]{};
    QLabel *m_cardNotes[4]{};
    QLabel *m_attentionText = nullptr;
    QLabel *m_rankingText = nullptr;
    QLabel *m_activityText = nullptr;
    QLabel *m_noticeText = nullptr;
    QWidget *m_heatmap = nullptr;
};
