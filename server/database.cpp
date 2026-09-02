#include "database.h"
#include "config.h"
#include "protocol.h"

#include <QSqlError>
#include <QSqlQuery>
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
