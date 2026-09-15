-- 用户群体画像：user 表扩展属性 + user_group_profile / user_group_insight 两表（结构版本 5）
-- 对现有库做增量 ALTER / CREATE，不要重跑会 DROP 表的 schema.sql。
--
-- 用法：
--   mysql -u charging_user -p123456 charging_system < sql/patch_v5_user_groups.sql
--
-- 服务端启动时 ensureSchema() 也会执行同等升级；本脚本可单独跑一遍。
--
-- 幂等（照 v4 的约定，与 v3 不同）：重复执行不报错、不重复写 schema_version。
-- ⚠️ MySQL 8 没有 `ADD COLUMN IF NOT EXISTS`，v4 靠 `CREATE TABLE IF NOT EXISTS`
--    拿到幂等，这里没有等价语法可用，所以六个 ADD COLUMN 各自用 information_schema
--    判存在 + 预处理语句包一层。
--    刻意【不】写成一条 ALTER 加六列：那样只要有一列已存在，整条就失败，
--    做不到「已存在字段时不报错」，而这正是本文件的要求。
--
-- 两处 ALTER 的 DDL 文本与 server/database.cpp 的 upgradeSchema() 逐字一致
-- （那边用 columnExists() 做幂等守卫，这里用预处理语句，DDL 字符串本身相同）；
-- 两张 CREATE TABLE 的文本与 sql/schema.sql、server/database.cpp 三处逐字一致。
-- 与 v3/v4 一样，字段与表只增不删，绝不 DROP。
--
-- 列含义（docs/数据.md 一.1）：
--   gender              性别分布
--   age_group           年龄段分布
--   city                城市用户分布
--   vehicle_type        车型分布
--   registration_source 注册来源分布（android/ios/web/qt/offline）
--   last_active_at      活跃、沉默用户分析
--
-- 两张新表（docs/数据.md 一.2 / 一.3）：
--   user_group_profile  每个用户一行的画像快照，主键就是 user_id
--   user_group_insight  群体运营建议，规则型生成，不调外部 AI
-- 统计口径见 docs/数据.md 三：画像/行为/趋势/价值一律只取
-- charge_order.status = 'settled'，这里是纯结构，不含口径逻辑。

USE charging_system;

-- gender
SET @ddl = (SELECT IF(COUNT(*) = 0,
    'ALTER TABLE `user` ADD COLUMN gender ENUM(''male'',''female'',''unknown'') NOT NULL DEFAULT ''unknown'' AFTER avatar',
    'DO 0')
  FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'user' AND COLUMN_NAME = 'gender');
PREPARE s FROM @ddl; EXECUTE s; DEALLOCATE PREPARE s;

-- age_group
SET @ddl = (SELECT IF(COUNT(*) = 0,
    'ALTER TABLE `user` ADD COLUMN age_group ENUM(''under_25'',''25_34'',''35_44'',''45_54'',''55_plus'',''unknown'') NOT NULL DEFAULT ''unknown'' AFTER gender',
    'DO 0')
  FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'user' AND COLUMN_NAME = 'age_group');
PREPARE s FROM @ddl; EXECUTE s; DEALLOCATE PREPARE s;

-- city
SET @ddl = (SELECT IF(COUNT(*) = 0,
    'ALTER TABLE `user` ADD COLUMN city VARCHAR(64) NOT NULL DEFAULT '''' AFTER age_group',
    'DO 0')
  FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'user' AND COLUMN_NAME = 'city');
PREPARE s FROM @ddl; EXECUTE s; DEALLOCATE PREPARE s;

-- vehicle_type
SET @ddl = (SELECT IF(COUNT(*) = 0,
    'ALTER TABLE `user` ADD COLUMN vehicle_type ENUM(''sedan'',''suv'',''mpv'',''commercial'',''other'',''unknown'') NOT NULL DEFAULT ''unknown'' AFTER city',
    'DO 0')
  FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'user' AND COLUMN_NAME = 'vehicle_type');
PREPARE s FROM @ddl; EXECUTE s; DEALLOCATE PREPARE s;

-- registration_source
SET @ddl = (SELECT IF(COUNT(*) = 0,
    'ALTER TABLE `user` ADD COLUMN registration_source ENUM(''android'',''ios'',''web'',''qt'',''offline'',''unknown'') NOT NULL DEFAULT ''unknown'' AFTER vehicle_type',
    'DO 0')
  FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'user' AND COLUMN_NAME = 'registration_source');
PREPARE s FROM @ddl; EXECUTE s; DEALLOCATE PREPARE s;

-- last_active_at
SET @ddl = (SELECT IF(COUNT(*) = 0,
    'ALTER TABLE `user` ADD COLUMN last_active_at DATETIME NULL AFTER last_login_at',
    'DO 0')
  FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'user' AND COLUMN_NAME = 'last_active_at');
PREPARE s FROM @ddl; EXECUTE s; DEALLOCATE PREPARE s;

CREATE TABLE IF NOT EXISTS user_group_profile (
    user_id             BIGINT       NOT NULL,
    total_orders        INT          NOT NULL DEFAULT 0,
    total_kwh           DECIMAL(12,2) NOT NULL DEFAULT 0.00,
    total_amount        DECIMAL(12,2) NOT NULL DEFAULT 0.00,
    avg_duration_seconds INT         NOT NULL DEFAULT 0,
    orders_30d          INT          NOT NULL DEFAULT 0,
    kwh_30d             DECIMAL(12,2) NOT NULL DEFAULT 0.00,
    amount_30d          DECIMAL(12,2) NOT NULL DEFAULT 0.00,
    active_days_30d     INT          NOT NULL DEFAULT 0,
    frequency_level     ENUM('high','medium','low','inactive') NOT NULL DEFAULT 'inactive',
    value_level         ENUM('high','medium','low') NOT NULL DEFAULT 'low',
    preferred_period    VARCHAR(32)  NOT NULL DEFAULT '',
    preferred_station_id BIGINT      NULL,
    preferred_pile_type ENUM('fast','slow','mixed') NOT NULL DEFAULT 'mixed',
    tags_json           JSON         NULL,
    generated_at        DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at          DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (user_id),
    KEY idx_group_frequency (frequency_level),
    KEY idx_group_value (value_level),
    KEY idx_group_station (preferred_station_id),
    CONSTRAINT fk_group_profile_user FOREIGN KEY (user_id) REFERENCES `user` (id),
    CONSTRAINT fk_group_profile_station FOREIGN KEY (preferred_station_id) REFERENCES station (id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS user_group_insight (
    id            BIGINT       NOT NULL AUTO_INCREMENT,
    insight_type  VARCHAR(32)  NOT NULL,
    target_group  VARCHAR(32)  NOT NULL DEFAULT '',
    title         VARCHAR(128) NOT NULL,
    content       VARCHAR(512) NOT NULL,
    reason        VARCHAR(512) NOT NULL DEFAULT '',
    priority      INT          NOT NULL DEFAULT 0,
    generated_at  DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    KEY idx_insight_time (generated_at),
    KEY idx_insight_priority (priority)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT IGNORE INTO schema_version (version, description) VALUES
(5, 'user 表加用户画像属性 + user_group_profile/user_group_insight 两表');
