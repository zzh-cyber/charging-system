#pragma once

// ============================================================================
// TcpServer - 监听端口并为每个连接派生一个处理线程
// ============================================================================

#include <QTcpServer>

class TcpServer : public QTcpServer
{
    Q_OBJECT
public:
    explicit TcpServer(QObject *parent = nullptr);

protected:
    // 每个新连接都会进入这里；我们为它创建独立线程 + ClientHandler
    void incomingConnection(qintptr socketDescriptor) override;
};
