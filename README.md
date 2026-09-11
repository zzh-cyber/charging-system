# 新能源汽车充电管理系统

Linux + Qt Widgets 实训项目。用户端、管理端、后台是三个独立进程：两端**不直连 MySQL**，只通过 TCP 连 `charging-server`；后台再访问数据库。

| 项 | 说明 |
|----|------|
| 目标环境 | Ubuntu **22.04 LTS**（含 WSL2）+ Qt **6.2.4**（不要升到更高版本） |
| 语言 / 构建 | C++17 · CMake 3.16+ · Ninja |
| 数据库 | MySQL 8.0 · 库名 `charging_system` |
| 通信 | TCP **9000**，帧格式：`4 字节大端长度 + UTF-8 JSON` |
| 演示账号 | 用户 `13800138001`（免密）；管理员 `admin` / `123456` |

---

## 1. 功能概览

**充电用户端 `charging-user`**

- 手机号免密登录 / 注册、改昵称头像、钱包充值
- 按地址定位（高德地理编码），附近电站卡片 + 地图 Marker
- 站内选闲置桩 → 预约 → 开始充电 → 结束出账 → 确认支付
- 一键导航（高德：驾车 / 步行）
- 智能客服「E小充」（悬浮球；可问答，也可按关键词代预约）

**PC 管理端 `charging-admin`**

- 管理员登录、工作台 / 营收折线图（近 7 / 30 日）
- 电桩状态、远程重启（充电中拒绝）
- 电站列表、站内桩明细、新增电站
- 用户列表、冻结 / 解冻
- 订单筛选、分页、详情

**后台 `charging-server`**

- 一连接一线程；按 `type` 分发；`token` 会话鉴权
- 全部业务 SQL 参数化；预约 / 充值 / 支付走事务

---

## 2. 小组分工

| 角色 | 姓名 | 主要范围 |
|------|------|----------|
| 组长 | 翟梓涵 | 收发框架、会话鉴权、部分管理端服务（重启 / 冻结 / 新增站 / 订单查询） |
| 成员 A | 朱雅琪 | 用户侧服务端业务；用户端高德地图 / 导航 / Marker |
| 成员 B | 马晓钰 | 用户端界面（登录、定位、电站卡片、充电页等） |
| 成员 C | 牛昀轶 | PC 管理端界面 |
| 成员 D | 邓雅心 | 数据库与种子数据；智能 AI 客服（界面 + `ai_chat`） |

---

## 3. 系统架构

```
charging-user  ─┐
                ├─ TCP :9000（长度前缀 JSON）─→  charging-server  ─→  MySQL :3306
charging-admin ─┘                                      │
                                                       │  QMYSQL DAO
                                                       ▼
                                              charging_system
```

- 用户端 / 管理端 **禁止** `#include` 或直连 MySQL。
- 除 `login` / `admin_login` 外，每个请求必须在 JSON **顶层**带 `token`。身份由服务端 `SessionManager` 反查，**不信任**客户端自报的 `user_id`。
- 地图 Key、大模型 Key **不经过**充电服务器业务报文。

---

## 4. 目录结构

```
charging-system/
├── CMakeLists.txt              顶层工程（四个子目录）
├── common/                     协议 + TCP 客户端（两端共用）
│   ├── protocol.h              错误码、MsgType、encode / tryDecode
│   ├── netclient.*             QTcpSocket 封装（connect / setToken / request）
│   └── runtimebootstrap.*      WSL / IBus 等运行时兼容
├── server/                     唯一后台（无界面）
│   ├── main.cpp                检查 QMYSQL → 对齐表结构 → listen(9000)
│   ├── config.h                端口、库地址；密码读 CHARGING_DB_PASSWORD
│   ├── tcpserver.*             一连接一 QThread
│   ├── clienthandler.*         分帧、鉴权、按 type 分发
│   ├── sessionmanager.*        内存 token 表
│   ├── database.*              全部业务 SQL
│   ├── aiservice.*             大模型问答（Key 读环境变量）
│   └── schema.qrc              打包 sql/schema.sql
├── client-user/                充电用户端（Qt Widgets + WebEngine）
├── client-admin/               PC 管理端（Qt Widgets + Charts）
├── sql/
│   ├── schema.sql              表结构 + 演示种子（约 72 站 / 399 桩 / 300+ 单）
│   └── seed_expand_stations.sql  可重复执行的增量补站
├── config/
│   ├── runtime.ini             WSL 下 Qt / WebEngine 运行参数
│   └── amap.ini                高德 JS / Web 服务 Key（定位、地图、导航）
├── tests/                      可选：协议级 Python 自测（需先开服务端）
└── docs/                       需求矩阵、接口契约、环境说明等
```

编译产物（不提交 Git）：

| 进程 | 可执行文件 |
|------|------------|
| 后台 | `build/server/charging-server` |
| 用户端 | `build/client-user/charging-user` |
| 管理端 | `build/client-admin/charging-admin` |

---

## 5. 环境依赖

已在 **Ubuntu 22.04.5 + GCC 11.4 + CMake 3.22 + Qt 6.2.4 + MySQL 8.0** 验证。请与课程机房 / BitDev 对齐，**不要**在 Windows 本机直接编译，也**不要**把 Qt 升到 6.2.4 以上。

| 组件 | 版本要求 | 用途 |
|------|----------|------|
| Ubuntu | 22.04 LTS | 操作系统（WSL2 亦可） |
| g++ | 11.x | 编译 |
| CMake | ≥ 3.16（仓库按 3.22 验证） | 构建 |
| Ninja | 1.x | 推荐生成器 |
| Qt 6 | **6.2.4** | Core / Gui / Widgets / Network / Sql |
| Qt Charts | 6.2.4 | 管理端营收折线图 |
| Qt WebEngine | 6.2.4 | 用户端地图与导航 |
| MySQL Server | 8.0.x | 业务库 |
| Qt MySQL 驱动 | `libqt6sql6-mysql` | 后台连库 |

自检：

```bash
cat /etc/os-release | grep PRETTY_NAME
g++ --version | head -1
cmake --version | head -1
qmake6 --version
mysql --version
ls /usr/lib/x86_64-linux-gnu/qt6/plugins/sqldrivers/   # 应有 libqsqlmysql.so
```

---

## 6. 安装依赖（一次性）

在 Ubuntu 22.04 / WSL 终端执行：

```bash
sudo apt update
sudo apt install -y \
  build-essential git cmake gdb ninja-build pkg-config \
  qt6-base-dev qt6-base-dev-tools \
  qt6-tools-dev qt6-tools-dev-tools \
  libqt6charts6-dev \
  qt6-webengine-dev \
  libgl1-mesa-dev libglu1-mesa-dev \
  libqt6sql6-mysql \
  mysql-server
```

注意包名：

- Charts 是 `libqt6charts6-dev`，不是 `qt6-charts-dev`
- WebEngine 是 `qt6-webengine-dev`，不是 `libqt6webengine6-dev`

启动 MySQL：

```bash
# 有 systemd 时
sudo systemctl enable --now mysql

# WSL 若 systemctl 不可用
sudo service mysql start
```

WSL 若 `systemctl` 完全不可用，可在 `/etc/wsl.conf` 写入：

```ini
[boot]
systemd=true
```

然后在 **Windows PowerShell** 执行 `wsl --shutdown`，再重新打开 WSL。

---

## 7. 准备数据库

在项目根目录操作。密码 `123456` 仅实训默认值。

```bash
sudo mysql <<'SQL'
CREATE DATABASE IF NOT EXISTS charging_system
  CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER IF NOT EXISTS 'charging_user'@'localhost'
  IDENTIFIED BY '123456';
GRANT ALL PRIVILEGES ON charging_system.* TO 'charging_user'@'localhost';
FLUSH PRIVILEGES;
SQL

mysql -u charging_user -p123456 charging_system < sql/schema.sql
```

连接参数（见 `server/config.h`）：

| 参数 | 值 |
|------|-----|
| 主机 | `127.0.0.1` |
| 端口 | `3306` |
| 库名 | `charging_system` |
| 用户 | `charging_user` |
| 密码 | `123456`（可用环境变量 `CHARGING_DB_PASSWORD` 覆盖） |

应有表：`user`、`admin`、`station`、`pile`、`charge_order`、`wallet_transactions`、`device_commands`、`operation_logs`、`schema_version`。

```bash
mysql -u charging_user -p123456 charging_system -e "SHOW TABLES;"
```

空库时，启动 `charging-server` 也会用打包进二进制的 `schema.sql` 建表；**已有库不会 DROP**，只做版本升级。改种子后请重新导入 `sql/schema.sql` 或重编并在空库上启动。

---

## 8. 编译

```bash
cd /path/to/charging-system

cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

改代码后只需：

```bash
cmake --build build
```

CMake 报找不到 Charts / WebEngine / OpenGL 时，按第 12 节补包后 **删掉 `build/` 再重新 cmake**。

---

## 9. 运行（必须开三个进程）

请先开 MySQL，再开服务端，最后开客户端。在**项目根目录**执行。

### 9.1 服务端（先启动）

```bash
cd /path/to/charging-system
./build/server/charging-server
```

成功时应看到类似：

```text
数据库结构版本: 3
充电系统服务器已启动，端口: 9000
```

**智能客服不用配 Key 也能演示。** 悬浮球、「E小充」窗口、关键词代预约都不依赖大模型。  
`AI_API_KEY` **不在工程目录里**（写在组员本机环境变量，不会随源码交上去）。只有需要「自由提问、大模型作答」时，才在启动服务端的同一终端执行：

```bash
export AI_API_KEY='你的密钥'
# 默认通义千问，一般不必再设：
# export AI_BASE_URL='https://dashscope.aliyuncs.com/compatible-mode'
# export AI_MODEL='qwen-plus'
./build/server/charging-server
```

不 export 时，自由提问会提示未配置 Key，**不影响登录、充电、导航、管理端。**

### 9.2 用户端

```bash
cd /path/to/charging-system
export QTWEBENGINEPROCESS_PATH=/usr/lib/qt6/libexec/QtWebEngineProcess
QT_QPA_PLATFORM=xcb ./build/client-user/charging-user
```

`config/runtime.ini` 已为 WSL 写了 `QT_QPA_PLATFORM=xcb` 和软件渲染；显式再 export 一次更稳妥。没有地图/导航需求时，缺 `QTWEBENGINEPROCESS_PATH` 也可能能进登录页，但进首页地图或导航页容易崩。

### 9.3 管理端

```bash
cd /path/to/charging-system
QT_QPA_PLATFORM=xcb ./build/client-admin/charging-admin
```

两端默认连接 **`127.0.0.1:9000`**。若在另一台机器打开客户端，需改三处 `kServerHost` 后重新编译：

- `client-user/loginwindow.cpp`
- `client-user/mainwindow.cpp`
- `client-admin/adminloginwindow.cpp`

改完必须重编对应用户端 / 管理端，只改源码不编译不会生效。

---

## 10. 演示账号与建议操作路径

| 角色 | 账号 | 密码 | 说明 |
|------|------|------|------|
| 普通用户 | `13800138001` | 无（免密） | 余额约 ¥92.50，适合完整充电闭环 |
| 普通用户 | `13800138002` | 无 | 余额 ¥300 |
| 冻结用户 | `13800138006` | 无 | 登录应提示冻结（code=6） |
| 管理员 | `admin` | `123456` | 预置账号，库中为哈希，无明文 |

未注册的 11 位手机号：登录页点 **注册** 会建号并直接登录。

**建议演示顺序（用户端）**

1. 用 `13800138001` 登录。
2. 选择区域、输入地址，点定位，查看附近电站卡片与地图 Marker。
3. 点某站「导航」，切换驾车 / 步行。
4. 进入电站 → 选 **闲置** 桩 → 预约 → 开始充电 → 结束充电 → 确认支付。
5. 点右下角悬浮球打开「E小充」（未配 `AI_API_KEY` 时仍可看窗口；代预约不依赖大模型）。

**建议演示顺序（管理端）**

1. `admin` / `123456` 登录。
2. 工作台 / 营收图切换 7 日、30 日。
3. 电桩列表筛选；充电中的桩重启应被拒绝。
4. 电站管理：点一行看右侧站内桩。
5. 用户管理：对测试号冻结 / 解冻。
6. 订单管理：筛选、打开详情。

---

## 11. 配置与环境变量

| 变量 / 文件 | 谁读 | 作用 |
|-------------|------|------|
| `CHARGING_DB_PASSWORD` | 服务端 | 覆盖默认库密码 `123456` |
| `AI_API_KEY` | 服务端 `AiService` | 大模型密钥。**不在仓库里**，不配也能跑；只影响自由问答 |
| `AI_BASE_URL` / `AI_MODEL` | 服务端 | 可选；代码默认通义 `qwen-plus` |
| `config/amap.ini` | 用户端 | 高德 JS Key、安全密钥、Web 服务 Key |
| `config/runtime.ini` | 用户端启动时 | `LIBGL_ALWAYS_SOFTWARE`、`QT_QPA_PLATFORM`、Chromium 参数 |
| `QTWEBENGINEPROCESS_PATH` | 用户端进程 | 指向 `QtWebEngineProcess`，WSL 下建议设置 |
| `QT_QPA_PLATFORM=xcb` | 两个 GUI | WSLg / 部分 Linux 桌面需要 |

高德 Key 已放在仓库 `config/amap.ini`，本机演示一般可直接用。若配额耗尽或 Key 失效，到高德控制台换新 Key 后改该文件（不必改源码）。

---

## 12. 通信协议（摘要）

一条消息 = **4 字节大端无符号长度** + **UTF-8 JSON**（`common/protocol.h`）。

```text
请求  { "type": "<接口名>", "token": "<登录下发的 UUID，登录类不带>", "data": { ... } }
响应  { "type": "<接口名>", "code": <int>, "msg": "<提示>", "data": { ... } }
```

| code | 含义 |
|-----:|------|
| 0 | 成功 |
| 2 | 请求格式错误 |
| 3 | 数据库错误 |
| 4 | 数据不存在 |
| 5 | 认证失败 |
| 6 | 账号冻结 |
| 7 | 余额不足 |
| 8 | 已有未完成订单 |
| 9 | token 无效 / 过期 / 角色不符（客户端应回登录页） |

充电闭环：`pile_list` → `reserve` → `start_charge` → `finish_charge`（出账、释放桩）→ `pay_charge`（扣款）。旧接口 `settle` 已停用。

完整字段见 `docs/api-contract.md`，需求条目见 `docs/requirements-matrix.md`。

---

## 13. 常见问题

| 现象 | 处理 |
|------|------|
| `QMYSQL 驱动不可用` | `sudo apt install libqt6sql6-mysql` |
| `数据库连接失败` | 先 `sudo service mysql start`；确认用户 `charging_user` 与库 `charging_system` 已创建 |
| `监听失败` 端口 9000 | 已有旧进程：`pkill -x charging-server` 后再开 |
| 客户端连不上 | 必须先开服务端；默认只连 `127.0.0.1` |
| CMake 找不到 Charts | `sudo apt install libqt6charts6-dev`，然后 `rm -rf build` 再 cmake |
| CMake 找不到 WebEngineWidgets | `sudo apt install qt6-webengine-dev`，同样要清 `build/` |
| `WrapOpenGL not found` | `sudo apt install libgl1-mesa-dev` |
| 用户端一进地图 / 导航就崩 | 设置 `QTWEBENGINEPROCESS_PATH`，并用 `QT_QPA_PLATFORM=xcb` |
| GUI 不弹窗 | 检查 `$DISPLAY`（WSLg 一般为 `:0`） |
| 登录「正在注册」很久 | 主窗口构造较重，属正常；确认服务端已启动且库已导入 |
| 导航空白 / 定位失败 | 检查本机能否访问 `restapi.amap.com`；必要时更新 `config/amap.ini` |
| AI 只提示未配置 Key | 给**服务端**进程 `export AI_API_KEY=...` 后重启服务端 |
| 改了服务器 IP 客户端仍连本机 | 改 `kServerHost` 后必须重新 `cmake --build build` |

---

## 14. 可选：协议自测

需已启动 `charging-server` 且库中有种子数据：

```bash
python3 tests/test_session_security.py
```

同目录还有管理端订单 / 电桩 / 用户列表等脚本，失败时看脚本内注释的前置条件。

---

## 15. 给老师的提交物（源码包，不要只丢 GitHub 链接）

请交**本地源码压缩包**（或 U 盘），不要让老师「重新 clone GitHub」作为唯一来源：本机还有未推送的 `README.md`、更新后的需求矩阵等。老师机上仍需按第 6～9 节**自己编译**，不要指望拷贝 `build/` 里的可执行文件（和你电脑的路径、Qt 安装位置绑定）。

打包示例（在项目**上一级**目录执行）：

```bash
cd ~
tar --exclude='charging-system/build' \
    --exclude='charging-system/.git' \
    --exclude='charging-system/.cursor' \
    -czf charging-system-submit.tar.gz charging-system
```

| 要打进去 | 不要打进去 |
|----------|------------|
| 源码、`sql/`、`config/amap.ini`、`README.md`、`docs/` | `build/`（体积大且换机器往往跑不了） |
| | `.git/`（可选去掉，减小体积） |
| | 本机 `~/.bashrc` 里的 `AI_API_KEY`（不会自动进压缩包） |

---

## 16. 其它文档

| 文件 | 内容 |
|------|------|
| `docs/requirements-matrix.md` | 需求矩阵（条目、负责人、完成状态） |
| `docs/api-contract.md` | 报文接口契约 |
| `docs/dev-environment.md` | 开发环境安装备忘 |
| `docs/midterm-review.md` | 结构与功能对照 |
| `docs/thread-model.md` | 服务端线程模型 |
| `sql/schema.sql` | 表结构与演示数据 |
