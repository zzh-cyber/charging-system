#ifndef PROFILEPAGE_H
#define PROFILEPAGE_H

#include <QWidget>

class QLabel;

class ProfilePage : public QWidget
{
    Q_OBJECT

public:
    explicit ProfilePage(QWidget *parent = nullptr);

    void setUserInfo(
        const QString &nickname,
        const QString &phone,
        double balance
    );

private:
    QLabel *m_nicknameLabel;
    QLabel *m_phoneLabel;
    QLabel *m_balanceLabel;
};

#endif // PROFILEPAGE_H
