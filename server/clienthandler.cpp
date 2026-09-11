#include "clienthandler.h"
#include "aiservice.h"
#include "database.h"
#include "protocol.h"
#include "sessionmanager.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QMutex>
#include <QMutexLocker>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QTcpSocket>
#include <QThread>


// ============================================================================
// AI 代预约会话上下文（跨多次 ai_chat 消息；多线程访问用互斥锁保护）
//   STEP_NONE        无挂起，普通对话
//   STEP_ASK_STATION 用户说"帮我预约"但还没给充电站，等他补站名
//   STEP_ASK_PILE    站已确定、已把空闲桩列给用户，等他从里面挑（方案B：先列后约）
// ============================================================================
namespace {
const int STEP_NONE = 0;
const int STEP_ASK_STATION = 1;
const int STEP_ASK_PILE = 2;

// 一个空闲充电桩的候选信息（列表展示 + 定位用）
struct PileOpt {
    int     no = -1;      // 站内序号（code 末段数字），对应"第X号桩"
    qint64  id = -1;      // 主键
    QString code;         // 形如 "SZ001-01"
    QString type;         // fast / slow
    double  power = 0;    // 功率 kW
};

// 某用户进行到一半的"AI 代预约"会话
struct AiReserveCtx {
    int     step = STEP_NONE;
    qint64  stationId = -1;
    QString stationName;  // 已确认的电站全名（话术展示用）
};

// "帮我预约最近的充电站"：按距离找最近且有空闲桩的站
struct NearestPick {
    bool    found = false;        // 找到可预约的最近站
    bool    anyStation = false;   // 至少能查到站点（区分"查询失败"与"全都没空桩"）
    qint64  id = -1;
    QString name;
    double  distanceKm = -1.0;
};

QHash<qint64, AiReserveCtx> g_reserveCtx;
QMutex g_reserveMutex;

bool containsAny(const QString &text, const QList<const char *> &kws)
{
    for (const char *kw : kws)
        if (text.contains(QString::fromUtf8(kw)))
            return true;
    return false;
}

// 请求日志用：列出 data 字段，脱敏手机号/密码，长文本截断
QString summarizeRequestData(const QJsonObject &data)
{
    QStringList parts;
    const QStringList keys = data.keys();
    for (const QString &key : keys) {
        if (key == QLatin1String("password") || key == QLatin1String("token")) {
            parts << key + QStringLiteral("=*");
            continue;
        }
        const QJsonValue value = data.value(key);
        QString text;
        if (value.isString())
            text = value.toString();
        else if (value.isDouble())
            text = QString::number(value.toDouble());
        else if (value.isBool())
            text = value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        else if (value.isArray())
            text = QStringLiteral("[%1]").arg(value.toArray().size());
        else if (value.isObject())
            text = QStringLiteral("{...}");
        else
            continue;

        if (key == QLatin1String("phone") && text.size() >= 7)
            text = text.left(3) + QStringLiteral("****") + text.right(4);
        if (text.size() > 48)
            text = text.left(48) + QStringLiteral("…");
        parts << key + QLatin1Char('=') + text;
    }
    return parts.join(QLatin1Char(' '));
}
} // namespace


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
        emit finished();//setSocketDescriptor：把已经 accept 的连接接到这个线程的 socket 上
        return;
    }
    connect(m_socket, &QTcpSocket::readyRead, this, &ClientHandler::onReadyRead);//readyRead：有数据可读时触发
    connect(m_socket, &QTcpSocket::disconnected, this, &ClientHandler::onDisconnected);//disconnected：连接断开时触发

    // 每个线程一个独立数据库连接
    const QString connName =
        QString("conn_%1").arg(reinterpret_cast<quintptr>(QThread::currentThread()));
    m_db = new Database(connName);
    if (!m_db->open())
        qWarning() << "DB open failed on thread:" << m_db->lastError();

    qInfo() << "client connected, thread:" << QThread::currentThread();
}

void ClientHandler::onReadyRead()//有数据可读时触发
{
    m_buffer.append(m_socket->readAll());
    QJsonObject req;
    while (Protocol::tryDecode(m_buffer, req))
        dispatch(req);
}

void ClientHandler::onDisconnected()//连接断开时触发
{
    qInfo() << "client disconnected, thread:" << QThread::currentThread();
    emit finished();
}

void ClientHandler::reply(const QJsonObject &resp)
{
    qInfo().noquote()
        << QStringLiteral("[send] type=%1 code=%2 msg=%3")
               .arg(resp.value(QStringLiteral("type")).toString(),
                    QString::number(resp.value(QStringLiteral("code")).toInt()),
                    resp.value(QStringLiteral("msg")).toString());

    if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->write(Protocol::encode(resp));
        m_socket->flush();
    }
}

void ClientHandler::dispatch(const QJsonObject &req)//分发请求，后端消息处理
{
    using namespace Protocol;

    const QString type = req.value("type").toString();
    const QJsonObject data = req.value("data").toObject();
    const QString dataSummary = summarizeRequestData(data);
    qInfo().noquote()
        << QStringLiteral("[recv] type=%1%2")
               .arg(type,
                    dataSummary.isEmpty()
                        ? QString()
                        : (QStringLiteral(" ") + dataSummary));

    if (type.isEmpty()) {
        reply(makeResponse(type, InvalidRequest, "缺少 type 字段"));
        return;
    }
    //鉴权门：验证 token 是否有效，并获取用户信息
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
        qInfo().noquote()
            << QStringLiteral("[auth] userId=%1 role=%2")
                   .arg(sess.userId)
                   .arg(sess.role);
    }

    if (!m_db || !m_db->isOpen()) {
        reply(makeResponse(type, DbError, "数据库未连接"));
        return;
    }

    int code = Unknown;
    QString msg = "未知错误";

    // ================= 登录链路样板（已实现，供其他接口照抄） =================
    // 登录：登录或注册，这里才发 token
    //只有 login、admin_login 不带 token。
    if (type == MsgType::Login) {
        QJsonObject out = m_db->loginOrRegister(//loginOrRegister：查/建 user 表，冻结返回 code=6，不建会话。
            data.value("phone").toString(),
            data.value("register").toBool(),
            code, msg);
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
    if (type == MsgType::FinishCharge) {
        const QJsonObject out = m_db->finishCharge(
            data.value("order_no").toString(), sess.userId, code, msg);
        reply(makeResponse(type, code, msg, out));
        return;
    }
    if (type == MsgType::PayCharge) {
        const QJsonObject out = m_db->payCharge(
            data.value("order_no").toString(), sess.userId, code, msg);
        reply(makeResponse(type, code, msg, out));
        return;
    }
    if (type == MsgType::Settle) {
        reply(makeResponse(
            type,
            InvalidRequest,
            "settle 已停用，请使用 finish_charge 结束充电，再使用 pay_charge 确认支付"));
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
    // AI 客服（方案 B）：调用外部大模型，返回答案
    // ------------------------------------------------------------------------
    if (type == MsgType::AiChat) {
        const QString question = data.value(QStringLiteral("question")).toString().trimmed();
        if (question.isEmpty()) {
            reply(makeResponse(type, InvalidRequest, QStringLiteral("问题不能为空")));
            return;
        }
        // 纯自研动作路由：先尝试"AI 代执行"（如帮用户预约电桩）。
        // 命中操作时直接返回结果，不再走文本问答。
        // 客户端若已定位，会带上 lat/lng，供"最近的充电站"检索使用。
        const bool hasLoc = data.contains(QStringLiteral("lat"))
            && data.value(QStringLiteral("lat")).isDouble()
            && data.contains(QStringLiteral("lng"))
            && data.value(QStringLiteral("lng")).isDouble();
        const double userLat = hasLoc ? data.value(QStringLiteral("lat")).toDouble() : 0.0;
        const double userLng = hasLoc ? data.value(QStringLiteral("lng")).toDouble() : 0.0;

        QJsonObject action;
        const QString acted = tryAiAction(sess.userId, question, action,
                                          userLat, userLng, hasLoc);
        if (!acted.isEmpty()) {
            QJsonObject out;
            out[QStringLiteral("answer")] = acted;
            // 动作成功（预约成功）时把结构化结果也带回，客户端据此把"充电"页切到"已预约"
            const QString ordNo = action.value(QStringLiteral("order_no")).toString();
            if (!ordNo.isEmpty()) {
                out[QStringLiteral("action")] = QStringLiteral("reserve_ok");
                out[QStringLiteral("order_no")] = ordNo;
                out[QStringLiteral("station_name")] =
                    action.value(QStringLiteral("station_name")).toString();
                out[QStringLiteral("pile_code")] =
                    action.value(QStringLiteral("pile_code")).toString();
            }
            reply(makeResponse(type, Protocol::Ok, QStringLiteral("ok"), out));
            return;
        }

        AiService ai;
        QString err;
        const QString answer = ai.ask(sess.userId, question, &err);
        if (answer.isEmpty()) {
            reply(makeResponse(type, Unknown, QStringLiteral("AI 客服暂不可用：") + err));
            return;
        }
        QJsonObject out;
        out[QStringLiteral("answer")] = answer;
        reply(makeResponse(type, Ok, QStringLiteral("ok"), out));
        return;
    }

    // ------------------------------------------------------------------------
    // 资料维护：改昵称（NO.18/76）和/或头像（NO.17/75）。
    // 身份取自会话，忽略报文里的 user_id。可只传 nickname、只传 avatar，或两者都传。
    // avatar 为路径/标识字符串（库字段 VARCHAR(255)），不接收图片二进制。
    // ------------------------------------------------------------------------
    //改资料：改昵称和/或头像
    if (type == MsgType::UpdateProfile) {
        const bool hasNickname = data.contains(QStringLiteral("nickname"));
        const bool hasAvatar = data.contains(QStringLiteral("avatar"));
        if (!hasNickname && !hasAvatar) {
            reply(makeResponse(type, InvalidRequest, "请提供 nickname 或 avatar"));
            return;
        }

        QString nickname;
        if (hasNickname) {
            nickname = data.value(QStringLiteral("nickname")).toString().trimmed();
            if (nickname.isEmpty()) {
                reply(makeResponse(type, InvalidRequest, "昵称不能为空"));
                return;
            }
            if (nickname.size() < 2 || nickname.size() > 20) {
                reply(makeResponse(type, InvalidRequest, "昵称长度需为 2~20 个字符"));
                return;
            }
        }

        QString avatar;
        if (hasAvatar) {
            const QJsonValue avatarVal = data.value(QStringLiteral("avatar"));
            if (!avatarVal.isString() && !avatarVal.isNull()) {
                reply(makeResponse(type, InvalidRequest, "avatar 须为字符串"));
                return;
            }
            avatar = avatarVal.toString().trimmed();
            if (avatar.size() > 255) {
                reply(makeResponse(type, InvalidRequest, "头像路径最长 255 个字符"));
                return;
            }
        }

        if (hasNickname && !m_db->updateNickname(sess.userId, nickname)) {
            reply(makeResponse(type, DbError, "昵称更新失败: " + m_db->lastError()));
            return;
        }
        if (hasAvatar && !m_db->updateAvatar(sess.userId, avatar)) {
            reply(makeResponse(type, DbError, "头像更新失败: " + m_db->lastError()));
            return;
        }

        QJsonObject out;
        if (hasNickname)
            out[QStringLiteral("nickname")] = nickname;
        if (hasAvatar)
            out[QStringLiteral("avatar")] = avatar;
        reply(makeResponse(type, Ok, "更新成功", out));
        return;
    }

    // ================= 管理端：用户管理 =================
        //列表类：把整个 data 交给 DAO（分页、keyword 在里面），例如 adminUserList(data)、adminPileList(data)、adminOrderList(data)
    if (type == MsgType::AdminUserList) {
        const QJsonObject out = m_db->adminUserList(data, code, msg);
        reply(makeResponse(type, code, msg, out));
        return;
    }
    if (type == MsgType::AdminUserFreeze) {
        const qint64 targetUserId = data.value("user_id").toVariant().toLongLong();
        const bool frozen = data.value("frozen").toBool();
        const QJsonObject out = m_db->adminUserFreeze(sess.userId, targetUserId, frozen, code, msg);
        if (code == Ok && frozen)
            SessionManager::instance().revokeByUser(targetUserId, QStringLiteral("user"));
        reply(makeResponse(type, code, msg, out));
        return;
    }

    // ================= 管理端：电桩 / 电站管理 =================
    if (type == MsgType::AdminPileList) {
        QJsonObject out;
        out = m_db->adminPileList(data, code, msg);
        if (code == Ok) {
            int statsCode = Ok;
            QString statsMsg;
            const QJsonObject stats = m_db->adminPileStats(statsCode, statsMsg);
            if (statsCode == Ok)
                out["stats"] = stats;
            else
                qWarning() << "admin_pile_list stats failed:" << statsMsg;
        }
        reply(makeResponse(type, code, msg, out));
        return;
    }
    if (type == MsgType::AdminPileRestart) {
        const QJsonObject out = m_db->adminPileRestart(
            sess.userId, data.value("pile_id").toVariant().toLongLong(), code, msg);
        reply(makeResponse(type, code, msg, out));
        return;
    }
    if (type == MsgType::AdminOrderList) {
        const QJsonObject out = m_db->adminOrderList(data, code, msg);
        reply(makeResponse(type, code, msg, out));
        return;
    }
    if (type == MsgType::AdminOrderDetail) {
        const QJsonObject out = m_db->adminOrderDetail(
            data.value("order_no").toString(), code, msg);
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
        // 身份取自会话（已过管理员鉴权门），加站与写审计日志在同一事务内完成
        const QJsonObject out = m_db->adminStationAdd(sess.userId, data, code, msg);
        reply(makeResponse(type, code, msg, out));
        return;
    }
    if (type == MsgType::AdminRevenueTrend) {
        // 管理员角色已由全局鉴权门校验（admin_ 前缀），此处直接查询
        const QJsonObject out = m_db->revenueTrend(data.value("days").toInt(7), code, msg);
        reply(makeResponse(type, code, msg, out));
        return;
    }

    // ================= 其余接口：占位，待各模块负责人实现 =================
    // 实现步骤：① 在 Database 里加对应查询方法；② 在此加一个 if 分支分发。
    reply(makeResponse(type, NotImplemented, "接口尚未实现: " + type));
}

// ============================================================================
// tryAiAction - 纯自研"AI 代执行"（路线二：不依赖外部大模型执行操作）
// ----------------------------------------------------------------------------
// 用关键词 + 全站名枚举把用户口语解析成预约动作，直接调用业务层 Database::reserve。
// 支持多轮：第一句只说"帮我预约"未给电站时，把 userId 挂起到 g_pendingReserveUsers；
// 下一条消息即便不含"预约"也能补全电站名直接执行。"算了/取消/不预约了"会清空挂起。
// 返回非空 = 已处理（成功/失败提示/引导追问都算处理）；返回空 = 未命中操作，
// 由调用方回落到普通文本问答。
// ============================================================================
QString ClientHandler::tryAiAction(qint64 userId, const QString &question,
                                   QJsonObject &actionOut,
                                   double userLat, double userLng,
                                   bool hasLocation)
{
    const QString q = question.trimmed();

    // ---- 1) 咨询类句子：不触碰预约，回落文本问答 ----
    if (containsAny(q, { "怎么", "如何", "怎样", "流程", "可以吗", "能吗",
                         "是啥", "什么", "哪些", "介绍", "？", "?" }))
        return {};

    const bool wantsReserve = containsAny(q, { "预约", "帮我订", "订桩", "占个桩" });
    const bool wantsCancel  = containsAny(q, { "算了", "不用了", "取消", "别预约", "不约了", "不约吧", "再想想" });

    // ==================== 工具小函数 ====================
    // 桩 code 形如 "XXX-01"，末段数字即站内序号
    auto noOf = [](const QString &c) -> int {
        const int dash = c.lastIndexOf('-');
        if (dash >= 0) {
            bool ok = false;
            const int n = c.mid(dash + 1).toInt(&ok);
            if (ok)
                return n;
        }
        return -1;
    };
    auto kindText = [](const QString &type) -> QString {
        return type == QStringLiteral("slow") ? QStringLiteral("慢充")
                                              : QStringLiteral("快充");
    };
    // 全站中文名去尾缀得别名（如"深圳市民中心充电站" -> "深圳市民中心"）
    auto stripTail = [](QString n) -> QString {
        const char *const tails[] = { "电动汽车充电站", "充电站", "换电站", "电站" };
        for (const char *t : tails) {
            const QString tail = QString::fromUtf8(t);
            if (n.endsWith(tail))
                n.chop(tail.size());
        }
        return n;
    };
    // 从句子解析出电站全名；命中返回 true 并给出 stationId/全名
    auto matchStation = [&](const QString &text, qint64 &sid, QString &fullName) -> bool {
        int rc = Protocol::Unknown;
        QString em;
        const QJsonArray stations = m_db->stationNameIndex(rc, em);
        int best = -1;
        bool hit = false;
        for (const auto &v : stations) {
            const QJsonObject s = v.toObject();
            const QString full = s.value(QStringLiteral("name")).toString();
            if (full.isEmpty())
                continue;
            const QString alias = stripTail(full);
            int len = -1;
            if (text.contains(full))
                len = full.size();
            else if (text.contains(alias))
                len = alias.size();
            if (len > best) {
                best = len;
                sid = s.value(QStringLiteral("id")).toVariant().toLongLong();
                fullName = full;
                hit = true;
            }
        }
        return hit;
    };
    // 两个文本共用的不重复汉字数（站名模糊匹配用）
    auto sharedScore = [](const QString &a, const QString &b) -> int {
        QSet<QChar> inA;
        for (const QChar ch : a)
            inA.insert(ch);
        QSet<QChar> seen;
        int n = 0;
        for (const QChar ch : b)
            if (inA.contains(ch) && !seen.contains(ch)) {
                ++n;
                seen.insert(ch);
            }
        return n;
    };
    // 在平台全部站点里找与给定文本最"像"的一个（≥2 个共同字才返回，否则空）
    auto suggestStation = [&](const QString &text) -> QString {
        int rc = Protocol::Unknown;
        QString em;
        const QJsonArray stations = m_db->stationNameIndex(rc, em);
        QString bestFull;
        int best = 0;
        for (const auto &v : stations) {
            const QJsonObject s = v.toObject();
            const QString full = s.value(QStringLiteral("name")).toString();
            if (full.isEmpty())
                continue;
            const int score = sharedScore(text, stripTail(full));
            if (score > best) {
                best = score;
                bestFull = full;
            }
        }
        return best >= 2 ? bestFull : QString();
    };
    // 去掉桩号、预约/语气词，尽量只留"地名+充电站"，用于没找到站时的提示文案
    auto prettyStation = [](const QString &text) -> QString {
        QString out = text;
        out.remove(QRegularExpression(QStringLiteral("第?\\s*[0-9]+\\s*号\\s*(充电桩|电桩|桩)?")));
        const char *const fillers[] = { "帮我预约", "我想预约", "我要预约", "帮忙预约",
                                        "麻烦预约", "帮我订一下", "帮我订", "预约一下",
                                        "帮我", "我想", "我要", "你好", "麻烦" };
        for (const char *f : fillers)
            out.replace(QString::fromUtf8(f), QString());
        out = out.trimmed();
        return out.isEmpty() ? text.trimmed() : out;
    };
    // 列出该站当前所有空闲桩（每次现查，状态可能已变化）
    auto idlePiles = [&](qint64 stationId) -> QList<PileOpt> {
        QList<PileOpt> out;
        int rc = Protocol::Unknown;
        QString em;
        const QJsonArray piles = m_db->pileList(stationId, rc, em);
        for (const auto &v : piles) {
            const QJsonObject p = v.toObject();
            if (p.value(QStringLiteral("status")).toString() != QStringLiteral("idle"))
                continue;
            PileOpt o;
            o.code = p.value(QStringLiteral("code")).toString();
            o.type = p.value(QStringLiteral("type")).toString();
            o.power = p.value(QStringLiteral("power_kw")).toDouble();
            o.id = p.value(QStringLiteral("id")).toVariant().toLongLong();
            o.no = noOf(o.code);
            out.append(o);
        }
        return out;
    };
    // 把空闲桩列表拼成给用户看的菜单："1号桩（快充 120kW）、2号桩（慢充 60kW）"
    auto menuText = [&](const QList<PileOpt> &opts) -> QString {
        QStringList parts;
        for (const PileOpt &o : opts) {
            const int no = o.no > 0 ? o.no : parts.size() + 1;
            parts << QStringLiteral("%1号桩（%2 %3kW）")
                         .arg(no)
                         .arg(kindText(o.type))
                         .arg(QString::number(o.power, 'f', 0));
        }
        return parts.join(QStringLiteral("、"));
    };
    // 从句子解析用户要的桩号："第2号桩"/"2号"/单独一个数字 -> 2
    auto parseNo = [](const QString &text) -> int {
        static const QRegularExpression re(
            QStringLiteral("第?\\s*([0-9]+)\\s*号\\s*(充电桩|电桩|桩)?"));
        const QRegularExpressionMatch m = re.match(text);
        if (m.hasMatch())
            return m.captured(1).toInt();
        static const QRegularExpression digits(QStringLiteral("^\\s*([0-9]+)\\s*$"));
        const QRegularExpressionMatch d = digits.match(text);
        return d.hasMatch() ? d.captured(1).toInt() : -1;
    };
    // 真正落库预约并返回话术（成功/各类失败均会清掉该用户挂起）
    auto doReserve = [&](qint64 uid, qint64 sid, const QString &sname, const PileOpt &o) -> QString {
        int rc = Protocol::Unknown;
        QString em;
        const QJsonObject out = m_db->reserve(uid, o.id, rc, em);
        {
            QMutexLocker lock(&g_reserveMutex);
            g_reserveCtx.remove(uid);
        }
        if (rc == Protocol::Ok) {
            // 预约成功的结构化信息带回给调用方，客户端据此把"充电"页同步成"已预约"
            actionOut[QStringLiteral("action")] = QStringLiteral("reserve_ok");
            actionOut[QStringLiteral("order_no")] =
                out.value(QStringLiteral("order_no")).toString();
            actionOut[QStringLiteral("station_name")] = sname;
            actionOut[QStringLiteral("pile_code")] = o.code;
            return QStringLiteral("搞定！已帮你预约「%1」%2（%3 %4kW）。订单号 %5。请按时到桩开始充电～")
                .arg(sname).arg(o.code).arg(kindText(o.type))
                .arg(QString::number(o.power, 'f', 0))
                .arg(out.value(QStringLiteral("order_no")).toString());
        }
        if (rc == Protocol::HasUnfinishedOrder)
            return QStringLiteral("你还有一笔未完成订单，需要先完成或取消它，才能预约新的充电位哦。");
        if (rc == Protocol::Frozen)
            return QStringLiteral("你的账号当前被冻结，暂时无法预约。");
        if (rc == Protocol::NotFound)
            return QStringLiteral("抱歉，这根电桩不存在，可能已经被移除了。");
        return QStringLiteral("预约没有成功，请稍后再试；也可以换一个充电站，或在「我的」里检查是否有未完成订单。");
    };

    // ==================== 主流程 ====================
    // 取出该用户现有上下文（只读副本）
    AiReserveCtx ctx;
    bool hadCtx = false;
    {
        QMutexLocker lock(&g_reserveMutex);
        auto it = g_reserveCtx.constFind(userId);
        if (it != g_reserveCtx.constEnd()) {
            ctx = it.value();
            hadCtx = true;
        }
    }

    // ---- 取消/算了：无论有无挂起都本地消费，不回落外部模型 ----
    if (wantsCancel) {
        {
            QMutexLocker lock(&g_reserveMutex);
            g_reserveCtx.remove(userId);
        }
        return hadCtx ? QStringLiteral("好嘞，已取消这次的预约，需要的时候随时找我～")
                      : QStringLiteral("好～你当前没有我正在处理的预约，想让我帮预约某个充电桩时随时喊我。");
    }

    // ---- 既没有预约意图、也没有挂起的会话 → 文本问答 ----
    if (!wantsReserve && !hadCtx)
        return {};

    // ---- 新一轮预约指令：作废旧上下文 ----
    if (wantsReserve) {
        QMutexLocker lock(&g_reserveMutex);
        g_reserveCtx.remove(userId);
        ctx = AiReserveCtx();
    }

    // 距离提示：走"最近的站"路径时拼到展示名后，如「XX站（距你约 1.2 公里）」
    QString stationNote;

    // 按用户位置找最近、且至少有一根空闲桩的充电站
    auto nearestPick = [&](double lat_, double lng_) -> NearestPick {
        NearestPick r;
        int rc = Protocol::Unknown;
        QString em;
        const QJsonArray arr = m_db->stationList(lat_, lng_, rc, em);
        if (rc != Protocol::Ok || arr.isEmpty())
            return r;   // 查询失败/无站点
        r.anyStation = true;
        for (const auto &v : arr) {   // 已按距离从近到远排序
            const QJsonObject s = v.toObject();
            if (s.value(QStringLiteral("idle")).toInt() > 0) {
                r.id = s.value(QStringLiteral("id")).toVariant().toLongLong();
                r.name = s.value(QStringLiteral("name")).toString();
                r.distanceKm = s.value(QStringLiteral("distance")).toDouble();
                r.found = true;
                break;
            }
        }
        return r;
    };

    // ---- 确定电站：AskPile 沿用已确认站；其余从本条解析/按位置找最近 ----
    qint64 sid = ctx.stationId;
    QString stationName = ctx.stationName;
    if (ctx.step != STEP_ASK_PILE) {
        const bool wantNearest = containsAny(q, { "最近", "就近", "附近", "离我近",
                                                  "近一点的", "近点", "就近的" });
        qint64 msid = -1;
        QString mname;
        if (!matchStation(q, msid, mname)) {
            if (wantNearest) {
                // 用户没点名站点，但想要"最近的充电站"
                if (!hasLocation) {
                    return QStringLiteral("我要先知道你的位置，才能帮你找最近的充电站哦～"
                                          "请先在页面上方地址栏输入城市/地址并点「定位」，"
                                          "再跟我说一次「帮我预约最近的充电桩」。");
                }
                const NearestPick pick = nearestPick(userLat, userLng);
                if (!pick.anyStation)
                    return QStringLiteral("我暂时没查到可用的充电站，请稍后再试～");
                if (!pick.found)
                    return QStringLiteral("离你最近的充电站当前都没有空闲电桩，"
                                          "可以稍后再试，或直接告诉我一个具体的充电站名～");
                sid = pick.id;
                stationName = pick.name;
                if (pick.distanceKm >= 0.0)
                    stationNote = QStringLiteral("（距你约 %1 公里）")
                                      .arg(QString::number(pick.distanceKm, 'f', 1));
            } else {
                // 把会话挂起为"等站名"（下一条报准确站名就能接上）
                {
                    QMutexLocker lock(&g_reserveMutex);
                    AiReserveCtx &c = g_reserveCtx[userId];
                    c.step = STEP_ASK_STATION;
                }
                // 本条还没出现"站/桩"字样 → 确实还没给站名，正常引导
                const bool looksStation = q.contains(QStringLiteral("充电站"))
                    || q.contains(QStringLiteral("电站"))
                    || q.contains(QStringLiteral("充电桩"))
                    || q.contains(QStringLiteral("电桩"))
                    || q.contains(QStringLiteral("站"));
                if (!looksStation)
                    return QStringLiteral("好的，我来帮你预约～你只要把充电站名发我（可带上桩号），"
                                          "比如：深圳市民中心充电站 第1号桩，我就能直接帮你约好。");
                // 报了站名但平台里没有：明确告知 + 尽量猜一个最接近的站，避免机械重复
                const QString shown = prettyStation(q);
                const QString guess = suggestStation(q);
                if (!guess.isEmpty())
                    return QStringLiteral("平台里没有「%1」这个站哦。你是不是想约「%2」？"
                                          "把平台里的完整站名发我（可带上桩号），我就帮你约～")
                        .arg(shown, guess);
                return QStringLiteral("平台里好像没有「%1」这个站。可以先去「充电站」页面看看附近的站点，"
                                      "把完整站名发给我～").arg(shown);
            }
        }
        if (msid > 0) {
            sid = msid;
            stationName = mname;
        }
    } else {
        // 正在选桩时改口：点名另一个站 → 换站；说"换最近/就近的" → 按位置找最近
        const bool wantNearest = containsAny(q, { "最近", "就近", "附近", "离我近" });
        qint64 msid = -1;
        QString mname;
        const bool namedOther = matchStation(q, msid, mname) && msid != ctx.stationId;
        if (namedOther) {
            sid = msid;
            stationName = mname;
        } else if (wantNearest) {
            if (!hasLocation) {
                return QStringLiteral("我要先知道你的位置，才能帮你找最近的充电站哦～"
                                      "请先在页面上方地址栏输入城市/地址并点「定位」。");
            }
            const NearestPick pick = nearestPick(userLat, userLng);
            if (!pick.anyStation)
                return QStringLiteral("我暂时没查到可用的充电站，请稍后再试～");
            if (!pick.found)
                return QStringLiteral("离你最近的充电站当前都没有空闲电桩，可以稍后再试～");
            sid = pick.id;
            stationName = pick.name;
            if (pick.distanceKm >= 0.0)
                stationNote = QStringLiteral("（距你约 %1 公里）")
                                  .arg(QString::number(pick.distanceKm, 'f', 1));
        }
    }

    // ---- 现查该站空闲桩 ----
    const QList<PileOpt> opts = idlePiles(sid);
    if (opts.isEmpty()) {
        {
            QMutexLocker lock(&g_reserveMutex);
            g_reserveCtx.remove(userId);
        }
        return QStringLiteral("抱歉，「%1」当前没有空闲电桩，请稍后再试或换一个充电站～")
            .arg(stationName);
    }

    const int wantNo = parseNo(q);

    // ---- 用户明确给了桩号：约那根 ----
    if (wantNo > 0) {
        for (const PileOpt &o : opts) {
            if (o.no == wantNo)
                return doReserve(userId, sid, stationName, o);
        }
        // 该桩此刻不可用（刚被约走/号不存在）：给出当前可选项，保持"选桩"状态
        {
            QMutexLocker lock(&g_reserveMutex);
            AiReserveCtx &c = g_reserveCtx[userId];
            c.step = STEP_ASK_PILE;
            c.stationId = sid;
            c.stationName = stationName;
        }
        return QStringLiteral("「%1」现在没有%2号桩了（可能刚被约走）。当前可约：%3。"
                              "换一个桩号告诉我，或直接说「随便」我来挑～")
            .arg(stationName).arg(wantNo).arg(menuText(opts));
    }

    // ---- 没给桩号：用户正在选桩（回了个"好的/约吧"之类）或让客服代挑 → 挑第一根 ----
    if (ctx.step == STEP_ASK_PILE
        || containsAny(q, { "随便", "都行", "你定", "你安排", "第一根", "第一个" })) {
        return doReserve(userId, sid, stationName, opts.first());
    }

    // ---- 只给了站名：列出空闲桩，等用户挑（方案B）----
    {
        QMutexLocker lock(&g_reserveMutex);
        AiReserveCtx &c = g_reserveCtx[userId];
        c.step = STEP_ASK_PILE;
        c.stationId = sid;
        c.stationName = stationName;
    }
    return QStringLiteral("「%1%2」现在能约的桩有：%3。\n回复桩号（比如「2号」）我就帮你约上；也可以说「随便」我来帮你挑一个～")
        .arg(stationName, stationNote).arg(menuText(opts));
}
