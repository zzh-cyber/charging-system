#include "database.h"
#include "config.h"
#include "protocol.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QUuid>
#include <QVariant>
#include <QVector>

#include <algorithm>
#include <cmath>


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
    q.prepare("SELECT id, username, password_hash, salt, status FROM admin WHERE username = ?");
    q.addBindValue(username);
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
    if (q.value("status").toString() == "disabled") {
        code = Protocol::AuthFailed;
        msg = "账号已停用";
        return data;
    }

    // SHA256(salt + 密码) 比对，避免明文存储
    const QByteArray hash = QCryptographicHash::hash(
        (q.value("salt").toString() + password).toUtf8(),
        QCryptographicHash::Sha256).toHex();
    if (QString::fromLatin1(hash) != q.value("password_hash").toString()) {
        code = Protocol::AuthFailed;
        msg = "账号或密码错误";
        return data;
    }

    // 记录最近登录时间
    QSqlQuery touch(m_db);
    touch.prepare("UPDATE admin SET last_login_at = NOW() WHERE id = ?");
    touch.addBindValue(q.value("id").toLongLong());
    touch.exec();

    data["id"]       = q.value("id").toLongLong();
    data["username"] = q.value("username").toString();
    code = Protocol::Ok;
    msg = "登录成功";
    return data;
}

QJsonArray Database::stationList(
    double userLat,
    double userLng,
    int &code,
    QString &msg)
{
    QJsonArray arr;

    // ------------------------------------------------------------------------
    // 用户坐标检查
    // ------------------------------------------------------------------------
    if (userLat < -90.0 ||
        userLat > 90.0 ||
        userLng < -180.0 ||
        userLng > 180.0) {

        code =
            Protocol::InvalidRequest;

        msg =
            "用户经纬度参数无效";

        return arr;
    }

    // ------------------------------------------------------------------------
    // 查询充电站
    // ------------------------------------------------------------------------
    QSqlQuery q(m_db);

    const QString sql =
        "SELECT "
        "s.id, "
        "s.name, "
        "s.address, "
        "s.longitude, "
        "s.latitude, "
        "s.price, "

        "(SELECT COUNT(*) "
        " FROM pile p "
        " WHERE p.station_id = s.id) AS total, "

        "(SELECT COUNT(*) "
        " FROM pile p "
        " WHERE p.station_id = s.id "
        " AND p.status = 'idle') AS idle "

        "FROM station s "
        "WHERE s.enabled = 1";

    if (!q.exec(sql)) {

        code =
            Protocol::DbError;

        msg =
            q.lastError().text();

        return arr;
    }

    // ------------------------------------------------------------------------
    // 临时保存：
    // 站点数据 + 距离
    // ------------------------------------------------------------------------
    struct StationItem
    {
        QJsonObject data;
        double distance = 0.0;
    };

    QVector<StationItem> stations;

    constexpr double earthRadiusKm =
        6371.0088;

    auto toRadians =
        [](double degree) {

            constexpr double pi =
                3.14159265358979323846;

            return degree *
                   pi /
                   180.0;
        };

    const double userLatRad =
        toRadians(userLat);

    // ------------------------------------------------------------------------
    // 计算每个站点与用户之间的距离
    // ------------------------------------------------------------------------
    while (q.next()) {

        bool latOk = false;
        bool lngOk = false;

        const double stationLat =
            q.value("latitude")
                .toDouble(&latOk);

        const double stationLng =
            q.value("longitude")
                .toDouble(&lngOk);

        if (!latOk ||
            !lngOk) {

            continue;
        }

        if (stationLat < -90.0 ||
            stationLat > 90.0 ||
            stationLng < -180.0 ||
            stationLng > 180.0) {

            continue;
        }

        // --------------------------------------------------------------------
        // Haversine 球面距离公式
        // --------------------------------------------------------------------
        const double stationLatRad =
            toRadians(
                stationLat);

        const double deltaLat =
            toRadians(
                stationLat -
                userLat);

        const double deltaLng =
            toRadians(
                stationLng -
                userLng);

        const double sinLat =
            std::sin(
                deltaLat /
                2.0);

        const double sinLng =
            std::sin(
                deltaLng /
                2.0);

        double a =
            sinLat * sinLat
            +
            std::cos(userLatRad)
            *
            std::cos(stationLatRad)
            *
            sinLng * sinLng;

        // 防止浮点误差
        if (a < 0.0)
            a = 0.0;

        if (a > 1.0)
            a = 1.0;

        const double c =
            2.0 *
            std::atan2(
                std::sqrt(a),
                std::sqrt(
                    1.0 - a));

        const double distance =
            earthRadiusKm *
            c;

        // --------------------------------------------------------------------
        // 返回数据
        // --------------------------------------------------------------------
        QJsonObject o;

        o["id"] =
            q.value("id")
                .toLongLong();

        o["name"] =
            q.value("name")
                .toString();

        o["address"] =
            q.value("address")
                .toString();

        o["longitude"] =
            stationLng;

        o["latitude"] =
            stationLat;

        o["price"] =
            q.value("price")
                .toDouble();

        o["total"] =
            q.value("total")
                .toInt();

        o["idle"] =
            q.value("idle")
                .toInt();

        // 单位 km
        o["distance"] =
            distance;

        StationItem item;

        item.data =
            o;

        item.distance =
            distance;

        stations.append(
            item);
    }

    // ------------------------------------------------------------------------
    // 按距离从近到远
    // ------------------------------------------------------------------------
    std::sort(
        stations.begin(),
        stations.end(),
        [](const StationItem &a,
           const StationItem &b) {

            return a.distance <
                   b.distance;
        });

    for (const StationItem &item :
         stations) {

        arr.append(
            item.data);
    }

    code =
        Protocol::Ok;

    msg =
        "ok";

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
    pileQuery.prepare("SELECT p.status, p.station_id, s.price "
                      "FROM pile p JOIN station s ON s.id = p.station_id "
                      "WHERE p.id = ? FOR UPDATE");
    pileQuery.addBindValue(pileId);
    if (!pileQuery.exec())
        return fail(Protocol::DbError, pileQuery.lastError().text());
    if (!pileQuery.next())
        return fail(Protocol::NotFound, "电桩不存在");
    if (pileQuery.value("status").toString() != "idle")
        return fail(Protocol::InvalidRequest, "电桩当前不可预约");
    const qint64 stationId = pileQuery.value("station_id").toLongLong();
    const double unitPrice = pileQuery.value("price").toDouble();

    const QString orderNo = QUuid::createUuid().toString(QUuid::Id128);
    QSqlQuery insertQuery(m_db);
    insertQuery.prepare("INSERT INTO charge_order "
                        "(order_no, user_id, station_id, pile_id, status, unit_price, reserve_time) "
                        "VALUES (?, ?, ?, ?, 'reserved', ?, NOW())");
    insertQuery.addBindValue(orderNo);
    insertQuery.addBindValue(userId);
    insertQuery.addBindValue(stationId);
    insertQuery.addBindValue(pileId);
    insertQuery.addBindValue(unitPrice);
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

QJsonObject Database::startCharge(const QString &orderNo, qint64 userId, int &code, QString &msg)
{
    QJsonObject data;
    const QString trimmedOrderNo = orderNo.trimmed();

    if (trimmedOrderNo.isEmpty() || userId <= 0) {
        code = Protocol::InvalidRequest;
        msg = "order_no 参数不正确";
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

    QSqlQuery orderQuery(m_db);
    orderQuery.prepare("SELECT pile_id, status FROM charge_order "
                       "WHERE order_no = ? AND user_id = ? FOR UPDATE");
    orderQuery.addBindValue(trimmedOrderNo);
    orderQuery.addBindValue(userId);
    if (!orderQuery.exec())
        return fail(Protocol::DbError, orderQuery.lastError().text());
    if (!orderQuery.next())
        return fail(Protocol::NotFound, "订单不存在");
    if (orderQuery.value("status").toString() != "reserved")
        return fail(Protocol::InvalidRequest, "订单当前状态不能开始充电");

    const qint64 pileId = orderQuery.value("pile_id").toLongLong();
    QSqlQuery pileQuery(m_db);
    pileQuery.prepare("SELECT status FROM pile WHERE id = ? FOR UPDATE");
    pileQuery.addBindValue(pileId);
    if (!pileQuery.exec())
        return fail(Protocol::DbError, pileQuery.lastError().text());
    if (!pileQuery.next())
        return fail(Protocol::NotFound, "电桩不存在");
    if (pileQuery.value("status").toString() == "fault")
        return fail(Protocol::InvalidRequest, "故障电桩不能开始充电");

    QSqlQuery updateOrder(m_db);
    updateOrder.prepare("UPDATE charge_order SET status = 'charging', start_time = NOW() "
                        "WHERE order_no = ? AND user_id = ? AND status = 'reserved'");
    updateOrder.addBindValue(trimmedOrderNo);
    updateOrder.addBindValue(userId);
    if (!updateOrder.exec())
        return fail(Protocol::DbError, updateOrder.lastError().text());
    if (updateOrder.numRowsAffected() != 1)
        return fail(Protocol::InvalidRequest, "订单当前状态不能开始充电");

    QSqlQuery updatePile(m_db);
    updatePile.prepare("UPDATE pile SET status = 'busy' WHERE id = ?");
    updatePile.addBindValue(pileId);
    if (!updatePile.exec())
        return fail(Protocol::DbError, updatePile.lastError().text());

    QSqlQuery timeQuery(m_db);
    timeQuery.prepare("SELECT start_time FROM charge_order WHERE order_no = ?");
    timeQuery.addBindValue(trimmedOrderNo);
    if (!timeQuery.exec() || !timeQuery.next())
        return fail(Protocol::DbError, timeQuery.lastError().text());

    if (!m_db.commit()) {
        const QString errorMessage = m_db.lastError().text();
        m_db.rollback();
        code = Protocol::DbError;
        msg = errorMessage;
        return data;
    }

    data["start_time"] = timeQuery.value("start_time").toString();
    code = Protocol::Ok;
    msg = "开始充电成功";
    return data;
}

QJsonObject Database::settle(const QString &orderNo, qint64 userId, double kwh, int &code, QString &msg)
{
    QJsonObject data;
    const QString trimmedOrderNo = orderNo.trimmed();

    if (trimmedOrderNo.isEmpty() || userId <= 0 || kwh <= 0) {
        code = Protocol::InvalidRequest;
        msg = "order_no 或 kwh 参数不正确";
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

    QSqlQuery query(m_db);
    query.prepare("SELECT o.id AS order_id, o.user_id, o.pile_id, o.status, o.start_time, "
                  "o.unit_price, u.balance, p.status AS pile_status "
                  "FROM charge_order o "
                  "JOIN `user` u ON u.id = o.user_id "
                  "JOIN pile p ON p.id = o.pile_id "
                  "WHERE o.order_no = ? AND o.user_id = ? FOR UPDATE");
    query.addBindValue(trimmedOrderNo);
    query.addBindValue(userId);
    if (!query.exec())
        return fail(Protocol::DbError, query.lastError().text());
    if (!query.next())
        return fail(Protocol::NotFound, "订单不存在");
    if (query.value("status").toString() != "charging")
        return fail(Protocol::InvalidRequest, "订单当前状态不能结算");
    if (query.value("pile_status").toString() == "fault")
        return fail(Protocol::InvalidRequest, "故障电桩不能结算");

    const double unitPrice = query.value("unit_price").toDouble();
    const double amount = kwh * unitPrice;
    const double balance = query.value("balance").toDouble();
    if (balance < amount)
        return fail(Protocol::InsufficientBalance, "余额不足");

    QSqlQuery updateUser(m_db);
    updateUser.prepare("UPDATE `user` SET balance = balance - ? WHERE id = ?");
    updateUser.addBindValue(amount);
    updateUser.addBindValue(query.value("user_id").toLongLong());
    if (!updateUser.exec() || updateUser.numRowsAffected() != 1)
        return fail(Protocol::DbError, updateUser.lastError().text());

    // 写钱包流水（NO.52：扣款留痕）
    QSqlQuery txn(m_db);
    txn.prepare("INSERT INTO wallet_transactions "
                "(transaction_no, user_id, type, amount, balance_before, balance_after, order_id) "
                "VALUES (?, ?, 'charge_pay', ?, ?, ?, ?)");
    txn.addBindValue(QUuid::createUuid().toString(QUuid::Id128));
    txn.addBindValue(query.value("user_id").toLongLong());
    txn.addBindValue(amount);
    txn.addBindValue(balance);
    txn.addBindValue(balance - amount);
    txn.addBindValue(query.value("order_id").toLongLong());
    if (!txn.exec())
        return fail(Protocol::DbError, txn.lastError().text());

    const qint64 durationSeconds =
        query.value("start_time").toDateTime().secsTo(QDateTime::currentDateTime());

    QSqlQuery updateOrder(m_db);
    updateOrder.prepare("UPDATE charge_order SET status = 'settled', end_time = NOW(), "
                        "kwh = ?, amount = ?, duration_seconds = ?, pay_request_id = ? "
                        "WHERE order_no = ? AND user_id = ? AND status = 'charging'");
    updateOrder.addBindValue(kwh);
    updateOrder.addBindValue(amount);
    updateOrder.addBindValue(durationSeconds);
    updateOrder.addBindValue(QUuid::createUuid().toString(QUuid::WithoutBraces));
    updateOrder.addBindValue(trimmedOrderNo);
    updateOrder.addBindValue(userId);
    if (!updateOrder.exec() || updateOrder.numRowsAffected() != 1)
        return fail(Protocol::DbError, updateOrder.lastError().text());

    QSqlQuery updatePile(m_db);
    updatePile.prepare("UPDATE pile p JOIN charge_order o ON o.pile_id = p.id "
                       "SET p.status = 'idle', p.total_count = p.total_count + 1, "
                       "p.total_hours = p.total_hours + "
                       "TIMESTAMPDIFF(SECOND, o.start_time, NOW()) / 3600.0 "
                       "WHERE p.id = ? AND o.order_no = ?");
    updatePile.addBindValue(query.value("pile_id").toLongLong());
    updatePile.addBindValue(trimmedOrderNo);
    if (!updatePile.exec() || updatePile.numRowsAffected() != 1)
        return fail(Protocol::DbError, updatePile.lastError().text());

    QSqlQuery balanceQuery(m_db);
    balanceQuery.prepare("SELECT balance FROM `user` WHERE id = ?");
    balanceQuery.addBindValue(query.value("user_id").toLongLong());
    if (!balanceQuery.exec() || !balanceQuery.next())
        return fail(Protocol::DbError, balanceQuery.lastError().text());

    if (!m_db.commit()) {
        const QString errorMessage = m_db.lastError().text();
        m_db.rollback();
        code = Protocol::DbError;
        msg = errorMessage;
        return data;
    }

    data["amount"] = amount;
    data["balance"] = balanceQuery.value("balance").toDouble();
    code = Protocol::Ok;
    msg = "结算成功";
    return data;
}

QJsonObject Database::recharge(qint64 userId, double amount, int &code, QString &msg)
{
    QJsonObject data;

    if (userId <= 0 || amount <= 0) {
        code = Protocol::InvalidRequest;
        msg = "user_id 或 amount 参数不正确";
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
    userQuery.prepare("SELECT status, balance FROM `user` WHERE id = ? FOR UPDATE");
    userQuery.addBindValue(userId);
    if (!userQuery.exec())
        return fail(Protocol::DbError, userQuery.lastError().text());
    if (!userQuery.next())
        return fail(Protocol::NotFound, "用户不存在");
    if (userQuery.value("status").toString() == "frozen")
        return fail(Protocol::Frozen, "账号已被冻结，无法充值");
    const double balanceBefore = userQuery.value("balance").toDouble();

    QSqlQuery updateQuery(m_db);
    updateQuery.prepare("UPDATE `user` SET balance = balance + ? WHERE id = ?");
    updateQuery.addBindValue(amount);
    updateQuery.addBindValue(userId);
    if (!updateQuery.exec() || updateQuery.numRowsAffected() != 1)
        return fail(Protocol::DbError, updateQuery.lastError().text());

    // 写钱包流水（NO.52：余额变化必须留痕）
    QSqlQuery insertQuery(m_db);
    insertQuery.prepare("INSERT INTO wallet_transactions "
                        "(transaction_no, user_id, type, amount, balance_before, balance_after) "
                        "VALUES (?, ?, 'recharge', ?, ?, ?)");
    insertQuery.addBindValue(QUuid::createUuid().toString(QUuid::Id128));
    insertQuery.addBindValue(userId);
    insertQuery.addBindValue(amount);
    insertQuery.addBindValue(balanceBefore);
    insertQuery.addBindValue(balanceBefore + amount);
    if (!insertQuery.exec())
        return fail(Protocol::DbError, insertQuery.lastError().text());

    QSqlQuery balanceQuery(m_db);
    balanceQuery.prepare("SELECT balance FROM `user` WHERE id = ?");
    balanceQuery.addBindValue(userId);
    if (!balanceQuery.exec() || !balanceQuery.next())
        return fail(Protocol::DbError, balanceQuery.lastError().text());

    if (!m_db.commit()) {
        const QString errorMessage = m_db.lastError().text();
        m_db.rollback();
        code = Protocol::DbError;
        msg = errorMessage;
        return data;
    }

    data["balance"] = balanceQuery.value("balance").toDouble();
    code = Protocol::Ok;
    msg = "充值成功";
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

QJsonObject Database::adminUserFreeze(qint64 adminId, qint64 userId, bool frozen, int &code, QString &msg)
{
    QJsonObject data;

    if (userId <= 0) {
        code = Protocol::InvalidRequest;
        msg = "user_id 参数不正确";
        return data;
    }

    const QString newStatus = frozen ? "frozen" : "normal";

    if (!m_db.transaction()) {
        code = Protocol::DbError;
        msg = m_db.lastError().text();
        return data;
    }

    // 查旧状态（日志 before_value 用）
    QSqlQuery sel(m_db);
    sel.prepare("SELECT status FROM `user` WHERE id = ?");
    sel.addBindValue(userId);
    if (!sel.exec()) {
        m_db.rollback();
        code = Protocol::DbError;
        msg = sel.lastError().text();
        return data;
    }
    if (!sel.next()) {
        m_db.rollback();
        code = Protocol::NotFound;
        msg = "用户不存在";
        return data;
    }
    const QString oldStatus = sel.value("status").toString();

    // 更新状态
    QSqlQuery q(m_db);
    q.prepare("UPDATE `user` SET status = ? WHERE id = ?");
    q.addBindValue(newStatus);
    q.addBindValue(userId);
    if (!q.exec()) {
        m_db.rollback();
        code = Protocol::DbError;
        msg = q.lastError().text();
        return data;
    }

    // 写操作日志（NO.58）
    if (!logOperation(adminId, frozen ? "freeze_user" : "unfreeze_user",
                      "user", userId, oldStatus, newStatus)) {
        m_db.rollback();
        code = Protocol::DbError;
        msg = m_lastError;
        return data;
    }

    if (!m_db.commit()) {
        m_db.rollback();
        code = Protocol::DbError;
        msg = m_db.lastError().text();
        return data;
    }

    data["id"]     = userId;
    data["status"] = newStatus;
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

QJsonObject Database::adminPileRestart(qint64 adminId, qint64 pileId, int &code, QString &msg)
{
    QJsonObject data;

    if (pileId <= 0) {
        code = Protocol::InvalidRequest;
        msg = "pile_id 参数不正确";
        return data;
    }

    if (!m_db.transaction()) {
        code = Protocol::DbError;
        msg = m_db.lastError().text();
        return data;
    }

    // 确认电桩存在并记录旧状态（日志 before_value 用）
    QSqlQuery existsQuery(m_db);
    existsQuery.prepare("SELECT status FROM pile WHERE id = ? LIMIT 1");
    existsQuery.addBindValue(pileId);
    if (!existsQuery.exec()) {
        m_db.rollback();
        code = Protocol::DbError;
        msg = existsQuery.lastError().text();
        return data;
    }
    if (!existsQuery.next()) {
        m_db.rollback();
        code = Protocol::NotFound;
        msg = "电桩不存在";
        return data;
    }
    const QString oldStatus = existsQuery.value("status").toString();

    // 写设备指令（模拟重启，直接标记成功）
    QSqlQuery cmd(m_db);
    cmd.prepare("INSERT INTO device_commands (command_no, pile_id, command, status, response_at) "
                "VALUES (?, ?, 'restart', 'success', NOW())");
    cmd.addBindValue(QUuid::createUuid().toString(QUuid::WithoutBraces));
    cmd.addBindValue(pileId);
    if (!cmd.exec()) {
        m_db.rollback();
        code = Protocol::DbError;
        msg = cmd.lastError().text();
        return data;
    }

    // 模拟重启：恢复为 idle
    QSqlQuery q(m_db);
    q.prepare("UPDATE pile SET status = 'idle' WHERE id = ?");
    q.addBindValue(pileId);
    if (!q.exec()) {
        m_db.rollback();
        code = Protocol::DbError;
        msg = q.lastError().text();
        return data;
    }

    // 写操作日志（NO.58）
    if (!logOperation(adminId, "restart_pile", "pile", pileId, oldStatus, "idle")) {
        m_db.rollback();
        code = Protocol::DbError;
        msg = m_lastError;
        return data;
    }

    if (!m_db.commit()) {
        m_db.rollback();
        code = Protocol::DbError;
        msg = m_db.lastError().text();
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
    // exec 成功即算成功：昵称与原值相同时 numRowsAffected() 为 0，不应视为失败。
    // userId 来自已校验的会话，必然存在，无需再用行数判断存在性。
    return true;
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

bool Database::addStation(qint64 adminId, const QString &code, const QString &name, const QString &address,
                          double lng, double lat, double price)
{
    if (!m_db.transaction()) {
        m_lastError = m_db.lastError().text();
        return false;
    }

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
        m_db.rollback();
        return false;
    }
    const qint64 stationId = q.lastInsertId().toLongLong();

    // 写操作日志（NO.58）
    if (!logOperation(adminId, "add_station", "station", stationId, "", code)) {
        m_db.rollback();
        return false;
    }

    if (!m_db.commit()) {
        m_lastError = m_db.lastError().text();
        m_db.rollback();
        return false;
    }
    return true;
}

// ============================================================================
// 操作日志（NO.58）
// ============================================================================

bool Database::logOperation(qint64 adminId, const QString &action, const QString &targetType,
                            qint64 targetId, const QString &beforeValue, const QString &afterValue,
                            const QString &reason)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO operation_logs "
              "(admin_id, action, target_type, target_id, before_value, after_value, result, reason) "
              "VALUES (?, ?, ?, ?, ?, ?, 'success', ?)");
    // 空字符串统一转成 "" 再绑定，避免 null QString 被绑成 NULL 触发 NOT NULL 约束
    const auto emptyIfNull = [](const QString &s) {
        return s.isNull() ? QStringLiteral("") : s;
    };
    q.addBindValue(adminId);
    q.addBindValue(action);
    q.addBindValue(targetType);
    q.addBindValue(targetId);
    q.addBindValue(emptyIfNull(beforeValue));
    q.addBindValue(emptyIfNull(afterValue));
    q.addBindValue(emptyIfNull(reason));
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
