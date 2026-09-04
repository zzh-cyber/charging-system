# Socket 接口契约（本期：功能前三项）

> 全组开发的"合同"。前后端对着这张表并行开发；改接口必须先改本文件并周知。

## 通用约定

- 传输：TCP，端口 **9000**。
- 帧格式：`4 字节大端长度头 + JSON 载荷`（`common/protocol.h` 已封装 `encode/tryDecode`）。
- 请求：`{ "type": "<接口名>", "token": "<登录后下发，可选>", "data": { ... } }`
- 响应：`{ "type": "<接口名>", "code": <int>, "msg": "<string>", "data": { ... } }`
- `code == 0` 成功；非 0 见错误码表。
- **token 阶段说明（第 1 步）**：`login` / `admin_login` 成功时在 `data.token` 下发 UUID。客户端登录后 `NetClient::setToken`，后续请求自动带到 JSON 顶层。**当前服务器不强制校验**，旧客户端不带 token 仍可联调。强制鉴权另一步打开。

## 错误码（`Protocol::ErrorCode`）

| code | 含义 |
|-----:|------|
| 0 | 成功 |
| 1 | 未知错误 |
| 2 | 请求格式错误 |
| 3 | 数据库错误 |
| 4 | 数据不存在 |
| 5 | 认证失败 |
| 6 | 账号被冻结 |
| 7 | 余额不足 |
| 8 | 存在未完成订单 |
| 9 | 会话无效（未带 token / token 无效 / 已过期）。**目前 `reserve`、`unfinished_order`、`recharge`、`start_charge`、`settle` 强制校验**；其余接口尚未打开 |
| 99 | 接口尚未实现（骨架占位） |

## 状态字段取值

- `user.status`: `normal` / `frozen`
- `pile.type`: `fast`(快充) / `slow`(慢充)
- `pile.status`: `idle`(闲置) / `busy`(在用) / `fault`(故障)
- `charge_order.status`: `reserved` / `charging` / `settled` / `cancelled`

---

## 一、用户端接口

| 接口 (type) | 说明 | 请求 data | 响应 data | 负责人 | 状态 |
|-------------|------|-----------|-----------|--------|------|
| `login` | 手机号免密登录/注册 | `{phone}` | `{id,phone,nickname,avatar,balance,token}` | 服务器A / 用户端B | ✅ 已实现（样板，已下发 token） |
| `user_info` | 获取用户信息 | `{user_id}` | `{id,phone,nickname,avatar,balance}` | 服务器A | ⬜ 待实现 |
| `update_profile` | 修改昵称/头像 | `{user_id,nickname?,avatar?}` | `{}` | 服务器A / 用户端B | ⬜ |
| `recharge` | 余额充值 | 顶层必须带 `token`；`data` 仅 `{amount}`。入账用户取自会话，忽略 `data.user_id` | `{balance}` | 服务器A / 用户端B | ✅ 已实现 |
| `station_list` | 充电站列表 | `{}` | `{list:[{id,name,address,longitude,latitude,price,total,idle}]}` | 服务器A / 成员D | ✅ 已实现（样板） |
| `pile_list` | 某站电桩列表 | `{station_id}` | `{list:[{id,code,type,power_kw,status}]}` | 服务器A / 成员D | ⬜ |
| `pile_detail` | 电桩详情 | `{pile_id}` | `{id,code,type,power_kw,status,total_count,total_hours}` | 成员D | ⬜ |
| `unfinished_order` | 查询未完成订单 | 顶层必须带 `token`；`data` 可为空。身份取自会话，忽略 `data.user_id` | `{order?:{order_no,pile_id,status,...}}` | 用户端B | ✅ 已实现 |
| `reserve` | 预约 | 顶层必须带 `token`；`data` 仅 `{pile_id}`。身份取自会话，忽略 `data.user_id` | `{order_no}` | 用户端B | ✅ 已实现 |
| `start_charge` | 开始充电 | 顶层必须带 `token`；`data` 为 `{order_no}`。服务器校验订单属于会话用户 | `{start_time}` | 用户端B | ✅ 已实现 |
| `settle` | 计费结算 | 顶层必须带 `token`；`data` 为 `{order_no,kwh}`。服务器校验订单属于会话用户 | `{amount,balance}` | 用户端B | ✅ 已实现 |

## 二、管理端接口

| 接口 (type) | 说明 | 请求 data | 响应 data | 负责人 | 状态 |
|-------------|------|-----------|-----------|--------|------|
| `admin_login` | 管理员登录 | `{username,password}` | `{id,username,token}` | 服务器A / 管理端C | ✅ 已实现（样板，已下发 token） |
| `admin_user_list` | 用户列表（keyword 空=全部；否则按手机号/昵称模糊搜索） | `{keyword?}` | `{list:[{id,phone,nickname,balance,status,created_at}]}` | 服务器A(组长) / 管理端C | ✅ 已实现 |
| `admin_user_freeze` | 冻结/解冻用户 | `{user_id,frozen:bool}`。`user_id` 是操作对象。冻结成功后该用户全部 user token 立即失效 | `{id,status}` | 服务器A(组长) / 管理端C | ✅ 已实现 |
| `admin_pile_list` | 电桩列表（含所属站名） | `{}` | `{list:[{id,code,station,type,power_kw,status,total_count,total_hours}]}` | 服务器A(组长) / 管理端C | ✅ 已实现 |
| `admin_pile_restart` | 远程重启电桩（fault/busy→idle） | `{pile_id}` | `{id,status}` | 服务器A(组长) / 管理端C | ✅ 已实现 |
| `admin_station_list` | 电站列表（含桩数、在线率） | `{}` | `{list:[{id,name,address,longitude,latitude,total,online_rate}]}` | 服务器A(组长) / 管理端C | ✅ 已实现 |
| `admin_station_add` | 新增电站 | `{name,address,longitude,latitude,price}` | `{id}` | 管理端C | ⬜ |
| `admin_order_list` | 订单列表 | `{keyword?,status?}` | `{list:[{order_no,user_phone,pile_code,status,...}]}` | 管理端C | ⬜ |

---

## 新增一个接口的步骤（照抄 `login`）

1. 在 `common/protocol.h` 的 `MsgType` 加接口名常量。
2. 在本文件登记接口（入参/出参/负责人）。
3. 服务器：在 `server/database.*` 加查询方法，在 `server/clienthandler.cpp` 的 `dispatch()` 加分发分支。
4. 客户端：用 `NetClient::request(makeRequest(type, data))` 调用并处理响应。
