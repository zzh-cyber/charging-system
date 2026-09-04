#include "netclient.h"
#include "protocol.h"

#include <QEventLoop>
#include <QTcpSocket>
#include <QTimer>

NetClient::NetClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_reconnectTimer(new QTimer(this))
{
    connect(m_socket, &QTcpSocket::readyRead, this, &NetClient::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &NetClient::onDisconnected);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &NetClient::attemptReconnect);
}

NetClient::~NetClient() = default;

bool NetClient::connectToServer(const QString &host, quint16 port, int timeoutMs)
{
    m_host = host;
    m_port = port;
    if (isConnected())
        return true;
    m_socket->connectToHost(host, port);
    return m_socket->waitForConnected(timeoutMs);
}

bool NetClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void NetClient::setToken(const QString &token)
{
    m_token = token;
}

void NetClient::clearToken()
{
    m_token.clear();
}

QString NetClient::token() const
{
    return m_token;
}

QJsonObject NetClient::withToken(const QJsonObject &request) const
{
    if (m_token.isEmpty() || request.contains(QStringLiteral("token")))
        return request;
    QJsonObject o = request;
    o[QStringLiteral("token")] = m_token;
    return o;
}

void NetClient::send(const QJsonObject &request)
{
    m_socket->write(Protocol::encode(withToken(request)));
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
    while (Protocol::tryDecode(m_buffer, obj)) {
        const int code = obj.value(QStringLiteral("code")).toInt(-1);
        if (code == Protocol::SessionInvalid)
            clearToken();
        emit responseReceived(obj);
        if (code == Protocol::SessionInvalid) {
            const QString msg = obj.value(QStringLiteral("msg")).toString();
            // 延后到当前 request() 的嵌套 QEventLoop 退出之后，避免关窗口时栈上还在用页面对象
            QTimer::singleShot(0, this, [this, msg]() {
                emit sessionInvalid(msg);
            });
        }
    }
}

void NetClient::onDisconnected()
{
    emit disconnected();
    // NO.7：断线自动重连，最多 3 次，间隔递增
    if (m_host.isEmpty())
        return;
    m_reconnectAttempts = 0;
    m_reconnectTimer->start(1000);   // 首次 1 秒后重试
}

void NetClient::attemptReconnect()
{
    if (isConnected()) {
        m_reconnectAttempts = 0;
        emit reconnected();
        return;
    }
    m_socket->abort();
    m_socket->connectToHost(m_host, m_port);
    if (m_socket->waitForConnected(2000)) {
        m_reconnectAttempts = 0;
        emit reconnected();
        return;
    }
    ++m_reconnectAttempts;
    // 递增间隔 1s→2s→4s，之后保持 4s 持续重试，直到连上
    int intervalMs = 4000;
    if (m_reconnectAttempts == 1)
        intervalMs = 1000;
    else if (m_reconnectAttempts == 2)
        intervalMs = 2000;
    m_reconnectTimer->start(intervalMs);
}
