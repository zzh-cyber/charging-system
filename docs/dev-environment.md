# 开发环境统一配置说明

> 目标：全组在 **WSL Ubuntu 22.04** 内开发，尽量对齐老师 **BitDev** 环境。  
> **不要** 在 Windows 本机装开发工具，**不要** 升级到 Qt 6.2.4 以上版本。

---

## 1. 环境版本标准（全组统一）

| 组件 | 要求版本 | 当前已验证 |
|------|----------|------------|
| 操作系统 | Ubuntu **22.04 LTS** | Ubuntu 22.04.5 LTS |
| GCC / G++ | **11.4** | 11.4.0 |
| CMake | **3.22.1** | 3.22.1 |
| GDB | **12.1** | 12.1 |
| Git | 2.x | 2.34.1 |
| Ninja | 1.x | 1.10.1 |
| Qt | **6.2.4**（不升级） | 6.2.4 |
| MySQL Server | 8.0.x | 8.0.46 |
| 开发方式 | WSL2 + Cursor / Qt Creator | WSLg GUI 可用 |

---

## 2. 新成员环境安装（一次性）

在 **WSL Ubuntu 22.04** 终端执行：

```bash
# 基础工具链
sudo apt update
sudo apt install -y build-essential git cmake gdb ninja pkg-config

# Qt 6.2.4 开发包（Ubuntu 22.04 仓库，不要装更高版本）
sudo apt install -y \
  qt6-base-dev qt6-base-dev-tools \
  qt6-tools-dev qt6-tools-dev-tools \
  qt6-declarative-dev \
  libqt6serialport6-dev \
  libgl1-mesa-dev libglu1-mesa-dev \
  designer-qt6 \
  libqt6sql6-mysql

# MySQL（本地数据库）
sudo apt install -y mysql-server
sudo systemctl enable --now mysql
```

### WSL 启用 systemd（若 `systemctl status mysql` 不可用）

编辑 `/etc/wsl.conf`：

```ini
[boot]
systemd=true

[user]
default=你的用户名
```

保存后在 **Windows PowerShell** 执行 `wsl --shutdown`，再重新打开 WSL。

---

## 3. 版本自检命令（每人跑一遍，结果应一致）

```bash
cat /etc/os-release | grep PRETTY_NAME
gcc --version | head -1
g++ --version | head -1
cmake --version | head -1
gdb --version | head -1
git --version
ninja --version
qmake6 --version
mysql --version
systemctl is-active mysql
echo "DISPLAY=$DISPLAY WAYLAND_DISPLAY=$WAYLAND_DISPLAY"
```

**期望输出要点：**
- `Ubuntu 22.04.x LTS`
- `Qt version 6.2.4`
- `mysql ... active`
- `DISPLAY=:0`、`WAYLAND_DISPLAY=wayland-0`（WSLg 正常）

---

## 4. MySQL 数据库配置（全组统一）

### 4.1 创建库和用户（组长执行一次，或每人本地各执行一次）

```bash
sudo mysql <<'SQL'
CREATE DATABASE IF NOT EXISTS charging_system
  CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER IF NOT EXISTS 'charging_user'@'localhost'
  IDENTIFIED BY '123456';
GRANT ALL PRIVILEGES ON charging_system.* TO 'charging_user'@'localhost';
FLUSH PRIVILEGES;
SQL
```

> 密码 `123456` 仅用于实训本地开发，**不要提交到 Git**。

### 4.2 导入表结构和测试数据

在项目根目录执行：

```bash
mysql -u charging_user -p123456 charging_system < sql/schema.sql
```

### 4.3 连接参数（代码里统一用这个）

| 参数 | 值 |
|------|-----|
| 驱动 | QMYSQL |
| 主机 | 127.0.0.1 |
| 端口 | 3306 |
| 数据库 | charging_system |
| 用户 | charging_user |
| 密码 | 123456 |

配置文件位置：`server/config.h`

### 4.4 验证数据库

```bash
mysql -u charging_user -p123456 charging_system -e "SHOW TABLES;"
```

应看到 6 张表：`user`、`admin`、`station`、`pile`、`charge_order`、`recharge`。

---

## 5. Qt / GUI 特别说明

### 5.1 Qt Designer 启动方式

**不要用** `/usr/bin/designer`（会报 `could not find a Qt installation`）。

正确方式：

```bash
QT_QPA_PLATFORM=xcb /usr/lib/qt6/bin/designer
```

### 5.2 SQL 驱动检查

```bash
ls /usr/lib/x86_64-linux-gnu/qt6/plugins/sqldrivers/
```

应包含：`libqsqlite.so`、`libqsqlmysql.so`。

### 5.3 GUI 程序运行（WSLg）

客户端需在 WSL 内运行，窗口会弹到 Windows 桌面：

```bash
QT_QPA_PLATFORM=xcb ./build/client-user/charging-user
```

---

## 6. 项目构建与运行（全组统一流程）

### 6.1 克隆仓库后首次构建

```bash
cd charging-system

# 导入数据库（若尚未导入）
mysql -u charging_user -p123456 charging_system < sql/schema.sql

# 构建
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### 6.2 日常运行（开 3 个终端）

**终端 1 — 服务器（必须先启动）**

```bash
./build/server/charging-server
# 应输出：充电系统服务器已启动，端口: 9000
```

**终端 2 — 用户端**

```bash
QT_QPA_PLATFORM=xcb ./build/client-user/charging-user
# 测试账号：13800138001
```

**终端 3 — 管理端**

```bash
QT_QPA_PLATFORM=xcb ./build/client-admin/charging-admin
# 默认账号：admin / 123456
```

### 6.3 修改代码后重新编译

```bash
cmake --build build
```

---

## 7. 项目目录说明

```
charging-system/
├── common/          # 公共层：协议 + 网络基类（组长维护）
├── server/          # 服务器：TCP + 多线程 + MySQL（成员A）
├── client-user/     # 充电用户端（成员B + D）
├── client-admin/    # PC 管理端（成员C）
├── sql/schema.sql   # 数据库脚本（成员D）
├── docs/
│   ├── dev-environment.md   # 本文件
│   └── api-contract.md      # Socket 接口契约
└── env-check/       # 环境验证 demo（参考用，不参与正式开发）
```

---

## 8. 常见问题

| 问题 | 原因 | 解决 |
|------|------|------|
| CMake 报 `WrapOpenGL not found` | 缺 OpenGL 开发头文件 | `sudo apt install libgl1-mesa-dev` |
| `QMYSQL` 驱动不可用 | 缺 MySQL 驱动包 | `sudo apt install libqt6sql6-mysql` |
| `designer: could not find Qt installation` | 用了 qtchooser 软链 | 改用 `/usr/lib/qt6/bin/designer` |
| 客户端连不上服务器 | 服务器未启动 | 先运行 `./build/server/charging-server` |
| GUI 不弹窗 | WSLg 未就绪 | 检查 `$DISPLAY` 和 `$WAYLAND_DISPLAY` |
| MySQL 连不上 | 服务未启动 | `sudo systemctl start mysql` |

---

## 9. 后续可选模块（本期不做，二期再装）

```bash
# 销售业绩图表 QChart
sudo apt install -y qt6-charts-dev

# 腾讯地图导航 QWebEngineView
sudo apt install -y qt6-webengine-dev
```

---

## 10. 全组统一检查清单（每人勾选）

- [ ] Ubuntu 22.04.5 LTS
- [ ] gcc/g++ 11.4.0
- [ ] cmake 3.22.1
- [ ] Qt 6.2.4（`qmake6 --version`）
- [ ] MySQL active，6 张表已导入
- [ ] QMYSQL 驱动存在
- [ ] 三端编译通过（server / user / admin）
- [ ] 服务器 9000 端口监听正常
- [ ] 用户端 13800138001 登录成功
- [ ] 管理端 admin/123456 登录成功
