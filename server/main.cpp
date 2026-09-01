#include "config.h"
#include "tcpserver.h"

#include <QCoreApplication>
#include <QDebug>
#include <QHostAddress>
#include <QSqlDatabase>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // 启动前确认 QMYSQL 驱动可用
    if (!QSqlDatabase::isDriverAvailable("QMYSQL")) {
        qCritical() << "QMYSQL 驱动不可用，请先安装 libqt6sql6-mysql";
        return 1;
    }

    TcpServer server;
    if (!server.listen(QHostAddress::Any, ServerConfig::ListenPort)) {
        qCritical() << "监听失败:" << server.errorString();
        return 1;
    }

    qInfo() << "充电系统服务器已启动，端口:" << ServerConfig::ListenPort;
    return app.exec();
}
