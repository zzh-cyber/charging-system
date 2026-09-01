#include "tcpserver.h"
#include "clienthandler.h"

#include <QThread>

TcpServer::TcpServer(QObject *parent)
    : QTcpServer(parent)
{
}

void TcpServer::incomingConnection(qintptr socketDescriptor)
{
    auto *thread = new QThread(this);
    auto *handler = new ClientHandler(socketDescriptor);
    handler->moveToThread(thread);

    // 线程启动 → 初始化 handler
    connect(thread, &QThread::started, handler, &ClientHandler::start);
    // 连接结束 → 退出线程并回收资源
    connect(handler, &ClientHandler::finished, thread, &QThread::quit);
    connect(handler, &ClientHandler::finished, handler, &ClientHandler::deleteLater);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);

    thread->start();
}
