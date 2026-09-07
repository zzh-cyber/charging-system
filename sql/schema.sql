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
    start_soc              DECIMAL(5,2) NULL,               -- 开始充电时模拟初始电量 %，仅展示
    battery_capacity_kwh   DECIMAL(6,2) NULL,               -- 模拟电池容量，仅展示
    target_soc             DECIMAL(5,2) NULL,               -- 模拟目标电量 %，仅展示
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
(2, '补齐 device_commands / operation_logs，pile 占用与心跳字段，charge_order pending_payment'),
(3, 'charge_order 增加模拟 SOC：start_soc / battery_capacity_kwh / target_soc');

-- ===================== 初始 / 测试数据 =====================

-- 默认管理员（密码 123456 以 SHA256(salt+密码) 入库，非明文）
INSERT INTO admin (username, password_hash, salt, role, status) VALUES
('admin', '9f2986ef2d5671e67d7d65439ecaef19a2e3c94f5b368e8eca9693cc27116474', '7f3a9c1e5b8d2f04', 'admin', 'active');

-- 充电站
INSERT INTO station (station_code, name, address, longitude, latitude, price, enabled) VALUES
('SZ001', '深圳市民中心充电站', '深圳市福田区福中三路市民中心停车场', 114.061000, 22.546000, 1.20, 1),
('SZ002', '福田CBD充电站',     '深圳市福田区益田路卓越世纪中心',       114.058000, 22.532000, 1.60, 1),
('SZ003', '南山科技园充电站',   '深圳市南山区科技园南区高新南一道',     113.945000, 22.540000, 1.30, 1);

INSERT INTO station (station_code, name, address, longitude, latitude, price, enabled) VALUES
('BJ001','北京国贸充电站','北京市朝阳区建国门外大街',116.46,39.908,1.5,1),
('BJ002','中关村充电站','北京市海淀区中关村大街',116.316,39.984,1.4,1),
('BJ003','北京西单充电站','北京市西城区西单北大街',116.38,39.91,1.3,1),
('BJ004','北京望京充电站','北京市朝阳区望京街',116.48,39.996,1.2,1),
('SH001','上海陆家嘴充电站','上海市浦东新区陆家嘴环路',121.5,31.24,1.6,1),
('SH002','上海虹桥充电站','上海市闵行区虹桥路',121.34,31.2,1.3,1),
('SH003','上海人民广场充电站','上海市黄浦区人民大道',121.47,31.23,1.5,1),
('SH004','上海张江充电站','上海市浦东新区张江路',121.59,31.2,1.4,1),
('GZ001','广州天河充电站','广州市天河区天河路',113.32,23.13,1.3,1),
('GZ002','广州珠江新城充电站','广州市天河区珠江新城',113.33,23.12,1.5,1),
('GZ003','广州白云充电站','广州市白云区白云大道',113.26,23.16,1.2,1),
('HZ001','杭州西湖充电站','杭州市西湖区文三路',120.15,30.27,1.2,1),
('HZ002','杭州滨江充电站','杭州市滨江区江南大道',120.21,30.21,1.4,1),
('HZ003','杭州未来科技城充电站','杭州市余杭区文一西路',120.02,30.28,1.3,1),
('NJ001','南京新街口充电站','南京市秦淮区中山南路',118.78,32.04,1.3,1),
('NJ002','南京河西充电站','南京市建邺区河西',118.72,32.0,1.2,1),
('NJ003','南京江宁充电站','南京市江宁区双龙大道',118.85,31.95,1.1,1);

INSERT INTO station (station_code, name, address, longitude, latitude, price, enabled) VALUES
('SZ004','深圳宝安机场充电站','深圳市宝安区机场南路深圳宝安国际机场',113.66,22.526,1.1,1),
('SZ005','罗湖万象城充电站','深圳市罗湖区宝安南路1881号万象城',113.675,22.538,1.2,1),
('SZ006','龙岗大运中心充电站','深圳市龙岗区龙翔大道大运中心体育馆',113.69,22.55,1.3,1),
('SZ007','龙华深圳北站充电站','深圳市龙华区民治街道致远中路深圳北站',113.705,22.562,1.4,1),
('SZ008','光明科学城充电站','深圳市光明区光明大道光明科学城',113.72,22.574,1.5,1),
('SZ009','坪山比亚迪充电站','深圳市坪山区比亚迪路比亚迪总部',113.735,22.526,1.1,1),
('SZ010','盐田海鲜街充电站','深圳市盐田区盐田海鲜街',113.75,22.538,1.2,1),
('SZ011','大鹏较场尾充电站','深圳市大鹏新区较场尾海滩',113.765,22.55,1.3,1),
('SZ012','前海自贸区充电站','深圳市南山区前海大道前海深港合作区',113.78,22.562,1.4,1),
('BJ005','三里屯充电站','北京市朝阳区三里屯路19号',116.145,39.924,1.5,1),
('BJ006','北京南站充电站','北京市丰台区北京南站路',116.16,39.876,1.1,1),
('BJ007','奥林匹克公园充电站','北京市朝阳区北辰东路奥林匹克公园',116.175,39.888,1.2,1),
('BJ008','五道口充电站','北京市海淀区成府路五道口',116.19,39.9,1.3,1),
('BJ009','王府井充电站','北京市东城区王府井大街',116.205,39.912,1.4,1),
('BJ010','西二旗充电站','北京市海淀区西二旗大街',116.22,39.924,1.5,1),
('BJ011','通州万达充电站','北京市通州区新华西街万达广场',116.235,39.876,1.1,1),
('BJ012','大兴机场充电站','北京市大兴区大兴国际机场',116.25,39.888,1.2,1),
('SH005','外滩充电站','上海市黄浦区中山东一路外滩',121.335,31.23,1.3,1),
('SH006','徐家汇充电站','上海市徐汇区漕溪北路徐家汇',121.35,31.242,1.4,1),
('SH007','静安寺充电站','上海市静安区南京西路静安寺',121.365,31.254,1.5,1),
('SH008','五角场充电站','上海市杨浦区淞沪路五角场',121.38,31.206,1.1,1),
('SH009','上海迪士尼充电站','上海市浦东新区川沙迪士尼乐园',121.395,31.218,1.2,1),
('SH010','上海火车站充电站','上海市静安区秣陵路上海火车站',121.41,31.23,1.3,1),
('SH011','莘庄充电站','上海市闵行区莘庄地铁站',121.425,31.242,1.4,1),
('SH012','长寿路充电站','上海市普陀区长寿路',121.44,31.254,1.5,1),
('GZ004','广州塔充电站','广州市海珠区阅江西路广州塔',113.245,23.106,1.1,1),
('GZ005','北京路步行街充电站','广州市越秀区北京路步行街',113.26,23.118,1.2,1),
('GZ006','琶洲会展中心充电站','广州市海珠区阅江中路琶洲展馆',113.275,23.13,1.3,1),
('GZ007','广州南站充电站','广州市番禺区广州南站',113.29,23.142,1.4,1),
('GZ008','白云机场充电站','广州市白云区白云国际机场',113.305,23.154,1.5,1),
('GZ009','黄埔科学城充电站','广州市黄埔区科学城',113.32,23.106,1.1,1),
('GZ010','沙面岛充电站','广州市荔湾区沙面大街',113.335,23.118,1.2,1),
('GZ011','大学城充电站','广州市番禺区大学城',113.35,23.13,1.3,1),
('GZ012','花都广场充电站','广州市花都区花都广场',113.365,23.142,1.4,1),
('HZ004','武林广场充电站','杭州市拱墅区武林广场',120.27,30.294,1.5,1),
('HZ005','钱江新城充电站','杭州市上城区钱江新城',120.285,30.246,1.1,1),
('HZ006','萧山机场充电站','杭州市萧山区萧山国际机场',120.3,30.258,1.2,1),
('HZ007','良渚文化村充电站','杭州市余杭区良渚文化村',120.315,30.27,1.3,1),
('HZ008','西溪湿地充电站','杭州市西湖区西溪湿地',120.33,30.282,1.4,1),
('HZ009','临平银泰充电站','杭州市临平区迎宾路银泰城',120.345,30.294,1.5,1),
('HZ010','富阳鹿山充电站','杭州市富阳区鹿山大道',120.36,30.246,1.1,1),
('HZ011','桐庐富春江充电站','杭州市桐庐县富春江畔',120.375,30.258,1.2,1),
('HZ012','九堡客运中心充电站','杭州市上城区九堡客运中心',120.39,30.27,1.3,1),
('NJ004','夫子庙充电站','南京市秦淮区贡院街夫子庙',119.035,32.072,1.4,1),
('NJ005','玄武湖充电站','南京市玄武区玄武湖公园',119.05,32.084,1.5,1),
('NJ006','南京南站充电站','南京市雨花台区南京南站',119.065,32.036,1.1,1),
('NJ007','中山陵充电站','南京市玄武区中山陵',119.08,32.048,1.2,1),
('NJ008','鼓楼广场充电站','南京市鼓楼区鼓楼广场',119.095,32.06,1.3,1),
('NJ009','长江大桥充电站','南京市浦口区长江大桥',119.11,32.072,1.4,1),
('NJ010','栖霞山充电站','南京市栖霞区栖霞山',119.125,32.084,1.5,1),
('NJ011','溧水万达充电站','南京市溧水区万达广场',119.14,32.036,1.1,1),
('NJ012','高淳老街充电站','南京市高淳区高淳老街',119.155,32.048,1.2,1);

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

INSERT INTO pile (station_id, code, type, power_kw, status, total_count, total_hours) VALUES
(4, 'BJ001-01', 'fast', 120.0, 'busy', 0, 0),
(4, 'BJ001-02', 'fast', 120.0, 'fault', 0, 0),
(4, 'BJ001-03', 'fast', 180.0, 'idle', 0, 0),
(4, 'BJ001-04', 'fast', 180.0, 'idle', 0, 0),
(4, 'BJ001-05', 'fast', 180.0, 'idle', 0, 0),
(4, 'BJ001-06', 'slow', 7.0, 'idle', 0, 0),
(5, 'BJ002-01', 'fast', 120.0, 'busy', 0, 0),
(5, 'BJ002-02', 'fast', 120.0, 'fault', 0, 0),
(5, 'BJ002-03', 'fast', 180.0, 'idle', 0, 0),
(5, 'BJ002-04', 'slow', 7.0, 'idle', 0, 0),
(6, 'BJ003-01', 'fast', 120.0, 'busy', 0, 0),
(6, 'BJ003-02', 'fast', 120.0, 'fault', 0, 0),
(6, 'BJ003-03', 'fast', 180.0, 'idle', 0, 0),
(6, 'BJ003-04', 'fast', 180.0, 'idle', 0, 0),
(6, 'BJ003-05', 'slow', 7.0, 'idle', 0, 0),
(7, 'BJ004-01', 'fast', 120.0, 'busy', 0, 0),
(7, 'BJ004-02', 'fast', 120.0, 'fault', 0, 0),
(7, 'BJ004-03', 'fast', 180.0, 'idle', 0, 0),
(7, 'BJ004-04', 'slow', 7.0, 'idle', 0, 0),
(8, 'SH001-01', 'fast', 120.0, 'busy', 0, 0),
(8, 'SH001-02', 'fast', 120.0, 'fault', 0, 0),
(8, 'SH001-03', 'fast', 180.0, 'idle', 0, 0),
(8, 'SH001-04', 'fast', 180.0, 'idle', 0, 0),
(8, 'SH001-05', 'fast', 180.0, 'idle', 0, 0),
(8, 'SH001-06', 'slow', 7.0, 'idle', 0, 0),
(9, 'SH002-01', 'fast', 120.0, 'busy', 0, 0),
(9, 'SH002-02', 'fast', 120.0, 'fault', 0, 0),
(9, 'SH002-03', 'fast', 180.0, 'idle', 0, 0),
(9, 'SH002-04', 'slow', 7.0, 'idle', 0, 0),
(10, 'SH003-01', 'fast', 120.0, 'busy', 0, 0),
(10, 'SH003-02', 'fast', 120.0, 'fault', 0, 0),
(10, 'SH003-03', 'fast', 180.0, 'idle', 0, 0),
(10, 'SH003-04', 'fast', 180.0, 'idle', 0, 0),
(10, 'SH003-05', 'slow', 7.0, 'idle', 0, 0),
(11, 'SH004-01', 'fast', 120.0, 'busy', 0, 0),
(11, 'SH004-02', 'fast', 120.0, 'fault', 0, 0),
(11, 'SH004-03', 'fast', 180.0, 'idle', 0, 0),
(11, 'SH004-04', 'slow', 7.0, 'idle', 0, 0),
(12, 'GZ001-01', 'fast', 120.0, 'busy', 0, 0),
(12, 'GZ001-02', 'fast', 120.0, 'fault', 0, 0),
(12, 'GZ001-03', 'fast', 180.0, 'idle', 0, 0),
(12, 'GZ001-04', 'fast', 180.0, 'idle', 0, 0),
(12, 'GZ001-05', 'fast', 180.0, 'idle', 0, 0),
(12, 'GZ001-06', 'slow', 7.0, 'idle', 0, 0),
(13, 'GZ002-01', 'fast', 120.0, 'busy', 0, 0),
(13, 'GZ002-02', 'fast', 120.0, 'fault', 0, 0),
(13, 'GZ002-03', 'fast', 180.0, 'idle', 0, 0),
(13, 'GZ002-04', 'slow', 7.0, 'idle', 0, 0),
(14, 'GZ003-01', 'fast', 120.0, 'busy', 0, 0),
(14, 'GZ003-02', 'fast', 120.0, 'fault', 0, 0),
(14, 'GZ003-03', 'fast', 180.0, 'idle', 0, 0),
(14, 'GZ003-04', 'fast', 180.0, 'idle', 0, 0),
(14, 'GZ003-05', 'slow', 7.0, 'idle', 0, 0),
(15, 'HZ001-01', 'fast', 120.0, 'busy', 0, 0),
(15, 'HZ001-02', 'fast', 120.0, 'fault', 0, 0),
(15, 'HZ001-03', 'fast', 180.0, 'idle', 0, 0),
(15, 'HZ001-04', 'fast', 180.0, 'idle', 0, 0),
(15, 'HZ001-05', 'slow', 7.0, 'idle', 0, 0),
(16, 'HZ002-01', 'fast', 120.0, 'busy', 0, 0),
(16, 'HZ002-02', 'fast', 120.0, 'fault', 0, 0),
(16, 'HZ002-03', 'fast', 180.0, 'idle', 0, 0),
(16, 'HZ002-04', 'slow', 7.0, 'idle', 0, 0),
(17, 'HZ003-01', 'fast', 120.0, 'busy', 0, 0),
(17, 'HZ003-02', 'fast', 120.0, 'fault', 0, 0),
(17, 'HZ003-03', 'fast', 180.0, 'idle', 0, 0),
(17, 'HZ003-04', 'fast', 180.0, 'idle', 0, 0),
(17, 'HZ003-05', 'slow', 7.0, 'idle', 0, 0),
(18, 'NJ001-01', 'fast', 120.0, 'busy', 0, 0),
(18, 'NJ001-02', 'fast', 120.0, 'fault', 0, 0),
(18, 'NJ001-03', 'fast', 180.0, 'idle', 0, 0),
(18, 'NJ001-04', 'fast', 180.0, 'idle', 0, 0),
(18, 'NJ001-05', 'slow', 7.0, 'idle', 0, 0),
(19, 'NJ002-01', 'fast', 120.0, 'busy', 0, 0),
(19, 'NJ002-02', 'fast', 120.0, 'fault', 0, 0),
(19, 'NJ002-03', 'fast', 180.0, 'idle', 0, 0),
(19, 'NJ002-04', 'slow', 7.0, 'idle', 0, 0),
(20, 'NJ003-01', 'fast', 120.0, 'busy', 0, 0),
(20, 'NJ003-02', 'fast', 120.0, 'fault', 0, 0),
(20, 'NJ003-03', 'fast', 180.0, 'idle', 0, 0),
(20, 'NJ003-04', 'slow', 7.0, 'idle', 0, 0);
INSERT INTO pile (station_id, code, type, power_kw, status, total_count, total_hours) VALUES
(21, 'SZ004-01', 'fast', 120.0, 'busy', 0, 0),
(21, 'SZ004-02', 'fast', 120.0, 'fault', 0, 0),
(21, 'SZ004-03', 'fast', 180.0, 'idle', 0, 0),
(21, 'SZ004-04', 'slow', 7.0, 'idle', 0, 0),
(21, 'SZ004-05', 'slow', 7.0, 'idle', 0, 0),
(22, 'SZ005-01', 'fast', 120.0, 'busy', 0, 0),
(22, 'SZ005-02', 'fast', 120.0, 'fault', 0, 0),
(22, 'SZ005-03', 'fast', 180.0, 'idle', 0, 0),
(22, 'SZ005-04', 'slow', 7.0, 'idle', 0, 0),
(22, 'SZ005-05', 'slow', 7.0, 'idle', 0, 0),
(23, 'SZ006-01', 'fast', 120.0, 'busy', 0, 0),
(23, 'SZ006-02', 'fast', 120.0, 'fault', 0, 0),
(23, 'SZ006-03', 'fast', 180.0, 'idle', 0, 0),
(23, 'SZ006-04', 'slow', 7.0, 'idle', 0, 0),
(23, 'SZ006-05', 'slow', 7.0, 'idle', 0, 0),
(24, 'SZ007-01', 'fast', 120.0, 'busy', 0, 0),
(24, 'SZ007-02', 'fast', 120.0, 'fault', 0, 0),
(24, 'SZ007-03', 'fast', 180.0, 'idle', 0, 0),
(24, 'SZ007-04', 'slow', 7.0, 'idle', 0, 0),
(24, 'SZ007-05', 'slow', 7.0, 'idle', 0, 0),
(25, 'SZ008-01', 'fast', 120.0, 'busy', 0, 0),
(25, 'SZ008-02', 'fast', 120.0, 'fault', 0, 0),
(25, 'SZ008-03', 'fast', 180.0, 'idle', 0, 0),
(25, 'SZ008-04', 'slow', 7.0, 'idle', 0, 0),
(25, 'SZ008-05', 'slow', 7.0, 'idle', 0, 0),
(26, 'SZ009-01', 'fast', 120.0, 'busy', 0, 0),
(26, 'SZ009-02', 'fast', 120.0, 'fault', 0, 0),
(26, 'SZ009-03', 'fast', 180.0, 'idle', 0, 0),
(26, 'SZ009-04', 'slow', 7.0, 'idle', 0, 0),
(26, 'SZ009-05', 'slow', 7.0, 'idle', 0, 0),
(27, 'SZ010-01', 'fast', 120.0, 'busy', 0, 0),
(27, 'SZ010-02', 'fast', 120.0, 'fault', 0, 0),
(27, 'SZ010-03', 'fast', 180.0, 'idle', 0, 0),
(27, 'SZ010-04', 'slow', 7.0, 'idle', 0, 0),
(27, 'SZ010-05', 'slow', 7.0, 'idle', 0, 0),
(28, 'SZ011-01', 'fast', 120.0, 'busy', 0, 0),
(28, 'SZ011-02', 'fast', 120.0, 'fault', 0, 0),
(28, 'SZ011-03', 'fast', 180.0, 'idle', 0, 0),
(28, 'SZ011-04', 'slow', 7.0, 'idle', 0, 0),
(28, 'SZ011-05', 'slow', 7.0, 'idle', 0, 0),
(29, 'SZ012-01', 'fast', 120.0, 'busy', 0, 0),
(29, 'SZ012-02', 'fast', 120.0, 'fault', 0, 0),
(29, 'SZ012-03', 'fast', 180.0, 'idle', 0, 0),
(29, 'SZ012-04', 'slow', 7.0, 'idle', 0, 0),
(29, 'SZ012-05', 'slow', 7.0, 'idle', 0, 0),
(30, 'BJ005-01', 'fast', 120.0, 'busy', 0, 0),
(30, 'BJ005-02', 'fast', 120.0, 'fault', 0, 0),
(30, 'BJ005-03', 'fast', 180.0, 'idle', 0, 0),
(30, 'BJ005-04', 'slow', 7.0, 'idle', 0, 0),
(30, 'BJ005-05', 'slow', 7.0, 'idle', 0, 0),
(31, 'BJ006-01', 'fast', 120.0, 'busy', 0, 0),
(31, 'BJ006-02', 'fast', 120.0, 'fault', 0, 0),
(31, 'BJ006-03', 'fast', 180.0, 'idle', 0, 0),
(31, 'BJ006-04', 'slow', 7.0, 'idle', 0, 0),
(31, 'BJ006-05', 'slow', 7.0, 'idle', 0, 0),
(32, 'BJ007-01', 'fast', 120.0, 'busy', 0, 0),
(32, 'BJ007-02', 'fast', 120.0, 'fault', 0, 0),
(32, 'BJ007-03', 'fast', 180.0, 'idle', 0, 0),
(32, 'BJ007-04', 'slow', 7.0, 'idle', 0, 0),
(32, 'BJ007-05', 'slow', 7.0, 'idle', 0, 0),
(33, 'BJ008-01', 'fast', 120.0, 'busy', 0, 0),
(33, 'BJ008-02', 'fast', 120.0, 'fault', 0, 0),
(33, 'BJ008-03', 'fast', 180.0, 'idle', 0, 0),
(33, 'BJ008-04', 'slow', 7.0, 'idle', 0, 0),
(33, 'BJ008-05', 'slow', 7.0, 'idle', 0, 0),
(34, 'BJ009-01', 'fast', 120.0, 'busy', 0, 0),
(34, 'BJ009-02', 'fast', 120.0, 'fault', 0, 0),
(34, 'BJ009-03', 'fast', 180.0, 'idle', 0, 0),
(34, 'BJ009-04', 'slow', 7.0, 'idle', 0, 0),
(34, 'BJ009-05', 'slow', 7.0, 'idle', 0, 0),
(35, 'BJ010-01', 'fast', 120.0, 'busy', 0, 0),
(35, 'BJ010-02', 'fast', 120.0, 'fault', 0, 0),
(35, 'BJ010-03', 'fast', 180.0, 'idle', 0, 0),
(35, 'BJ010-04', 'slow', 7.0, 'idle', 0, 0),
(35, 'BJ010-05', 'slow', 7.0, 'idle', 0, 0),
(36, 'BJ011-01', 'fast', 120.0, 'busy', 0, 0),
(36, 'BJ011-02', 'fast', 120.0, 'fault', 0, 0),
(36, 'BJ011-03', 'fast', 180.0, 'idle', 0, 0),
(36, 'BJ011-04', 'slow', 7.0, 'idle', 0, 0),
(36, 'BJ011-05', 'slow', 7.0, 'idle', 0, 0),
(37, 'BJ012-01', 'fast', 120.0, 'busy', 0, 0),
(37, 'BJ012-02', 'fast', 120.0, 'fault', 0, 0),
(37, 'BJ012-03', 'fast', 180.0, 'idle', 0, 0),
(37, 'BJ012-04', 'slow', 7.0, 'idle', 0, 0),
(37, 'BJ012-05', 'slow', 7.0, 'idle', 0, 0),
(38, 'SH005-01', 'fast', 120.0, 'busy', 0, 0),
(38, 'SH005-02', 'fast', 120.0, 'fault', 0, 0),
(38, 'SH005-03', 'fast', 180.0, 'idle', 0, 0),
(38, 'SH005-04', 'slow', 7.0, 'idle', 0, 0),
(38, 'SH005-05', 'slow', 7.0, 'idle', 0, 0),
(39, 'SH006-01', 'fast', 120.0, 'busy', 0, 0),
(39, 'SH006-02', 'fast', 120.0, 'fault', 0, 0),
(39, 'SH006-03', 'fast', 180.0, 'idle', 0, 0),
(39, 'SH006-04', 'slow', 7.0, 'idle', 0, 0),
(39, 'SH006-05', 'slow', 7.0, 'idle', 0, 0),
(40, 'SH007-01', 'fast', 120.0, 'busy', 0, 0),
(40, 'SH007-02', 'fast', 120.0, 'fault', 0, 0),
(40, 'SH007-03', 'fast', 180.0, 'idle', 0, 0),
(40, 'SH007-04', 'slow', 7.0, 'idle', 0, 0),
(40, 'SH007-05', 'slow', 7.0, 'idle', 0, 0),
(41, 'SH008-01', 'fast', 120.0, 'busy', 0, 0),
(41, 'SH008-02', 'fast', 120.0, 'fault', 0, 0),
(41, 'SH008-03', 'fast', 180.0, 'idle', 0, 0),
(41, 'SH008-04', 'slow', 7.0, 'idle', 0, 0),
(41, 'SH008-05', 'slow', 7.0, 'idle', 0, 0),
(42, 'SH009-01', 'fast', 120.0, 'busy', 0, 0),
(42, 'SH009-02', 'fast', 120.0, 'fault', 0, 0),
(42, 'SH009-03', 'fast', 180.0, 'idle', 0, 0),
(42, 'SH009-04', 'slow', 7.0, 'idle', 0, 0),
(42, 'SH009-05', 'slow', 7.0, 'idle', 0, 0),
(43, 'SH010-01', 'fast', 120.0, 'busy', 0, 0),
(43, 'SH010-02', 'fast', 120.0, 'fault', 0, 0),
(43, 'SH010-03', 'fast', 180.0, 'idle', 0, 0),
(43, 'SH010-04', 'slow', 7.0, 'idle', 0, 0),
(43, 'SH010-05', 'slow', 7.0, 'idle', 0, 0),
(44, 'SH011-01', 'fast', 120.0, 'busy', 0, 0),
(44, 'SH011-02', 'fast', 120.0, 'fault', 0, 0),
(44, 'SH011-03', 'fast', 180.0, 'idle', 0, 0),
(44, 'SH011-04', 'slow', 7.0, 'idle', 0, 0),
(44, 'SH011-05', 'slow', 7.0, 'idle', 0, 0),
(45, 'SH012-01', 'fast', 120.0, 'busy', 0, 0),
(45, 'SH012-02', 'fast', 120.0, 'fault', 0, 0),
(45, 'SH012-03', 'fast', 180.0, 'idle', 0, 0),
(45, 'SH012-04', 'slow', 7.0, 'idle', 0, 0),
(45, 'SH012-05', 'slow', 7.0, 'idle', 0, 0),
(46, 'GZ004-01', 'fast', 120.0, 'busy', 0, 0),
(46, 'GZ004-02', 'fast', 120.0, 'fault', 0, 0),
(46, 'GZ004-03', 'fast', 180.0, 'idle', 0, 0),
(46, 'GZ004-04', 'slow', 7.0, 'idle', 0, 0),
(46, 'GZ004-05', 'slow', 7.0, 'idle', 0, 0),
(47, 'GZ005-01', 'fast', 120.0, 'busy', 0, 0),
(47, 'GZ005-02', 'fast', 120.0, 'fault', 0, 0),
(47, 'GZ005-03', 'fast', 180.0, 'idle', 0, 0),
(47, 'GZ005-04', 'slow', 7.0, 'idle', 0, 0),
(47, 'GZ005-05', 'slow', 7.0, 'idle', 0, 0),
(48, 'GZ006-01', 'fast', 120.0, 'busy', 0, 0),
(48, 'GZ006-02', 'fast', 120.0, 'fault', 0, 0),
(48, 'GZ006-03', 'fast', 180.0, 'idle', 0, 0),
(48, 'GZ006-04', 'slow', 7.0, 'idle', 0, 0),
(48, 'GZ006-05', 'slow', 7.0, 'idle', 0, 0),
(49, 'GZ007-01', 'fast', 120.0, 'busy', 0, 0),
(49, 'GZ007-02', 'fast', 120.0, 'fault', 0, 0),
(49, 'GZ007-03', 'fast', 180.0, 'idle', 0, 0),
(49, 'GZ007-04', 'slow', 7.0, 'idle', 0, 0),
(49, 'GZ007-05', 'slow', 7.0, 'idle', 0, 0),
(50, 'GZ008-01', 'fast', 120.0, 'busy', 0, 0),
(50, 'GZ008-02', 'fast', 120.0, 'fault', 0, 0),
(50, 'GZ008-03', 'fast', 180.0, 'idle', 0, 0),
(50, 'GZ008-04', 'slow', 7.0, 'idle', 0, 0),
(50, 'GZ008-05', 'slow', 7.0, 'idle', 0, 0),
(51, 'GZ009-01', 'fast', 120.0, 'busy', 0, 0),
(51, 'GZ009-02', 'fast', 120.0, 'fault', 0, 0),
(51, 'GZ009-03', 'fast', 180.0, 'idle', 0, 0),
(51, 'GZ009-04', 'slow', 7.0, 'idle', 0, 0),
(51, 'GZ009-05', 'slow', 7.0, 'idle', 0, 0),
(52, 'GZ010-01', 'fast', 120.0, 'busy', 0, 0),
(52, 'GZ010-02', 'fast', 120.0, 'fault', 0, 0),
(52, 'GZ010-03', 'fast', 180.0, 'idle', 0, 0),
(52, 'GZ010-04', 'slow', 7.0, 'idle', 0, 0),
(52, 'GZ010-05', 'slow', 7.0, 'idle', 0, 0),
(53, 'GZ011-01', 'fast', 120.0, 'busy', 0, 0),
(53, 'GZ011-02', 'fast', 120.0, 'fault', 0, 0),
(53, 'GZ011-03', 'fast', 180.0, 'idle', 0, 0),
(53, 'GZ011-04', 'slow', 7.0, 'idle', 0, 0),
(53, 'GZ011-05', 'slow', 7.0, 'idle', 0, 0),
(54, 'GZ012-01', 'fast', 120.0, 'busy', 0, 0),
(54, 'GZ012-02', 'fast', 120.0, 'fault', 0, 0),
(54, 'GZ012-03', 'fast', 180.0, 'idle', 0, 0),
(54, 'GZ012-04', 'slow', 7.0, 'idle', 0, 0),
(54, 'GZ012-05', 'slow', 7.0, 'idle', 0, 0),
(55, 'HZ004-01', 'fast', 120.0, 'busy', 0, 0),
(55, 'HZ004-02', 'fast', 120.0, 'fault', 0, 0),
(55, 'HZ004-03', 'fast', 180.0, 'idle', 0, 0),
(55, 'HZ004-04', 'slow', 7.0, 'idle', 0, 0),
(55, 'HZ004-05', 'slow', 7.0, 'idle', 0, 0),
(56, 'HZ005-01', 'fast', 120.0, 'busy', 0, 0),
(56, 'HZ005-02', 'fast', 120.0, 'fault', 0, 0),
(56, 'HZ005-03', 'fast', 180.0, 'idle', 0, 0),
(56, 'HZ005-04', 'slow', 7.0, 'idle', 0, 0),
(56, 'HZ005-05', 'slow', 7.0, 'idle', 0, 0),
(57, 'HZ006-01', 'fast', 120.0, 'busy', 0, 0),
(57, 'HZ006-02', 'fast', 120.0, 'fault', 0, 0),
(57, 'HZ006-03', 'fast', 180.0, 'idle', 0, 0),
(57, 'HZ006-04', 'slow', 7.0, 'idle', 0, 0),
(57, 'HZ006-05', 'slow', 7.0, 'idle', 0, 0),
(58, 'HZ007-01', 'fast', 120.0, 'busy', 0, 0),
(58, 'HZ007-02', 'fast', 120.0, 'fault', 0, 0),
(58, 'HZ007-03', 'fast', 180.0, 'idle', 0, 0),
(58, 'HZ007-04', 'slow', 7.0, 'idle', 0, 0),
(58, 'HZ007-05', 'slow', 7.0, 'idle', 0, 0),
(59, 'HZ008-01', 'fast', 120.0, 'busy', 0, 0),
(59, 'HZ008-02', 'fast', 120.0, 'fault', 0, 0),
(59, 'HZ008-03', 'fast', 180.0, 'idle', 0, 0),
(59, 'HZ008-04', 'slow', 7.0, 'idle', 0, 0),
(59, 'HZ008-05', 'slow', 7.0, 'idle', 0, 0),
(60, 'HZ009-01', 'fast', 120.0, 'busy', 0, 0),
(60, 'HZ009-02', 'fast', 120.0, 'fault', 0, 0),
(60, 'HZ009-03', 'fast', 180.0, 'idle', 0, 0),
(60, 'HZ009-04', 'slow', 7.0, 'idle', 0, 0),
(60, 'HZ009-05', 'slow', 7.0, 'idle', 0, 0),
(61, 'HZ010-01', 'fast', 120.0, 'busy', 0, 0),
(61, 'HZ010-02', 'fast', 120.0, 'fault', 0, 0),
(61, 'HZ010-03', 'fast', 180.0, 'idle', 0, 0),
(61, 'HZ010-04', 'slow', 7.0, 'idle', 0, 0),
(61, 'HZ010-05', 'slow', 7.0, 'idle', 0, 0),
(62, 'HZ011-01', 'fast', 120.0, 'busy', 0, 0),
(62, 'HZ011-02', 'fast', 120.0, 'fault', 0, 0),
(62, 'HZ011-03', 'fast', 180.0, 'idle', 0, 0),
(62, 'HZ011-04', 'slow', 7.0, 'idle', 0, 0),
(62, 'HZ011-05', 'slow', 7.0, 'idle', 0, 0),
(63, 'HZ012-01', 'fast', 120.0, 'busy', 0, 0),
(63, 'HZ012-02', 'fast', 120.0, 'fault', 0, 0),
(63, 'HZ012-03', 'fast', 180.0, 'idle', 0, 0),
(63, 'HZ012-04', 'slow', 7.0, 'idle', 0, 0),
(63, 'HZ012-05', 'slow', 7.0, 'idle', 0, 0),
(64, 'NJ004-01', 'fast', 120.0, 'busy', 0, 0),
(64, 'NJ004-02', 'fast', 120.0, 'fault', 0, 0),
(64, 'NJ004-03', 'fast', 180.0, 'idle', 0, 0),
(64, 'NJ004-04', 'slow', 7.0, 'idle', 0, 0),
(64, 'NJ004-05', 'slow', 7.0, 'idle', 0, 0),
(65, 'NJ005-01', 'fast', 120.0, 'busy', 0, 0),
(65, 'NJ005-02', 'fast', 120.0, 'fault', 0, 0),
(65, 'NJ005-03', 'fast', 180.0, 'idle', 0, 0),
(65, 'NJ005-04', 'slow', 7.0, 'idle', 0, 0),
(65, 'NJ005-05', 'slow', 7.0, 'idle', 0, 0),
(66, 'NJ006-01', 'fast', 120.0, 'busy', 0, 0),
(66, 'NJ006-02', 'fast', 120.0, 'fault', 0, 0),
(66, 'NJ006-03', 'fast', 180.0, 'idle', 0, 0),
(66, 'NJ006-04', 'slow', 7.0, 'idle', 0, 0),
(66, 'NJ006-05', 'slow', 7.0, 'idle', 0, 0),
(67, 'NJ007-01', 'fast', 120.0, 'busy', 0, 0),
(67, 'NJ007-02', 'fast', 120.0, 'fault', 0, 0),
(67, 'NJ007-03', 'fast', 180.0, 'idle', 0, 0),
(67, 'NJ007-04', 'slow', 7.0, 'idle', 0, 0),
(67, 'NJ007-05', 'slow', 7.0, 'idle', 0, 0),
(68, 'NJ008-01', 'fast', 120.0, 'busy', 0, 0),
(68, 'NJ008-02', 'fast', 120.0, 'fault', 0, 0),
(68, 'NJ008-03', 'fast', 180.0, 'idle', 0, 0),
(68, 'NJ008-04', 'slow', 7.0, 'idle', 0, 0),
(68, 'NJ008-05', 'slow', 7.0, 'idle', 0, 0),
(69, 'NJ009-01', 'fast', 120.0, 'busy', 0, 0),
(69, 'NJ009-02', 'fast', 120.0, 'fault', 0, 0),
(69, 'NJ009-03', 'fast', 180.0, 'idle', 0, 0),
(69, 'NJ009-04', 'slow', 7.0, 'idle', 0, 0),
(69, 'NJ009-05', 'slow', 7.0, 'idle', 0, 0),
(70, 'NJ010-01', 'fast', 120.0, 'busy', 0, 0),
(70, 'NJ010-02', 'fast', 120.0, 'fault', 0, 0),
(70, 'NJ010-03', 'fast', 180.0, 'idle', 0, 0),
(70, 'NJ010-04', 'slow', 7.0, 'idle', 0, 0),
(70, 'NJ010-05', 'slow', 7.0, 'idle', 0, 0),
(71, 'NJ011-01', 'fast', 120.0, 'busy', 0, 0),
(71, 'NJ011-02', 'fast', 120.0, 'fault', 0, 0),
(71, 'NJ011-03', 'fast', 180.0, 'idle', 0, 0),
(71, 'NJ011-04', 'slow', 7.0, 'idle', 0, 0),
(71, 'NJ011-05', 'slow', 7.0, 'idle', 0, 0),
(72, 'NJ012-01', 'fast', 120.0, 'busy', 0, 0),
(72, 'NJ012-02', 'fast', 120.0, 'fault', 0, 0),
(72, 'NJ012-03', 'fast', 180.0, 'idle', 0, 0),
(72, 'NJ012-04', 'slow', 7.0, 'idle', 0, 0),
(72, 'NJ012-05', 'slow', 7.0, 'idle', 0, 0);

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
