#pragma once

// ============================================================================
// PC 管理端 - 管理员登录界面
// 默认账号：admin / 123456
// ============================================================================

#include <QWidget>

class QLineEdit;
class QPushButton;
class NetClient;
class AdminMainWindow;

class AdminLoginWindow : public QWidget
{
    Q_OBJECT

public:
    explicit AdminLoginWindow(QWidget *parent = nullptr);
    ~AdminLoginWindow();

private slots:
    void onLoginClicked();
    void onSessionInvalid(const QString &msg);

private:
    QLineEdit *m_userEdit;
    QLineEdit *m_pwdEdit;
    QPushButton *m_loginBtn;
    NetClient *m_net;

    // 登录成功后显示的管理端主窗口
    AdminMainWindow *m_mainWindow;
};