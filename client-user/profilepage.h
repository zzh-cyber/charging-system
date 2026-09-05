#ifndef PROFILEPAGE_H
#define PROFILEPAGE_H

#include <QWidget>

class QLabel;
class QPushButton;
class QDoubleSpinBox;
class QResizeEvent;

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
    void setBalance(
        double balance);

    // update_profile 改昵称成功后调用
    void setNickname(
        const QString &nickname);

protected:
    void resizeEvent(
        QResizeEvent *event) override;

signals:
    // MainWindow 使用现有 recharge 接口处理
    void rechargeRequested(
        double amount);

    // 用户确认新昵称后发出，
    // MainWindow 继续走 update_profile 接口
    void nicknameChangeRequested(
        const QString &nickname);

private:
    void applyResponsiveStyle();

    QLabel *m_nicknameLabel = nullptr;
    QLabel *m_phoneLabel = nullptr;
    QLabel *m_balanceLabel = nullptr;
    QLabel *m_tipLabel = nullptr;

    QDoubleSpinBox *m_amountSpin = nullptr;

    QPushButton *m_rechargeButton = nullptr;
    QPushButton *m_editNickButton = nullptr;

    QString m_nickname;

    double m_balance = 0.0;
};

#endif // PROFILEPAGE_H
