#pragma once

// ============================================================================
// 充电用户端 - 登录/注册界面
// 已注册手机号免密登录；未注册时点「注册」，仍走 login 报文
// （data.register=true）。
// ============================================================================

#include <QJsonObject>
#include <QWidget>

class QLineEdit;
class QPushButton;
class QLabel;
class NetClient;
class QResizeEvent;

class LoginWindow : public QWidget
{
    Q_OBJECT

public:
    explicit LoginWindow(
        QWidget *parent = nullptr);

protected:
    void resizeEvent(
        QResizeEvent *event) override;

private slots:
    void onLoginClicked();
    void onRegisterClicked();

private:
    void applyResponsiveStyle();

    QJsonObject sendLoginRequest(
        const QString &phone,
        bool registerMode);

    void enterMainWindow(
        const QJsonObject &userData);

    QLineEdit *m_phoneEdit = nullptr;

    QPushButton *m_loginBtn = nullptr;
    QPushButton *m_registerBtn = nullptr;

    QLabel *m_hint = nullptr;

    NetClient *m_net = nullptr;
};
