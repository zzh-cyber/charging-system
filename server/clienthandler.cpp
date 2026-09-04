#include "clienthandler.h"
#include "database.h"
#include "protocol.h"
#include "sessionmanager.h"

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

    Session sess;
    const bool isLogin = (type == MsgType::Login || type == MsgType::AdminLogin);
    if (!isLogin) {
        const QString token = req.value("token").toString();
        if (!SessionManager::instance().validate(token, sess)) {
            reply(makeResponse(type, SessionInvalid, "登录已失效，请重新登录"));
            return;
        }
        const bool adminApi = type.startsWith(QLatin1String("admin_"));
        if (adminApi && sess.role != QLatin1String("admin")) {
            reply(makeResponse(type, SessionInvalid, "需要管理员权限"));
            return;
        }
        if (!adminApi && sess.role != QLatin1String("user")) {
            reply(makeResponse(type, SessionInvalid, "登录已失效，请重新登录"));
            return;
        }
    }

    if (!m_db || !m_db->isOpen()) {
        reply(makeResponse(type, DbError, "数据库未连接"));
        return;
    }

    int code = Unknown;
    QString msg = "未知错误";

    // ================= 登录链路样板（已实现，供其他接口照抄） =================
    if (type == MsgType::Login) {
        QJsonObject out = m_db->loginOrRegister(data.value("phone").toString(), code, msg);
        if (code == Ok) {
            const qint64 uid = out.value("id").toVariant().toLongLong();
            out["token"] = SessionManager::instance().create(uid, QStringLiteral("user"));
        }
        reply(makeResponse(type, code, msg, out));
        return;
    }
    if (type == MsgType::AdminLogin) {
        QJsonObject out = m_db->adminLogin(data.value("username").toString(),
                                           data.value("password").toString(), code, msg);
        if (code == Ok) {
            const qint64 uid = out.value("id").toVariant().toLongLong();
            out["token"] = SessionManager::instance().create(uid, QStringLiteral("admin"));
        }
        reply(makeResponse(type, code, msg, out));
        return;
    }
    if (type == MsgType::StationList) {

    // ------------------------------------------------------------------------
    // 附近充电站查询要求客户端提供当前位置
    // ------------------------------------------------------------------------
        if (!data.contains("lat") ||
            !data.contains("lng") ||
            !data.value("lat").isDouble() ||
            !data.value("lng").isDouble()) {

            reply(
                makeResponse(
                    type,
                    InvalidRequest,
                    "缺少当前位置经纬度"));

            return;
        }

        const double lat =
            data.value("lat").toDouble();

        const double lng =
            data.value("lng").toDouble();

        if (lat < -90.0 ||
            lat > 90.0 ||
            lng < -180.0 ||
            lng > 180.0) {

            reply(
                makeResponse(
                    type,
                    InvalidRequest,
                    "经纬度参数无效"));

            return;
        }

        QJsonObject out;

        out["list"] =
            m_db->stationList(
                lat,
                lng,
                code,
                msg);

        reply(
            makeResponse(
                type,
                code,
                msg,
                out));

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
            sess.userId,
            data.value("pile_id").toVariant().toLongLong(), code, msg);
        reply(makeResponse(type, code, msg, out));
        return;
    }
    if (type == MsgType::UnfinishedOrder) {
        QJsonObject out;
        out["order"] = m_db->unfinishedOrder(sess.userId, code, msg);
        reply(makeResponse(type, code, msg, out));
        return;
    }
    if (type == MsgType::StartCharge) {
        const QJsonObject out = m_db->startCharge(
            data.value("order_no").toString(), sess.userId, code, msg);
        reply(makeResponse(type, code, msg, out));
        return;
    }
    if (type == MsgType::Settle) {
        const QJsonObject out = m_db->settle(
            data.value("order_no").toString(),
            sess.userId,
            data.value("kwh").toDouble(), code, msg);
        reply(makeResponse(type, code, msg, out));
        return;
    }
    if (type == MsgType::Recharge) {
        const QJsonObject out = m_db->recharge(
            sess.userId,
            data.value("amount").toDouble(), code, msg);
        reply(makeResponse(type, code, msg, out));
        return;
    }

    // ------------------------------------------------------------------------
    // 资料维护：改昵称（NO.18/76）。身份取自会话，忽略报文里的 user_id。
    // 头像（NO.17/75）后续在本分支追加 avatar 字段处理。
    // ------------------------------------------------------------------------
    if (type == MsgType::UpdateProfile) {
        const QString nickname = data.value("nickname").toString().trimmed();
        if (nickname.isEmpty()) {
            reply(makeResponse(type, InvalidRequest, "昵称不能为空"));
            return;
        }
        if (nickname.size() < 2 || nickname.size() > 20) {
            reply(makeResponse(type, InvalidRequest, "昵称长度需为 2~20 个字符"));
            return;
        }
        if (!m_db->updateNickname(sess.userId, nickname)) {
            reply(makeResponse(type, DbError, "昵称更新失败: " + m_db->lastError()));
            return;
        }
        QJsonObject out;
        out["nickname"] = nickname;
        reply(makeResponse(type, Ok, "更新成功", out));
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
        const qint64 targetUserId = data.value("user_id").toVariant().toLongLong();
        const bool frozen = data.value("frozen").toBool();
        const QJsonObject out = m_db->adminUserFreeze(targetUserId, frozen, code, msg);
        if (code == Ok && frozen)
            SessionManager::instance().revokeByUser(targetUserId, QStringLiteral("user"));
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
    if (type == MsgType::AdminStationAdd) {
        const QJsonObject out = m_db->adminStationAdd(data, code, msg);
        reply(makeResponse(type, code, msg, out));
        return;
    }
    if (type == QStringLiteral("revenue_trend_query")) {
        Session sess;
        if (!SessionManager::instance().validate(req.value("token").toString(), sess) || sess.role != QLatin1String("admin")) {
            reply(makeResponse(type, SessionInvalid, "登录已失效，请重新登录")); return;
        }
        const QJsonObject out = m_db->revenueTrend(data.value("days").toInt(7), code, msg);
        reply(makeResponse(type, code, msg, out)); return;
    }

    // ================= 其余接口：占位，待各模块负责人实现 =================
    // 实现步骤：① 在 Database 里加对应查询方法；② 在此加一个 if 分支分发。
    reply(makeResponse(type, NotImplemented, "接口尚未实现: " + type));
}
