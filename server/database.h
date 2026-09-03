#pragma once

// ============================================================================
// Database - 服务器端数据库访问层（DAO）
// ----------------------------------------------------------------------------
// 重要：QSqlDatabase 不是线程安全的，每个处理线程必须使用自己独立的连接。
//       因此本类在每个 ClientHandler 线程里各创建一个实例，连接名带线程标识。
//
// 本文件目前实现了登录链路样板（loginOrRegister / adminLogin / stationList）。
// 其余接口留给对应同学在此按同样的模式补充。
// ============================================================================

#include <QJsonArray>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QString>

class Database
{
public:
    explicit Database(const QString &connectionName);
    ~Database();

    // 打开连接（读取 ServerConfig 中的配置）
    bool open();
    bool isOpen() const;
    QString lastError() const { return m_lastError; }

    // ---- 业务查询：出参 code/msg 用于返回给客户端 ----

    // 手机号免密登录/注册：存在则返回，不存在则自动创建
    QJsonObject loginOrRegister(const QString &phone, int &code, QString &msg);

    // 管理员登录
    QJsonObject adminLogin(const QString &username, const QString &password,
                           int &code, QString &msg);

    // 充电站列表（含总桩数/空闲数）
    QJsonArray stationList(int &code, QString &msg);

    // 某站电桩列表
    QJsonArray pileList(qint64 stationId, int &code, QString &msg);

    // 预约充电桩，成功时返回订单号
    QJsonObject reserve(qint64 userId, qint64 pileId, int &code, QString &msg);

    // 查询最近一条未完成订单
    QJsonObject unfinishedOrder(qint64 userId, int &code, QString &msg);

    // 开始充电
    QJsonObject startCharge(const QString &orderNo,int &code, QString &msg);

    // 结算订单
    QJsonObject settle(const QString &orderNo, double kwh, int &code, QString &msg);

    // 用户充值
    QJsonObject recharge(qint64 userId, double amount,int &code, QString &msg);

    // ---- 管理端 ----

    // 用户列表（keyword 为空则全部；否则按手机号/昵称模糊搜索）
    QJsonArray adminUserList(const QString &keyword, int &code, QString &msg);

    // 冻结/解冻用户（frozen = true 冻结，false 解冻）
    QJsonObject adminUserFreeze(qint64 userId, bool frozen, int &code, QString &msg);

    // 全部电桩列表（含所属电站名、累计次数/时长）
    QJsonArray adminPileList(int &code, QString &msg);

    // 远程重启电桩：fault/busy → idle，返回新状态
    QJsonObject adminPileRestart(qint64 pileId, int &code, QString &msg);

    // 电站列表（含桩总数、在线率）
    QJsonArray adminStationList(int &code, QString &msg);

private:
    QSqlDatabase m_db;
    QString      m_connName;
    QString      m_lastError;
};
