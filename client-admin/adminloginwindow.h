#pragma once

// ============================================================================
// PC 管理端 - 管理员登录界面（M1 链路样板）
// 默认账号：admin / 123456
// ============================================================================

#include <QWidget>

class QLineEdit;
class QPushButton;
class NetClient;

class AdminLoginWindow : public QWidget
{
    Q_OBJECT
public:
    explicit AdminLoginWindow(QWidget *parent = nullptr);

private slots:
    void onLoginClicked();

private:
    QLineEdit   *m_userEdit;
    QLineEdit   *m_pwdEdit;
    QPushButton *m_loginBtn;
    NetClient   *m_net;
};
