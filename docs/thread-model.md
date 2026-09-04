# 线程模型说明

> 每新增/修改涉及多线程的模块，在此追加一节。  
> 格式见 `docs/development-rules.md` 第七节。

---

## 服务器 - ClientHandler - 2026-09-01 - 组长（地基）

### 线程职责

| 线程 | 职责 |
|------|------|
| **主线程** | `QCoreApplication` 事件循环；`TcpServer::listen()` 接受连接 |
| **ClientHandler 线程**（每连接一个） | Socket 收发、JSON 解析、Database 查询、写回响应 |

### 跨线程通信

- `TcpServer::incomingConnection` 在主线程被调用
- 创建 `QThread` + `ClientHandler`，`handler->moveToThread(thread)`
- 通过 `QThread::started` 信号触发 `ClientHandler::start()`（QueuedConnection 自动）

### 共享资源与锁

| 资源 | 保护方式 | 访问线程 |
|------|----------|----------|
| MySQL 连接 | 每 ClientHandler 线程独立 `Database` 实例（独立 connectionName） | 各自 Handler 线程 |
| 会话表 | 见下方 SessionManager，`QMutex` 保护 | 所有 Handler 线程 |

---

## 服务器 - SessionManager - 2026-09-04 - 组长（token 第 1 步）

### 线程职责

| 线程 | 职责 |
|------|------|
| **任意 ClientHandler 线程** | 登录成功调用 `create`；后续（第 3 步）`dispatch` 调用 `validate` / `revoke` |

### 跨线程通信

- `SessionManager` 是进程内单例，不跨进程、不落库
- 各 Handler 线程通过 `SessionManager::instance()` 访问同一张 `QHash`

### 共享资源与锁

| 资源 | 保护方式 | 访问线程 |
|------|----------|----------|
| `m_sessions` | `QMutex` + `QMutexLocker`，所有读写都加锁 | 所有 ClientHandler 线程 |
| token 字符串 | 登录时 `QUuid::createUuid()` 生成，作为 Hash key | 创建线程写入，校验线程只读 key |

### 验证

- 同一账号连续登录两次，应得到两个不同 token（允许多端在线）
- 登录响应 `data.token` 为无花括号 UUID
- 不带 token 的旧请求仍能通过（第 1 步不强制鉴权）

---

## 服务器 - reserve 按 token 认人 - 2026-09-04 - 组长

### 线程职责

| 线程 | 职责 |
|------|------|
| **ClientHandler 线程** | `dispatch` 在处理 `reserve` 时调用 `SessionManager::validate`，用返回的 `sess.userId` 调 `Database::reserve` |

### 跨线程通信

- 无新增跨线程对象；仍通过已有单例访问会话表
- 未改 `TcpServer` 派生线程、未改 `NetClient` socket 收发

### 共享资源与锁

| 资源 | 保护方式 | 访问线程 |
|------|----------|----------|
| `m_sessions` | 仍由 `SessionManager` 内部 `QMutex` 覆盖 `validate` | 发起预约的 Handler 线程 |

### 验证

- 无 token 或 token 无效：`reserve` 返回 `code=9`
- 报文里伪造 `data.user_id`：订单仍记在 token 对应用户上
- `station_list` 等其它接口本步不校验 token

---

## 服务器 - unfinished_order 按 token 认人 - 2026-09-04 - 组长

### 线程职责

| 线程 | 职责 |
|------|------|
| **ClientHandler 线程** | `dispatch` 处理 `unfinished_order` 时 `validate` token，用 `sess.userId` 查未完成订单 |

### 跨线程通信

- 无新增跨线程对象；会话表仍由 `SessionManager` 单例 + `QMutex` 保护
- 未改 `TcpServer` / `NetClient` 收发

### 共享资源与锁

| 资源 | 保护方式 | 访问线程 |
|------|----------|----------|
| `m_sessions` | `SessionManager::validate` 内部加锁 | 发起查询的 Handler 线程 |

### 验证

- 无 token：`unfinished_order` 返回 `code=9`
- 伪造 `data.user_id`：返回的仍是 token 对应用户的订单
- `reserve` 行为不变；其它未改接口仍不强制 token

---

## 服务器 - recharge 按 token 认人 - 2026-09-04 - 组长

### 线程职责

| 线程 | 职责 |
|------|------|
| **ClientHandler 线程** | `dispatch` 处理 `recharge` 时 `validate` token，用 `sess.userId` 入账 |

### 跨线程通信

- 无新增跨线程对象；未改 `TcpServer` / `NetClient`

### 共享资源与锁

| 资源 | 保护方式 | 访问线程 |
|------|----------|----------|
| `m_sessions` | `SessionManager::validate` 内部加锁 | 发起充值的 Handler 线程 |

### 验证

- 无 token：`recharge` 返回 `code=9`
- 伪造 `data.user_id`：余额加在 token 对应用户上，不给报文里的 id 入账

---

## 服务器 - start_charge / settle 校验订单归属 - 2026-09-04 - 组长

### 线程职责

| 线程 | 职责 |
|------|------|
| **ClientHandler 线程** | 校验 token 后把 `sess.userId` 传入 `startCharge` / `settle`；SQL 按 `order_no + user_id` 锁定订单 |

### 跨线程通信

- 无新增跨线程对象；未改 `TcpServer` / `NetClient`

### 共享资源与锁

| 资源 | 保护方式 | 访问线程 |
|------|----------|----------|
| `m_sessions` | `SessionManager::validate` 内部加锁 | 发起开始充电/结算的 Handler 线程 |
| 订单行 | InnoDB `FOR UPDATE`（原有） | 同一 Handler 线程内的 Database 连接 |

### 验证

- 无 token：两接口返回 `code=9`
- 用他人 `order_no`：返回订单不存在，不能开始或结算

---

## 服务器 - 冻结踢下线 - 2026-09-04 - 组长

### 线程职责

| 线程 | 职责 |
|------|------|
| **ClientHandler 线程** | `admin_user_freeze` 成功且 `frozen=true` 时调用 `revokeByUser(targetUserId, "user")` |

### 跨线程通信

- 无新增对象；踢人写的是全局会话表，其它连接上的 Handler 下次 `validate` 会失败
- 未改 `TcpServer` / `NetClient`

### 共享资源与锁

| 资源 | 保护方式 | 访问线程 |
|------|----------|----------|
| `m_sessions` | `revokeByUser` 内部 `QMutex` | 执行冻结的 Handler 线程 |

### 验证

- 用户登录拿到 token 后被冻结：原 token 再调 `unfinished_order`/`recharge` 返回 `code=9`
- 解冻不恢复旧 token，需重新登录

---

## 服务器 - dispatch 全局鉴权门 - 2026-09-04 - 组长

### 线程职责

| 线程 | 职责 |
|------|------|
| **ClientHandler 线程** | 每个非登录请求在 `dispatch` 入口 `validate` token，并按 `admin_*` / 用户接口检查 `sess.role` |

### 跨线程通信

- 无新增跨线程对象；登录豁免，其余接口共用同一扇门
- 未改 `TcpServer` 派生线程、未改 `NetClient` 收发

### 共享资源与锁

| 资源 | 保护方式 | 访问线程 |
|------|----------|----------|
| `m_sessions` | `validate` 内部 `QMutex` | 每个业务请求所在的 Handler 线程 |

### 验证

- 不带 token 的 `station_list` / `admin_user_list` 返回 `code=9`
- 用户 token 调 `admin_*`、管理员 token 调 `reserve` 返回 `code=9`
- `login` / `admin_login` 仍不需要 token

---

## 客户端 - code=9 回登录页 - 2026-09-04 - 组长

### 线程职责

| 线程 | 职责 |
|------|------|
| **UI 线程** | `NetClient` 收包、`clearToken`、延后发出 `sessionInvalid`；主窗口弹窗并回到登录页 |

### 跨线程通信

- 未改 `TcpServer` 派生线程，未改 socket 读写路径；只在解码后增加 `code==9` 处理
- `sessionInvalid` 用 `QTimer::singleShot(0)` 抛出，让 `request()` 的嵌套 `QEventLoop` 先退出，再关窗口

### 共享资源与锁

| 资源 | 保护方式 | 访问线程 |
|------|----------|----------|
| `m_token` | 仅 UI 线程读写 | UI 线程 |

### 验证

- 编译用户端、管理端
- 登录后把本地 token 清掉再发业务请求，或服务端冻结用户后继续操作：弹「登录已失效」并回到登录页

---

## 客户端 - NetClient - 2026-09-01 - 组长（地基）

### 线程职责

| 线程 | 职责 |
|------|------|
| **UI 线程** | `LoginWindow` 界面；调用 `NetClient::request()` |
| **Qt 网络内部线程** | `QTcpSocket` 实际 IO（Qt 内部管理） |

### 跨线程通信

- `NetClient::request()` 在 UI 线程调用
- 内部 `QEventLoop` 阻塞等待 `responseReceived` 信号
- **当前实现为同步阻塞**（适合登录等低频操作）
- 后续高频场景（如充电进度）须改为异步 + 信号回调，**禁止在 UI 线程长时间阻塞**

### 共享资源与锁

| 资源 | 保护方式 | 访问线程 |
|------|----------|----------|
| `m_buffer` | 仅在 `onReadyRead`（socket 所在线程）访问 | 网络线程 |
| `request()` 同步调用 | 每次调用独立，无并发 | UI 线程 |

### 待办（后续单步实现）

- [ ] 充电进度等高频接口：新增 `NetClient::sendAsync()` + UI 侧 QueuedConnection 槽（单独 PR）

---
