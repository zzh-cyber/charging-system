#pragma once

// ============================================================================
// NetClient - 客户端网络基类（公共层，用户端 / 管理端共用）
// ----------------------------------------------------------------------------
// 提供两种用法：
//   1) 异步：send() 发送，responseReceived() 信号里处理返回；
//   2) 同步：request() 发送并阻塞等待一条返回（适合顺序式界面逻辑，新手友好）。
// 另外内置断线自动重连（NO.7）：断开后递增间隔持续重试，直到连上。
// ============================================================================

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

class QTcpSocket;
class QTimer;

class NetClient : public QObject
{
    Q_OBJECT
public:
    explicit NetClient(QObject *parent = nullptr);
    ~NetClient() override;

    // 连接服务器，成功返回 true（会记住 host/port 用于断线重连）
    bool connectToServer(const QString &host, quint16 port, int timeoutMs = 3000);
    bool isConnected() const;

    // 登录成功后把服务器下发的 token 存下来；send/request 会自动附到 JSON 顶层。
    // 未 setToken 时行为与原来完全一致（请求不带 token）。
    void setToken(const QString &token);
    void clearToken();
    QString token() const;

    // 异步发送
    void send(const QJsonObject &request);

    // 同步请求：发送后阻塞等待一条响应；超时或出错返回空对象（code 字段可判断）
    QJsonObject request(const QJsonObject &req, int timeoutMs = 5000);

signals:
    void responseReceived(const QJsonObject &resp);
    void disconnected();
    void reconnected();      // 断线后自动重连成功

private slots:
    void onReadyRead();
    void onDisconnected();
    void attemptReconnect();

private:
    QJsonObject withToken(const QJsonObject &request) const;

    QTcpSocket *m_socket;
    QByteArray  m_buffer;
    QTimer     *m_reconnectTimer;
    QString     m_host;
    QString     m_token;
    quint16     m_port = 0;
    int         m_reconnectAttempts = 0;
};
