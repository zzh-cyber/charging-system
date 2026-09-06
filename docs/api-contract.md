# Socket 接口契约（本期：功能前三项）

> 全组开发的"合同"。前后端对着这张表并行开发；改接口必须先改本文件并周知。

## 通用约定

- 传输：TCP，端口 **9000**。
- 帧格式：`4 字节大端长度头 + JSON 载荷`（`common/protocol.h` 已封装 `encode/tryDecode`）。
- 请求：`{ "type": "<接口名>", "token": "<除 login/admin_login 外必填>", "data": { ... } }`
- 响应：`{ "type": "<接口名>", "code": <int>, "msg": "<string>", "data": { ... } }`
- `code == 0` 成功；非 0 见错误码表。
- **鉴权（已开闸）**：仅 `login` / `admin_login` 不带 token。其余接口必须在 JSON 顶层带有效 token。`admin_*` 还要求会话角色为 admin；用户端接口要求角色为 user。失败统一 `code=9`。

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
| 9 | 会话无效（未带 token / token 无效 / 已过期 / 角色不符）。除登录外全部接口强制校验 |
| 99 | 接口尚未实现（骨架占位） |

## 状态字段取值

- `user.status`: `normal` / `frozen`
- `pile.type`: `fast`(快充) / `slow`(慢充)
- `pile.status`: `idle`(闲置) / `busy`(在用) / `fault`(故障)
- `charge_order.status`: `reserved` / `charging` / `pending_payment` / `settled` / `cancelled`

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
| `unfinished_order` | 查询未完成订单 | 顶层必须带 `token`；`data` 可为空。身份取自会话，忽略 `data.user_id` | `{order?:{order_no,pile_id,status,power_kw,unit_price,duration_seconds,kwh,amount,start_soc?,battery_capacity_kwh?,target_soc?,...}}`。`charging` 按时长现算电量金额；`pending_payment` 用库中已出账单。SOC 三字段为模拟展示，不参与结算；未开始充电或旧单可能缺省 | 用户端B | ✅ 已实现 |
| `reserve` | 预约 | 顶层必须带 `token`；`data` 仅 `{pile_id}`。身份取自会话，忽略 `data.user_id` | `{order_no}` | 用户端B | ✅ 已实现 |
| `start_charge` | 开始充电 | 顶层必须带 `token`；`data` 为 `{order_no}`。服务器校验订单属于会话用户 | `{start_time,power_kw,unit_price,start_soc,battery_capacity_kwh,target_soc}`。SOC 在本单 `start_charge` 时写入订单并固定，客户端按公式展示进度，不参与 `finish_charge` 计费 | 用户端B | ✅ 已实现 |
| `finish_charge` | 结束充电出账（不扣款） | 顶层必须带 `token`；`data` 为 `{order_no}`。服务端按功率×时长算 kwh/金额 | `{end_time,duration_seconds,kwh,amount,unit_price,power_kw}`。订单 `charging→pending_payment`，释放电桩 | 用户端B | ✅ 已实现 |
| `pay_charge` | 确认支付扣款 | 顶层必须带 `token`；`data` 为 `{order_no}`。只处理 `pending_payment` | `{amount,balance,duration_seconds,kwh}`。成功 `settled`；余额不足 `code=7` 且订单仍待支付 | 用户端B | ✅ 已实现 |
| `settle` | 旧结算（已停用） | — | `code=2`，提示改用 `finish_charge` + `pay_charge` | 用户端B | ⛔ 已停用 |

## 二、管理端接口

| 接口 (type) | 说明 | 请求 data | 响应 data | 负责人 | 状态 |
|-------------|------|-----------|-----------|--------|------|
| `admin_login` | 管理员登录 | `{username,password}` | `{id,username,token}` | 服务器A / 管理端C | ✅ 已实现（样板，已下发 token） |
| `admin_user_list` | 用户列表（keyword 空=全部；否则按手机号/昵称模糊搜索） | `{keyword?}` | `{list:[{id,phone,nickname,balance,status,created_at}]}` | 服务器A(组长) / 管理端C | ✅ 已实现 |
| `admin_user_freeze` | 冻结/解冻用户 | `{user_id,frozen:bool}`。`user_id` 是操作对象。冻结成功后该用户全部 user token 立即失效 | `{id,status}` | 服务器A(组长) / 管理端C | ✅ 已实现 |
| `admin_pile_list` | 电桩列表（含所属站名，支持服务端筛选+分页）及启用电桩状态统计 | `{station_id?,type?,status?,code?,page?,page_size?}`；`type` 为 `fast/slow`，`status` 为 `idle/busy/fault`，`code` 模糊匹配；`page` 默认 1，`page_size` 默认 20、最大 50 | `{list:[{id,code,station,type,power_kw,status,total_count,total_hours}],total,page,page_size,stats:{idle:{count,rate},busy:{count,rate},fault:{count,rate},total,stat_time}}`；`stats` 仍为全局启用电桩统计 | 服务器A(组长) / 管理端C | ✅ 已实现 |
| `admin_pile_restart` | 远程重启电桩（fault/busy→idle） | `{pile_id}` | `{id,status}` | 服务器A(组长) / 管理端C | ✅ 已实现 |
| `admin_station_list` | 电站列表（含桩数、在线率） | `{}` | `{list:[{id,name,address,longitude,latitude,total,online_rate}]}` | 服务器A(组长) / 管理端C | ✅ 已实现 |
| `admin_station_add` | 新增电站 | `{name,address,longitude,latitude,price}` | `{id}` | 管理端C | ⬜ |
| `admin_order_list` | 订单列表（筛选+分页；仪表盘今日单量/最近订单复用本接口） | 空字段/`0` 表示不筛。`{order_no?,phone?,user_id?,station_id?,pile_id?,pile_code?,keyword?,status?,start_time?,end_time?,page?,page_size?}`。`keyword` 匹配订单号/手机号/桩编号；`start_time`/`end_time` 按 `COALESCE(start_time,reserve_time,created_at)` 过滤（可只传 `yyyy-MM-dd`）。`page` 从 1，`page_size` 默认 20、最大 50。`status` 须为 `reserved/charging/pending_payment/settled/cancelled` | `{list:[{id,order_no,user_id,nickname,phone,station_id,station_name,pile_id,pile_code,status,kwh,duration_seconds,unit_price,amount,reserve_time,start_time,end_time,created_at,updated_at}],total,page,page_size}`。默认 `created_at DESC`。金额/电量/时长取库中值 | 服务器A(组长) / 管理端C | ✅ 已实现 |
| `admin_order_detail` | 订单详情（只读） | `{order_no}` | 与列表单行相同字段（包在 `data` 根上）。不存在 `code=4`。不做删单/改金额/改状态/退款 | 服务器A(组长) / 管理端C | ✅ 已实现 |

---

## 新增一个接口的步骤（照抄 `login`）

1. 在 `common/protocol.h` 的 `MsgType` 加接口名常量。
2. 在本文件登记接口（入参/出参/负责人）。
3. 服务器：在 `server/database.*` 加查询方法，在 `server/clienthandler.cpp` 的 `dispatch()` 加分发分支。
4. 客户端：用 `NetClient::request(makeRequest(type, data))` 调用并处理响应。
