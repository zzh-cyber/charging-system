#pragma once

// ============================================================================
// NetClient - 客户端网络基类（公共层，用户端 / 管理端共用）
// ----------------------------------------------------------------------------
// 提供两种用法：
//   1) 异步：send() 发送，responseReceived() 信号里处理返回；
//   2) 同步：request() 发送并阻塞等待一条返回（适合顺序式界面逻辑，新手友好）。
// ============================================================================

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

class QTcpSocket;

class NetClient : public QObject
{
    Q_OBJECT
public:
    explicit NetClient(QObject *parent = nullptr);
    ~NetClient() override;

    // 连接服务器，成功返回 true
    bool connectToServer(const QString &host, quint16 port, int timeoutMs = 3000);
    bool isConnected() const;

    // 异步发送
    void send(const QJsonObject &request);

    // 同步请求：发送后阻塞等待一条响应；超时或出错返回空对象（code 字段可判断）
    QJsonObject request(const QJsonObject &req, int timeoutMs = 5000);

signals:
    void responseReceived(const QJsonObject &resp);
    void disconnected();

private slots:
    void onReadyRead();

private:
    QTcpSocket *m_socket;
    QByteArray  m_buffer;
};
