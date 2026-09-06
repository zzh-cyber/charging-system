-- charge_order 增加模拟车辆 SOC 字段（结构版本 3）
-- 对现有库做增量 ALTER，不要重跑会 DROP 表的 schema.sql。
--
-- 用法：
--   mysql -u charging_user -p123456 charging_system < sql/patch_v3_order_soc.sql
--
-- 服务端启动时 ensureSchema() 也会执行同等升级；本脚本可单独跑一遍。

USE charging_system;

ALTER TABLE charge_order
    ADD COLUMN start_soc DECIMAL(5,2) NULL
        COMMENT '开始充电时模拟初始电量%，仅展示不参与结算' AFTER unit_price,
    ADD COLUMN battery_capacity_kwh DECIMAL(6,2) NULL
        COMMENT '模拟电池容量 kWh，仅展示' AFTER start_soc,
    ADD COLUMN target_soc DECIMAL(5,2) NULL
        COMMENT '模拟目标电量%，仅展示' AFTER battery_capacity_kwh;

INSERT INTO schema_version (version, description) VALUES
(3, 'charge_order 增加模拟 SOC：start_soc / battery_capacity_kwh / target_soc');
