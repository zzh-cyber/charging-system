#include "database.h"
#include "config.h"
#include "protocol.h"

#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
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

        // 记录最近登录时间
        QSqlQuery touch(m_db);
        touch.prepare("UPDATE `user` SET last_login_at = NOW() WHERE id = ?");
        touch.addBindValue(q.value("id").toLongLong());
        touch.exec();

        code = Protocol::Ok;
        msg = "登录成功";
        return data;
    }

    // 不存在 → 自动创建，默认昵称"用户+手机后4位"
    const QString nickname = "用户" + phone.right(4);
    QSqlQuery ins(m_db);
    ins.prepare("INSERT INTO `user` (phone, nickname, balance, status, last_login_at) "
                "VALUES (?, ?, 0.00, 'normal', NOW())");
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
        "FROM station s WHERE s.enabled = 1 ORDER BY s.id";
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

// ============================================================================
// 事务
// ============================================================================

bool Database::beginTransaction()
{
    return m_db.transaction();
}

bool Database::commitTransaction()
{
    return m_db.commit();
}

bool Database::rollbackTransaction()
{
    return m_db.rollback();
}

// ============================================================================
// 结构初始化：启动时检测 schema 并缺表自动初始化
// ============================================================================

int Database::schemaVersion()
{
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("SELECT MAX(version) FROM schema_version"));
    if (q.next() && !q.value(0).isNull())
        return q.value(0).toInt();
    return 0;
}

bool Database::ensureSchema()
{
    // 核心表已存在 → 视为已初始化，直接返回
    QSqlQuery q(m_db);
    q.exec(QStringLiteral(
        "SELECT COUNT(*) FROM information_schema.tables "
        "WHERE table_schema = DATABASE() AND table_name = 'user'"));
    if (q.next() && q.value(0).toInt() > 0)
        return true;

    // 缺表 → 读取打包进程序的 schema.sql 并执行
    QFile file(QStringLiteral(":/sql/schema.sql"));
    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = QStringLiteral("无法读取 schema.sql");
        return false;
    }
    return executeScript(QString::fromUtf8(file.readAll()));
}

bool Database::executeScript(const QString &sql)
{
    // 去掉 -- 注释行后按分号切分，逐条执行（脚本无存储过程/字符串内分号）
    QStringList cleaned;
    const QStringList lines = sql.split('\n');
    cleaned.reserve(lines.size());
    for (const QString &line : lines) {
        const QString t = line.trimmed();
        if (t.isEmpty() || t.startsWith(QLatin1String("--")))
            continue;
        cleaned << line;
    }

    const QStringList stmts = cleaned.join('\n').split(';', Qt::SkipEmptyParts);
    for (const QString &raw : stmts) {
        const QString s = raw.trimmed();
        if (s.isEmpty() || s.startsWith(QLatin1String("USE ")))
            continue;
        QSqlQuery stmt(m_db);
        if (!stmt.exec(s)) {
            m_lastError = stmt.lastError().text()
                          + QStringLiteral(" (语句: ") + s.left(60) + QStringLiteral(")");
            return false;
        }
    }
    return true;
}

// ============================================================================
// 用户资料维护（NO.51）
// ============================================================================

bool Database::updateNickname(qint64 userId, const QString &nickname)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE `user` SET nickname = ? WHERE id = ?");
    q.addBindValue(nickname);
    q.addBindValue(userId);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return q.numRowsAffected() > 0;
}

bool Database::updateAvatar(qint64 userId, const QString &avatarPath)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE `user` SET avatar = ? WHERE id = ?");
    q.addBindValue(avatarPath);
    q.addBindValue(userId);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return q.numRowsAffected() > 0;
}

// ============================================================================
// 充电站管理（NO.53）
// ============================================================================

bool Database::addStation(const QString &code, const QString &name, const QString &address,
                          double lng, double lat, double price)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO station (station_code, name, address, longitude, latitude, price, enabled) "
              "VALUES (?, ?, ?, ?, ?, ?, 1)");
    q.addBindValue(code);
    q.addBindValue(name);
    q.addBindValue(address);
    q.addBindValue(lng);
    q.addBindValue(lat);
    q.addBindValue(price);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

bool Database::updateStation(qint64 stationId, const QString &name, const QString &address,
                             double lng, double lat, double price)
{
    // updated_at 由 ON UPDATE CURRENT_TIMESTAMP 自动同步
    QSqlQuery q(m_db);
    q.prepare("UPDATE station SET name = ?, address = ?, longitude = ?, latitude = ?, price = ? "
              "WHERE id = ?");
    q.addBindValue(name);
    q.addBindValue(address);
    q.addBindValue(lng);
    q.addBindValue(lat);
    q.addBindValue(price);
    q.addBindValue(stationId);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return q.numRowsAffected() > 0;
}
