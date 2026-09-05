-- 新能源汽车充电管理系统 - 数据库结构与测试数据
-- 目标库：charging_system（已由 charging_user 拥有全部权限）
-- 执行方式：mysql -u charging_user -p charging_system < sql/schema.sql
--
-- 说明：
--   以 PR #11 为基线（user/admin/station/pile/charge_order/wallet_transactions/schema_version）。
--   本脚本补齐 device_commands、operation_logs，以及 pile/charge_order 缺字段。
--   钱包表沿用 PR #11 定义，不再引入第二份 wallet_transactions。
--   状态类字段统一用 ENUM，便于在数据库里直接阅读。

USE charging_system;

SET FOREIGN_KEY_CHECKS = 0;
DROP TABLE IF EXISTS wallet_transactions;
DROP TABLE IF EXISTS device_commands;
DROP TABLE IF EXISTS operation_logs;
DROP TABLE IF EXISTS recharge;
DROP TABLE IF EXISTS charge_order;
DROP TABLE IF EXISTS pile;
DROP TABLE IF EXISTS station;
DROP TABLE IF EXISTS admin;
DROP TABLE IF EXISTS `user`;
DROP TABLE IF EXISTS schema_version;
SET FOREIGN_KEY_CHECKS = 1;

-- 充电用户
CREATE TABLE `user` (
    id            BIGINT        NOT NULL AUTO_INCREMENT,
    phone         VARCHAR(11)   NOT NULL,
    nickname      VARCHAR(64)   NOT NULL DEFAULT '',
    avatar        VARCHAR(255)  NOT NULL DEFAULT '',
    balance       DECIMAL(10,2) NOT NULL DEFAULT 0.00,
    status        ENUM('normal','frozen') NOT NULL DEFAULT 'normal',
    created_at    DATETIME      NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at    DATETIME      NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    last_login_at DATETIME      NULL,
    PRIMARY KEY (id),
    UNIQUE KEY uk_user_phone (phone),
    KEY idx_user_status (status),
    KEY idx_user_created (created_at),
    CONSTRAINT chk_user_balance CHECK (balance >= 0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 管理员
CREATE TABLE admin (
    id            BIGINT       NOT NULL AUTO_INCREMENT,
    username      VARCHAR(64)  NOT NULL,
    password_hash VARCHAR(128) NOT NULL,
    salt          VARCHAR(64)  NOT NULL,
    role          VARCHAR(32)  NOT NULL DEFAULT 'admin',
    status        ENUM('active','disabled') NOT NULL DEFAULT 'active',
    last_login_at DATETIME     NULL,
    created_at    DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at    DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    UNIQUE KEY uk_admin_username (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 充电站
CREATE TABLE station (
    id           BIGINT        NOT NULL AUTO_INCREMENT,
    station_code VARCHAR(32)   NOT NULL,
    name         VARCHAR(128)  NOT NULL,
    address      VARCHAR(255)  NOT NULL DEFAULT '',
    longitude    DECIMAL(10,6) NOT NULL DEFAULT 0,
    latitude     DECIMAL(10,6) NOT NULL DEFAULT 0,
    price        DECIMAL(6,2)  NOT NULL DEFAULT 1.00,   -- 元/度
    enabled      TINYINT(1)    NOT NULL DEFAULT 1,
    created_at   DATETIME      NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at   DATETIME      NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    UNIQUE KEY uk_station_code (station_code),
    KEY idx_station_enabled (enabled),
    KEY idx_station_name (name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 充电桩
CREATE TABLE pile (
    id              BIGINT      NOT NULL AUTO_INCREMENT,
    station_id      BIGINT      NOT NULL,
    code            VARCHAR(32) NOT NULL,                 -- 电桩编号，如 SZ001-01
    type            ENUM('fast','slow') NOT NULL DEFAULT 'fast',   -- 快充/慢充
    power_kw        DECIMAL(6,1) NOT NULL DEFAULT 120.0,           -- 功率 kW
    status          ENUM('idle','busy','fault') NOT NULL DEFAULT 'idle', -- 闲置/在用/故障
    current_user_id BIGINT      NULL,                     -- 当前占用用户（预约/充电中）
    last_online_at  DATETIME    NULL,                     -- 最近一次心跳时间
    enabled         TINYINT(1)  NOT NULL DEFAULT 1,
    total_count     INT         NOT NULL DEFAULT 0,       -- 累计充电次数
    total_hours     DECIMAL(10,1) NOT NULL DEFAULT 0,     -- 累计充电时长
    PRIMARY KEY (id),
    UNIQUE KEY uk_pile_code (code),
    KEY idx_pile_station (station_id),
    KEY idx_pile_station_status (station_id, status),
    CONSTRAINT fk_pile_station FOREIGN KEY (station_id) REFERENCES station (id),
    CONSTRAINT fk_pile_user FOREIGN KEY (current_user_id) REFERENCES `user` (id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 充电订单（预约-充电-计费-结算）
CREATE TABLE charge_order (
    id               BIGINT        NOT NULL AUTO_INCREMENT,
    order_no         VARCHAR(32)   NOT NULL,
    user_id          BIGINT        NOT NULL,
    station_id       BIGINT        NOT NULL,
    pile_id          BIGINT        NOT NULL,
    status           ENUM('reserved','charging','pending_payment','settled','cancelled') NOT NULL DEFAULT 'reserved',
    unit_price       DECIMAL(6,2)  NOT NULL DEFAULT 0.00,   -- 下单时固化的单价（元/度）
    reserve_time     DATETIME      NULL,
    start_time       DATETIME      NULL,
    end_time         DATETIME      NULL,
    duration_seconds INT           NOT NULL DEFAULT 0,
    kwh              DECIMAL(10,2) NOT NULL DEFAULT 0,      -- 充电电量（度）
    amount           DECIMAL(10,2) NOT NULL DEFAULT 0,      -- 结算金额（元）
    pay_request_id   VARCHAR(64)   NULL,                    -- 结算幂等键
    created_at       DATETIME      NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at       DATETIME      NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    UNIQUE KEY uk_order_no (order_no),
    UNIQUE KEY uk_pay_request (pay_request_id),
    KEY idx_order_user (user_id),
    KEY idx_order_pile (pile_id),
    KEY idx_order_station (station_id),
    KEY idx_order_user_status (user_id, status),
    KEY idx_order_end_status (end_time, status),
    KEY idx_order_pile_start (pile_id, start_time),
    CONSTRAINT fk_order_user FOREIGN KEY (user_id) REFERENCES `user` (id),
    CONSTRAINT fk_order_pile FOREIGN KEY (pile_id) REFERENCES pile (id),
    CONSTRAINT fk_order_station FOREIGN KEY (station_id) REFERENCES station (id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 充值流水
CREATE TABLE wallet_transactions (
    id             BIGINT        NOT NULL AUTO_INCREMENT,
    transaction_no VARCHAR(32)   NOT NULL,
    user_id        BIGINT        NOT NULL,
    type           ENUM('recharge','charge_pay','refund') NOT NULL,
    amount         DECIMAL(10,2) NOT NULL,
    balance_before DECIMAL(10,2) NOT NULL,
    balance_after  DECIMAL(10,2) NOT NULL,
    order_id       BIGINT        NULL,
    created_at     DATETIME      NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    UNIQUE KEY uk_txn_no (transaction_no),
    KEY idx_txn_user (user_id),
    KEY idx_txn_order (order_id),
    CONSTRAINT fk_txn_user FOREIGN KEY (user_id) REFERENCES `user` (id),
    CONSTRAINT fk_txn_order FOREIGN KEY (order_id) REFERENCES charge_order (id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 数据库结构版本（启动时检测用）
CREATE TABLE schema_version (
    version     INT          NOT NULL,
    description VARCHAR(255) NOT NULL DEFAULT '',
    applied_at  DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (version)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 设备指令（远程重启等，记录下发与执行结果）
CREATE TABLE device_commands (
    id          BIGINT      NOT NULL AUTO_INCREMENT,
    command_no  VARCHAR(64) NOT NULL,
    pile_id     BIGINT      NOT NULL,
    command     VARCHAR(32) NOT NULL,                    -- 如 restart
    status      ENUM('pending','success','failed') NOT NULL DEFAULT 'pending',
    request_at  DATETIME    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    response_at DATETIME    NULL,
    error_code  VARCHAR(32) NULL,
    PRIMARY KEY (id),
    UNIQUE KEY uk_device_command_no (command_no),
    KEY idx_device_pile_status (pile_id, status),
    CONSTRAINT fk_device_pile FOREIGN KEY (pile_id) REFERENCES pile (id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 运维操作日志（远程重启 / 冻结用户 / 新增电站等关键写操作审计）
CREATE TABLE operation_logs (
    id           BIGINT      NOT NULL AUTO_INCREMENT,
    admin_id     BIGINT      NOT NULL,
    action       VARCHAR(64) NOT NULL,                   -- 如 pile_restart / user_freeze
    target_type  VARCHAR(32) NOT NULL DEFAULT '',        -- 如 pile / user / station
    target_id    BIGINT      NULL,
    before_value VARCHAR(255) NOT NULL DEFAULT '',
    after_value  VARCHAR(255) NOT NULL DEFAULT '',
    result       ENUM('success','failed') NOT NULL DEFAULT 'success',
    reason       VARCHAR(255) NOT NULL DEFAULT '',
    created_at   DATETIME    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    KEY idx_oplog_admin_time (admin_id, created_at),
    CONSTRAINT fk_oplog_admin FOREIGN KEY (admin_id) REFERENCES admin (id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO schema_version (version, description) VALUES
(1, 'PR#11 基线：核心表 + wallet_transactions + 管理员加盐哈希'),
(2, '补齐 device_commands / operation_logs，pile 占用与心跳字段，charge_order pending_payment');

-- ===================== 初始 / 测试数据 =====================

-- 默认管理员（密码 123456 以 SHA256(salt+密码) 入库，非明文）
INSERT INTO admin (username, password_hash, salt, role, status) VALUES
('admin', '9f2986ef2d5671e67d7d65439ecaef19a2e3c94f5b368e8eca9693cc27116474', '7f3a9c1e5b8d2f04', 'admin', 'active');

-- 充电站
INSERT INTO station (station_code, name, address, longitude, latitude, price, enabled) VALUES
('SZ001', '深圳市民中心充电站', '深圳市福田区福中三路市民中心停车场', 114.061000, 22.546000, 1.20, 1),
('SZ002', '福田CBD充电站',     '深圳市福田区益田路卓越世纪中心',       114.058000, 22.532000, 1.60, 1),
('SZ003', '南山科技园充电站',   '深圳市南山区科技园南区高新南一道',     113.945000, 22.540000, 1.30, 1);

-- 充电桩（站1：6桩；站2：4桩；站3：4桩）
INSERT INTO pile (station_id, code, type, power_kw, status, total_count, total_hours) VALUES
(1, 'SZ001-01', 'fast', 120.0, 'idle',  120, 300.5),
(1, 'SZ001-02', 'fast', 120.0, 'busy',   98, 250.0),
(1, 'SZ001-03', 'fast', 120.0, 'fault',  40, 110.0),
(1, 'SZ001-04', 'fast', 120.0, 'idle',   77, 190.0),
(1, 'SZ001-05', 'slow',   7.0, 'idle',   30,  95.5),
(1, 'SZ001-06', 'slow',   7.0, 'idle',   25,  80.0),
(2, 'SZ002-01', 'fast', 120.0, 'idle',   60, 150.0),
(2, 'SZ002-02', 'fast', 120.0, 'busy',   55, 140.0),
(2, 'SZ002-03', 'slow',   7.0, 'idle',   20,  60.0),
(2, 'SZ002-04', 'slow',   7.0, 'fault',  10,  30.0),
(3, 'SZ003-01', 'fast', 180.0, 'idle',   88, 220.0),
(3, 'SZ003-02', 'fast', 180.0, 'idle',   90, 230.0),
(3, 'SZ003-03', 'fast', 120.0, 'busy',   70, 175.0),
(3, 'SZ003-04', 'slow',   7.0, 'idle',   15,  45.0);

-- 测试用户
INSERT INTO `user` (phone, nickname, balance, status) VALUES
('13800138001', '用户8001', 92.50, 'normal'),
('13800138002', '用户8002', 300.00, 'normal'),
('13800138006', '用户8006', 5.00, 'frozen');

-- 一条已结算订单示例
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id)
VALUES ('CD20260828001', 1, 1, 1, 'settled', 1.20, '2026-08-28 11:20:00', '2026-08-28 11:29:00', '2026-08-28 12:05:00', 2160, 30.00, 36.00, 'PAY20260828001');

-- 跨月已结算订单（NO.56 营收趋势测试数据）
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id) VALUES
('CD20260715001', 2, 1, 2, 'settled', 1.20, '2026-07-15 09:00:00', '2026-07-15 09:10:00', '2026-07-15 10:00:00', 3000, 40.00, 48.00, 'PAY20260715001'),
('CD20260810001', 1, 2, 7, 'settled', 1.60, '2026-08-10 14:00:00', '2026-08-10 14:10:00', '2026-08-10 15:00:00', 3000, 30.00, 48.00, 'PAY20260810001'),
('CD20260901001', 2, 3, 11, 'settled', 1.30, '2026-09-01 08:00:00', '2026-09-01 08:05:00', '2026-09-01 09:00:00', 3300, 50.00, 65.00, 'PAY20260901001');

-- 只读视图：电站汇总（总桩数、空闲桩数）
DROP VIEW IF EXISTS v_station_summary;
CREATE VIEW v_station_summary AS
SELECT s.id AS station_id, s.station_code, s.name,
       (SELECT COUNT(*) FROM pile p WHERE p.station_id = s.id) AS pile_count,
       (SELECT COUNT(*) FROM pile p WHERE p.station_id = s.id AND p.status = 'idle') AS idle_count
FROM station s;

-- 只读视图：电桩使用统计（充电次数、累计时长）
DROP VIEW IF EXISTS v_pile_usage;
CREATE VIEW v_pile_usage AS
SELECT p.id AS pile_id, p.code AS pile_code,
       COUNT(o.id) AS charge_count,
       COALESCE(SUM(o.duration_seconds), 0) AS total_duration_seconds
FROM pile p
LEFT JOIN charge_order o ON o.pile_id = p.id AND o.status = 'settled'
GROUP BY p.id, p.code;
