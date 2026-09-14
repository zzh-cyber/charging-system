-- 清除「用户群体管理大屏」的演示数据（docs/数据.md 二）
-- 配套 sql/seed_user_groups_demo.sql，供数据老化后「刷新」用：
--   mysql -u charging_user -p123456 charging_system < sql/reset_user_groups_demo.sql
--   mysql -u charging_user -p123456 charging_system < sql/seed_user_groups_demo.sql
-- （文档 九.10 要的「一键初始化/刷新命令」就是这两行）
--
-- 只删本批：手机号段 1370000% 的 80 个用户，以及单号前缀 UG 的订单。
-- 存量 40 个用户一行都不删 —— 其中 31 人身上挂着本批的补单，那些单按单号前缀删掉，
-- 人留下（六列画像属性也留着，重跑 seed 会再写一遍）。
--
-- ⚠️ 两个坑：
--   1. `LIKE 'UG%'` 在 utf8mb4_unicode_ci 下**大小写不敏感**，`'ug...' LIKE 'UG%'`
--      实测为 1。圈范围必须用 `LIKE BINARY`，否则将来某个单号前缀只是大小写不同
--      也会被一起删掉。（手机号是纯数字，没有这个问题。）
--   2. 按 user.id 删之前要先把指向它的外键行清掉，分别是
--      charge_order / pile.current_user_id / user_group_profile / wallet_transactions。
--      实测 pile.current_user_id 全表为 NULL（存量根本不维护它），但画像任务跑过之后
--      user_group_profile 会有行，不清就删不掉用户。
--
-- 事务包住全部删除：要么全删掉，要么一行不动。

USE charging_system;
SET @anchor = NOW();

SELECT '清除前' AS stage,
       (SELECT COUNT(*) FROM `user` WHERE phone LIKE '1370000%')            AS 本批用户,
       (SELECT COUNT(*) FROM charge_order WHERE order_no LIKE BINARY 'UG%') AS 本批订单,
       (SELECT COUNT(*) FROM `user`)                                        AS 全库用户,
       (SELECT COUNT(*) FROM charge_order)                                  AS 全库订单;

START TRANSACTION;

-- 本批订单（含挂在存量 31 个补单用户身上的那些）
DELETE FROM charge_order WHERE order_no LIKE BINARY 'UG%';

-- 本批用户身上其余来源的行，按外键依赖从叶到根清
DELETE p FROM user_group_profile p
  JOIN `user` u ON u.id = p.user_id WHERE u.phone LIKE '1370000%';
DELETE w FROM wallet_transactions w
  JOIN `user` u ON u.id = w.user_id WHERE u.phone LIKE '1370000%';
DELETE o FROM charge_order o
  JOIN `user` u ON u.id = o.user_id WHERE u.phone LIKE '1370000%';
UPDATE pile SET current_user_id = NULL
  WHERE current_user_id IN (SELECT id FROM `user` WHERE phone LIKE '1370000%');

DELETE FROM `user` WHERE phone LIKE '1370000%';

COMMIT;

SELECT '清除后' AS stage,
       (SELECT COUNT(*) FROM `user` WHERE phone LIKE '1370000%')            AS 本批用户,
       (SELECT COUNT(*) FROM charge_order WHERE order_no LIKE BINARY 'UG%') AS 本批订单,
       (SELECT COUNT(*) FROM `user`)                                        AS 全库用户,
       (SELECT COUNT(*) FROM charge_order)                                  AS 全库订单;
-- 期望本批用户 0、本批订单 0，全库回到 40 人 / 337 单（若中途有人跑过别的业务，
-- 后两个数会略高，那是正常的，别当成没删干净）。

-- 存量用户的六列画像属性**不还原**：导入前它们本来就是全表 'unknown'/''，
-- 没有可还原的原值；重跑 seed 会按新的随机种子重新写一遍。
SELECT '存量用户属性仍在' AS 说明,
       SUM(gender <> 'unknown')   AS 性别已填,
       SUM(city <> '')            AS 城市已填,
       SUM(last_active_at IS NOT NULL) AS 活跃时间已填
FROM `user`;
