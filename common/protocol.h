#pragma once

// ============================================================================
// 充电管理系统 - 通信协议（公共层）
// ----------------------------------------------------------------------------
// 传输：TCP。每条消息 = 4 字节大端长度头 + JSON 载荷。
// 请求：{ "type": "<接口名>", "token": "<登录后下发，可选>", "data": { ... } }
// 响应：{ "type": "<接口名>", "code": <错误码>, "msg": "<提示>", "data": { ... } }
// code == 0 表示成功，非 0 见 ErrorCode。
// token 由登录接口写入 data.token；后续请求放在 JSON 顶层。
// 当前阶段服务器只下发、不强制校验；强制鉴权另一步打开。
// 服务器与客户端都复用本文件，保证两端"说同一种话"。
// ============================================================================

#include <QByteArray>
#include <QDataStream>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

namespace Protocol {

// ------- 错误码 -------
enum ErrorCode {
    Ok                 = 0,   // 成功
    Unknown            = 1,   // 未知错误
    InvalidRequest     = 2,   // 请求格式错误
    DbError            = 3,   // 数据库错误
    NotFound           = 4,   // 数据不存在
    AuthFailed         = 5,   // 认证失败（账号或密码错误）
    Frozen             = 6,   // 账号被冻结
    InsufficientBalance= 7,   // 余额不足
    HasUnfinishedOrder = 8,   // 存在未完成订单
    SessionInvalid     = 9,   // 未带 token / token 无效 / 已过期 → 客户端应回登录页
    NotImplemented     = 99   // 接口尚未实现（骨架占位）
};

// ------- 接口名（type 字段取值），前后端共用同一批常量 -------
namespace MsgType {
// 用户端
inline constexpr const char *Login        = "login";          // 手机号免密登录/注册
inline constexpr const char *UserInfo     = "user_info";      // 获取用户信息
inline constexpr const char *UpdateProfile= "update_profile"; // 修改昵称/头像
inline constexpr const char *Recharge     = "recharge";       // 余额充值
inline constexpr const char *StationList  = "station_list";   // 附近充电站列表
inline constexpr const char *PileList     = "pile_list";      // 某站电桩列表
inline constexpr const char *PileDetail   = "pile_detail";    // 电桩详情
inline constexpr const char *Reserve      = "reserve";        // 预约
inline constexpr const char *StartCharge  = "start_charge";   // 开始充电
inline constexpr const char *Settle       = "settle";         // 计费结算
inline constexpr const char *UnfinishedOrder = "unfinished_order"; // 查询未完成订单

// 管理端
inline constexpr const char *AdminLogin       = "admin_login";
inline constexpr const char *AdminUserList     = "admin_user_list";
inline constexpr const char *AdminUserFreeze   = "admin_user_freeze";   // 冻结/解冻
inline constexpr const char *AdminPileList     = "admin_pile_list";
inline constexpr const char *AdminPileRestart  = "admin_pile_restart";  // 远程重启
inline constexpr const char *AdminStationList  = "admin_station_list";
inline constexpr const char *AdminStationAdd   = "admin_station_add";
inline constexpr const char *AdminOrderList    = "admin_order_list";
} // namespace MsgType

// ------- 构造请求/响应 -------
inline QJsonObject makeRequest(const QString &type,
                               const QJsonObject &data = {},
                               const QString &token = {})
{
    QJsonObject o;
    o["type"] = type;
    o["data"] = data;
    if (!token.isEmpty())
        o["token"] = token;
    return o;
}

inline QJsonObject makeResponse(const QString &type, int code, const QString &msg,
                                const QJsonObject &data = {})
{
    QJsonObject o;
    o["type"] = type;
    o["code"] = code;
    o["msg"]  = msg;
    o["data"] = data;
    return o;
}

// ------- 编解码（长度头 + JSON）-------

// 把 JSON 对象打包成可直接写入 socket 的帧
inline QByteArray encode(const QJsonObject &obj)
{
    const QByteArray payload = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    QByteArray frame;
    QDataStream ds(&frame, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian);
    ds << static_cast<quint32>(payload.size());
    frame.append(payload);
    return frame;
}

// 尝试从缓冲区取出一条完整消息：
//   返回 true  → 成功取出一条（已从 buffer 中移除），结果写入 out
//   返回 false → 数据还不完整，等待更多字节
inline bool tryDecode(QByteArray &buffer, QJsonObject &out)
{
    if (buffer.size() < 4)
        return false;

    quint32 len = 0;
    {
        QDataStream ds(buffer);
        ds.setByteOrder(QDataStream::BigEndian);
        ds >> len;
    }

    if (buffer.size() < static_cast<int>(4 + len))
        return false;

    const QByteArray payload = buffer.mid(4, len);
    buffer.remove(0, 4 + len);

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
    out = (err.error == QJsonParseError::NoError && doc.isObject()) ? doc.object()
                                                                    : QJsonObject();
    return true;
}

} // namespace Protocol
