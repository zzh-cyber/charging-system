-- 新能源汽车充电管理系统 - 数据库结构与测试数据
-- 目标库：charging_system（已由 charging_user 拥有全部权限）
-- 执行方式：mysql -u charging_user -p charging_system < sql/schema.sql
--
-- 说明：
--   本期只覆盖功能前三项（用户端 / PC 管理端 / 数据库端）所需的核心表。
--   状态类字段统一用 ENUM，便于在数据库里直接阅读。

USE charging_system;

SET FOREIGN_KEY_CHECKS = 0;
DROP TABLE IF EXISTS recharge;
DROP TABLE IF EXISTS charge_order;
DROP TABLE IF EXISTS pile;
DROP TABLE IF EXISTS station;
DROP TABLE IF EXISTS admin;
DROP TABLE IF EXISTS `user`;
SET FOREIGN_KEY_CHECKS = 1;

-- 充电用户
CREATE TABLE `user` (
    id          BIGINT       NOT NULL AUTO_INCREMENT,
    phone       VARCHAR(11)  NOT NULL,
    nickname    VARCHAR(64)  NOT NULL DEFAULT '',
    avatar      VARCHAR(255) NOT NULL DEFAULT '',
    balance     DECIMAL(10,2) NOT NULL DEFAULT 0.00,
    status      ENUM('normal','frozen') NOT NULL DEFAULT 'normal',
    created_at  DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    UNIQUE KEY uk_user_phone (phone)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 管理员
CREATE TABLE admin (
    id          BIGINT       NOT NULL AUTO_INCREMENT,
    username    VARCHAR(64)  NOT NULL,
    password    VARCHAR(128) NOT NULL,
    created_at  DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    UNIQUE KEY uk_admin_username (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 充电站
CREATE TABLE station (
    id          BIGINT       NOT NULL AUTO_INCREMENT,
    name        VARCHAR(128) NOT NULL,
    address     VARCHAR(255) NOT NULL DEFAULT '',
    longitude   DECIMAL(10,6) NOT NULL DEFAULT 0,
    latitude    DECIMAL(10,6) NOT NULL DEFAULT 0,
    price       DECIMAL(6,2) NOT NULL DEFAULT 1.00,   -- 元/度
    created_at  DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 充电桩
CREATE TABLE pile (
    id           BIGINT      NOT NULL AUTO_INCREMENT,
    station_id   BIGINT      NOT NULL,
    code         VARCHAR(32) NOT NULL,                 -- 电桩编号，如 SZ001-01
    type         ENUM('fast','slow') NOT NULL DEFAULT 'fast',   -- 快充/慢充
    power_kw     DECIMAL(6,1) NOT NULL DEFAULT 120.0,           -- 功率 kW
    status       ENUM('idle','busy','fault') NOT NULL DEFAULT 'idle', -- 闲置/在用/故障
    total_count  INT         NOT NULL DEFAULT 0,       -- 累计充电次数
    total_hours  DECIMAL(10,1) NOT NULL DEFAULT 0,     -- 累计充电时长
    PRIMARY KEY (id),
    UNIQUE KEY uk_pile_code (code),
    KEY idx_pile_station (station_id),
    CONSTRAINT fk_pile_station FOREIGN KEY (station_id) REFERENCES station (id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 充电订单（预约-充电-计费-结算）
CREATE TABLE charge_order (
    id            BIGINT      NOT NULL AUTO_INCREMENT,
    order_no      VARCHAR(32) NOT NULL,
    user_id       BIGINT      NOT NULL,
    pile_id       BIGINT      NOT NULL,
    status        ENUM('reserved','charging','settled','cancelled') NOT NULL DEFAULT 'reserved',
    reserve_time  DATETIME    NULL,
    start_time    DATETIME    NULL,
    end_time      DATETIME    NULL,
    kwh           DECIMAL(10,2) NOT NULL DEFAULT 0,    -- 充电电量（度）
    amount        DECIMAL(10,2) NOT NULL DEFAULT 0,    -- 结算金额（元）
    created_at    DATETIME    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    UNIQUE KEY uk_order_no (order_no),
    KEY idx_order_user (user_id),
    KEY idx_order_pile (pile_id),
    CONSTRAINT fk_order_user FOREIGN KEY (user_id) REFERENCES `user` (id),
    CONSTRAINT fk_order_pile FOREIGN KEY (pile_id) REFERENCES pile (id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 充值流水
CREATE TABLE recharge (
    id          BIGINT      NOT NULL AUTO_INCREMENT,
    user_id     BIGINT      NOT NULL,
    amount      DECIMAL(10,2) NOT NULL,
    created_at  DATETIME    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    KEY idx_recharge_user (user_id),
    CONSTRAINT fk_recharge_user FOREIGN KEY (user_id) REFERENCES `user` (id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ===================== 初始 / 测试数据 =====================

-- 默认管理员（注意：明文密码仅用于实训，正式环境应存哈希）
INSERT INTO admin (username, password) VALUES ('admin', '123456');

-- 充电站
INSERT INTO station (name, address, longitude, latitude, price) VALUES
('深圳市民中心充电站', '深圳市福田区福中三路市民中心停车场', 114.061000, 22.546000, 1.20),
('福田CBD充电站',     '深圳市福田区益田路卓越世纪中心',       114.058000, 22.532000, 1.60),
('南山科技园充电站',   '深圳市南山区科技园南区高新南一道',     113.945000, 22.540000, 1.30);

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
INSERT INTO charge_order (order_no, user_id, pile_id, status, reserve_time, start_time, end_time, kwh, amount)
VALUES ('CD20260828001', 1, 1, 'settled', '2026-08-28 11:20:00', '2026-08-28 11:29:00', '2026-08-28 12:05:00', 30.00, 36.00);
