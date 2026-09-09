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

-- 充电桩（每站 4~7 根；仅部分站点有故障/在用桩，其余空闲）
INSERT INTO pile (station_id, code, type, power_kw, status, total_count, total_hours) VALUES
(1, 'SZ001-01', 'fast', 120.0, 'idle', 0, 0),
(1, 'SZ001-02', 'fast', 180.0, 'idle', 0, 0),
(1, 'SZ001-03', 'fast', 120.0, 'busy', 0, 0),
(1, 'SZ001-04', 'slow', 7.0, 'busy', 0, 0),
(2, 'SZ002-01', 'fast', 120.0, 'idle', 0, 0),
(2, 'SZ002-02', 'fast', 180.0, 'idle', 0, 0),
(2, 'SZ002-03', 'fast', 120.0, 'idle', 0, 0),
(2, 'SZ002-04', 'slow', 7.0, 'idle', 0, 0),
(2, 'SZ002-05', 'slow', 7.0, 'idle', 0, 0),
(2, 'SZ002-06', 'fast', 120.0, 'idle', 0, 0),
(3, 'SZ003-01', 'fast', 120.0, 'idle', 0, 0),
(3, 'SZ003-02', 'fast', 180.0, 'idle', 0, 0),
(3, 'SZ003-03', 'fast', 120.0, 'busy', 0, 0),
(3, 'SZ003-04', 'slow', 7.0, 'idle', 0, 0),
(4, 'BJ001-01', 'fast', 180.0, 'fault', 0, 0),
(4, 'BJ001-02', 'fast', 120.0, 'idle', 0, 0),
(4, 'BJ001-03', 'fast', 120.0, 'idle', 0, 0),
(4, 'BJ001-04', 'slow', 7.0, 'idle', 0, 0),
(4, 'BJ001-05', 'fast', 180.0, 'fault', 0, 0),
(5, 'BJ002-01', 'fast', 180.0, 'idle', 0, 0),
(5, 'BJ002-02', 'fast', 180.0, 'idle', 0, 0),
(5, 'BJ002-03', 'slow', 7.0, 'busy', 0, 0),
(5, 'BJ002-04', 'fast', 120.0, 'idle', 0, 0),
(5, 'BJ002-05', 'slow', 7.0, 'idle', 0, 0),
(5, 'BJ002-06', 'fast', 120.0, 'idle', 0, 0),
(6, 'BJ003-01', 'fast', 180.0, 'idle', 0, 0),
(6, 'BJ003-02', 'fast', 180.0, 'idle', 0, 0),
(6, 'BJ003-03', 'slow', 7.0, 'idle', 0, 0),
(6, 'BJ003-04', 'fast', 180.0, 'busy', 0, 0),
(6, 'BJ003-05', 'fast', 120.0, 'idle', 0, 0),
(6, 'BJ003-06', 'fast', 120.0, 'idle', 0, 0),
(7, 'BJ004-01', 'fast', 180.0, 'busy', 0, 0),
(7, 'BJ004-02', 'fast', 120.0, 'fault', 0, 0),
(7, 'BJ004-03', 'fast', 180.0, 'busy', 0, 0),
(7, 'BJ004-04', 'slow', 7.0, 'busy', 0, 0),
(7, 'BJ004-05', 'fast', 180.0, 'idle', 0, 0),
(7, 'BJ004-06', 'fast', 120.0, 'idle', 0, 0),
(8, 'SH001-01', 'slow', 7.0, 'idle', 0, 0),
(8, 'SH001-02', 'fast', 120.0, 'idle', 0, 0),
(8, 'SH001-03', 'fast', 120.0, 'idle', 0, 0),
(8, 'SH001-04', 'fast', 180.0, 'fault', 0, 0),
(9, 'SH002-01', 'slow', 7.0, 'idle', 0, 0),
(9, 'SH002-02', 'fast', 120.0, 'idle', 0, 0),
(9, 'SH002-03', 'fast', 120.0, 'idle', 0, 0),
(9, 'SH002-04', 'fast', 120.0, 'idle', 0, 0),
(10, 'SH003-01', 'slow', 7.0, 'idle', 0, 0),
(10, 'SH003-02', 'fast', 120.0, 'idle', 0, 0),
(10, 'SH003-03', 'slow', 7.0, 'idle', 0, 0),
(10, 'SH003-04', 'fast', 120.0, 'idle', 0, 0),
(10, 'SH003-05', 'fast', 120.0, 'idle', 0, 0),
(11, 'SH004-01', 'fast', 180.0, 'idle', 0, 0),
(11, 'SH004-02', 'slow', 7.0, 'idle', 0, 0),
(11, 'SH004-03', 'slow', 7.0, 'busy', 0, 0),
(11, 'SH004-04', 'fast', 120.0, 'busy', 0, 0),
(11, 'SH004-05', 'fast', 120.0, 'idle', 0, 0),
(12, 'GZ001-01', 'fast', 180.0, 'busy', 0, 0),
(12, 'GZ001-02', 'fast', 180.0, 'idle', 0, 0),
(12, 'GZ001-03', 'fast', 180.0, 'busy', 0, 0),
(12, 'GZ001-04', 'slow', 7.0, 'busy', 0, 0),
(12, 'GZ001-05', 'fast', 180.0, 'idle', 0, 0),
(13, 'GZ002-01', 'fast', 180.0, 'idle', 0, 0),
(13, 'GZ002-02', 'fast', 120.0, 'fault', 0, 0),
(13, 'GZ002-03', 'slow', 7.0, 'idle', 0, 0),
(13, 'GZ002-04', 'fast', 120.0, 'idle', 0, 0),
(13, 'GZ002-05', 'slow', 7.0, 'idle', 0, 0),
(13, 'GZ002-06', 'fast', 120.0, 'idle', 0, 0),
(13, 'GZ002-07', 'fast', 180.0, 'idle', 0, 0),
(14, 'GZ003-01', 'fast', 120.0, 'idle', 0, 0),
(14, 'GZ003-02', 'fast', 180.0, 'idle', 0, 0),
(14, 'GZ003-03', 'fast', 120.0, 'fault', 0, 0),
(14, 'GZ003-04', 'slow', 7.0, 'idle', 0, 0),
(14, 'GZ003-05', 'fast', 120.0, 'idle', 0, 0),
(14, 'GZ003-06', 'fast', 180.0, 'fault', 0, 0),
(15, 'HZ001-01', 'slow', 7.0, 'idle', 0, 0),
(15, 'HZ001-02', 'fast', 120.0, 'idle', 0, 0),
(15, 'HZ001-03', 'fast', 180.0, 'busy', 0, 0),
(15, 'HZ001-04', 'slow', 7.0, 'fault', 0, 0),
(16, 'HZ002-01', 'fast', 120.0, 'idle', 0, 0),
(16, 'HZ002-02', 'fast', 180.0, 'idle', 0, 0),
(16, 'HZ002-03', 'fast', 120.0, 'idle', 0, 0),
(16, 'HZ002-04', 'fast', 180.0, 'idle', 0, 0),
(16, 'HZ002-05', 'fast', 180.0, 'idle', 0, 0),
(16, 'HZ002-06', 'slow', 7.0, 'fault', 0, 0),
(16, 'HZ002-07', 'fast', 180.0, 'fault', 0, 0),
(17, 'HZ003-01', 'fast', 120.0, 'idle', 0, 0),
(17, 'HZ003-02', 'slow', 7.0, 'idle', 0, 0),
(17, 'HZ003-03', 'fast', 120.0, 'idle', 0, 0),
(17, 'HZ003-04', 'fast', 180.0, 'idle', 0, 0),
(17, 'HZ003-05', 'slow', 7.0, 'idle', 0, 0),
(17, 'HZ003-06', 'fast', 120.0, 'idle', 0, 0),
(18, 'NJ001-01', 'fast', 120.0, 'idle', 0, 0),
(18, 'NJ001-02', 'fast', 180.0, 'busy', 0, 0),
(18, 'NJ001-03', 'slow', 7.0, 'busy', 0, 0),
(18, 'NJ001-04', 'fast', 120.0, 'idle', 0, 0),
(18, 'NJ001-05', 'fast', 180.0, 'busy', 0, 0),
(19, 'NJ002-01', 'fast', 120.0, 'idle', 0, 0),
(19, 'NJ002-02', 'fast', 120.0, 'idle', 0, 0),
(19, 'NJ002-03', 'fast', 180.0, 'idle', 0, 0),
(19, 'NJ002-04', 'slow', 7.0, 'idle', 0, 0),
(19, 'NJ002-05', 'fast', 180.0, 'idle', 0, 0),
(19, 'NJ002-06', 'slow', 7.0, 'idle', 0, 0),
(20, 'NJ003-01', 'slow', 7.0, 'idle', 0, 0),
(20, 'NJ003-02', 'fast', 120.0, 'idle', 0, 0),
(20, 'NJ003-03', 'fast', 180.0, 'idle', 0, 0),
(20, 'NJ003-04', 'slow', 7.0, 'busy', 0, 0),
(21, 'SZ004-01', 'fast', 180.0, 'busy', 0, 0),
(21, 'SZ004-02', 'fast', 180.0, 'idle', 0, 0),
(21, 'SZ004-03', 'fast', 180.0, 'fault', 0, 0),
(21, 'SZ004-04', 'slow', 7.0, 'fault', 0, 0),
(21, 'SZ004-05', 'slow', 7.0, 'busy', 0, 0),
(22, 'SZ005-01', 'slow', 7.0, 'idle', 0, 0),
(22, 'SZ005-02', 'fast', 180.0, 'idle', 0, 0),
(22, 'SZ005-03', 'fast', 120.0, 'busy', 0, 0),
(22, 'SZ005-04', 'slow', 7.0, 'idle', 0, 0),
(22, 'SZ005-05', 'fast', 180.0, 'idle', 0, 0),
(22, 'SZ005-06', 'fast', 180.0, 'idle', 0, 0),
(23, 'SZ006-01', 'fast', 120.0, 'idle', 0, 0),
(23, 'SZ006-02', 'fast', 120.0, 'idle', 0, 0),
(23, 'SZ006-03', 'slow', 7.0, 'idle', 0, 0),
(23, 'SZ006-04', 'slow', 7.0, 'idle', 0, 0),
(23, 'SZ006-05', 'fast', 120.0, 'idle', 0, 0),
(23, 'SZ006-06', 'fast', 120.0, 'idle', 0, 0),
(23, 'SZ006-07', 'fast', 180.0, 'idle', 0, 0),
(24, 'SZ007-01', 'fast', 120.0, 'busy', 0, 0),
(24, 'SZ007-02', 'fast', 180.0, 'fault', 0, 0),
(24, 'SZ007-03', 'fast', 180.0, 'busy', 0, 0),
(24, 'SZ007-04', 'slow', 7.0, 'idle', 0, 0),
(24, 'SZ007-05', 'slow', 7.0, 'fault', 0, 0),
(25, 'SZ008-01', 'fast', 120.0, 'idle', 0, 0),
(25, 'SZ008-02', 'fast', 120.0, 'idle', 0, 0),
(25, 'SZ008-03', 'fast', 120.0, 'idle', 0, 0),
(25, 'SZ008-04', 'fast', 180.0, 'idle', 0, 0),
(25, 'SZ008-05', 'slow', 7.0, 'idle', 0, 0),
(25, 'SZ008-06', 'slow', 7.0, 'idle', 0, 0),
(25, 'SZ008-07', 'fast', 120.0, 'idle', 0, 0),
(26, 'SZ009-01', 'fast', 120.0, 'idle', 0, 0),
(26, 'SZ009-02', 'slow', 7.0, 'idle', 0, 0),
(26, 'SZ009-03', 'fast', 120.0, 'idle', 0, 0),
(26, 'SZ009-04', 'fast', 180.0, 'idle', 0, 0),
(26, 'SZ009-05', 'fast', 180.0, 'idle', 0, 0),
(26, 'SZ009-06', 'fast', 120.0, 'idle', 0, 0),
(26, 'SZ009-07', 'fast', 180.0, 'idle', 0, 0),
(27, 'SZ010-01', 'fast', 180.0, 'idle', 0, 0),
(27, 'SZ010-02', 'fast', 120.0, 'idle', 0, 0),
(27, 'SZ010-03', 'fast', 180.0, 'idle', 0, 0),
(27, 'SZ010-04', 'slow', 7.0, 'idle', 0, 0),
(27, 'SZ010-05', 'slow', 7.0, 'idle', 0, 0),
(28, 'SZ011-01', 'fast', 180.0, 'busy', 0, 0),
(28, 'SZ011-02', 'fast', 180.0, 'busy', 0, 0),
(28, 'SZ011-03', 'fast', 120.0, 'idle', 0, 0),
(28, 'SZ011-04', 'fast', 180.0, 'idle', 0, 0),
(28, 'SZ011-05', 'fast', 180.0, 'idle', 0, 0),
(28, 'SZ011-06', 'slow', 7.0, 'idle', 0, 0),
(28, 'SZ011-07', 'fast', 120.0, 'idle', 0, 0),
(29, 'SZ012-01', 'fast', 180.0, 'busy', 0, 0),
(29, 'SZ012-02', 'fast', 120.0, 'idle', 0, 0),
(29, 'SZ012-03', 'fast', 180.0, 'idle', 0, 0),
(29, 'SZ012-04', 'slow', 7.0, 'busy', 0, 0),
(29, 'SZ012-05', 'fast', 180.0, 'idle', 0, 0),
(29, 'SZ012-06', 'fast', 180.0, 'busy', 0, 0),
(30, 'BJ005-01', 'fast', 180.0, 'idle', 0, 0),
(30, 'BJ005-02', 'fast', 180.0, 'idle', 0, 0),
(30, 'BJ005-03', 'slow', 7.0, 'idle', 0, 0),
(30, 'BJ005-04', 'fast', 120.0, 'idle', 0, 0),
(30, 'BJ005-05', 'slow', 7.0, 'idle', 0, 0),
(30, 'BJ005-06', 'fast', 180.0, 'idle', 0, 0),
(31, 'BJ006-01', 'fast', 180.0, 'busy', 0, 0),
(31, 'BJ006-02', 'fast', 180.0, 'idle', 0, 0),
(31, 'BJ006-03', 'slow', 7.0, 'fault', 0, 0),
(31, 'BJ006-04', 'slow', 7.0, 'busy', 0, 0),
(32, 'BJ007-01', 'fast', 120.0, 'idle', 0, 0),
(32, 'BJ007-02', 'slow', 7.0, 'idle', 0, 0),
(32, 'BJ007-03', 'fast', 180.0, 'idle', 0, 0),
(32, 'BJ007-04', 'fast', 120.0, 'idle', 0, 0),
(33, 'BJ008-01', 'fast', 120.0, 'idle', 0, 0),
(33, 'BJ008-02', 'fast', 120.0, 'idle', 0, 0),
(33, 'BJ008-03', 'fast', 180.0, 'idle', 0, 0),
(33, 'BJ008-04', 'fast', 180.0, 'idle', 0, 0),
(33, 'BJ008-05', 'fast', 180.0, 'idle', 0, 0),
(33, 'BJ008-06', 'fast', 180.0, 'idle', 0, 0),
(33, 'BJ008-07', 'slow', 7.0, 'idle', 0, 0),
(34, 'BJ009-01', 'slow', 7.0, 'idle', 0, 0),
(34, 'BJ009-02', 'fast', 180.0, 'idle', 0, 0),
(34, 'BJ009-03', 'fast', 180.0, 'idle', 0, 0),
(34, 'BJ009-04', 'fast', 120.0, 'idle', 0, 0),
(34, 'BJ009-05', 'fast', 180.0, 'idle', 0, 0),
(34, 'BJ009-06', 'fast', 180.0, 'idle', 0, 0),
(35, 'BJ010-01', 'fast', 120.0, 'idle', 0, 0),
(35, 'BJ010-02', 'fast', 180.0, 'idle', 0, 0),
(35, 'BJ010-03', 'slow', 7.0, 'idle', 0, 0),
(35, 'BJ010-04', 'fast', 180.0, 'idle', 0, 0),
(35, 'BJ010-05', 'fast', 120.0, 'idle', 0, 0),
(35, 'BJ010-06', 'fast', 120.0, 'idle', 0, 0),
(35, 'BJ010-07', 'fast', 180.0, 'idle', 0, 0),
(36, 'BJ011-01', 'fast', 120.0, 'idle', 0, 0),
(36, 'BJ011-02', 'fast', 120.0, 'idle', 0, 0),
(36, 'BJ011-03', 'fast', 120.0, 'idle', 0, 0),
(36, 'BJ011-04', 'slow', 7.0, 'idle', 0, 0),
(36, 'BJ011-05', 'fast', 120.0, 'idle', 0, 0),
(37, 'BJ012-01', 'slow', 7.0, 'busy', 0, 0),
(37, 'BJ012-02', 'fast', 120.0, 'idle', 0, 0),
(37, 'BJ012-03', 'fast', 120.0, 'busy', 0, 0),
(37, 'BJ012-04', 'fast', 180.0, 'idle', 0, 0),
(37, 'BJ012-05', 'slow', 7.0, 'idle', 0, 0),
(38, 'SH005-01', 'slow', 7.0, 'idle', 0, 0),
(38, 'SH005-02', 'fast', 180.0, 'idle', 0, 0),
(38, 'SH005-03', 'slow', 7.0, 'idle', 0, 0),
(38, 'SH005-04', 'fast', 120.0, 'idle', 0, 0),
(38, 'SH005-05', 'fast', 120.0, 'idle', 0, 0),
(38, 'SH005-06', 'fast', 180.0, 'idle', 0, 0),
(38, 'SH005-07', 'fast', 120.0, 'idle', 0, 0),
(39, 'SH006-01', 'slow', 7.0, 'idle', 0, 0),
(39, 'SH006-02', 'slow', 7.0, 'idle', 0, 0),
(39, 'SH006-03', 'fast', 180.0, 'idle', 0, 0),
(39, 'SH006-04', 'fast', 120.0, 'idle', 0, 0),
(40, 'SH007-01', 'fast', 180.0, 'idle', 0, 0),
(40, 'SH007-02', 'slow', 7.0, 'idle', 0, 0),
(40, 'SH007-03', 'slow', 7.0, 'idle', 0, 0),
(40, 'SH007-04', 'fast', 180.0, 'idle', 0, 0),
(40, 'SH007-05', 'fast', 180.0, 'idle', 0, 0),
(40, 'SH007-06', 'fast', 180.0, 'idle', 0, 0),
(41, 'SH008-01', 'slow', 7.0, 'idle', 0, 0),
(41, 'SH008-02', 'slow', 7.0, 'idle', 0, 0),
(41, 'SH008-03', 'fast', 180.0, 'idle', 0, 0),
(41, 'SH008-04', 'fast', 180.0, 'idle', 0, 0),
(41, 'SH008-05', 'fast', 180.0, 'idle', 0, 0),
(42, 'SH009-01', 'fast', 120.0, 'idle', 0, 0),
(42, 'SH009-02', 'fast', 180.0, 'idle', 0, 0),
(42, 'SH009-03', 'fast', 120.0, 'fault', 0, 0),
(42, 'SH009-04', 'fast', 120.0, 'idle', 0, 0),
(42, 'SH009-05', 'fast', 180.0, 'idle', 0, 0),
(42, 'SH009-06', 'slow', 7.0, 'idle', 0, 0),
(42, 'SH009-07', 'fast', 180.0, 'idle', 0, 0),
(43, 'SH010-01', 'fast', 120.0, 'idle', 0, 0),
(43, 'SH010-02', 'slow', 7.0, 'idle', 0, 0),
(43, 'SH010-03', 'fast', 120.0, 'idle', 0, 0),
(43, 'SH010-04', 'fast', 180.0, 'idle', 0, 0),
(43, 'SH010-05', 'fast', 180.0, 'idle', 0, 0),
(44, 'SH011-01', 'fast', 180.0, 'idle', 0, 0),
(44, 'SH011-02', 'fast', 120.0, 'idle', 0, 0),
(44, 'SH011-03', 'fast', 120.0, 'idle', 0, 0),
(44, 'SH011-04', 'fast', 180.0, 'busy', 0, 0),
(44, 'SH011-05', 'fast', 180.0, 'busy', 0, 0),
(44, 'SH011-06', 'slow', 7.0, 'busy', 0, 0),
(45, 'SH012-01', 'slow', 7.0, 'idle', 0, 0),
(45, 'SH012-02', 'fast', 120.0, 'busy', 0, 0),
(45, 'SH012-03', 'slow', 7.0, 'idle', 0, 0),
(45, 'SH012-04', 'fast', 180.0, 'busy', 0, 0),
(46, 'GZ004-01', 'fast', 120.0, 'idle', 0, 0),
(46, 'GZ004-02', 'slow', 7.0, 'idle', 0, 0),
(46, 'GZ004-03', 'fast', 180.0, 'busy', 0, 0),
(46, 'GZ004-04', 'fast', 120.0, 'idle', 0, 0),
(46, 'GZ004-05', 'fast', 120.0, 'idle', 0, 0),
(46, 'GZ004-06', 'fast', 180.0, 'busy', 0, 0),
(46, 'GZ004-07', 'fast', 180.0, 'idle', 0, 0),
(47, 'GZ005-01', 'fast', 180.0, 'idle', 0, 0),
(47, 'GZ005-02', 'fast', 180.0, 'idle', 0, 0),
(47, 'GZ005-03', 'slow', 7.0, 'idle', 0, 0),
(47, 'GZ005-04', 'fast', 120.0, 'fault', 0, 0),
(47, 'GZ005-05', 'fast', 180.0, 'idle', 0, 0),
(47, 'GZ005-06', 'fast', 120.0, 'fault', 0, 0),
(47, 'GZ005-07', 'fast', 180.0, 'idle', 0, 0),
(48, 'GZ006-01', 'fast', 120.0, 'idle', 0, 0),
(48, 'GZ006-02', 'fast', 180.0, 'fault', 0, 0),
(48, 'GZ006-03', 'slow', 7.0, 'idle', 0, 0),
(48, 'GZ006-04', 'fast', 180.0, 'fault', 0, 0),
(48, 'GZ006-05', 'fast', 120.0, 'idle', 0, 0),
(49, 'GZ007-01', 'slow', 7.0, 'busy', 0, 0),
(49, 'GZ007-02', 'fast', 180.0, 'idle', 0, 0),
(49, 'GZ007-03', 'fast', 120.0, 'idle', 0, 0),
(49, 'GZ007-04', 'slow', 7.0, 'idle', 0, 0),
(49, 'GZ007-05', 'fast', 180.0, 'idle', 0, 0),
(50, 'GZ008-01', 'fast', 180.0, 'busy', 0, 0),
(50, 'GZ008-02', 'fast', 180.0, 'busy', 0, 0),
(50, 'GZ008-03', 'fast', 180.0, 'busy', 0, 0),
(50, 'GZ008-04', 'slow', 7.0, 'idle', 0, 0),
(50, 'GZ008-05', 'slow', 7.0, 'idle', 0, 0),
(50, 'GZ008-06', 'fast', 180.0, 'idle', 0, 0),
(51, 'GZ009-01', 'fast', 180.0, 'idle', 0, 0),
(51, 'GZ009-02', 'slow', 7.0, 'idle', 0, 0),
(51, 'GZ009-03', 'fast', 180.0, 'idle', 0, 0),
(51, 'GZ009-04', 'fast', 180.0, 'idle', 0, 0),
(52, 'GZ010-01', 'slow', 7.0, 'idle', 0, 0),
(52, 'GZ010-02', 'fast', 120.0, 'idle', 0, 0),
(52, 'GZ010-03', 'fast', 180.0, 'idle', 0, 0),
(52, 'GZ010-04', 'fast', 120.0, 'idle', 0, 0),
(52, 'GZ010-05', 'fast', 120.0, 'idle', 0, 0),
(52, 'GZ010-06', 'slow', 7.0, 'idle', 0, 0),
(52, 'GZ010-07', 'fast', 120.0, 'idle', 0, 0),
(53, 'GZ011-01', 'fast', 180.0, 'idle', 0, 0),
(53, 'GZ011-02', 'slow', 7.0, 'busy', 0, 0),
(53, 'GZ011-03', 'slow', 7.0, 'idle', 0, 0),
(53, 'GZ011-04', 'fast', 180.0, 'idle', 0, 0),
(53, 'GZ011-05', 'fast', 180.0, 'idle', 0, 0),
(54, 'GZ012-01', 'slow', 7.0, 'busy', 0, 0),
(54, 'GZ012-02', 'fast', 180.0, 'idle', 0, 0),
(54, 'GZ012-03', 'fast', 180.0, 'idle', 0, 0),
(54, 'GZ012-04', 'slow', 7.0, 'busy', 0, 0),
(54, 'GZ012-05', 'fast', 120.0, 'idle', 0, 0),
(54, 'GZ012-06', 'fast', 120.0, 'idle', 0, 0),
(55, 'HZ004-01', 'fast', 180.0, 'idle', 0, 0),
(55, 'HZ004-02', 'slow', 7.0, 'idle', 0, 0),
(55, 'HZ004-03', 'fast', 120.0, 'idle', 0, 0),
(55, 'HZ004-04', 'fast', 120.0, 'idle', 0, 0),
(55, 'HZ004-05', 'fast', 180.0, 'fault', 0, 0),
(55, 'HZ004-06', 'fast', 120.0, 'idle', 0, 0),
(56, 'HZ005-01', 'slow', 7.0, 'idle', 0, 0),
(56, 'HZ005-02', 'fast', 120.0, 'idle', 0, 0),
(56, 'HZ005-03', 'fast', 180.0, 'idle', 0, 0),
(56, 'HZ005-04', 'fast', 120.0, 'idle', 0, 0),
(56, 'HZ005-05', 'slow', 7.0, 'idle', 0, 0),
(57, 'HZ006-01', 'fast', 180.0, 'idle', 0, 0),
(57, 'HZ006-02', 'fast', 120.0, 'idle', 0, 0),
(57, 'HZ006-03', 'slow', 7.0, 'idle', 0, 0),
(57, 'HZ006-04', 'fast', 180.0, 'idle', 0, 0),
(57, 'HZ006-05', 'slow', 7.0, 'idle', 0, 0),
(58, 'HZ007-01', 'fast', 120.0, 'idle', 0, 0),
(58, 'HZ007-02', 'slow', 7.0, 'idle', 0, 0),
(58, 'HZ007-03', 'fast', 120.0, 'idle', 0, 0),
(58, 'HZ007-04', 'fast', 120.0, 'idle', 0, 0),
(58, 'HZ007-05', 'fast', 120.0, 'idle', 0, 0),
(59, 'HZ008-01', 'fast', 120.0, 'idle', 0, 0),
(59, 'HZ008-02', 'slow', 7.0, 'idle', 0, 0),
(59, 'HZ008-03', 'fast', 120.0, 'idle', 0, 0),
(59, 'HZ008-04', 'fast', 180.0, 'idle', 0, 0),
(59, 'HZ008-05', 'fast', 180.0, 'idle', 0, 0),
(60, 'HZ009-01', 'fast', 120.0, 'idle', 0, 0),
(60, 'HZ009-02', 'slow', 7.0, 'idle', 0, 0),
(60, 'HZ009-03', 'slow', 7.0, 'idle', 0, 0),
(60, 'HZ009-04', 'fast', 120.0, 'idle', 0, 0),
(60, 'HZ009-05', 'fast', 180.0, 'busy', 0, 0),
(60, 'HZ009-06', 'fast', 180.0, 'idle', 0, 0),
(61, 'HZ010-01', 'slow', 7.0, 'busy', 0, 0),
(61, 'HZ010-02', 'fast', 120.0, 'idle', 0, 0),
(61, 'HZ010-03', 'fast', 180.0, 'idle', 0, 0),
(61, 'HZ010-04', 'fast', 180.0, 'busy', 0, 0),
(61, 'HZ010-05', 'fast', 120.0, 'idle', 0, 0),
(61, 'HZ010-06', 'slow', 7.0, 'idle', 0, 0),
(62, 'HZ011-01', 'slow', 7.0, 'idle', 0, 0),
(62, 'HZ011-02', 'fast', 120.0, 'fault', 0, 0),
(62, 'HZ011-03', 'fast', 180.0, 'idle', 0, 0),
(62, 'HZ011-04', 'slow', 7.0, 'fault', 0, 0),
(62, 'HZ011-05', 'fast', 180.0, 'idle', 0, 0),
(63, 'HZ012-01', 'slow', 7.0, 'fault', 0, 0),
(63, 'HZ012-02', 'fast', 180.0, 'busy', 0, 0),
(63, 'HZ012-03', 'fast', 120.0, 'idle', 0, 0),
(63, 'HZ012-04', 'slow', 7.0, 'fault', 0, 0),
(64, 'NJ004-01', 'slow', 7.0, 'idle', 0, 0),
(64, 'NJ004-02', 'fast', 120.0, 'idle', 0, 0),
(64, 'NJ004-03', 'fast', 180.0, 'idle', 0, 0),
(64, 'NJ004-04', 'fast', 120.0, 'idle', 0, 0),
(64, 'NJ004-05', 'fast', 120.0, 'idle', 0, 0),
(64, 'NJ004-06', 'slow', 7.0, 'idle', 0, 0),
(65, 'NJ005-01', 'slow', 7.0, 'fault', 0, 0),
(65, 'NJ005-02', 'slow', 7.0, 'busy', 0, 0),
(65, 'NJ005-03', 'fast', 120.0, 'idle', 0, 0),
(65, 'NJ005-04', 'fast', 180.0, 'busy', 0, 0),
(65, 'NJ005-05', 'fast', 120.0, 'fault', 0, 0),
(66, 'NJ006-01', 'slow', 7.0, 'idle', 0, 0),
(66, 'NJ006-02', 'fast', 120.0, 'fault', 0, 0),
(66, 'NJ006-03', 'fast', 180.0, 'fault', 0, 0),
(66, 'NJ006-04', 'fast', 120.0, 'idle', 0, 0),
(66, 'NJ006-05', 'slow', 7.0, 'idle', 0, 0),
(67, 'NJ007-01', 'fast', 120.0, 'idle', 0, 0),
(67, 'NJ007-02', 'fast', 180.0, 'fault', 0, 0),
(67, 'NJ007-03', 'fast', 180.0, 'idle', 0, 0),
(67, 'NJ007-04', 'fast', 180.0, 'fault', 0, 0),
(67, 'NJ007-05', 'fast', 180.0, 'idle', 0, 0),
(67, 'NJ007-06', 'slow', 7.0, 'idle', 0, 0),
(68, 'NJ008-01', 'slow', 7.0, 'busy', 0, 0),
(68, 'NJ008-02', 'slow', 7.0, 'idle', 0, 0),
(68, 'NJ008-03', 'fast', 180.0, 'fault', 0, 0),
(68, 'NJ008-04', 'fast', 120.0, 'fault', 0, 0),
(69, 'NJ009-01', 'fast', 180.0, 'idle', 0, 0),
(69, 'NJ009-02', 'fast', 120.0, 'idle', 0, 0),
(69, 'NJ009-03', 'fast', 120.0, 'fault', 0, 0),
(69, 'NJ009-04', 'fast', 120.0, 'idle', 0, 0),
(69, 'NJ009-05', 'fast', 120.0, 'busy', 0, 0),
(69, 'NJ009-06', 'slow', 7.0, 'idle', 0, 0),
(69, 'NJ009-07', 'fast', 180.0, 'fault', 0, 0),
(70, 'NJ010-01', 'fast', 180.0, 'idle', 0, 0),
(70, 'NJ010-02', 'fast', 180.0, 'busy', 0, 0),
(70, 'NJ010-03', 'fast', 180.0, 'busy', 0, 0),
(70, 'NJ010-04', 'slow', 7.0, 'busy', 0, 0),
(70, 'NJ010-05', 'fast', 180.0, 'idle', 0, 0),
(70, 'NJ010-06', 'fast', 120.0, 'idle', 0, 0),
(70, 'NJ010-07', 'fast', 180.0, 'idle', 0, 0),
(71, 'NJ011-01', 'fast', 120.0, 'idle', 0, 0),
(71, 'NJ011-02', 'slow', 7.0, 'idle', 0, 0),
(71, 'NJ011-03', 'fast', 180.0, 'busy', 0, 0),
(71, 'NJ011-04', 'fast', 180.0, 'idle', 0, 0),
(71, 'NJ011-05', 'fast', 180.0, 'busy', 0, 0),
(71, 'NJ011-06', 'slow', 7.0, 'idle', 0, 0),
(72, 'NJ012-01', 'fast', 120.0, 'fault', 0, 0),
(72, 'NJ012-02', 'fast', 120.0, 'busy', 0, 0),
(72, 'NJ012-03', 'fast', 180.0, 'idle', 0, 0),
(72, 'NJ012-04', 'fast', 120.0, 'idle', 0, 0),
(72, 'NJ012-05', 'fast', 120.0, 'idle', 0, 0),
(72, 'NJ012-06', 'slow', 7.0, 'idle', 0, 0);

-- 测试用户
INSERT INTO `user` (phone, nickname, balance, status) VALUES
('13800138001', '用户8001', 92.50, 'normal'),
('13800138002', '用户8002', 300.00, 'normal'),
('13800138006', '用户8006', 5.00, 'frozen');

-- 已结算订单示例（营收/查询演示）
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260828001',1,1,1,'settled',1.20,'2026-08-28 11:20:00','2026-08-28 11:29:00','2026-08-28 12:05:00',2160,30.00,36.00,'PAY20260828001','2026-08-28 11:20:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260715001',2,1,2,'settled',1.20,'2026-07-15 09:00:00','2026-07-15 09:10:00','2026-07-15 10:00:00',3000,40.00,48.00,'PAY20260715001','2026-07-15 09:00:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260810001',1,2,5,'settled',1.60,'2026-08-10 14:00:00','2026-08-10 14:10:00','2026-08-10 15:00:00',3000,30.00,48.00,'PAY20260810001','2026-08-10 14:00:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260901001',2,3,11,'settled',1.30,'2026-09-01 08:00:00','2026-09-01 08:05:00','2026-09-01 09:00:00',3300,50.00,65.00,'PAY20260901001','2026-09-01 08:00:00');

-- 补齐订单：每座电站订单数达 4~5 条；桩引用随桩重排同步更新（2026-09-10 生成）
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260701001',3,1,4,'pending_payment',1.20,'2026-07-01 10:21:00','2026-07-01 10:46:00','2026-07-01 13:14:50',8930,28.29,33.95,NULL,'2026-07-01 10:21:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260611001',2,1,1,'cancelled',1.20,'2026-06-11 23:31:00',NULL,NULL,0,0.00,0.00,NULL,'2026-06-11 23:31:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260720001',2,2,7,'cancelled',1.60,'2026-07-20 17:41:00',NULL,NULL,0,0.00,0.00,NULL,'2026-07-20 17:41:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260906001',3,2,8,'charging',1.60,'2026-09-06 09:09:00','2026-09-06 09:18:00',NULL,1527,25.33,40.53,NULL,'2026-09-06 09:09:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260802001',3,2,9,'settled',1.60,'2026-08-02 15:15:00','2026-08-02 15:33:00','2026-08-02 16:20:46',2866,35.19,56.30,'PAY20260802001','2026-08-02 15:15:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260608001',2,3,13,'reserved',1.30,'2026-06-08 14:46:00',NULL,NULL,0,0.00,0.00,NULL,'2026-06-08 14:46:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260701002',1,3,14,'pending_payment',1.30,'2026-07-01 02:06:00','2026-07-01 02:20:00','2026-07-01 02:56:00',2160,30.58,39.75,NULL,'2026-07-01 02:06:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260707001',1,3,11,'settled',1.30,'2026-07-07 14:16:00','2026-07-07 14:42:00','2026-07-07 15:26:31',2671,48.27,62.75,'PAY20260707001','2026-07-07 14:16:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260820001',2,3,12,'settled',1.30,'2026-08-20 22:04:00','2026-08-20 22:17:00','2026-08-20 22:51:42',2082,28.09,36.52,'PAY20260820001','2026-08-20 22:04:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260625001',1,4,16,'settled',1.50,'2026-06-25 16:54:00','2026-06-25 17:21:00','2026-06-25 19:41:32',8432,23.84,35.76,'PAY20260625001','2026-06-25 16:54:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260807001',1,4,17,'pending_payment',1.50,'2026-08-07 04:59:00','2026-08-07 05:12:00','2026-08-07 07:39:09',8829,36.84,55.26,NULL,'2026-08-07 04:59:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260907001',2,4,18,'settled',1.50,'2026-09-07 14:09:00','2026-09-07 14:39:00','2026-09-07 16:51:04',7924,37.18,55.77,'PAY20260907001','2026-09-07 14:09:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260729001',1,4,19,'cancelled',1.50,'2026-07-29 08:14:00',NULL,NULL,0,0.00,0.00,NULL,'2026-07-29 08:14:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260702001',1,5,21,'settled',1.40,'2026-07-02 12:11:00','2026-07-02 12:23:00','2026-07-02 13:52:58',5398,64.07,89.70,'PAY20260702001','2026-07-02 12:11:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260905001',1,5,22,'reserved',1.40,'2026-09-05 14:17:00',NULL,NULL,0,0.00,0.00,NULL,'2026-09-05 14:17:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260817001',2,5,23,'pending_payment',1.40,'2026-08-17 08:32:00','2026-08-17 08:49:00','2026-08-17 10:36:39',6459,43.60,61.04,NULL,'2026-08-17 08:32:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260616001',2,5,24,'settled',1.40,'2026-06-16 05:17:00','2026-06-16 05:31:00','2026-06-16 06:41:57',4257,37.65,52.71,'PAY20260616001','2026-06-16 05:17:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260608002',2,6,27,'settled',1.30,'2026-06-08 01:29:00','2026-06-08 01:41:00','2026-06-08 02:29:38',2918,59.99,77.99,'PAY20260608001','2026-06-08 01:29:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260704001',1,6,28,'settled',1.30,'2026-07-04 04:07:00','2026-07-04 04:18:00','2026-07-04 06:25:57',7677,52.62,68.41,'PAY20260704001','2026-07-04 04:07:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260619001',3,6,29,'charging',1.30,'2026-06-19 20:53:00','2026-06-19 21:02:00',NULL,1482,24.68,32.08,NULL,'2026-06-19 20:53:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260723001',1,6,30,'pending_payment',1.30,'2026-07-23 22:26:00','2026-07-23 22:50:00','2026-07-24 01:12:01',8521,55.35,71.95,NULL,'2026-07-23 22:26:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260601001',1,7,33,'pending_payment',1.20,'2026-06-01 02:35:00','2026-06-01 02:54:00','2026-06-01 04:00:27',3987,27.15,32.58,NULL,'2026-06-01 02:35:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260617001',1,7,34,'settled',1.20,'2026-06-17 06:52:00','2026-06-17 07:06:00','2026-06-17 07:45:56',2396,23.53,28.24,'PAY20260617001','2026-06-17 06:52:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260606001',2,7,35,'charging',1.20,'2026-06-06 14:09:00','2026-06-06 14:12:00',NULL,1306,21.44,25.73,NULL,'2026-06-06 14:09:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260711001',3,7,36,'settled',1.20,'2026-07-11 22:42:00','2026-07-11 22:44:00','2026-07-12 00:19:14',5714,34.60,41.52,'PAY20260711001','2026-07-11 22:42:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260607001',2,7,37,'settled',1.20,'2026-06-07 14:24:00','2026-06-07 14:39:00','2026-06-07 15:56:01',4621,21.07,25.28,'PAY20260607001','2026-06-07 14:24:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260608003',2,8,39,'settled',1.60,'2026-06-08 13:26:00','2026-06-08 13:36:00','2026-06-08 14:33:30',3450,67.96,108.74,'PAY20260608002','2026-06-08 13:26:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260620001',1,8,40,'cancelled',1.60,'2026-06-20 06:45:00',NULL,NULL,0,0.00,0.00,NULL,'2026-06-20 06:45:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260718001',1,8,41,'settled',1.60,'2026-07-18 06:34:00','2026-07-18 06:42:00','2026-07-18 08:18:42',5802,59.00,94.40,'PAY20260718001','2026-07-18 06:34:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260728001',2,8,38,'reserved',1.60,'2026-07-28 14:28:00',NULL,NULL,0,0.00,0.00,NULL,'2026-07-28 14:28:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260807002',2,9,43,'charging',1.30,'2026-08-07 23:49:00','2026-08-07 23:51:00',NULL,2147,11.49,14.94,NULL,'2026-08-07 23:49:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260905002',3,9,44,'settled',1.30,'2026-09-05 13:21:00','2026-09-05 13:40:00','2026-09-05 14:27:10',2830,53.24,69.21,'PAY20260905001','2026-09-05 13:21:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260615001',1,9,45,'cancelled',1.30,'2026-06-15 01:11:00',NULL,NULL,0,0.00,0.00,NULL,'2026-06-15 01:11:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260703001',3,9,42,'settled',1.30,'2026-07-03 08:44:00','2026-07-03 08:50:00','2026-07-03 09:42:30',3150,36.28,47.16,'PAY20260703001','2026-07-03 08:44:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260807003',1,10,47,'settled',1.50,'2026-08-07 22:10:00','2026-08-07 22:12:00','2026-08-07 23:24:24',4344,56.24,84.36,'PAY20260807001','2026-08-07 22:10:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260807004',1,10,48,'settled',1.50,'2026-08-07 10:49:00','2026-08-07 10:53:00','2026-08-07 12:21:21',5301,69.10,103.65,'PAY20260807002','2026-08-07 10:49:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260823001',3,10,49,'settled',1.50,'2026-08-23 05:59:00','2026-08-23 06:15:00','2026-08-23 06:51:30',2190,53.49,80.23,'PAY20260823001','2026-08-23 05:59:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260620002',2,10,50,'settled',1.50,'2026-06-20 05:05:00','2026-06-20 05:26:00','2026-06-20 06:50:16',5056,49.36,74.04,'PAY20260620001','2026-06-20 05:05:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260626001',2,11,52,'settled',1.40,'2026-06-26 06:38:00','2026-06-26 06:50:00','2026-06-26 07:32:37',2557,53.41,74.77,'PAY20260626001','2026-06-26 06:38:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260813001',1,11,53,'settled',1.40,'2026-08-13 07:04:00','2026-08-13 07:20:00','2026-08-13 08:35:04',4504,67.24,94.14,'PAY20260813001','2026-08-13 07:04:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260804001',3,11,54,'settled',1.40,'2026-08-04 09:39:00','2026-08-04 09:55:00','2026-08-04 11:14:34',4774,41.09,57.53,'PAY20260804001','2026-08-04 09:39:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260716001',3,11,55,'settled',1.40,'2026-07-16 06:50:00','2026-07-16 07:15:00','2026-07-16 09:22:59',7679,38.18,53.45,'PAY20260716001','2026-07-16 06:50:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260805001',1,11,51,'settled',1.40,'2026-08-05 12:50:00','2026-08-05 13:16:00','2026-08-05 14:54:00',5880,48.25,67.55,'PAY20260805001','2026-08-05 12:50:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260627001',3,12,57,'cancelled',1.30,'2026-06-27 23:33:00',NULL,NULL,0,0.00,0.00,NULL,'2026-06-27 23:33:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260821001',3,12,58,'charging',1.30,'2026-08-21 01:47:00','2026-08-21 01:55:00',NULL,941,28.32,36.82,NULL,'2026-08-21 01:47:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260821002',3,12,59,'settled',1.30,'2026-08-21 22:22:00','2026-08-21 22:27:00','2026-08-21 23:33:58',4018,38.78,50.41,'PAY20260821001','2026-08-21 22:22:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260825001',2,12,60,'settled',1.30,'2026-08-25 23:51:00','2026-08-26 00:18:00','2026-08-26 01:15:13',3433,68.13,88.57,'PAY20260825001','2026-08-25 23:51:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260618001',1,12,56,'settled',1.30,'2026-06-18 08:51:00','2026-06-18 08:56:00','2026-06-18 09:52:17',3377,37.55,48.81,'PAY20260618001','2026-06-18 08:51:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260705001',1,13,62,'settled',1.50,'2026-07-05 09:44:00','2026-07-05 10:05:00','2026-07-05 11:38:31',5611,33.22,49.83,'PAY20260705001','2026-07-05 09:44:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260901002',1,13,63,'settled',1.50,'2026-09-01 00:18:00','2026-09-01 00:28:00','2026-09-01 02:57:07',8947,44.33,66.50,'PAY20260901002','2026-09-01 00:18:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260828002',3,13,64,'pending_payment',1.50,'2026-08-28 09:48:00','2026-08-28 10:02:00','2026-08-28 11:26:46',5086,44.81,67.22,NULL,'2026-08-28 09:48:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260602001',2,13,65,'settled',1.50,'2026-06-02 00:31:00','2026-06-02 00:44:00','2026-06-02 02:20:50',5810,68.41,102.61,'PAY20260602001','2026-06-02 00:31:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260609001',2,14,69,'settled',1.20,'2026-06-09 08:51:00','2026-06-09 09:15:00','2026-06-09 10:27:50',4370,44.95,53.94,'PAY20260609001','2026-06-09 08:51:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260722001',3,14,70,'settled',1.20,'2026-07-22 20:56:00','2026-07-22 21:15:00','2026-07-22 22:22:37',4057,55.66,66.79,'PAY20260722001','2026-07-22 20:56:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260827001',2,14,71,'pending_payment',1.20,'2026-08-27 00:02:00','2026-08-27 00:04:00','2026-08-27 01:02:25',3505,54.21,65.05,NULL,'2026-08-27 00:02:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260719001',3,14,72,'settled',1.20,'2026-07-19 21:49:00','2026-07-19 22:06:00','2026-07-19 23:16:51',4251,46.14,55.37,'PAY20260719001','2026-07-19 21:49:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260830001',2,14,73,'settled',1.20,'2026-08-30 03:48:00','2026-08-30 03:57:00','2026-08-30 05:17:09',4809,37.70,45.24,'PAY20260830001','2026-08-30 03:48:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260803001',1,15,75,'settled',1.20,'2026-08-03 06:04:00','2026-08-03 06:26:00','2026-08-03 08:09:38',6218,30.73,36.88,'PAY20260803001','2026-08-03 06:04:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260811001',3,15,76,'settled',1.20,'2026-08-11 04:45:00','2026-08-11 05:06:00','2026-08-11 05:56:59',3059,65.18,78.22,'PAY20260811001','2026-08-11 04:45:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260812001',1,15,77,'charging',1.20,'2026-08-12 21:46:00','2026-08-12 22:00:00',NULL,1880,22.89,27.47,NULL,'2026-08-12 21:46:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260617002',2,15,74,'cancelled',1.20,'2026-06-17 20:09:00',NULL,NULL,0,0.00,0.00,NULL,'2026-06-17 20:09:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260716002',2,16,79,'pending_payment',1.40,'2026-07-16 14:19:00','2026-07-16 14:45:00','2026-07-16 16:14:25',5365,59.61,83.45,NULL,'2026-07-16 14:19:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260625002',2,16,80,'settled',1.40,'2026-06-25 07:02:00','2026-06-25 07:26:00','2026-06-25 08:18:55',3175,43.27,60.58,'PAY20260625002','2026-06-25 07:02:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260603001',2,16,81,'settled',1.40,'2026-06-03 05:38:00','2026-06-03 05:54:00','2026-06-03 07:43:33',6573,57.07,79.90,'PAY20260603001','2026-06-03 05:38:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260609002',2,16,82,'pending_payment',1.40,'2026-06-09 15:22:00','2026-06-09 15:38:00','2026-06-09 16:58:45',4845,39.65,55.51,NULL,'2026-06-09 15:22:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260804002',3,17,86,'cancelled',1.30,'2026-08-04 07:54:00',NULL,NULL,0,0.00,0.00,NULL,'2026-08-04 07:54:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260731001',2,17,87,'settled',1.30,'2026-07-31 21:17:00','2026-07-31 21:25:00','2026-07-31 22:24:54',3594,21.58,28.05,'PAY20260731001','2026-07-31 21:17:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260823002',3,17,88,'settled',1.30,'2026-08-23 07:33:00','2026-08-23 08:03:00','2026-08-23 09:24:14',4874,63.10,82.03,'PAY20260823002','2026-08-23 07:33:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260814001',3,17,89,'settled',1.30,'2026-08-14 00:19:00','2026-08-14 00:46:00','2026-08-14 02:27:28',6088,68.94,89.62,'PAY20260814001','2026-08-14 00:19:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260603002',1,18,92,'reserved',1.30,'2026-06-03 12:41:00',NULL,NULL,0,0.00,0.00,NULL,'2026-06-03 12:41:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260708001',1,18,93,'cancelled',1.30,'2026-07-08 08:59:00',NULL,NULL,0,0.00,0.00,NULL,'2026-07-08 08:59:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260605001',3,18,94,'pending_payment',1.30,'2026-06-05 08:21:00','2026-06-05 08:26:00','2026-06-05 09:20:55',3295,32.73,42.55,NULL,'2026-06-05 08:21:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260705002',2,18,95,'charging',1.30,'2026-07-05 14:22:00','2026-07-05 14:32:00',NULL,2209,25.20,32.76,NULL,'2026-07-05 14:22:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260814002',3,18,91,'settled',1.30,'2026-08-14 21:28:00','2026-08-14 21:53:00','2026-08-15 00:08:54',8154,60.39,78.51,'PAY20260814002','2026-08-14 21:28:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260708002',2,19,97,'charging',1.20,'2026-07-08 03:32:00','2026-07-08 03:40:00',NULL,1873,6.26,7.51,NULL,'2026-07-08 03:32:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260731002',1,19,98,'settled',1.20,'2026-07-31 03:21:00','2026-07-31 03:45:00','2026-07-31 04:21:30',2190,61.52,73.82,'PAY20260731002','2026-07-31 03:21:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260814003',1,19,99,'settled',1.20,'2026-08-14 10:40:00','2026-08-14 10:45:00','2026-08-14 13:07:09',8529,20.09,24.11,'PAY20260814003','2026-08-14 10:40:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260716003',1,19,100,'charging',1.20,'2026-07-16 02:39:00','2026-07-16 02:46:00',NULL,2280,6.01,7.21,NULL,'2026-07-16 02:39:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260715002',3,19,101,'charging',1.20,'2026-07-15 18:58:00','2026-07-15 19:00:00',NULL,1895,16.05,19.26,NULL,'2026-07-15 18:58:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260829001',1,20,103,'pending_payment',1.10,'2026-08-29 14:13:00','2026-08-29 14:26:00','2026-08-29 15:58:56',5576,29.76,32.74,NULL,'2026-08-29 14:13:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260719002',2,20,104,'settled',1.10,'2026-07-19 11:27:00','2026-07-19 11:56:00','2026-07-19 14:15:47',8387,53.58,58.94,'PAY20260719002','2026-07-19 11:27:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260611002',3,20,105,'cancelled',1.10,'2026-06-11 08:23:00',NULL,NULL,0,0.00,0.00,NULL,'2026-06-11 08:23:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260725001',1,20,102,'charging',1.10,'2026-07-25 08:41:00','2026-07-25 08:44:00',NULL,767,5.71,6.28,NULL,'2026-07-25 08:41:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260903001',3,20,103,'reserved',1.10,'2026-09-03 03:15:00',NULL,NULL,0,0.00,0.00,NULL,'2026-09-03 03:15:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260629001',2,21,107,'pending_payment',1.10,'2026-06-29 18:56:00','2026-06-29 19:02:00','2026-06-29 21:07:09',7509,25.66,28.23,NULL,'2026-06-29 18:56:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260630001',2,21,108,'charging',1.10,'2026-06-30 20:41:00','2026-06-30 20:56:00',NULL,1215,9.13,10.04,NULL,'2026-06-30 20:41:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260612001',3,21,109,'settled',1.10,'2026-06-12 12:58:00','2026-06-12 13:25:00','2026-06-12 15:00:14',5714,33.07,36.38,'PAY20260612001','2026-06-12 12:58:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260813002',1,21,110,'reserved',1.10,'2026-08-13 07:04:00',NULL,NULL,0,0.00,0.00,NULL,'2026-08-13 07:04:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260904001',3,21,106,'settled',1.10,'2026-09-04 11:06:00','2026-09-04 11:29:00','2026-09-04 12:40:34',4294,66.12,72.73,'PAY20260904001','2026-09-04 11:06:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260805002',2,22,112,'reserved',1.20,'2026-08-05 04:05:00',NULL,NULL,0,0.00,0.00,NULL,'2026-08-05 04:05:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260626002',2,22,113,'pending_payment',1.20,'2026-06-26 10:06:00','2026-06-26 10:25:00','2026-06-26 12:24:27',7167,41.85,50.22,NULL,'2026-06-26 10:06:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260710001',1,22,114,'settled',1.20,'2026-07-10 02:50:00','2026-07-10 03:03:00','2026-07-10 05:28:19',8719,45.54,54.65,'PAY20260710001','2026-07-10 02:50:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260905003',2,22,115,'cancelled',1.20,'2026-09-05 21:35:00',NULL,NULL,0,0.00,0.00,NULL,'2026-09-05 21:35:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260720002',3,23,118,'settled',1.30,'2026-07-20 20:58:00','2026-07-20 21:05:00','2026-07-20 21:43:05',2285,53.11,69.04,'PAY20260720001','2026-07-20 20:58:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260721001',2,23,119,'settled',1.30,'2026-07-21 19:45:00','2026-07-21 20:11:00','2026-07-21 22:26:57',8157,44.87,58.33,'PAY20260721001','2026-07-21 19:45:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260603003',2,23,120,'pending_payment',1.30,'2026-06-03 21:47:00','2026-06-03 21:49:00','2026-06-03 23:30:29',6089,48.34,62.84,NULL,'2026-06-03 21:47:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260831001',2,23,121,'settled',1.30,'2026-08-31 04:12:00','2026-08-31 04:18:00','2026-08-31 04:48:54',1854,35.17,45.72,'PAY20260831001','2026-08-31 04:12:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260704002',1,23,122,'settled',1.30,'2026-07-04 16:55:00','2026-07-04 17:09:00','2026-07-04 18:33:58',5098,59.44,77.27,'PAY20260704002','2026-07-04 16:55:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260831002',2,24,125,'settled',1.40,'2026-08-31 08:51:00','2026-08-31 09:01:00','2026-08-31 10:11:44',4244,58.41,81.77,'PAY20260831002','2026-08-31 08:51:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260614001',2,24,126,'pending_payment',1.40,'2026-06-14 14:25:00','2026-06-14 14:52:00','2026-06-14 17:07:21',8121,29.34,41.08,NULL,'2026-06-14 14:25:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260706001',2,24,127,'settled',1.40,'2026-07-06 11:07:00','2026-07-06 11:18:00','2026-07-06 13:20:14',7334,27.02,37.83,'PAY20260706001','2026-07-06 11:07:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260613001',2,24,128,'settled',1.40,'2026-06-13 18:01:00','2026-06-13 18:16:00','2026-06-13 19:56:03',6003,21.29,29.81,'PAY20260613001','2026-06-13 18:01:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260817002',1,24,124,'settled',1.40,'2026-08-17 14:45:00','2026-08-17 15:07:00','2026-08-17 17:15:20',7700,49.84,69.78,'PAY20260817001','2026-08-17 14:45:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260722002',2,25,130,'pending_payment',1.50,'2026-07-22 19:17:00','2026-07-22 19:41:00','2026-07-22 20:39:59',3539,30.27,45.41,NULL,'2026-07-22 19:17:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260625003',3,25,131,'charging',1.50,'2026-06-25 22:13:00','2026-06-25 22:25:00',NULL,1263,20.59,30.88,NULL,'2026-06-25 22:13:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260904002',3,25,132,'cancelled',1.50,'2026-09-04 22:25:00',NULL,NULL,0,0.00,0.00,NULL,'2026-09-04 22:25:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260815001',1,25,133,'charging',1.50,'2026-08-15 08:55:00','2026-08-15 09:12:00',NULL,1690,25.56,38.34,NULL,'2026-08-15 08:55:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260903002',1,25,134,'settled',1.50,'2026-09-03 08:55:00','2026-09-03 09:13:00','2026-09-03 11:19:58',7618,43.78,65.67,'PAY20260903001','2026-09-03 08:55:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260901003',3,26,137,'settled',1.10,'2026-09-01 12:25:00','2026-09-01 12:46:00','2026-09-01 13:43:31',3451,42.50,46.75,'PAY20260901003','2026-09-01 12:25:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260716004',3,26,138,'settled',1.10,'2026-07-16 04:00:00','2026-07-16 04:24:00','2026-07-16 05:40:17',4577,36.02,39.62,'PAY20260716002','2026-07-16 04:00:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260710002',2,26,139,'pending_payment',1.10,'2026-07-10 12:05:00','2026-07-10 12:07:00','2026-07-10 14:18:23',7883,53.68,59.05,NULL,'2026-07-10 12:05:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260805003',1,26,140,'settled',1.10,'2026-08-05 14:26:00','2026-08-05 14:46:00','2026-08-05 15:50:20',3860,35.53,39.08,'PAY20260805002','2026-08-05 14:26:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260823003',3,26,141,'settled',1.10,'2026-08-23 03:35:00','2026-08-23 03:54:00','2026-08-23 04:56:58',3778,49.08,53.99,'PAY20260823003','2026-08-23 03:35:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260622001',2,27,144,'settled',1.20,'2026-06-22 05:34:00','2026-06-22 05:50:00','2026-06-22 07:29:34',5974,48.28,57.94,'PAY20260622001','2026-06-22 05:34:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260616002',1,27,145,'charging',1.20,'2026-06-16 14:56:00','2026-06-16 15:02:00',NULL,2219,14.28,17.14,NULL,'2026-06-16 14:56:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260830002',3,27,146,'settled',1.20,'2026-08-30 18:29:00','2026-08-30 18:48:00','2026-08-30 19:30:46',2566,40.11,48.13,'PAY20260830002','2026-08-30 18:29:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260904003',1,27,147,'charging',1.20,'2026-09-04 22:41:00','2026-09-04 22:54:00',NULL,1739,16.00,19.20,NULL,'2026-09-04 22:41:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260817003',3,27,143,'settled',1.20,'2026-08-17 16:18:00','2026-08-17 16:32:00','2026-08-17 18:45:48',8028,41.77,50.12,'PAY20260817002','2026-08-17 16:18:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260711002',3,28,149,'settled',1.30,'2026-07-11 22:41:00','2026-07-11 23:06:00','2026-07-12 01:12:29',7589,59.77,77.70,'PAY20260711002','2026-07-11 22:41:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260819001',2,28,150,'charging',1.30,'2026-08-19 04:12:00','2026-08-19 04:26:00',NULL,1266,5.62,7.31,NULL,'2026-08-19 04:12:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260811002',1,28,151,'settled',1.30,'2026-08-11 01:05:00','2026-08-11 01:14:00','2026-08-11 01:53:57',2397,67.29,87.48,'PAY20260811002','2026-08-11 01:05:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260812002',2,28,152,'settled',1.30,'2026-08-12 17:00:00','2026-08-12 17:08:00','2026-08-12 17:49:56',2516,25.86,33.62,'PAY20260812001','2026-08-12 17:00:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260601002',3,28,153,'cancelled',1.30,'2026-06-01 19:32:00',NULL,NULL,0,0.00,0.00,NULL,'2026-06-01 19:32:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260722003',1,29,156,'cancelled',1.40,'2026-07-22 10:32:00',NULL,NULL,0,0.00,0.00,NULL,'2026-07-22 10:32:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260630002',1,29,157,'pending_payment',1.40,'2026-06-30 01:23:00','2026-06-30 01:51:00','2026-06-30 04:06:17',8117,45.79,64.11,NULL,'2026-06-30 01:23:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260603004',2,29,158,'settled',1.40,'2026-06-03 07:18:00','2026-06-03 07:27:00','2026-06-03 08:40:56',4436,31.76,44.46,'PAY20260603002','2026-06-03 07:18:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260806001',2,29,159,'pending_payment',1.40,'2026-08-06 08:11:00','2026-08-06 08:18:00','2026-08-06 09:25:58',4078,66.16,92.62,NULL,'2026-08-06 08:11:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260827002',3,29,160,'settled',1.40,'2026-08-27 20:10:00','2026-08-27 20:14:00','2026-08-27 21:32:57',4737,46.06,64.48,'PAY20260827001','2026-08-27 20:10:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260721002',2,30,162,'charging',1.50,'2026-07-21 18:48:00','2026-07-21 18:52:00',NULL,785,8.81,13.21,NULL,'2026-07-21 18:48:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260727001',3,30,163,'cancelled',1.50,'2026-07-27 04:52:00',NULL,NULL,0,0.00,0.00,NULL,'2026-07-27 04:52:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260625004',3,30,164,'reserved',1.50,'2026-06-25 02:00:00',NULL,NULL,0,0.00,0.00,NULL,'2026-06-25 02:00:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260821003',2,30,165,'settled',1.50,'2026-08-21 11:03:00','2026-08-21 11:11:00','2026-08-21 13:01:31',6631,47.99,71.98,'PAY20260821002','2026-08-21 11:03:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260701003',3,31,168,'settled',1.10,'2026-07-01 22:22:00','2026-07-01 22:24:00','2026-07-02 00:05:56',6116,40.89,44.98,'PAY20260701001','2026-07-01 22:22:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260808001',1,31,169,'pending_payment',1.10,'2026-08-08 02:29:00','2026-08-08 02:38:00','2026-08-08 03:15:16',2236,53.17,58.49,NULL,'2026-08-08 02:29:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260626003',3,31,170,'settled',1.10,'2026-06-26 14:00:00','2026-06-26 14:13:00','2026-06-26 14:46:53',2033,59.66,65.63,'PAY20260626002','2026-06-26 14:00:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260624001',1,31,167,'cancelled',1.10,'2026-06-24 03:40:00',NULL,NULL,0,0.00,0.00,NULL,'2026-06-24 03:40:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260702002',1,32,172,'charging',1.20,'2026-07-02 09:18:00','2026-07-02 09:27:00',NULL,1065,25.86,31.03,NULL,'2026-07-02 09:18:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260612002',1,32,173,'pending_payment',1.20,'2026-06-12 18:45:00','2026-06-12 19:00:00','2026-06-12 20:17:25',4645,31.50,37.80,NULL,'2026-06-12 18:45:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260617003',1,32,174,'settled',1.20,'2026-06-17 21:07:00','2026-06-17 21:21:00','2026-06-17 22:14:16',3196,53.09,63.71,'PAY20260617002','2026-06-17 21:07:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260702003',1,32,171,'settled',1.20,'2026-07-02 01:17:00','2026-07-02 01:23:00','2026-07-02 02:43:03',4803,36.91,44.29,'PAY20260702002','2026-07-02 01:17:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260812003',3,32,172,'settled',1.20,'2026-08-12 16:29:00','2026-08-12 16:36:00','2026-08-12 18:29:49',6829,62.16,74.59,'PAY20260812002','2026-08-12 16:29:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260725002',2,33,176,'settled',1.30,'2026-07-25 06:32:00','2026-07-25 06:42:00','2026-07-25 08:45:00',7380,45.55,59.21,'PAY20260725001','2026-07-25 06:32:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260710003',3,33,177,'charging',1.30,'2026-07-10 20:45:00','2026-07-10 20:47:00',NULL,2253,14.22,18.49,NULL,'2026-07-10 20:45:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260830003',1,33,178,'settled',1.30,'2026-08-30 00:37:00','2026-08-30 01:03:00','2026-08-30 02:25:23',4943,41.78,54.31,'PAY20260830003','2026-08-30 00:37:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260906002',2,33,179,'settled',1.30,'2026-09-06 14:06:00','2026-09-06 14:34:00','2026-09-06 15:17:47',2627,59.23,77.00,'PAY20260906001','2026-09-06 14:06:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260722004',3,34,183,'settled',1.40,'2026-07-22 04:56:00','2026-07-22 05:08:00','2026-07-22 05:49:15',2475,63.31,88.63,'PAY20260722002','2026-07-22 04:56:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260713001',3,34,184,'charging',1.40,'2026-07-13 03:52:00','2026-07-13 04:12:00',NULL,822,29.26,40.96,NULL,'2026-07-13 03:52:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260831003',2,34,185,'settled',1.40,'2026-08-31 09:10:00','2026-08-31 09:35:00','2026-08-31 10:13:27',2307,55.46,77.64,'PAY20260831003','2026-08-31 09:10:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260619002',2,34,186,'settled',1.40,'2026-06-19 20:50:00','2026-06-19 21:05:00','2026-06-19 22:26:43',4903,67.55,94.57,'PAY20260619001','2026-06-19 20:50:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260623001',3,34,187,'settled',1.40,'2026-06-23 13:55:00','2026-06-23 13:57:00','2026-06-23 15:50:49',6829,36.06,50.48,'PAY20260623001','2026-06-23 13:55:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260705003',3,35,189,'charging',1.50,'2026-07-05 10:07:00','2026-07-05 10:26:00',NULL,1792,14.48,21.72,NULL,'2026-07-05 10:07:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260904004',1,35,190,'settled',1.50,'2026-09-04 16:18:00','2026-09-04 16:48:00','2026-09-04 18:10:38',4958,61.73,92.59,'PAY20260904002','2026-09-04 16:18:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260615002',3,35,191,'pending_payment',1.50,'2026-06-15 05:42:00','2026-06-15 05:46:00','2026-06-15 07:08:48',4968,66.34,99.51,NULL,'2026-06-15 05:42:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260813003',1,35,192,'cancelled',1.50,'2026-08-13 06:28:00',NULL,NULL,0,0.00,0.00,NULL,'2026-08-13 06:28:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260903003',2,35,193,'reserved',1.50,'2026-09-03 14:46:00',NULL,NULL,0,0.00,0.00,NULL,'2026-09-03 14:46:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260719003',3,36,196,'charging',1.10,'2026-07-19 16:21:00','2026-07-19 16:23:00',NULL,771,5.77,6.35,NULL,'2026-07-19 16:21:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260710004',3,36,197,'pending_payment',1.10,'2026-07-10 11:15:00','2026-07-10 11:24:00','2026-07-10 13:30:58',7618,46.35,50.99,NULL,'2026-07-10 11:15:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260626004',2,36,198,'cancelled',1.10,'2026-06-26 03:45:00',NULL,NULL,0,0.00,0.00,NULL,'2026-06-26 03:45:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260717001',3,36,199,'settled',1.10,'2026-07-17 12:48:00','2026-07-17 13:05:00','2026-07-17 13:38:47',2027,59.62,65.58,'PAY20260717001','2026-07-17 12:48:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260905004',2,37,201,'settled',1.20,'2026-09-05 01:06:00','2026-09-05 01:16:00','2026-09-05 02:40:17',5057,62.80,75.36,'PAY20260905002','2026-09-05 01:06:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260711003',1,37,202,'cancelled',1.20,'2026-07-11 08:28:00',NULL,NULL,0,0.00,0.00,NULL,'2026-07-11 08:28:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260704003',1,37,203,'cancelled',1.20,'2026-07-04 04:45:00',NULL,NULL,0,0.00,0.00,NULL,'2026-07-04 04:45:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260626005',1,37,204,'charging',1.20,'2026-06-26 19:57:00','2026-06-26 20:03:00',NULL,1431,9.64,11.57,NULL,'2026-06-26 19:57:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260609003',1,38,206,'pending_payment',1.30,'2026-06-09 00:35:00','2026-06-09 00:42:00','2026-06-09 01:16:52',2092,61.60,80.08,NULL,'2026-06-09 00:35:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260804003',1,38,207,'settled',1.30,'2026-08-04 20:01:00','2026-08-04 20:25:00','2026-08-04 21:48:21',5001,66.16,86.01,'PAY20260804002','2026-08-04 20:01:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260809001',2,38,208,'cancelled',1.30,'2026-08-09 11:22:00',NULL,NULL,0,0.00,0.00,NULL,'2026-08-09 11:22:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260616003',1,38,209,'pending_payment',1.30,'2026-06-16 00:07:00','2026-06-16 00:33:00','2026-06-16 01:51:28',4708,52.02,67.63,NULL,'2026-06-16 00:07:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260618002',1,39,213,'settled',1.40,'2026-06-18 09:32:00','2026-06-18 09:52:00','2026-06-18 11:36:33',6273,51.48,72.07,'PAY20260618002','2026-06-18 09:32:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260624002',3,39,214,'reserved',1.40,'2026-06-24 01:53:00',NULL,NULL,0,0.00,0.00,NULL,'2026-06-24 01:53:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260727002',2,39,215,'settled',1.40,'2026-07-27 10:58:00','2026-07-27 11:21:00','2026-07-27 13:19:35',7115,30.95,43.33,'PAY20260727001','2026-07-27 10:58:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260704004',2,39,212,'reserved',1.40,'2026-07-04 04:09:00',NULL,NULL,0,0.00,0.00,NULL,'2026-07-04 04:09:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260827003',1,39,213,'settled',1.40,'2026-08-27 10:41:00','2026-08-27 11:06:00','2026-08-27 13:34:42',8922,30.09,42.13,'PAY20260827002','2026-08-27 10:41:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260807005',1,40,217,'charging',1.50,'2026-08-07 05:23:00','2026-08-07 05:30:00',NULL,2369,5.34,8.01,NULL,'2026-08-07 05:23:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260721003',3,40,218,'settled',1.50,'2026-07-21 15:17:00','2026-07-21 15:29:00','2026-07-21 17:54:57',8757,68.67,103.00,'PAY20260721002','2026-07-21 15:17:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260701004',1,40,219,'settled',1.50,'2026-07-01 01:49:00','2026-07-01 01:53:00','2026-07-01 03:41:04',6484,56.05,84.07,'PAY20260701002','2026-07-01 01:49:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260630003',3,40,220,'settled',1.50,'2026-06-30 07:10:00','2026-06-30 07:33:00','2026-06-30 08:51:48',4728,61.31,91.97,'PAY20260630001','2026-06-30 07:10:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260615003',2,41,223,'charging',1.10,'2026-06-15 13:12:00','2026-06-15 13:29:00',NULL,1996,28.27,31.10,NULL,'2026-06-15 13:12:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260722005',1,41,224,'settled',1.10,'2026-07-22 15:07:00','2026-07-22 15:29:00','2026-07-22 17:56:22',8842,68.40,75.24,'PAY20260722003','2026-07-22 15:07:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260705004',2,41,225,'cancelled',1.10,'2026-07-05 00:37:00',NULL,NULL,0,0.00,0.00,NULL,'2026-07-05 00:37:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260714001',3,41,226,'settled',1.10,'2026-07-14 12:13:00','2026-07-14 12:36:00','2026-07-14 15:05:07',8947,36.61,40.27,'PAY20260714001','2026-07-14 12:13:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260708003',3,41,222,'cancelled',1.10,'2026-07-08 11:48:00',NULL,NULL,0,0.00,0.00,NULL,'2026-07-08 11:48:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260622002',3,42,228,'pending_payment',1.20,'2026-06-22 10:36:00','2026-06-22 11:04:00','2026-06-22 12:15:04',4264,20.18,24.22,NULL,'2026-06-22 10:36:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260828003',1,42,229,'pending_payment',1.20,'2026-08-28 20:03:00','2026-08-28 20:21:00','2026-08-28 20:53:51',1971,29.45,35.34,NULL,'2026-08-28 20:03:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260901004',3,42,230,'charging',1.20,'2026-09-01 15:25:00','2026-09-01 15:33:00',NULL,1213,13.59,16.31,NULL,'2026-09-01 15:25:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260810002',1,42,231,'settled',1.20,'2026-08-10 11:10:00','2026-08-10 11:19:00','2026-08-10 13:36:55',8275,41.86,50.23,'PAY20260810002','2026-08-10 11:10:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260616004',3,43,235,'pending_payment',1.30,'2026-06-16 02:46:00','2026-06-16 03:10:00','2026-06-16 04:53:02',6182,21.34,27.74,NULL,'2026-06-16 02:46:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260624003',1,43,236,'settled',1.30,'2026-06-24 22:32:00','2026-06-24 22:49:00','2026-06-25 01:12:44',8624,28.24,36.71,'PAY20260624001','2026-06-24 22:32:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260705005',2,43,237,'settled',1.30,'2026-07-05 09:06:00','2026-07-05 09:18:00','2026-07-05 11:27:26',7766,44.44,57.77,'PAY20260705002','2026-07-05 09:06:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260828004',2,43,238,'cancelled',1.30,'2026-08-28 15:18:00',NULL,NULL,0,0.00,0.00,NULL,'2026-08-28 15:18:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260811003',3,44,240,'settled',1.40,'2026-08-11 21:36:00','2026-08-11 21:48:00','2026-08-11 22:25:13',2233,63.42,88.79,'PAY20260811003','2026-08-11 21:36:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260715003',1,44,241,'pending_payment',1.40,'2026-07-15 18:15:00','2026-07-15 18:22:00','2026-07-15 20:18:40',7000,61.00,85.40,NULL,'2026-07-15 18:15:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260614002',1,44,242,'settled',1.40,'2026-06-14 06:31:00','2026-06-14 06:33:00','2026-06-14 08:18:56',6356,21.25,29.75,'PAY20260614001','2026-06-14 06:31:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260603005',3,44,243,'pending_payment',1.40,'2026-06-03 19:52:00','2026-06-03 20:19:00','2026-06-03 21:03:13',2653,40.73,57.02,NULL,'2026-06-03 19:52:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260829002',2,45,246,'settled',1.50,'2026-08-29 12:51:00','2026-08-29 13:01:00','2026-08-29 13:37:58',2218,22.81,34.21,'PAY20260829001','2026-08-29 12:51:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260805004',3,45,247,'pending_payment',1.50,'2026-08-05 09:03:00','2026-08-05 09:21:00','2026-08-05 09:51:15',1815,23.52,35.28,NULL,'2026-08-05 09:03:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260722006',3,45,248,'settled',1.50,'2026-07-22 06:50:00','2026-07-22 06:55:00','2026-07-22 09:07:11',7931,59.14,88.71,'PAY20260722004','2026-07-22 06:50:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260809002',1,45,245,'settled',1.50,'2026-08-09 21:04:00','2026-08-09 21:10:00','2026-08-09 23:13:41',7421,27.21,40.81,'PAY20260809001','2026-08-09 21:04:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260818001',2,46,250,'settled',1.10,'2026-08-18 20:45:00','2026-08-18 20:48:00','2026-08-18 21:30:18',2538,46.62,51.28,'PAY20260818001','2026-08-18 20:45:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260809003',2,46,251,'pending_payment',1.10,'2026-08-09 06:00:00','2026-08-09 06:17:00','2026-08-09 07:51:25',5665,41.43,45.57,NULL,'2026-08-09 06:00:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260603006',2,46,252,'settled',1.10,'2026-06-03 10:25:00','2026-06-03 10:53:00','2026-06-03 12:25:24',5544,68.05,74.86,'PAY20260603003','2026-06-03 10:25:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260906003',3,46,253,'reserved',1.10,'2026-09-06 18:36:00',NULL,NULL,0,0.00,0.00,NULL,'2026-09-06 18:36:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260807006',3,47,257,'cancelled',1.20,'2026-08-07 15:05:00',NULL,NULL,0,0.00,0.00,NULL,'2026-08-07 15:05:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260829003',1,47,258,'cancelled',1.20,'2026-08-29 01:43:00',NULL,NULL,0,0.00,0.00,NULL,'2026-08-29 01:43:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260601003',2,47,259,'cancelled',1.20,'2026-06-01 01:56:00',NULL,NULL,0,0.00,0.00,NULL,'2026-06-01 01:56:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260823004',2,47,260,'settled',1.20,'2026-08-23 09:41:00','2026-08-23 10:07:00','2026-08-23 10:51:58',2698,53.76,64.51,'PAY20260823004','2026-08-23 09:41:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260712001',3,47,261,'cancelled',1.20,'2026-07-12 17:44:00',NULL,NULL,0,0.00,0.00,NULL,'2026-07-12 17:44:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260828005',1,48,264,'settled',1.30,'2026-08-28 17:11:00','2026-08-28 17:13:00','2026-08-28 17:57:01',2641,41.32,53.72,'PAY20260828002','2026-08-28 17:11:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260712002',2,48,265,'settled',1.30,'2026-07-12 13:38:00','2026-07-12 13:45:00','2026-07-12 15:26:17',6077,55.25,71.83,'PAY20260712001','2026-07-12 13:38:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260703002',1,48,266,'pending_payment',1.30,'2026-07-03 10:28:00','2026-07-03 10:52:00','2026-07-03 11:52:09',3609,64.02,83.23,NULL,'2026-07-03 10:28:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260902001',2,48,267,'pending_payment',1.30,'2026-09-02 10:23:00','2026-09-02 10:47:00','2026-09-02 13:13:44',8804,59.94,77.92,NULL,'2026-09-02 10:23:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260625005',2,49,269,'charging',1.40,'2026-06-25 01:48:00','2026-06-25 01:50:00',NULL,638,20.10,28.14,NULL,'2026-06-25 01:48:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260619003',2,49,270,'settled',1.40,'2026-06-19 06:59:00','2026-06-19 07:05:00','2026-06-19 09:24:13',8353,30.34,42.48,'PAY20260619002','2026-06-19 06:59:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260614003',1,49,271,'pending_payment',1.40,'2026-06-14 20:12:00','2026-06-14 20:27:00','2026-06-14 21:20:40',3220,48.34,67.68,NULL,'2026-06-14 20:12:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260813004',2,49,272,'reserved',1.40,'2026-08-13 10:15:00',NULL,NULL,0,0.00,0.00,NULL,'2026-08-13 10:15:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260622003',3,50,274,'reserved',1.50,'2026-06-22 22:10:00',NULL,NULL,0,0.00,0.00,NULL,'2026-06-22 22:10:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260907002',2,50,275,'settled',1.50,'2026-09-07 11:00:00','2026-09-07 11:26:00','2026-09-07 12:51:11',5111,30.41,45.62,'PAY20260907002','2026-09-07 11:00:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260710005',2,50,276,'settled',1.50,'2026-07-10 07:34:00','2026-07-10 07:56:00','2026-07-10 09:46:24',6624,32.14,48.21,'PAY20260710002','2026-07-10 07:34:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260613002',1,50,277,'reserved',1.50,'2026-06-13 08:35:00',NULL,NULL,0,0.00,0.00,NULL,'2026-06-13 08:35:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260821004',1,51,280,'charging',1.10,'2026-08-21 15:02:00','2026-08-21 15:12:00',NULL,2197,20.69,22.76,NULL,'2026-08-21 15:02:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260902002',2,51,281,'reserved',1.10,'2026-09-02 22:47:00',NULL,NULL,0,0.00,0.00,NULL,'2026-09-02 22:47:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260602002',3,51,282,'settled',1.10,'2026-06-02 23:38:00','2026-06-02 23:50:00','2026-06-03 01:43:42',6822,20.27,22.30,'PAY20260602002','2026-06-02 23:38:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260626006',3,51,279,'pending_payment',1.10,'2026-06-26 20:47:00','2026-06-26 21:15:00','2026-06-26 22:09:15',3255,58.20,64.02,NULL,'2026-06-26 20:47:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260609004',3,52,284,'reserved',1.20,'2026-06-09 05:16:00',NULL,NULL,0,0.00,0.00,NULL,'2026-06-09 05:16:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260819002',2,52,285,'cancelled',1.20,'2026-08-19 18:52:00',NULL,NULL,0,0.00,0.00,NULL,'2026-08-19 18:52:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260823005',1,52,286,'settled',1.20,'2026-08-23 23:01:00','2026-08-23 23:19:00','2026-08-24 01:36:03',8223,21.77,26.12,'PAY20260823005','2026-08-23 23:01:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260815002',2,52,287,'settled',1.20,'2026-08-15 11:34:00','2026-08-15 11:55:00','2026-08-15 14:21:35',8795,46.04,55.25,'PAY20260815001','2026-08-15 11:34:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260807007',1,53,291,'settled',1.30,'2026-08-07 09:57:00','2026-08-07 10:11:00','2026-08-07 12:24:50',8030,50.77,66.00,'PAY20260807003','2026-08-07 09:57:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260704005',1,53,292,'cancelled',1.30,'2026-07-04 00:23:00',NULL,NULL,0,0.00,0.00,NULL,'2026-07-04 00:23:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260623002',1,53,293,'settled',1.30,'2026-06-23 19:43:00','2026-06-23 20:06:00','2026-06-23 21:05:44',3584,21.51,27.96,'PAY20260623002','2026-06-23 19:43:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260724001',2,53,294,'settled',1.30,'2026-07-24 10:43:00','2026-07-24 11:07:00','2026-07-24 13:25:57',8337,39.08,50.80,'PAY20260724001','2026-07-24 10:43:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260811004',2,54,296,'charging',1.40,'2026-08-11 11:09:00','2026-08-11 11:11:00',NULL,1324,11.22,15.71,NULL,'2026-08-11 11:09:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260820002',2,54,297,'settled',1.40,'2026-08-20 22:12:00','2026-08-20 22:36:00','2026-08-20 23:59:38',5018,65.73,92.02,'PAY20260820002','2026-08-20 22:12:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260723002',3,54,298,'settled',1.40,'2026-07-23 19:58:00','2026-07-23 20:15:00','2026-07-23 20:47:09',1929,36.23,50.72,'PAY20260723001','2026-07-23 19:58:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260830004',3,54,299,'settled',1.40,'2026-08-30 15:33:00','2026-08-30 15:47:00','2026-08-30 17:03:04',4564,24.72,34.61,'PAY20260830004','2026-08-30 15:33:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260630004',2,55,302,'cancelled',1.50,'2026-06-30 04:05:00',NULL,NULL,0,0.00,0.00,NULL,'2026-06-30 04:05:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260627002',1,55,303,'pending_payment',1.50,'2026-06-27 17:44:00','2026-06-27 17:55:00','2026-06-27 19:29:23',5663,30.75,46.12,NULL,'2026-06-27 17:44:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260625006',1,55,304,'settled',1.50,'2026-06-25 05:30:00','2026-06-25 05:37:00','2026-06-25 06:55:22',4702,35.28,52.92,'PAY20260625003','2026-06-25 05:30:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260730001',3,55,305,'cancelled',1.50,'2026-07-30 22:36:00',NULL,NULL,0,0.00,0.00,NULL,'2026-07-30 22:36:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260819003',1,56,308,'settled',1.10,'2026-08-19 21:08:00','2026-08-19 21:24:00','2026-08-19 22:46:21',4941,42.63,46.89,'PAY20260819001','2026-08-19 21:08:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260707002',2,56,309,'settled',1.10,'2026-07-07 02:33:00','2026-07-07 02:55:00','2026-07-07 03:27:20',1940,43.61,47.97,'PAY20260707002','2026-07-07 02:33:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260617004',3,56,310,'settled',1.10,'2026-06-17 09:38:00','2026-06-17 10:06:00','2026-06-17 11:57:55',6715,52.61,57.87,'PAY20260617003','2026-06-17 09:38:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260602003',1,56,311,'charging',1.10,'2026-06-02 22:56:00','2026-06-02 22:58:00',NULL,2355,23.61,25.97,NULL,'2026-06-02 22:56:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260714002',1,57,313,'settled',1.20,'2026-07-14 10:49:00','2026-07-14 11:10:00','2026-07-14 12:42:43',5563,63.63,76.36,'PAY20260714002','2026-07-14 10:49:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260701005',1,57,314,'settled',1.20,'2026-07-01 13:11:00','2026-07-01 13:19:00','2026-07-01 14:24:56',3956,38.33,46.00,'PAY20260701003','2026-07-01 13:11:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260820003',1,57,315,'pending_payment',1.20,'2026-08-20 20:11:00','2026-08-20 20:23:00','2026-08-20 21:52:31',5371,58.49,70.19,NULL,'2026-08-20 20:11:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260617005',1,57,316,'cancelled',1.20,'2026-06-17 21:14:00',NULL,NULL,0,0.00,0.00,NULL,'2026-06-17 21:14:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260624004',1,57,312,'pending_payment',1.20,'2026-06-24 15:43:00','2026-06-24 15:52:00','2026-06-24 16:55:56',3836,50.80,60.96,NULL,'2026-06-24 15:43:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260804004',1,58,318,'settled',1.30,'2026-08-04 20:46:00','2026-08-04 21:08:00','2026-08-04 21:51:23',2603,21.30,27.69,'PAY20260804003','2026-08-04 20:46:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260701006',3,58,319,'pending_payment',1.30,'2026-07-01 15:20:00','2026-07-01 15:45:00','2026-07-01 18:12:23',8843,39.64,51.53,NULL,'2026-07-01 15:20:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260711004',1,58,320,'settled',1.30,'2026-07-11 23:40:00','2026-07-11 23:47:00','2026-07-12 01:33:56',6416,34.82,45.27,'PAY20260711003','2026-07-11 23:40:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260908001',1,58,321,'cancelled',1.30,'2026-09-08 12:18:00',NULL,NULL,0,0.00,0.00,NULL,'2026-09-08 12:18:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260711005',1,59,323,'settled',1.40,'2026-07-11 12:53:00','2026-07-11 13:08:00','2026-07-11 14:26:14',4694,60.26,84.36,'PAY20260711004','2026-07-11 12:53:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260723003',2,59,324,'settled',1.40,'2026-07-23 10:38:00','2026-07-23 10:43:00','2026-07-23 12:04:03',4863,33.42,46.79,'PAY20260723002','2026-07-23 10:38:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260817004',2,59,325,'settled',1.40,'2026-08-17 17:02:00','2026-08-17 17:21:00','2026-08-17 19:22:22',7282,62.48,87.47,'PAY20260817003','2026-08-17 17:02:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260709001',2,59,326,'settled',1.40,'2026-07-09 17:02:00','2026-07-09 17:13:00','2026-07-09 17:52:09',2349,34.35,48.09,'PAY20260709001','2026-07-09 17:02:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260605002',3,60,328,'settled',1.50,'2026-06-05 04:26:00','2026-06-05 04:31:00','2026-06-05 05:46:17',4517,22.51,33.77,'PAY20260605001','2026-06-05 04:26:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260805005',2,60,329,'charging',1.50,'2026-08-05 18:11:00','2026-08-05 18:27:00',NULL,787,16.42,24.63,NULL,'2026-08-05 18:11:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260819004',3,60,330,'pending_payment',1.50,'2026-08-19 05:09:00','2026-08-19 05:34:00','2026-08-19 07:34:27',7227,37.85,56.78,NULL,'2026-08-19 05:09:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260629002',2,60,331,'charging',1.50,'2026-06-29 10:54:00','2026-06-29 11:11:00',NULL,623,26.87,40.30,NULL,'2026-06-29 10:54:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260622004',3,60,332,'settled',1.50,'2026-06-22 01:49:00','2026-06-22 01:53:00','2026-06-22 03:17:34',5074,55.41,83.11,'PAY20260622002','2026-06-22 01:49:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260609005',2,61,334,'settled',1.10,'2026-06-09 06:05:00','2026-06-09 06:17:00','2026-06-09 07:05:00',2880,33.40,36.74,'PAY20260609002','2026-06-09 06:05:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260706002',2,61,335,'reserved',1.10,'2026-07-06 10:08:00',NULL,NULL,0,0.00,0.00,NULL,'2026-07-06 10:08:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260906004',2,61,336,'settled',1.10,'2026-09-06 02:02:00','2026-09-06 02:19:00','2026-09-06 04:42:43',8623,57.63,63.39,'PAY20260906002','2026-09-06 02:02:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260817005',3,61,337,'settled',1.10,'2026-08-17 00:59:00','2026-08-17 01:22:00','2026-08-17 03:17:12',6912,29.64,32.60,'PAY20260817004','2026-08-17 00:59:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260906005',1,61,338,'settled',1.10,'2026-09-06 06:52:00','2026-09-06 06:54:00','2026-09-06 08:09:37',4537,56.50,62.15,'PAY20260906003','2026-09-06 06:52:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260907003',3,62,340,'settled',1.20,'2026-09-07 11:04:00','2026-09-07 11:18:00','2026-09-07 12:02:53',2693,52.54,63.05,'PAY20260907003','2026-09-07 11:04:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260901005',2,62,341,'settled',1.20,'2026-09-01 13:11:00','2026-09-01 13:32:00','2026-09-01 14:09:47',2267,57.91,69.49,'PAY20260901004','2026-09-01 13:11:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260726001',2,62,342,'settled',1.20,'2026-07-26 02:13:00','2026-07-26 02:38:00','2026-07-26 03:52:59',4499,36.91,44.29,'PAY20260726001','2026-07-26 02:13:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260829004',3,62,343,'settled',1.20,'2026-08-29 07:11:00','2026-08-29 07:18:00','2026-08-29 08:43:20',5120,51.93,62.32,'PAY20260829002','2026-08-29 07:11:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260621001',3,63,345,'cancelled',1.30,'2026-06-21 22:01:00',NULL,NULL,0,0.00,0.00,NULL,'2026-06-21 22:01:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260802002',2,63,346,'reserved',1.30,'2026-08-02 20:20:00',NULL,NULL,0,0.00,0.00,NULL,'2026-08-02 20:20:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260712003',1,63,347,'settled',1.30,'2026-07-12 12:39:00','2026-07-12 13:01:00','2026-07-12 13:49:18',2898,57.90,75.27,'PAY20260712002','2026-07-12 12:39:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260908002',1,63,344,'settled',1.30,'2026-09-08 21:16:00','2026-09-08 21:35:00','2026-09-08 23:16:56',6116,47.26,61.44,'PAY20260908001','2026-09-08 21:16:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260630005',3,63,345,'charging',1.30,'2026-06-30 06:41:00','2026-06-30 06:58:00',NULL,1189,20.40,26.52,NULL,'2026-06-30 06:41:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260722007',3,64,349,'charging',1.40,'2026-07-22 07:05:00','2026-07-22 07:19:00',NULL,2259,23.17,32.44,NULL,'2026-07-22 07:05:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260831004',1,64,350,'settled',1.40,'2026-08-31 00:46:00','2026-08-31 00:48:00','2026-08-31 02:16:57',5337,51.33,71.86,'PAY20260831004','2026-08-31 00:46:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260828006',1,64,351,'settled',1.40,'2026-08-28 03:32:00','2026-08-28 03:51:00','2026-08-28 05:53:42',7362,47.08,65.91,'PAY20260828003','2026-08-28 03:32:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260629003',1,64,352,'settled',1.40,'2026-06-29 09:44:00','2026-06-29 10:00:00','2026-06-29 10:34:30',2070,46.23,64.72,'PAY20260629001','2026-06-29 09:44:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260803002',1,64,353,'pending_payment',1.40,'2026-08-03 15:55:00','2026-08-03 16:17:00','2026-08-03 18:41:03',8643,26.20,36.68,NULL,'2026-08-03 15:55:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260826001',1,65,355,'settled',1.50,'2026-08-26 03:24:00','2026-08-26 03:46:00','2026-08-26 05:49:06',7386,34.34,51.51,'PAY20260826001','2026-08-26 03:24:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260702004',2,65,356,'charging',1.50,'2026-07-02 13:32:00','2026-07-02 13:35:00',NULL,1939,13.23,19.84,NULL,'2026-07-02 13:32:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260601004',3,65,357,'settled',1.50,'2026-06-01 23:46:00','2026-06-02 00:16:00','2026-06-02 01:52:37',5797,59.94,89.91,'PAY20260601001','2026-06-01 23:46:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260706003',3,65,358,'settled',1.50,'2026-07-06 14:59:00','2026-07-06 15:02:00','2026-07-06 17:21:06',8346,44.07,66.11,'PAY20260706002','2026-07-06 14:59:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260824001',3,66,360,'reserved',1.10,'2026-08-24 04:16:00',NULL,NULL,0,0.00,0.00,NULL,'2026-08-24 04:16:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260724002',1,66,361,'pending_payment',1.10,'2026-07-24 17:32:00','2026-07-24 17:39:00','2026-07-24 18:33:06',3246,68.18,75.00,NULL,'2026-07-24 17:32:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260905005',3,66,362,'settled',1.10,'2026-09-05 05:30:00','2026-09-05 05:42:00','2026-09-05 06:45:36',3816,35.78,39.36,'PAY20260905003','2026-09-05 05:30:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260625007',3,66,363,'settled',1.10,'2026-06-25 23:41:00','2026-06-25 23:57:00','2026-06-26 01:49:35',6755,41.83,46.01,'PAY20260625004','2026-06-25 23:41:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260626007',2,66,359,'charging',1.10,'2026-06-26 10:48:00','2026-06-26 10:58:00',NULL,1280,9.13,10.04,NULL,'2026-06-26 10:48:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260725003',3,67,365,'charging',1.20,'2026-07-25 15:09:00','2026-07-25 15:21:00',NULL,633,25.19,30.23,NULL,'2026-07-25 15:09:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260626008',1,67,366,'settled',1.20,'2026-06-26 06:00:00','2026-06-26 06:10:00','2026-06-26 08:33:46',8626,41.58,49.90,'PAY20260626003','2026-06-26 06:00:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260618003',2,67,367,'settled',1.20,'2026-06-18 12:18:00','2026-06-18 12:31:00','2026-06-18 13:17:16',2776,27.60,33.12,'PAY20260618003','2026-06-18 12:18:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260723004',1,67,368,'pending_payment',1.20,'2026-07-23 13:46:00','2026-07-23 14:00:00','2026-07-23 16:01:37',7297,64.59,77.51,NULL,'2026-07-23 13:46:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260723005',2,67,369,'settled',1.20,'2026-07-23 14:05:00','2026-07-23 14:07:00','2026-07-23 15:19:52',4372,40.36,48.43,'PAY20260723003','2026-07-23 14:05:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260606002',3,68,371,'settled',1.30,'2026-06-06 04:13:00','2026-06-06 04:42:00','2026-06-06 05:22:29',2429,49.91,64.88,'PAY20260606001','2026-06-06 04:13:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260902003',2,68,372,'charging',1.30,'2026-09-02 01:02:00','2026-09-02 01:18:00',NULL,2186,17.34,22.54,NULL,'2026-09-02 01:02:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260818002',1,68,373,'settled',1.30,'2026-08-18 01:46:00','2026-08-18 01:57:00','2026-08-18 02:44:36',2856,31.84,41.39,'PAY20260818002','2026-08-18 01:46:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260629004',1,68,370,'pending_payment',1.30,'2026-06-29 01:53:00','2026-06-29 02:02:00','2026-06-29 04:05:48',7428,58.23,75.70,NULL,'2026-06-29 01:53:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260826002',3,69,375,'pending_payment',1.40,'2026-08-26 23:28:00','2026-08-26 23:36:00','2026-08-27 00:17:33',2493,58.68,82.15,NULL,'2026-08-26 23:28:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260618004',1,69,376,'pending_payment',1.40,'2026-06-18 23:26:00','2026-06-18 23:44:00','2026-06-19 02:01:13',8233,43.52,60.93,NULL,'2026-06-18 23:26:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260811005',2,69,377,'reserved',1.40,'2026-08-11 22:09:00',NULL,NULL,0,0.00,0.00,NULL,'2026-08-11 22:09:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260822001',3,69,378,'settled',1.40,'2026-08-22 13:10:00','2026-08-22 13:37:00','2026-08-22 15:27:24',6624,22.39,31.35,'PAY20260822001','2026-08-22 13:10:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260801001',3,70,382,'settled',1.50,'2026-08-01 21:53:00','2026-08-01 22:14:00','2026-08-02 00:10:52',7012,56.24,84.36,'PAY20260801001','2026-08-01 21:53:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260729002',3,70,383,'settled',1.50,'2026-07-29 06:47:00','2026-07-29 07:04:00','2026-07-29 08:07:37',3817,52.22,78.33,'PAY20260729001','2026-07-29 06:47:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260707003',1,70,384,'pending_payment',1.50,'2026-07-07 05:54:00','2026-07-07 06:18:00','2026-07-07 08:25:56',7676,31.82,47.73,NULL,'2026-07-07 05:54:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260908003',3,70,385,'settled',1.50,'2026-09-08 10:45:00','2026-09-08 11:13:00','2026-09-08 12:48:21',5721,44.42,66.63,'PAY20260908002','2026-09-08 10:45:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260731003',1,70,386,'settled',1.50,'2026-07-31 02:19:00','2026-07-31 02:47:00','2026-07-31 03:18:24',1884,61.15,91.72,'PAY20260731003','2026-07-31 02:19:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260615004',2,71,389,'settled',1.10,'2026-06-15 07:25:00','2026-06-15 07:45:00','2026-06-15 08:28:25',2605,40.93,45.02,'PAY20260615001','2026-06-15 07:25:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260815003',1,71,390,'settled',1.10,'2026-08-15 14:48:00','2026-08-15 15:07:00','2026-08-15 16:25:23',4703,37.57,41.33,'PAY20260815002','2026-08-15 14:48:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260604001',2,71,391,'cancelled',1.10,'2026-06-04 22:03:00',NULL,NULL,0,0.00,0.00,NULL,'2026-06-04 22:03:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260827004',1,71,392,'settled',1.10,'2026-08-27 04:03:00','2026-08-27 04:23:00','2026-08-27 05:57:59',5699,52.16,57.38,'PAY20260827003','2026-08-27 04:03:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260703003',2,72,395,'charging',1.20,'2026-07-03 04:33:00','2026-07-03 04:35:00',NULL,1488,19.34,23.21,NULL,'2026-07-03 04:33:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260703004',1,72,396,'charging',1.20,'2026-07-03 19:25:00','2026-07-03 19:38:00',NULL,1793,14.00,16.80,NULL,'2026-07-03 19:25:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260618005',3,72,397,'settled',1.20,'2026-06-18 18:44:00','2026-06-18 19:00:00','2026-06-18 20:44:26',6266,61.03,73.24,'PAY20260618004','2026-06-18 18:44:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260615005',3,72,398,'settled',1.20,'2026-06-15 13:25:00','2026-06-15 13:32:00','2026-06-15 14:09:50',2270,52.03,62.44,'PAY20260615002','2026-06-15 13:25:00');
INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price, reserve_time, start_time, end_time, duration_seconds, kwh, amount, pay_request_id, created_at) VALUES ('CD20260810003',2,72,399,'settled',1.20,'2026-08-10 13:40:00','2026-08-10 13:58:00','2026-08-10 14:35:39',2259,44.58,53.50,'PAY20260810003','2026-08-10 13:40:00');

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
