#include "netclient.h"
#include "protocol.h"

#include <QEventLoop>
#include <QTcpSocket>
#include <QTimer>

NetClient::NetClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
{
    connect(m_socket, &QTcpSocket::readyRead, this, &NetClient::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &NetClient::disconnected);
}

NetClient::~NetClient() = default;

bool NetClient::connectToServer(const QString &host, quint16 port, int timeoutMs)
{
    if (isConnected())
        return true;
    m_socket->connectToHost(host, port);
    return m_socket->waitForConnected(timeoutMs);
}

bool NetClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void NetClient::send(const QJsonObject &request)
{
    m_socket->write(Protocol::encode(request));
    m_socket->flush();
}

QJsonObject NetClient::request(const QJsonObject &req, int timeoutMs)
{
    if (!isConnected())
        return Protocol::makeResponse(req.value("type").toString(),
                                      Protocol::Unknown, "未连接到服务器");

    QEventLoop loop;
    QJsonObject result;
    bool got = false;

    auto conn = connect(this, &NetClient::responseReceived, &loop,
                        [&](const QJsonObject &resp) {
                            result = resp;
                            got = true;
                            loop.quit();
                        });

    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);

    send(req);
    loop.exec();
    disconnect(conn);

    if (!got)
        return Protocol::makeResponse(req.value("type").toString(),
                                      Protocol::Unknown, "请求超时");
    return result;
}

void NetClient::onReadyRead()
{
    m_buffer.append(m_socket->readAll());
    QJsonObject obj;
    while (Protocol::tryDecode(m_buffer, obj))
        emit responseReceived(obj);
}
