#include "clienthandler.h"
#include "database.h"
#include "protocol.h"

#include <QDateTime>
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

bool ClientHandler::validateAdminSession()
{
    if (!authenticated)
        return false;

    const QDateTime now = QDateTime::currentDateTimeUtc();
    if (!lastActivity.isValid() || lastActivity.secsTo(now) >= 30 * 60) {
        authenticated = false;
        adminId = 0;
        role.clear();
        lastActivity = QDateTime();
        return false;
    }

    if (role != QStringLiteral("admin"))
        return false;

    lastActivity = now;
    return true;
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

    // 统一管理员权限门：登录接口负责建立会话本身，不需要先验证会话。
    if (type.startsWith(QStringLiteral("admin_"))
        && type != MsgType::AdminLogin
        && !validateAdminSession()) {
        reply(makeResponse(type, AuthFailed, "管理员未认证或会话已过期"));
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
        // 登录请求开始时先清除旧会话，避免失败登录沿用此前连接的身份。
        authenticated = false;
        adminId = 0;
        role.clear();
        lastActivity = QDateTime();

        if (code == Protocol::Ok) {
            const qint64 loginAdminId = out.value("id").toVariant().toLongLong();
            const Database::AdminRoleQueryResult roleResult =
                m_db->getAdminRole(loginAdminId);
            if (roleResult.code == Protocol::Ok) {
                adminId = loginAdminId;
                role = roleResult.role;
                lastActivity = QDateTime::currentDateTimeUtc();
                authenticated = true;
            } else {
                code = roleResult.code;
                msg = roleResult.msg;
            }
        }

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
    if (type == MsgType::Reserve) {
        const QJsonObject out = m_db->reserve(
            data.value("user_id").toVariant().toLongLong(),
            data.value("pile_id").toVariant().toLongLong(), code, msg);
        reply(makeResponse(type, code, msg, out));
        return;
    }
    if (type == MsgType::UnfinishedOrder) {
        QJsonObject out;
        out["order"] = m_db->unfinishedOrder(
            data.value("user_id").toVariant().toLongLong(), code, msg);
        reply(makeResponse(type, code, msg, out));
        return;
    }
    if (type == MsgType::StartCharge) {
        const QJsonObject out = m_db->startCharge(
            data.value("order_no").toString(), code, msg);
        reply(makeResponse(type, code, msg, out));
        return;
    }
    if (type == MsgType::Settle) {
        const QJsonObject out = m_db->settle(
            data.value("order_no").toString(),
            data.value("kwh").toDouble(), code, msg);
        reply(makeResponse(type, code, msg, out));
        return;
    }
    if (type == MsgType::Recharge) {
        const QJsonObject out = m_db->recharge(
            data.value("user_id").toVariant().toLongLong(),
            data.value("amount").toDouble(), code, msg);
        reply(makeResponse(type, code, msg, out));
        return;
    }

    // ================= 管理端：用户管理 =================
    if (type == MsgType::AdminUserList) {
        QJsonObject out;
        out["list"] = m_db->adminUserList(data.value("keyword").toString(), code, msg);
        reply(makeResponse(type, code, msg, out));
        return;
    }
    if (type == MsgType::AdminUserFreeze) {
        const QJsonObject out = m_db->adminUserFreeze(
            data.value("user_id").toVariant().toLongLong(),
            data.value("frozen").toBool(), code, msg);
        reply(makeResponse(type, code, msg, out));
        return;
    }

    // ================= 管理端：电桩 / 电站管理 =================
    if (type == MsgType::AdminPileList) {
        QJsonObject out;
        out["list"] = m_db->adminPileList(code, msg);
        reply(makeResponse(type, code, msg, out));
        return;
    }
    if (type == MsgType::AdminPileRestart) {
        const QJsonObject out = m_db->adminPileRestart(
            data.value("pile_id").toVariant().toLongLong(), code, msg);
        reply(makeResponse(type, code, msg, out));
        return;
    }
    if (type == MsgType::AdminStationList) {
        QJsonObject out;
        out["list"] = m_db->adminStationList(code, msg);
        reply(makeResponse(type, code, msg, out));
        return;
    }

    // ================= 其余接口：占位，待各模块负责人实现 =================
    // 实现步骤：① 在 Database 里加对应查询方法；② 在此加一个 if 分支分发。
    reply(makeResponse(type, NotImplemented, "接口尚未实现: " + type));
}
