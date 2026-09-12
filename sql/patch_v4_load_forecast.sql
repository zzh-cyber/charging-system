-- 新增负荷预测结果表 load_forecast（结构版本 4）
-- 对现有库做增量 CREATE，不要重跑会 DROP 表的 schema.sql。
--
-- 用法：
--   mysql -u charging_user -p123456 charging_system < sql/patch_v4_load_forecast.sql
--
-- 服务端启动时 ensureSchema() 也会执行同等升级；本脚本可单独跑一遍。
-- 与 v3 不同，本脚本是幂等的（CREATE TABLE IF NOT EXISTS + INSERT IGNORE），
-- 重复执行不会报错，也不会重复写 schema_version。
--
-- 表归属：第二阶段「充电负荷智能预测」。由 Spark MLlib 训练脚本写入，
-- 服务端 station_list（给 Qt 用户端）与 Flask 大屏只读。
--
-- 列含义：
--   station_id    电站，须是演示库真实站（有外键约束）
--   generated_at  本批训练时间，整批必须是同一个值
--   horizon_hours 预测步长，只允许 1 / 6 / 24
--   pred_kwh      预测电量（度）
--   pred_idle     预测空闲桩数
--   pred_util     预测占用率 %
--   is_peak       pred_util >= 80 为 1
--   congestion    拥堵档：low / mid / high
--
-- generated_at 刻意不给 DEFAULT CURRENT_TIMESTAMP：
-- 读取方（Qt station_list / Flask）按 MAX(generated_at) 取最新一批。
-- 若给默认值，逐行 INSERT 时每行会拿到各自的 NOW()，同一批 generated_at 不一致，
-- MAX() 只会捞回最后一行，表现为「预测数据几乎为空」。漏填直接报错反而是好的失败方式。
-- 写数据时务必给整批传同一个 generated_at。

USE charging_system;

CREATE TABLE IF NOT EXISTS load_forecast (
    id            BIGINT        NOT NULL AUTO_INCREMENT,
    station_id    BIGINT        NOT NULL,
    generated_at  DATETIME      NOT NULL,
    horizon_hours INT           NOT NULL,
    pred_kwh      DECIMAL(10,2) NOT NULL DEFAULT 0.00,
    pred_idle     INT           NOT NULL DEFAULT 0,
    pred_util     DECIMAL(5,2)  NOT NULL DEFAULT 0.00,
    is_peak       TINYINT(1)    NOT NULL DEFAULT 0,
    congestion    ENUM('low','mid','high') NOT NULL DEFAULT 'mid',
    PRIMARY KEY (id),
    UNIQUE KEY uk_forecast_batch (station_id, generated_at, horizon_hours),
    KEY idx_forecast_latest (generated_at),
    KEY idx_forecast_station (station_id, horizon_hours),
    CONSTRAINT fk_forecast_station FOREIGN KEY (station_id) REFERENCES station (id),
    CONSTRAINT chk_forecast_horizon CHECK (horizon_hours IN (1, 6, 24)),
    CONSTRAINT chk_forecast_util    CHECK (pred_util BETWEEN 0 AND 100),
    CONSTRAINT chk_forecast_idle    CHECK (pred_idle >= 0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT IGNORE INTO schema_version (version, description) VALUES
(4, '新增 load_forecast：负荷预测结果（1/6/24h），供 station_list 与大屏读取');
