#pragma once

#include <QVector>
#include <QWidget>

class NetClient;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTableWidgetItem;

struct ManagedUserItem {
    qint64 id = 0;
    QString phone;
    QString nickname;
    double balance = 0.0;
    QString status;
    QString createdAt;
};

class UserManagerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit UserManagerWidget(NetClient *netClient, QWidget *parent = nullptr);

public slots:
    void loadUsers();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void applyFilters();
    void resetFilters();

private:
    void initUI();
    void populateTable(const QVector<ManagedUserItem> &users);
    void updateSummary();
    void showUserDetails(qint64 userId);
    void setUserFrozen(qint64 userId, bool frozen, QWidget *trigger = nullptr);
    const ManagedUserItem *findUser(qint64 userId) const;
    static QString statusText(const QString &status);
    static QString formatBalance(double balance);
    static QString formatDateTime(const QString &value);
    static QTableWidgetItem *tableItem(const QString &text);

    NetClient *m_net = nullptr;
    QVector<ManagedUserItem> m_users;
    QLineEdit *m_searchEdit = nullptr;
    QComboBox *m_statusFilter = nullptr;
    QPushButton *m_refreshBtn = nullptr;
    QTableWidget *m_table = nullptr;
    QLabel *m_totalValue = nullptr;
    QLabel *m_normalValue = nullptr;
    QLabel *m_frozenValue = nullptr;
    QLabel *m_footerLabel = nullptr;
    QLabel *m_emptyLabel = nullptr;
};
