#include "database.h"
#include "config.h"
#include "protocol.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>

Database::Database(const QString &connectionName)
    : m_connName(connectionName)
{
}

Database::~Database()
{
    if (m_db.isOpen())
        m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connName);
}

bool Database::open()
{
    m_db = QSqlDatabase::addDatabase("QMYSQL", m_connName);
    m_db.setHostName(ServerConfig::DbHost);
    m_db.setPort(ServerConfig::DbPort);
    m_db.setDatabaseName(ServerConfig::DbName);
    m_db.setUserName(ServerConfig::DbUser);
    m_db.setPassword(ServerConfig::DbPassword);

    if (!m_db.open()) {
        m_lastError = m_db.lastError().text();
        return false;
    }
    return true;
}

bool Database::isOpen() const
{
    return m_db.isOpen();
}

QJsonObject Database::loginOrRegister(const QString &phone, int &code, QString &msg)
{
    QJsonObject data;

    if (phone.size() != 11) {
        code = Protocol::InvalidRequest;
        msg = "手机号格式不正确";
        return data;
    }

    QSqlQuery q(m_db);
    q.prepare("SELECT id, nickname, avatar, balance, status FROM `user` WHERE phone = ?");
    q.addBindValue(phone);
    if (!q.exec()) {
        code = Protocol::DbError;
        msg = q.lastError().text();
        return data;
    }

    if (q.next()) {
        // 已存在
        const QString status = q.value("status").toString();
        if (status == "frozen") {
            code = Protocol::Frozen;
            msg = "账号已被冻结";
            return data;
        }
        data["id"]       = q.value("id").toLongLong();
        data["phone"]    = phone;
        data["nickname"] = q.value("nickname").toString();
        data["avatar"]   = q.value("avatar").toString();
        data["balance"]  = q.value("balance").toDouble();
        code = Protocol::Ok;
        msg = "登录成功";
        return data;
    }

    // 不存在 → 自动创建，默认昵称"用户+手机后4位"
    const QString nickname = "用户" + phone.right(4);
    QSqlQuery ins(m_db);
    ins.prepare("INSERT INTO `user` (phone, nickname, balance, status) "
                "VALUES (?, ?, 0.00, 'normal')");
    ins.addBindValue(phone);
    ins.addBindValue(nickname);
    if (!ins.exec()) {
        code = Protocol::DbError;
        msg = ins.lastError().text();
        return data;
    }

    data["id"]       = ins.lastInsertId().toLongLong();
    data["phone"]    = phone;
    data["nickname"] = nickname;
    data["avatar"]   = QString();
    data["balance"]  = 0.0;
    code = Protocol::Ok;
    msg = "注册并登录成功";
    return data;
}

QJsonObject Database::adminLogin(const QString &username, const QString &password,
                                 int &code, QString &msg)
{
    QJsonObject data;

    QSqlQuery q(m_db);
    q.prepare("SELECT id, username FROM admin WHERE username = ? AND password = ?");
    q.addBindValue(username);
    q.addBindValue(password);
    if (!q.exec()) {
        code = Protocol::DbError;
        msg = q.lastError().text();
        return data;
    }

    if (!q.next()) {
        code = Protocol::AuthFailed;
        msg = "账号或密码错误";
        return data;
    }

    data["id"]       = q.value("id").toLongLong();
    data["username"] = q.value("username").toString();
    code = Protocol::Ok;
    msg = "登录成功";
    return data;
}

QJsonArray Database::stationList(int &code, QString &msg)
{
    QJsonArray arr;

    QSqlQuery q(m_db);
    const QString sql =
        "SELECT s.id, s.name, s.address, s.longitude, s.latitude, s.price, "
        "  (SELECT COUNT(*) FROM pile p WHERE p.station_id = s.id) AS total, "
        "  (SELECT COUNT(*) FROM pile p WHERE p.station_id = s.id AND p.status = 'idle') AS idle "
        "FROM station s ORDER BY s.id";
    if (!q.exec(sql)) {
        code = Protocol::DbError;
        msg = q.lastError().text();
        return arr;
    }

    while (q.next()) {
        QJsonObject o;
        o["id"]        = q.value("id").toLongLong();
        o["name"]      = q.value("name").toString();
        o["address"]   = q.value("address").toString();
        o["longitude"] = q.value("longitude").toDouble();
        o["latitude"]  = q.value("latitude").toDouble();
        o["price"]     = q.value("price").toDouble();
        o["total"]     = q.value("total").toInt();
        o["idle"]      = q.value("idle").toInt();
        arr.append(o);
    }

    code = Protocol::Ok;
    msg = "ok";
    return arr;
}

QJsonArray Database::pileList(qint64 stationId, int &code, QString &msg)
{
    QJsonArray arr;

    if (stationId <= 0) {
        code = Protocol::InvalidRequest;
        msg = "station_id 参数不正确";
        return arr;
    }

    QSqlQuery q(m_db);
    q.prepare("SELECT id, code, type, power_kw, status "
              "FROM pile WHERE station_id = ? ORDER BY id");
    q.addBindValue(stationId);
    if (!q.exec()) {
        code = Protocol::DbError;
        msg = q.lastError().text();
        return arr;
    }

    while (q.next()) {
        QJsonObject o;
        o["id"]       = q.value("id").toLongLong();
        o["code"]     = q.value("code").toString();
        o["type"]     = q.value("type").toString();
        o["power_kw"] = q.value("power_kw").toDouble();
        o["status"]   = q.value("status").toString();
        arr.append(o);
    }

    code = Protocol::Ok;
    msg = "ok";
    return arr;
}

QJsonObject Database::reserve(qint64 userId, qint64 pileId, int &code, QString &msg)
{
    QJsonObject data;

    if (userId <= 0 || pileId <= 0) {
        code = Protocol::InvalidRequest;
        msg = "user_id 或 pile_id 参数不正确";
        return data;
    }

    if (!m_db.transaction()) {
        code = Protocol::DbError;
        msg = m_db.lastError().text();
        return data;
    }

    const auto fail = [this, &code, &msg, &data](int errorCode,
                                                 const QString &errorMessage) {
        m_db.rollback();
        code = errorCode;
        msg = errorMessage;
        return data;
    };

    QSqlQuery userQuery(m_db);
    userQuery.prepare("SELECT status FROM `user` WHERE id = ? FOR UPDATE");
    userQuery.addBindValue(userId);
    if (!userQuery.exec())
        return fail(Protocol::DbError, userQuery.lastError().text());
    if (!userQuery.next())
        return fail(Protocol::NotFound, "用户不存在");
    if (userQuery.value("status").toString() == "frozen")
        return fail(Protocol::Frozen, "账号已被冻结");

    QSqlQuery orderQuery(m_db);
    orderQuery.prepare("SELECT id FROM charge_order "
                       "WHERE user_id = ? AND status IN ('reserved','charging') "
                       "LIMIT 1 FOR UPDATE");
    orderQuery.addBindValue(userId);
    if (!orderQuery.exec())
        return fail(Protocol::DbError, orderQuery.lastError().text());
    if (orderQuery.next())
        return fail(Protocol::HasUnfinishedOrder, "存在未完成订单");

    QSqlQuery pileQuery(m_db);
    pileQuery.prepare("SELECT status FROM pile WHERE id = ? FOR UPDATE");
    pileQuery.addBindValue(pileId);
    if (!pileQuery.exec())
        return fail(Protocol::DbError, pileQuery.lastError().text());
    if (!pileQuery.next())
        return fail(Protocol::NotFound, "电桩不存在");
    if (pileQuery.value("status").toString() != "idle")
        return fail(Protocol::InvalidRequest, "电桩当前不可预约");

    const QString orderNo = QUuid::createUuid().toString(QUuid::Id128);
    QSqlQuery insertQuery(m_db);
    insertQuery.prepare("INSERT INTO charge_order "
                        "(order_no, user_id, pile_id, status, reserve_time) "
                        "VALUES (?, ?, ?, 'reserved', NOW())");
    insertQuery.addBindValue(orderNo);
    insertQuery.addBindValue(userId);
    insertQuery.addBindValue(pileId);
    if (!insertQuery.exec())
        return fail(Protocol::DbError, insertQuery.lastError().text());

    QSqlQuery updateQuery(m_db);
    updateQuery.prepare("UPDATE pile SET status = 'busy' WHERE id = ? AND status = 'idle'");
    updateQuery.addBindValue(pileId);
    if (!updateQuery.exec())
        return fail(Protocol::DbError, updateQuery.lastError().text());
    if (updateQuery.numRowsAffected() != 1)
        return fail(Protocol::DbError, "更新电桩状态失败");

    if (!m_db.commit()) {
        const QString errorMessage = m_db.lastError().text();
        m_db.rollback();
        code = Protocol::DbError;
        msg = errorMessage;
        return data;
    }

    data["order_no"] = orderNo;
    code = Protocol::Ok;
    msg = "预约成功";
    return data;
}

QJsonObject Database::unfinishedOrder(qint64 userId, int &code, QString &msg)
{
    QJsonObject data;

    if (userId <= 0) {
        code = Protocol::InvalidRequest;
        msg = "user_id 参数不正确";
        return data;
    }

    QSqlQuery q(m_db);
    q.prepare("SELECT order_no, pile_id, status, reserve_time, start_time "
              "FROM charge_order "
              "WHERE user_id = ? AND status IN ('reserved','charging') "
              "ORDER BY id DESC LIMIT 1");
    q.addBindValue(userId);
    if (!q.exec()) {
        code = Protocol::DbError;
        msg = q.lastError().text();
        return data;
    }

    if (!q.next()) {
        code = Protocol::Ok;
        msg = "ok";
        return data;
    }

    data["order_no"]    = q.value("order_no").toString();
    data["pile_id"]     = q.value("pile_id").toLongLong();
    data["status"]      = q.value("status").toString();
    data["reserve_time"] = q.value("reserve_time").toString();
    data["start_time"]   = q.value("start_time").toString();
    code = Protocol::Ok;
    msg = "ok";
    return data;
}

QJsonArray Database::adminUserList(const QString &keyword, int &code, QString &msg)
{
    QJsonArray arr;

    QSqlQuery q(m_db);
    if (keyword.trimmed().isEmpty()) {
        q.prepare("SELECT id, phone, nickname, balance, status, created_at "
                  "FROM `user` ORDER BY id");
    } else {
        q.prepare("SELECT id, phone, nickname, balance, status, created_at "
                  "FROM `user` WHERE phone LIKE ? OR nickname LIKE ? ORDER BY id");
        const QString like = "%" + keyword.trimmed() + "%";
        q.addBindValue(like);
        q.addBindValue(like);
    }
    if (!q.exec()) {
        code = Protocol::DbError;
        msg = q.lastError().text();
        return arr;
    }

    while (q.next()) {
        QJsonObject o;
        o["id"]         = q.value("id").toLongLong();
        o["phone"]      = q.value("phone").toString();
        o["nickname"]   = q.value("nickname").toString();
        o["balance"]    = q.value("balance").toDouble();
        o["status"]     = q.value("status").toString();
        o["created_at"] = q.value("created_at").toString();
        arr.append(o);
    }

    code = Protocol::Ok;
    msg = "ok";
    return arr;
}

QJsonObject Database::adminUserFreeze(qint64 userId, bool frozen, int &code, QString &msg)
{
    QJsonObject data;

    if (userId <= 0) {
        code = Protocol::InvalidRequest;
        msg = "user_id 参数不正确";
        return data;
    }

    QSqlQuery q(m_db);
    q.prepare("UPDATE `user` SET status = ? WHERE id = ?");
    q.addBindValue(frozen ? "frozen" : "normal");
    q.addBindValue(userId);
    if (!q.exec()) {
        code = Protocol::DbError;
        msg = q.lastError().text();
        return data;
    }
    if (q.numRowsAffected() == 0) {
        code = Protocol::NotFound;
        msg = "用户不存在";
        return data;
    }

    data["id"]     = userId;
    data["status"] = frozen ? "frozen" : "normal";
    code = Protocol::Ok;
    msg = frozen ? "已冻结" : "已解冻";
    return data;
}

QJsonArray Database::adminPileList(int &code, QString &msg)
{
    QJsonArray arr;

    QSqlQuery q(m_db);
    const QString sql =
        "SELECT p.id, p.code, s.name AS station, p.type, p.power_kw, p.status, "
        "  p.total_count, p.total_hours "
        "FROM pile p LEFT JOIN station s ON p.station_id = s.id "
        "ORDER BY p.id";
    if (!q.exec(sql)) {
        code = Protocol::DbError;
        msg = q.lastError().text();
        return arr;
    }

    while (q.next()) {
        QJsonObject o;
        o["id"]          = q.value("id").toLongLong();
        o["code"]        = q.value("code").toString();
        o["station"]     = q.value("station").toString();
        o["type"]        = q.value("type").toString();
        o["power_kw"]    = q.value("power_kw").toDouble();
        o["status"]      = q.value("status").toString();
        o["total_count"] = q.value("total_count").toInt();
        o["total_hours"] = q.value("total_hours").toDouble();
        arr.append(o);
    }

    code = Protocol::Ok;
    msg = "ok";
    return arr;
}

QJsonObject Database::adminPileRestart(qint64 pileId, int &code, QString &msg)
{
    QJsonObject data;

    if (pileId <= 0) {
        code = Protocol::InvalidRequest;
        msg = "pile_id 参数不正确";
        return data;
    }

    // 模拟向电桩下发重启指令：将 fault/busy 复位为 idle
    QSqlQuery q(m_db);
    q.prepare("UPDATE pile SET status = 'idle' WHERE id = ?");
    q.addBindValue(pileId);
    if (!q.exec()) {
        code = Protocol::DbError;
        msg = q.lastError().text();
        return data;
    }
    if (q.numRowsAffected() == 0) {
        code = Protocol::NotFound;
        msg = "电桩不存在";
        return data;
    }

    data["id"]     = pileId;
    data["status"] = "idle";
    code = Protocol::Ok;
    msg = "重启成功";
    return data;
}

QJsonArray Database::adminStationList(int &code, QString &msg)
{
    QJsonArray arr;

    QSqlQuery q(m_db);
    const QString sql =
        "SELECT s.id, s.name, s.address, s.longitude, s.latitude, "
        "  (SELECT COUNT(*) FROM pile p WHERE p.station_id = s.id) AS total, "
        "  (SELECT COUNT(*) FROM pile p WHERE p.station_id = s.id AND p.status <> 'fault') AS online "
        "FROM station s ORDER BY s.id";
    if (!q.exec(sql)) {
        code = Protocol::DbError;
        msg = q.lastError().text();
        return arr;
    }

    while (q.next()) {
        const int total  = q.value("total").toInt();
        const int online = q.value("online").toInt();

        QJsonObject o;
        o["id"]          = q.value("id").toLongLong();
        o["name"]        = q.value("name").toString();
        o["address"]     = q.value("address").toString();
        o["longitude"]   = q.value("longitude").toDouble();
        o["latitude"]    = q.value("latitude").toDouble();
        o["total"]       = total;
        o["online_rate"] = total > 0 ? static_cast<double>(online) / total : 0.0;
        arr.append(o);
    }

    code = Protocol::Ok;
    msg = "ok";
    return arr;
}
