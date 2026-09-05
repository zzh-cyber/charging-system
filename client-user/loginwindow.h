#pragma once

// ============================================================================
// 充电用户端 - 登录/注册界面（M1 链路样板）
// 手机号免密登录：输入 11 位手机号 → 服务器不存在则自动注册 → 返回用户信息。
// ============================================================================

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
    explicit LoginWindow(QWidget *parent = nullptr);

protected:
    void resizeEvent(
        QResizeEvent *event) override;

private slots:
    void onLoginClicked();
    void onRegisterClicked();


private:
    void applyResponsiveStyle();
    QLineEdit   *m_phoneEdit;
    QPushButton *m_loginBtn;
    QLabel      *m_hint;
    NetClient   *m_net;
    QPushButton *m_registerBtn = nullptr;

QJsonObject sendLoginRequest(
    const QString &phone,
    bool registerMode);

};
