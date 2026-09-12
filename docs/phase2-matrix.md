# 第二阶段需求矩阵（大数据可视化大屏 + 充电负荷智能预测）

> **项目名称**：新能源汽车充电管理系统（接第一阶段 Qt + MySQL）  
> **节点**：9/12 环境就绪 · **9/15 两项验收**  
> **编制日期**：2026-09-12　**修订**：2026-09-12 晚（按老师六步：模拟脏数据 → PySpark 探查/清洗 → SparkSQL 四层 → Flask+Vue+ECharts → Spark MLlib）  
> **状态图例**：○ 完成　△ 进行中　× 未着手  

第一阶段四项（用户端 / 管理端 / 服务端 / 数据库）不变。本文件覆盖老师文档 **（4）大数据可视化大屏**、**（5）充电负荷智能预测**，并按老师给出的作业步骤落地。  
故障诊断、需求调度、智能风控、实时天气 API、三节点 Hadoop、YARN、Hive：**9/15 不做**。  
**禁止**把脏数据和大量模拟订单写入演示库 `charging_system`（Qt / TCP:9000 / 演示号余额约 92.50 保持干净）。

老师六步与本项目对应：

| 老师步骤 | 本项目落地 |
|----------|------------|
| （1）按第一阶段表结构生成模拟数据，并加入场景质量问题 | 从 `station`/`pile` 维表拷结构，生成 1～3 万条模拟订单 + 5%～10% 脏行 → HDFS **ODS** |
| （2）PySpark 发现数据质量问题 | 扫 ODS：空值、范围异常、重复键、引用完整性，写出质量报告 |
| （3）PySpark 数据清洗 | ODS → **DWD**（合法充电订单明细） |
| （4）SparkSQL：ODS→DWD→DWS→ADS | DWD 明细 → **DWS** 站×小时负荷 → **ADS** KPI/预警/训练宽表 |
| （5）Flask + Vue + ECharts | Flask 读 ADS，Vue+ECharts 大屏（CDN，不必 HBuilderX） |
| （6）Spark MLlib 预测 | 读 DWS/ADS，预测 1h/6h/24h，写回 ADS + MySQL `load_forecast` |
| （附加）用户端推荐 | 现有 `station_list` 附带预测字段，首页智能推荐 |

Spark：3.4.1，`spark-submit`，`local[*]`，UI http://localhost:4040。HDFS：`hdfs://localhost:8020`。充电后台继续 **9000**。

---

## 总思路

```text
charging_system 维表（station/pile，只读、干净）
        │  gen_ods.py --seed（邓雅心）
        ▼
模拟订单 1～3 万条 + 脏数据 5%～10%     ← 老师（1）
        │  hdfs dfs -put
        ▼
HDFS ODS  /user/charging/ods/           ← 原始层
        │  PySpark 探查 → qa/report     ← 老师（2）
        │  PySpark 清洗                 ← 老师（3）
        ▼
HDFS DWD  /user/charging/dwd/           ← 清洗后订单明细
        │  SparkSQL 聚合                ← 老师（4）
        ▼
HDFS DWS  /user/charging/dws/station_hour/
HDFS ADS  /user/charging/ads/           ← KPI、预警、训练宽表
        │
        ├─► Flask /api/*  + Vue + ECharts   ← 老师（5）（4）大屏
        └─► spark-submit MLlib 1h/6h/24h    ← 老师（6）
                    │
                    ├─► ADS forecast + latest.json
                    └─► MySQL load_forecast
                              └─► station_list → 用户端推荐（附加）
```

被问「为何不集群」：课设数据量是 MB 级；`local[*]` 模拟分布式 API，数据变大只需改 `spark.master`。

---

## 数据规模与质量问题（老师（1），必须先做）

从第一阶段表结构生成，**时间 14～30 天**，带工作日早高峰（8–9）晚高峰（17–20）和周末差异。`--seed` 固定，可复现。

**脏数据场景（每类至少数十行，合计约 5%～10%）**

| 场景 | 怎么造 | PySpark 怎么发现 | 清洗规则（DWD） |
|------|--------|------------------|-----------------|
| 空值 | `start_time` 或 `kwh` 为空 | `isNull` 计数 | 丢弃或无法分摊电量的行 |
| 范围异常 | `kwh < 0`；SOC 不在 0–100 | `where` 计数 | 丢弃负电量；SOC 裁剪或置空 |
| 时间颠倒 | `end_time < start_time` | 比较两列 | 丢弃 |
| 重复单号 | 同一 `order_no` 两行 | `groupBy.order_no.count>1` | 保留 `updated_at`/`created_at` 最新一行 |
| 孤儿引用 | `station_id` / `pile_id` 在维表不存在 | left anti join 维表 | 丢弃 |
| 状态矛盾 | `settled` 但 `duration_seconds=0` 且 kwh>0 | 组合条件计数 | 丢弃或按规则重算（本阶段丢弃） |

维表 CSV（`ods_station.csv` / `ods_pile.csv`）从演示库导出，**保持干净**。

HDFS 目录约定：

```text
/user/charging/ods/charge_order/
/user/charging/ods/station/
/user/charging/ods/pile/
/user/charging/qa/report.json          # 探查结果
/user/charging/dwd/charge_order/
/user/charging/dws/station_hour/
/user/charging/ads/kpis/
/user/charging/ads/forecast/
```

不必上 Hive：Spark 读写 CSV/Parquet + `createOrReplaceTempView` + SparkSQL 即可。

---

## 数仓四层口径（老师（4））

| 层 | 内容 | 主要字段 |
|----|------|----------|
| ODS | 含脏数据的模拟订单 + 干净维表 | 与第一阶段 `charge_order`/`station`/`pile` 列对齐 |
| DWD | 清洗后的订单事实 | 合法 `order_no,station_id,pile_id,start_time,end_time,kwh,amount,status` |
| DWS | 站×小时汇总 | 同原 `station_load_hourly`：kwh、order_count、busy_piles、idle_piles、total_piles、hour_of_day、weekday、is_weekend、is_holiday |
| ADS | 应用数据 | ① 大屏 KPI/今日负荷/预警 ② MLlib 训练宽表 ③ 预测结果（1/6/24h） |

小时聚合（DWD→DWS）：对 DWD 中已开始订单按 `[start_time,end_time)` 拆整点；充电中 `end` 用生成脚本的「当前模拟时刻」。kwh 按重叠秒数分摊；busy 为同时段重叠订单数并截断为 total_piles。

---

## MySQL 只多一张预测表（给 Qt 用）

演示库 **禁止** 灌 ODS 脏数据。仅增量：

**`load_forecast`**（schema_version=4，`upgradeSchema` 增量 CREATE，禁止 DROP）

| 字段 | 含义 |
|------|------|
| station_id | 电站（须是演示库真实站） |
| generated_at | 本批训练时间 |
| horizon_hours | 1 / 6 / 24 |
| pred_kwh / pred_idle / pred_util | 预测电量、空闲桩、占用率 % |
| is_peak | util≥80 为 1 |
| congestion | idle 占比 ≥50% 为 `low`；&lt;20% 或 is_peak 为 `high`；其余 `mid` |

读最新 `MAX(generated_at)`。MLlib 预测的 `station_id` 必须能对上演示库电站，用户端才显示得出来。

Flask 大屏以 HDFS ADS / 导出 JSON 为准，不直连业务库。

### Flask 给 Vue 的接口（老师（5），13 号可先 mock）

| 方法 | 路径 | 内容 |
|------|------|------|
| GET | `/api/quality` | 探查报告（空值/异常/重复/孤儿行数） |
| GET | `/api/kpis` | 今日电量、营收、空闲/在用/故障桩、高峰小时、预警数 |
| GET | `/api/load` | `load_today`、`load_forecast_24h` |
| GET | `/api/stations` | 各站占用率与 h1/h6/h24 |
| GET | `/api/alerts` | 高峰预警列表 |

JSON 字段与下面结构对齐（便于先 mock）：

```json
{
  "generated_at": "2026-09-13T21:00:00",
  "kpis": {
    "today_kwh": 1280.5,
    "today_revenue": 1920.75,
    "idle_piles": 210,
    "busy_piles": 160,
    "fault_piles": 29,
    "peak_hour": "18:00",
    "alert_count": 3
  },
  "quality": {
    "ods_rows": 20000,
    "null_start_time": 120,
    "negative_kwh": 80,
    "dup_order_no": 40,
    "orphan_station": 30,
    "dwd_rows": 18500
  },
  "load_today": [{"hour": 0, "kwh": 12.3}],
  "load_forecast_24h": [{"offset": 1, "kwh": 15.0}],
  "stations": [{
    "station_id": 1,
    "name": "市民中心充电站",
    "idle": 4,
    "total": 6,
    "forecast": {
      "h1":  {"kwh": 8.2, "idle": 3, "util": 50.0, "is_peak": 0, "congestion": "mid"},
      "h6":  {"kwh": 22.0, "idle": 1, "util": 83.3, "is_peak": 1, "congestion": "high"},
      "h24": {"kwh": 18.0, "idle": 2, "util": 66.7, "is_peak": 0, "congestion": "mid"}
    }
  }],
  "alerts": [
    {"station_id": 1, "name": "市民中心充电站", "horizon": 6, "reason": "6小时后预测占用率 83%"}
  ]
}
```

大屏 6 区：KPI、质量问题摘要（体现老师（2））、今日负荷、未来 24h 预测、各站占用率、预警列表。Vue 3 + ECharts 5 均走 CDN；Flask 端口 **8081**（避开 YARN 8088、HDFS 9870、充电 9000）。

### `station_list` 增量字段（附加，不改 type）

现有：`id,name,address,longitude,latitude,price,total,idle,distance`。  
新增：`forecast_idle_1h/_6h/_24h`、`forecast_util_1h/_6h/_24h`、`congestion`（默认 1h）、`recommend_score`（`forecast_idle_1h×10 - forecast_util_1h`）、`is_peak_1h`。  
服务端仍按距离排。用户端「智能推荐」打开后：congestion 升序 → score 降序 → distance 升序。

---

## 人员与文件边界

| 模块 | 主负责人 | 主要文件 | 说明 |
|------|----------|----------|------|
| （1）模拟数据+脏数据+上 ODS | 邓雅心 | `bigdata/gen_ods.py`、维表导出、`sql/patch_v4_load_forecast.sql` | 不写脏数据进 `charging_system`；不跑 MLlib |
| （2）（3）（4）（6）探查/清洗/分层/预测 | 翟梓涵 | `bigdata/dq_clean.py`、`bigdata/warehouse.sql.py`（或同一 `pipeline.py` 分阶段）、`bigdata/train_forecast.py`、`bigdata/hdfs_sync.sh` | 全部 `spark-submit`；演示不用 sklearn |
| （5）Flask+Vue+ECharts | 牛昀轶 | `dashboard/app.py`、`dashboard/frontend/`（Vue CDN + ECharts） | 只调 Flask `/api`；不直连 MySQL；不进 `client-admin/` |
| 预测接入服务端 | 朱雅琪 | `server/database.cpp` 的 `stationList` | 只读 `load_forecast` |
| 用户端推荐 | 马晓钰 | `client-user/stationlistpage.*`、`stationcardwidget.*` | 不直连 MySQL；不读 HDFS |

同一 `spark-submit bigdata/pipeline.py` 可按参数跑 `--stage qa|clean|dws|ads|train`，避免五个脚本起五个 SparkContext。

---

# 需求矩阵

> 详细说明 = 具体实现方法。预计日期 = 当天必须可验收。

| NO. | 大分类 | 中分类 | 小分类 | 详细说明 | 负责人 | 预计日期 | 状态 | 困难 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 108 | 老师（1）模拟数据 | 维表导出 | 按第一阶段表结构出 ODS 维表 | `gen_ods.py` 从 `charging_system.station/pile` 导出 CSV（列与 `schema.sql` 一致）。禁止 DROP 业务表。 | 邓雅心 | 2026-09-12 | × | 只读演示库 |
| 109 | 老师（1）模拟数据 | 足量订单 | 生成 14～30 天、1～3 万条模拟充电订单 | 按现有站/桩、单价、功率生成 `start_time/end_time/kwh/amount/status`，日内早/晚高峰与周末差异。`--seed` 可复现。文件 `bigdata/out/ods_charge_order.csv`。 | 邓雅心 | 2026-09-13 | × | 不要插入 `charge_order` 业务表 |
| 110 | 老师（1）模拟数据 | 质量问题 | 注入约 5%～10% 场景脏数据 | 覆盖空值、负 kwh、时间颠倒、重复 order_no、孤儿 station/pile、状态矛盾（见上表）。脏行带 `dq_tag` 列便于自测（ODS 可多一列，DWD 去掉）。 | 邓雅心 | 2026-09-13 | × | 每类都能在探查报告里对上 |
| 111 | 老师（1）存储 | 上传 HDFS ODS | 三维表进 `/user/charging/ods/` | `hdfs dfs -mkdir -p` 后 `-put -f`。Hadoop 未启动则非 0 退出。验收：`hdfs dfs -ls -R /user/charging/ods`。 | 邓雅心 | 2026-09-13 | × | HDFS 8020；`source ~/.hadoop_env.sh` |
| 112 | 数据库端 | 预测结果表 | MySQL `load_forecast`，schema_version=4 | `patch_v4_load_forecast.sql` + `upgradeSchema` 增量 CREATE。仅此表进演示库，给 `station_list` 用。 | 邓雅心 | 2026-09-12 | × | 禁止把 ODS 灌进 MySQL |
| 113 | 老师（2）探查 | PySpark 发现质量问题 | 空值/范围/重复/孤儿计数 | `spark-submit` 读 ODS：`isNull`、`kwh<0`、`end<start`、`groupBy order_no`、anti-join 维表。写出 `/user/charging/qa/report.json` 并 print。截图 Spark UI 4040。 | 翟梓涵 | 2026-09-13 | × | 数字须与邓雅心注入量级相符 |
| 114 | 老师（3）清洗 | PySpark ODS→DWD | 按规则过滤/去重 | 丢弃空 start/负 kwh/时间颠倒/孤儿；重复 order_no 留最新。`dwd_rows` 写入报告。写出 `/user/charging/dwd/charge_order/`。 | 翟梓涵 | 2026-09-13 | × | 不在清洗阶段做预测 |
| 115 | 老师（4）SparkSQL | DWD→DWS | 站×小时负荷 | TempView + SparkSQL 按整点分摊 kwh、统计占用/空闲。写出 `/user/charging/dws/station_hour/`。节假日用内置日历，不调天气 API。 | 翟梓涵 | 2026-09-13 | × | SQL 要能在答辩打开讲解 |
| 116 | 老师（4）SparkSQL | DWS→ADS | KPI、预警、训练宽表 | SparkSQL 汇总今日 kwh/营收（amount 分摊或按日）、桩状态可用演示库 pile 快照或 DWS 最新小时。预警：占用率≥80%。训练宽表含 lag-1、近 24h 均值、horizon 展开 1/6/24。写出 `/user/charging/ads/`。 | 翟梓涵 | 2026-09-14 | × | Flask 只读 ADS |
| 117 | 老师（6）预测 | Spark MLlib | 1h/6h/24h 负荷与空闲桩 | `spark-submit` 读 ADS 宽表。`VectorAssembler`：hour、weekday、is_weekend、is_holiday、lag、24h 均值、**horizon**。MLlib `RandomForestRegressor` 或 `GBTRegressor`，kwh 与 idle 各一模型。裁剪 idle∈[0,total]、util∈[0,100]；≥80% 为高峰。写 ADS forecast + `load_forecast`（pymysql，station_id 映射演示库）+ Flask 用 JSON。 | 翟梓涵 | 2026-09-14 | × | 演示主路径不用 sklearn；local[*] |
| 118 | 老师（6）部署 | 一键流水线 | qa→clean→dws→ads→train | `hdfs_sync.sh`：NameNode 存活则 `spark-submit pipeline.py`；禁止 namenode -format。结束打印各层行数、4040、Flask JSON 路径。 | 翟梓涵 | 2026-09-14 | × | 与充电 9000 同时开 |
| 119 | 老师（5）大屏 | Flask API | 提供 /api/quality 与看板数据 | `dashboard/app.py` 读 ADS/qa JSON（或仓库 mock）。端口 8081。13 号 mock 与上文结构一致。 | 牛昀轶 | 2026-09-13 | × | 不直连业务 MySQL |
| 120 | 老师（5）大屏 | Vue+ECharts | 6 区运营看板 | Vue 3 CDN + ECharts 5 CDN。KPI、质量摘要、今日负荷、24h 预测、各站占用、预警。`axios/fetch` 调 Flask。空数据不白屏。 | 牛昀轶 | 2026-09-13 | × | 不用 HBuilderX、不用 `http.server` 当验收主路径 |
| 121 | 老师（5）大屏 | 接真 ADS | 流水线后刷新大屏 | 14 号改 fetch 真接口；质量区数字与 qa/report 一致；预警可点高亮站。 | 牛昀轶 | 2026-09-14 | × | Flask 与 Spark 写文件的路径约定好 |
| 122 | 服务端 | 预测接入 | station_list 附带预测 | `stationList` 查最新 `load_forecast` 拼字段。无预测不崩首页。不新增 MsgType。 | 朱雅琪 | 2026-09-13 | × | 13 号可 mock 字段 |
| 123 | 服务端 | 可测 | 登录后可见新字段 | 演示号 `13800138001` + token 调 `station_list`。样例补 `docs/api-contract.md`。 | 朱雅琪 | 2026-09-14 | × | 须 load_forecast 有数 |
| 124 | 用户端 | 智能推荐开关 | 距离序 / 预测序 | 默认关=NO.4。打开后排缓存：congestion → score → distance。缺字段当 mid/0。 | 马晓钰 | 2026-09-13 | × | 不改导航 |
| 125 | 用户端 | 卡片标记 | 低拥堵 / 高峰将至 | low 绿、is_peak_1h 橙、high 红；缺字段不崩。 | 马晓钰 | 2026-09-13 | × | — |
| 126 | 联调 | 15 号验收 | 按老师六步演示 | ① ODS 脏数据 + qa 报告（（1）（2））。② DWD/DWS/ADS 路径与 SparkSQL（（3）（4））。③ Flask+Vue 大屏（（5））。④ spark-submit MLlib + 4040 + 用户端推荐（（6）+附加）。演示号余额约 92.50。 | 翟梓涵（组织）全员 | 2026-09-15 | × | 录屏按六步，不要只播大屏 |

---

## 每人每天任务

### 9/12（六）今晚 — 对齐口径，邓开工造数

| 人员 | 当天功能 | 实现方法 | 当晚验收 |
|------|----------|----------|----------|
| 邓雅心 | NO.108 + NO.112 | 导出维表脚本骨架；`load_forecast` 建表 SQL | 演示库多预测表、旧订单还在 |
| 翟梓涵 | 锁合同 | 本文件；建 `bigdata/`、`dashboard/frontend/` | 群里确认六步与目录 |
| 朱雅琪 | 读 station_list 增量 | 对照现有键 | 能说出要加的键 |
| 牛昀轶 | 大屏 6 区 + `/api` 路径 | 对照上文 JSON | 与 mock 对得上 |
| 马晓钰 | 定标签位置 | 看卡片价格/空闲行 | 不改导航 |

### 9/13（日）— 造数+探查清洗+各端 mock

| 人员 | 当天功能 | 实现方法 | 当晚验收 |
|------|----------|----------|----------|
| 邓雅心 | NO.109–111 | 1～3 万单 + 脏数据 + put ODS | HDFS ODS 有文件；能口头举例脏行 |
| 翟梓涵 | NO.113–115 | spark-submit 探查、清洗、DWS | qa/report 有数；DWD&lt;ODS；4040 能开 |
| 牛昀轶 | NO.119–120 | Flask mock + Vue 六区 | `http://localhost:8081` 有图 |
| 朱雅琪 | NO.122 mock | station_list 带新键 | 响应里能看到 congestion |
| 马晓钰 | NO.124–125 | 开关+标签 | 开/关顺序变化；缺字段不崩 |

### 9/14（一）— ADS + MLlib + 真接口

| 人员 | 当天功能 | 实现方法 | 当晚验收 |
|------|----------|----------|----------|
| 邓雅心 | 修生成器 | 脏数据比例、唯一 order_no 冲突只出现在 ODS | 探查分类与注入一致 |
| 翟梓涵 | NO.116–118 | SparkSQL ADS + MLlib + 写 load_forecast | 大屏与 SQL 对得上；用户端能读到预测 |
| 牛昀轶 | NO.121 | 接真 Flask | 质量区、预警与报告一致 |
| 朱雅琪 | NO.123 | JOIN 真表 | 标签与库一致 |
| 马晓钰 | 对比站 | 近而 high / 稍远而 low | 开推荐后 low 在前 |

### 9/15（二）— 只按六步录屏，不新开功能

| 人员 | 当天功能 | 实现方法 | 当晚验收 |
|------|----------|----------|----------|
| 全员 | NO.126 | 录：（1）脏数据（2）探查（3）清洗（4）四层路径（5）大屏（6）MLlib+推荐 | 老师六步都能指到文件 |
| 翟梓涵 | 合 PR | 冲突按文件边界 | 9000 与 8020 同时活 |
| 邓雅心 | 口播 | 「业务库干净；ODS 才是模拟脏数据」 | 问数据从哪来能答 |

---

## 15 号砍掉（问到就说「下一迭代」）

- 实时气象 API、故障诊断、需求调度、风控评分  
- 三节点 Hadoop、YARN 上跑 Spark、Hive 建仓  
- 用 sklearn 代替 Spark MLlib 作验收主路径  
- 把脏数据写入 `charging_system.charge_order`  
- 把 ECharts 塞进 Qt 管理端；用 HBuilderX 代替 Vue  
- 新开充电 TCP `MsgType`、改充电端口 9000  
