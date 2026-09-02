#include "clienthandler.h"
#include "database.h"
#include "protocol.h"

#include <QDebug>
#include <QJsonObject>
#include <QTcpSocket>
#include <QThread>

ClientHandler::ClientHandler(qintptr socketDescriptor, QObject *parent)
    : QObject(parent)
    , m_descriptor(socketDescriptor)
{
}

ClientHandler::~ClientHandler()
{
    delete m_db;
}

void ClientHandler::start()
{
    m_socket = new QTcpSocket(this);
    if (!m_socket->setSocketDescriptor(m_descriptor)) {
        qWarning() << "setSocketDescriptor failed:" << m_socket->errorString();
        emit finished();
        return;
    }
    connect(m_socket, &QTcpSocket::readyRead, this, &ClientHandler::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &ClientHandler::onDisconnected);

    // 每个线程一个独立数据库连接
    const QString connName =
        QString("conn_%1").arg(reinterpret_cast<quintptr>(QThread::currentThread()));
    m_db = new Database(connName);
    if (!m_db->open())
        qWarning() << "DB open failed on thread:" << m_db->lastError();

    qInfo() << "client connected, thread:" << QThread::currentThread();
}

void ClientHandler::onReadyRead()
{
    m_buffer.append(m_socket->readAll());
    QJsonObject req;
    while (Protocol::tryDecode(m_buffer, req))
        dispatch(req);
}

void ClientHandler::onDisconnected()
{
    qInfo() << "client disconnected, thread:" << QThread::currentThread();
    emit finished();
}

void ClientHandler::reply(const QJsonObject &resp)
{
    if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->write(Protocol::encode(resp));
        m_socket->flush();
    }
}

void ClientHandler::dispatch(const QJsonObject &req)
{
    using namespace Protocol;

    const QString type = req.value("type").toString();
    const QJsonObject data = req.value("data").toObject();

    if (type.isEmpty()) {
        reply(makeResponse(type, InvalidRequest, "缺少 type 字段"));
        return;
    }
    if (!m_db || !m_db->isOpen()) {
        reply(makeResponse(type, DbError, "数据库未连接"));
        return;
    }

    int code = Unknown;
    QString msg = "未知错误";

    // ================= 登录链路样板（已实现，供其他接口照抄） =================
    if (type == MsgType::Login) {
        const QJsonObject out = m_db->loginOrRegister(data.value("phone").toString(), code, msg);
        reply(makeResponse(type, code, msg, out));
        return;
    }
    if (type == MsgType::AdminLogin) {
        const QJsonObject out = m_db->adminLogin(data.value("username").toString(),
                                                 data.value("password").toString(), code, msg);
        reply(makeResponse(type, code, msg, out));
        return;
    }
    if (type == MsgType::StationList) {
        QJsonObject out;
        out["list"] = m_db->stationList(code, msg);
        reply(makeResponse(type, code, msg, out));
        return;
    }
    if (type == MsgType::PileList) {
        QJsonObject out;
        out["list"] = m_db->pileList(data.value("station_id").toVariant().toLongLong(),
                                      code, msg);
        reply(makeResponse(type, code, msg, out));
        return;
    }

    // ================= 其余接口：占位，待各模块负责人实现 =================
    // 实现步骤：① 在 Database 里加对应查询方法；② 在此加一个 if 分支分发。
    reply(makeResponse(type, NotImplemented, "接口尚未实现: " + type));
}
