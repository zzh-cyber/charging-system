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

#include <functional>

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

    // 手机号免密登录；registerMode=true 时不存在则创建，否则返回可注册
    QJsonObject loginOrRegister(const QString &phone, bool registerMode,
                                int &code, QString &msg);

    // 管理员登录
    QJsonObject adminLogin(const QString &username, const QString &password,
                           int &code, QString &msg);

    // 附近充电站列表（根据用户经纬度计算距离并排序）
    QJsonArray stationList(double userLat,
                        double userLng,
                        int &code,
                        QString &msg);


    // 某站电桩列表
    QJsonArray pileList(qint64 stationId, int &code, QString &msg);

    // 预约充电桩，成功时返回订单号
    QJsonObject reserve(qint64 userId, qint64 pileId, int &code, QString &msg);

    // 查询最近一条未完成订单
    QJsonObject unfinishedOrder(qint64 userId, int &code, QString &msg);

    // 开始充电
    QJsonObject startCharge(const QString &orderNo, qint64 userId, int &code, QString &msg);

    // 结束充电出账（不扣款）
    QJsonObject finishCharge(const QString &orderNo, qint64 userId, int &code, QString &msg);

    // 确认支付扣款
    QJsonObject payCharge(const QString &orderNo, qint64 userId, int &code, QString &msg);

    // 用户充值
    QJsonObject recharge(qint64 userId, double amount,int &code, QString &msg);

    // ---- 管理端 ----

    // 用户列表（keyword 空=全部；否则参数化 LIKE 手机号/昵称，带分页）
    QJsonObject adminUserList(const QJsonObject &input, int &code, QString &msg);

    // 冻结/解冻用户（frozen = true 冻结，false 解冻）
    QJsonObject adminUserFreeze(qint64 adminId, qint64 userId, bool frozen, int &code, QString &msg);

    // 全部电桩列表（含所属电站名、累计次数/时长）
    QJsonObject adminPileList(const QJsonObject &input, int &code, QString &msg);
    // 启用电桩状态数量及占比统计
    QJsonObject adminPileStats(int &code, QString &msg);

    // 远程重启：写 device_commands + 桩置 idle + 审计；充电中拒绝
    QJsonObject adminPileRestart(qint64 adminId, qint64 pileId, int &code, QString &msg);

    // 订单列表（筛选+分页，NO.107）
    QJsonObject adminOrderList(const QJsonObject &input, int &code, QString &msg);
    // 订单详情（只读）
    QJsonObject adminOrderDetail(const QString &orderNo, int &code, QString &msg);

    // 电站列表（含桩总数、在线率）
    QJsonArray adminStationList(int &code, QString &msg);
    // 新增电站（管理员，NO.101/102）：入参含 adminId，事务内写审计日志
    QJsonObject adminStationAdd(qint64 adminId, const QJsonObject &input, int &code, QString &msg);
    // 营收趋势（NO.30–33）：返回近 7/30 日按日营收 + 今日/本月/总营收
    QJsonObject revenueTrend(int days, int &code, QString &msg);

    // ---- 高效查询（NO.56）----
    QJsonArray pileUsageStats(int &code, QString &msg);

    // ---- 事务 ----
    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();

    // ---- 结构初始化 ----
    int  schemaVersion();          // 返回当前结构版本（未初始化返回 0）
    bool ensureSchema();           // 缺表则初始化；已有库按 schema_version 增量 ALTER

    // ---- 用户资料维护（NO.51）----
    bool updateNickname(qint64 userId, const QString &nickname);
    bool updateAvatar(qint64 userId, const QString &avatarPath);

    // ---- 充电站管理（NO.53）----
    // 新增电站统一走 adminStationAdd()；此处仅保留更新。
    bool updateStation(qint64 stationId, const QString &name, const QString &address,
                       double lng, double lat, double price);

private:
    bool executeScript(const QString &sql);  // 逐条执行 SQL 脚本
    bool upgradeSchema();                    // 已有库按版本增量升级（禁止 DROP）
    bool columnExists(const QString &table, const QString &column);

    // NO.59：事务包装，死锁(1213)/锁等待超时(1205)时有限重试
    bool runInTransaction(std::function<bool()> body, int maxRetries = 3);

    // NO.58：写操作日志（operation_logs）
    bool logOperation(qint64 adminId, const QString &action, const QString &targetType,
                      qint64 targetId, const QString &beforeValue, const QString &afterValue,
                      const QString &reason = QString());

    QSqlDatabase m_db;
    QString      m_connName;
    QString      m_lastError;
};
