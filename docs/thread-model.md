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
