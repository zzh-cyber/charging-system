#pragma once

// ============================================================================
// ClientHandler - 单个客户端连接的处理器（运行在独立线程中）
// ----------------------------------------------------------------------------
// 每来一个连接，TcpServer 就创建一个 ClientHandler 并 moveToThread 到新线程，
// 由此实现"多线程并发处理"（对应实训的多线程考点）。
// 处理器在自己的线程里：建 socket、开数据库连接、收包、分发、回包。
// ============================================================================

#include <QByteArray>
#include <QJsonObject>
#include <QObject>

class QTcpSocket;
class Database;

class ClientHandler : public QObject
{
    Q_OBJECT
public:
    explicit ClientHandler(qintptr socketDescriptor, QObject *parent = nullptr);
    ~ClientHandler() override;

public slots:
    void start();   // 线程启动后调用：初始化 socket 与数据库连接

signals:
    void finished(); // 连接结束，通知外层回收线程

private slots:
    void onReadyRead();
    void onDisconnected();

private:
    void dispatch(const QJsonObject &req);   // 按 type 分发
    void reply(const QJsonObject &resp);     // 发送响应

    qintptr     m_descriptor;
    QTcpSocket *m_socket = nullptr;
    Database   *m_db = nullptr;
    QByteArray  m_buffer;
};
