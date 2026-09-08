#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QVector>
#include <QWidget>

class NetClient;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QResizeEvent;
class QScrollArea;
class QTimer;
class QVBoxLayout;

struct PileStatusItem
{
    qint64 id = 0;
    QString code;
    QString station;
    QString type;
    double powerKw = 0.0;
    QString status;
};

class PileStatusWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PileStatusWidget(NetClient *netClient, QWidget *parent = nullptr);
    void applyRefreshSettings(bool autoRefresh, int intervalMs, bool pauseWhenHidden, bool pageVisible);

signals:
    void openPileManageRequested(qint64 pileId);

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void loadStatus();
    void applyFilters();

private:
    void initUI();
    void rebuildCards(bool preserveScrollPosition = true);
    void updateStationFilter();
    void updateSummary(const QJsonObject &stats);
    QWidget *createPileCard(const PileStatusItem &item);
    QString statusText(const QString &status) const;
    int cardColumnCount() const;
    QDateTime parseDateTime(const QString &text) const;

    NetClient *m_net = nullptr;
    QVector<PileStatusItem> m_items;
    QLineEdit *m_searchEdit = nullptr;
    QComboBox *m_stationCombo = nullptr;
    QComboBox *m_statusCombo = nullptr;
    QPushButton *m_refreshBtn = nullptr;
    QLabel *m_totalValue = nullptr;
    QLabel *m_idleValue = nullptr;
    QLabel *m_busyValue = nullptr;
    QLabel *m_faultValue = nullptr;
    QLabel *m_lastUpdateLabel = nullptr;
    QScrollArea *m_scrollArea = nullptr;
    QWidget *m_cardsContainer = nullptr;
    QVBoxLayout *m_cardsLayout = nullptr;
    QTimer *m_timer = nullptr;
    int m_currentColumnCount = 0;
    bool m_requestInFlight = false;
    bool m_autoRefresh = true;
    bool m_pauseWhenHidden = true;
    bool m_pageVisible = false;
};
