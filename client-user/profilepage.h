#ifndef PROFILEPAGE_H
#define PROFILEPAGE_H

#include <QWidget>

class QLabel;
class QPushButton;
class QDoubleSpinBox;

class ProfilePage : public QWidget
{
    Q_OBJECT

public:
    explicit ProfilePage(
        QWidget *parent = nullptr);

    void setUserInfo(
        const QString &nickname,
        const QString &phone,
        double balance);

    // recharge 成功后调用
    void setBalance(double balance);

signals:
    // 后续 MainWindow 使用现有 recharge 接口处理
    void rechargeRequested(double amount);

private:
    QLabel *m_nicknameLabel = nullptr;
    QLabel *m_phoneLabel = nullptr;
    QLabel *m_balanceLabel = nullptr;
    QLabel *m_tipLabel = nullptr;

    QDoubleSpinBox *m_amountSpin = nullptr;
    QPushButton *m_rechargeButton = nullptr;

    double m_balance = 0.0;
};

#endif // PROFILEPAGE_H
