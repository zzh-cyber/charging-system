# 第二阶段需求矩阵（大数据可视化大屏 + 充电负荷智能预测）

> **项目名称**：新能源汽车充电管理系统（接第一阶段 Qt + MySQL）  
> **节点**：9/12 环境就绪 · **9/15 两项验收**  
> **编制日期**：2026-09-12　**修订**：2026-09-12 晚（预测引擎改为 Spark MLlib，不再用 sklearn 作演示主路径）  
> **状态图例**：○ 完成　△ 进行中　× 未着手  

第一阶段四项（用户端 / 管理端 / 服务端 / 数据库）不变。本文件只覆盖老师文档 **（4）大数据可视化大屏**、**（5）机器学习智能分析 · 充电负荷预测**。  
故障诊断、需求调度、智能风控、实时天气 API、三节点 Hadoop、YARN 上跑 Spark：**9/15 不做**。  
**预测必须用 Spark**（本机 `local[*]` + MLlib），数据仍是订单聚合/补齐的小表（MB 级），模拟分布式训练过程。

---

## （4）（5）总思路

两项是同一条链路的两端，不是两套系统。

业务库订单 → 做成「每站每小时」小表（自己生成/补齐，几 MB）→ 进 HDFS → **`spark-submit` 本机模式跑 MLlib** 得到未来 1h/6h/24h 负荷和空闲桩 → 写回 MySQL 并生成 JSON → **（4）ECharts 大屏**给运营看预警，**（5）用户端**按预测推荐空站。

被问「为何不集群」：课设数据量是 MB 级、单节点伪分布式；Spark 用 `local[*]` 模拟分布式 API，算法与上集群时相同，数据变大只需改 `spark.master`。

---

## 实现总览（先看这个，再看表）

数据仍以 MySQL `charging_system` 为准。Hadoop 负责把小时 CSV 放进 HDFS；Spark 负责读 HDFS、MLlib 回归。预测结果必须写回 MySQL，用户端才能走现有 TCP:9000，大屏读 JSON，互不抢 9000 端口。

```text
charge_order / pile / station / wallet_transactions
        │  etl_export.py（邓雅心）
        ▼
station_load_hourly（MySQL） + CSV          ← 几 MB，允许补齐时序
        │  hdfs dfs -put（邓雅心）
        ▼
HDFS /user/charging/load_hourly.csv
        │  spark-submit local[*]  train_forecast.py（翟梓涵 / Spark MLlib）
        ▼
load_forecast（MySQL） + dashboard/data/latest.json
        │                    │
        │                    └─►（4）ECharts 大屏只读 JSON（牛昀轶）
        └─► station_list 附加预测字段（朱雅琪）
                    └─►（5）用户端卡片推荐（马晓钰）
```

HDFS：`hdfs://localhost:8020`（不要用 9000）。充电后台继续 `charging-server:9000`。  
Spark：3.4.1（预编译 Hadoop 3），`JAVA_HOME` 指向 JDK 8u261，`HADOOP_CONF_DIR` 指向本机 Hadoop conf；`spark.master=local[*]`，`spark.driver.memory=1g`。训练时浏览器打开 **http://localhost:4040** 作为「用了 Spark」的截图。

### 新增表（schema_version = 4，邓雅心建，启动走 upgradeSchema 增量 ALTER，禁止 DROP）

**`station_load_hourly`**（训练样本，一站一小时一行）

| 字段 | 类型 | 含义 |
|------|------|------|
| station_id | BIGINT | 电站 |
| hour_ts | DATETIME | 整点，如 2026-09-12 18:00:00 |
| kwh | DECIMAL(10,2) | 该小时充电量 |
| order_count | INT | 该小时进行中/结束的订单数 |
| busy_piles | INT | 该小时占用桩数（由订单重叠估算） |
| idle_piles | INT | total_piles - busy_piles（故障桩本阶段视为不可用，不另建模） |
| total_piles | INT | 该站桩总数快照 |
| is_weekend | TINYINT | 周六日=1 |
| is_holiday | TINYINT | 节假日=1（用内置日历，不调天气 API） |
| hour_of_day | TINYINT | 0–23 |
| weekday | TINYINT | 0=周一 … 6=周日 |

**`load_forecast`**（模型输出，用户端和大屏都读它）

| 字段 | 类型 | 含义 |
|------|------|------|
| station_id | BIGINT | 电站 |
| generated_at | DATETIME | 本次训练时间 |
| horizon_hours | TINYINT | 1 / 6 / 24 |
| pred_kwh | DECIMAL(10,2) | 预测充电量 |
| pred_idle | INT | 预测空闲桩数 |
| pred_util | DECIMAL(5,2) | 预测占用率 %，busy/total×100 |
| is_peak | TINYINT | 占用率 ≥ 80 为 1 |
| congestion | ENUM('low','mid','high') | idle 占比 ≥50% 为 low；&lt;20% 或 is_peak=1 为 high；其余 mid |

同一 `generated_at` 批次写入；读的时候取 `MAX(generated_at)`，避免旧预测混入。

### `dashboard/data/latest.json`（牛昀轶只读这份，13 号先接下面结构的假数据）

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
  "load_today": [{"hour": 0, "kwh": 12.3}, {"hour": 1, "kwh": 8.1}],
  "load_forecast_24h": [{"offset": 1, "kwh": 15.0}, {"offset": 24, "kwh": 40.2}],
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

### `station_list` 增量字段（朱雅琪写，马晓钰读；无预测时字段可缺省，客户端当 mid）

现有：`id,name,address,longitude,latitude,price,total,idle,distance`。  
新增（不改 type 名，不新增报文，身份仍只来自 token）：

| 字段 | 类型 | 含义 |
|------|------|------|
| forecast_idle_1h / _6h / _24h | int | 预测空闲桩 |
| forecast_util_1h / _6h / _24h | number | 预测占用率 |
| congestion | string | `low` / `mid` / `high`，默认用 **1h** |
| recommend_score | number | `forecast_idle_1h × 10 - forecast_util_1h`，越大越该推荐 |
| is_peak_1h | bool | 1h 是否高峰 |

排序：服务端仍按距离升序（保持 NO.3/NO.4）。用户端打开「智能推荐」后，在客户端对缓存数组 `stable_sort`：congestion 升序（low→high），同档按 recommend_score 降序，再按 distance 升序。

---

## 人员与文件边界（个人成果）

| 模块 | 主负责人 | 主要文件 | 说明 |
|------|----------|----------|------|
| 数据采集/存储/预处理 | 邓雅心 | `sql/patch_v4_load_forecast.sql`、`sql/schema.sql` 增表、`bigdata/etl_export.py` | 不改 Qt 界面；不训练模型 |
| 模型训练/HDFS/JSON/联调 | 翟梓涵 | Spark 安装与 `spark-env.sh`、`bigdata/train_forecast.py`（**spark-submit**）、`bigdata/hdfs_sync.sh`、`dashboard/data/latest.json`（由作业生成）、合 PR | 不改卡片 UI；不画 ECharts；演示主路径不用 sklearn |
| 运营大屏 Web | 牛昀轶 | `dashboard/index.html`、`dashboard/js/dashboard.js`、`dashboard/css/` | 只 fetch `data/latest.json`；不直连 MySQL；不进 `client-admin/` |
| 预测接入服务端 | 朱雅琪 | `server/database.cpp` 的 `stationList`、`server/clienthandler.cpp`（仅当 dispatch 要改） | 不改用户端卡片；不跑 Python |
| 用户端推荐 | 马晓钰 | `client-user/stationlistpage.*`、`client-user/stationcardwidget.*` | 不直连 MySQL；不读 JSON |

---

# 需求矩阵

> 详细说明 = 具体实现方法（落到表/字段/文件/算法）。预计日期 = 当天必须可验收。

| NO. | 大分类 | 中分类 | 小分类 | 详细说明 | 负责人 | 预计日期 | 状态 | 困难 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 108 | 数据库端 | 负荷时序表 | 新增 station_load_hourly / load_forecast，schema_version=4 | 写 `sql/patch_v4_load_forecast.sql`：CREATE 两表（字段见上文），`INSERT schema_version (4, '负荷时序与预测表')`。在 `Database::upgradeSchema()` 按 v3 同样模式：表不存在则 CREATE，version&lt;4 则插入版本行。禁止 DROP。现有 charge_order 数据保留。 | 邓雅心 | 2026-09-12 | × | 须与翟梓涵当晚对完字段再建，避免 13 号改表 |
| 109 | 数据库端 | 小时负荷聚合 | 从 charge_order 滚出每站每小时 kwh / 占用 | `etl_export.py` 读 MySQL：对 settled/charging/pending_payment 且 start_time 非空的订单，按 `[start_time, end_time)` 拆到整点（充电中订单 end 用 NOW()）。该小时 kwh = 订单 kwh ×（重叠秒数/duration_seconds），duration=0 则跳过。busy_piles = 该小时内该站重叠订单数上限截断为 total_piles。idle_piles = total_piles - busy_piles。写入 `station_load_hourly`。现网约 300 单会很稀，允许按「该站已有小时均值 × 日内曲线（早高峰 8–9、晚高峰 17–20）」补 14 天 × 24h × 各站，补齐行 `is_holiday` 用内置日期表（国庆/周末），**不调用天气 API**。 | 邓雅心 | 2026-09-13 | × | 补齐种子须可复现，脚本带 `--seed` 固定随机数 |
| 110 | 数据库端 | 导出 HDFS | CSV 上传证明大数据存储 | 脚本把 `station_load_hourly` 导出为 `bigdata/out/load_hourly.csv`（表头与表字段一致）。调用 `hdfs dfs -mkdir -p /user/charging` 与 `hdfs dfs -put -f ... /user/charging/load_hourly.csv`。验收：`hdfs dfs -ls /user/charging` 看得到文件。Hadoop 未启动时脚本报错退出码非 0，不写半份。 | 邓雅心 | 2026-09-13 | × | WSL 需先 `source ~/.hadoop_env.sh`；HDFS 是 8020 |
| 111 | 机器学习 | 特征与训练 | Spark MLlib 预测 1h/6h/24h 负荷与空闲桩 | 安装 Spark 3.4.1-bin-hadoop3 到 `~/opt/module/`（单机 Hadoop 包里没有，需另下）。`train_forecast.py` 必须用 `spark-submit` 启动：`SparkSession` 读 `hdfs://localhost:8020/user/charging/load_hourly.csv`（HDFS 不可用时再读本地 CSV，答辩以 HDFS 为准）。`VectorAssembler` 特征：hour_of_day、weekday、is_weekend、is_holiday、近 24h kwh/idle 均值、lag-1，**horizon（1/6/24）作为一列**。标签：对应整点的 kwh、idle_piles。算法：Spark MLlib `RandomForestRegressor` 或 `GBTRegressor`，kwh 与 idle **各一个模型**，全站共用，station_id 作数值/类别列（样本不足则该站小时均值兜底）。预测占用率 = 1 - pred_idle/total，裁剪到 0–100%；idle 裁剪到 [0,total]；≥80% 标 is_peak。写出 `load_forecast`（只留最新 `generated_at` 批次）和 `dashboard/data/latest.json`。训练过程中打开 http://localhost:4040。 | 翟梓涵 | 2026-09-13 | × | 数据仅 MB 级，用 local[*] 模拟即可；不要配 YARN。口播：存储 Hadoop、预测 Spark MLlib |
| 112 | 机器学习 | 模型部署 | 一键：HDFS + spark-submit + 出 JSON | `bigdata/hdfs_sync.sh`：检查 `jps` 有 NameNode，没有则 `start-hadoop-local.sh`；再调 etl（若表空）和 `spark-submit ... train_forecast.py`。成功后打印 `load_forecast` 行数、JSON 路径、Spark UI 地址。**禁止再次 namenode -format**。 | 翟梓涵 | 2026-09-13 | × | 与充电 9000 同时开；HDFS 8020/9870；Spark UI 4040 |
| 113 | 大数据大屏 | 页面骨架 | Web 看板 6 区布局 | `dashboard/index.html` 深色运营风：顶栏标题「充电运营健康度看板」+ 刷新时间；第一行 4 张 KPI（今日电量/营收/空闲桩/预警数）；第二行 今日负荷折线 + 未来 24h 预测折线；第三行 各站占用率柱状 + 桩状态饼图（idle/busy/fault）；第四行 高峰预警列表。图表用 ECharts 5 CDN（`cdn.jsdelivr.net/npm/echarts@5`），不引入 Vue/React、不用 HBuilderX。本地 `python3 -m http.server 8080 --directory dashboard`，浏览器 `http://localhost:8080`。 | 牛昀轶 | 2026-09-13 | × | 13 号用仓库内 `data/latest.mock.json` 画通，字段与真 JSON 完全一致 |
| 114 | 大数据大屏 | 绑定真实 JSON | fetch latest.json 渲染 | `dashboard.js`：`fetch('./data/latest.json', {cache:'no-store'})`，校验 `kpis/stations/alerts` 存在。KPI 用 `kpis.*`；折线 x 为 hour/offset、y 为 kwh；柱状用 `stations[].forecast.h1.util`；饼图用 kpis 的 idle/busy/fault；预警表渲染 `alerts[]`，`is_peak` 行标红。JSON 404 或字段缺失显示「预测未生成」空状态，不白屏。14 号换成训练脚本覆盖的 `data/latest.json`。 | 牛昀轶 | 2026-09-14 | × | file:// 打不开 fetch，必须用 http.server |
| 115 | 大数据大屏 | 运营预警 | 标出 1h/6h/24h 高峰站 | 预警列表按 horizon 分组；点击一行高亮柱状图对应 station_id。顶栏「预警数」= `kpis.alert_count`。文案写清「预测占用率≥80%」，避免写成已发生故障。 | 牛昀轶 | 2026-09-14 | × | — |
| 116 | 服务端业务处理 | 预测接入 | station_list 附带负荷预测 | 在 `Database::stationList` 现有 SELECT 之后，再查 `load_forecast` 中 `generated_at = (SELECT MAX(generated_at) FROM load_forecast)`，按 station_id + horizon_hours 拼进每条电站 JSON（字段见上文）。无预测行则不写这些键（或 congestion=`mid`）。**禁止**客户端传 user_id；token 鉴权门不变。不新增 MsgType。联表失败时仍返回原 station 列表，预测字段缺失，保证第一阶段首页不挂。 | 朱雅琪 | 2026-09-13 | × | 13 号可先写死 mock 字段，14 号改查表 |
| 117 | 服务端业务处理 | 预测查询可测 | 登录后 station_list 能看到新字段 | 用现有演示号 `13800138001` 登录拿 token，发 `station_list`（lat/lng 与首页一致）。至少 1 个站含 `congestion` 与 `forecast_idle_1h`。把请求/响应样例补进 `docs/api-contract.md` 本节附录。 | 朱雅琪 | 2026-09-14 | × | 须等 load_forecast 有数据；可用 sql 插 2 行自测 |
| 118 | 充电用户端 | 智能推荐开关 | 首页可切换距离 / 预测推荐 | `StationListPage` 在 5/10 条切换旁加「智能推荐」开关，默认关（保持 NO.4 距离序）。打开后对**已缓存**的完整列表 `stable_sort`：congestion 权重 low=0/mid=1/high=2 升序，同档 recommend_score 降序，再 distance 升序；再按 5/10 截取。无预测字段的站 congestion 视为 mid、score 视为 0。关闭开关恢复距离序。不重新请求也可以切（与 NO.4 条数切换同一缓存）。 | 马晓钰 | 2026-09-13 | × | 13 号用假字段就能排，14 号接真接口 |
| 119 | 充电用户端 | 卡片拥堵标记 | 低拥堵 / 高峰将至 | `StationCardWidget` 在价格/空闲旁增加标签：`congestion==low` 显示绿色「低拥堵」；`is_peak_1h==true` 显示橙色「高峰将至」；high 显示红色「易排队」。缺字段不显示标签、不崩溃。空闲数仍用现有 `idle`（实时），预测空闲可用小字「1h后约 N 桩」。 | 马晓钰 | 2026-09-13 | × | 不要改导航按钮逻辑（朱雅琪） |
| 120 | 联调与演示 | 15 号验收链路 | 三条演示必须通 | ① `jps` 有 NameNode/DataNode，`hdfs dfs -ls /user/charging` 有 csv；`spark-submit` 能跑完，浏览器 4040 能看到作业。② 大屏 6 区有数，能指出至少 1 个 6h 高峰站。③ 用户端开「智能推荐」后，低拥堵站排到距离较近的高拥堵站前面（准备 2 个站对比）。录屏按「HDFS → spark-submit → 大屏 → 用户端」四段。 | 翟梓涵（组织）全员 | 2026-09-15 | × | 演示号余额保持约 92.50，不在联调里乱充值 |

---

## 每人每天任务（功能 + 实现方法）

### 9/12（六）今晚 — 对齐字段，邓雅心先建表

| 人员 | 当天要实现的功能 | 具体实现方法 | 当晚验收 |
|------|------------------|--------------|----------|
| 邓雅心 | NO.108 建表 | 提交 `patch_v4_load_forecast.sql` + `upgradeSchema` v4 分支 | 本地 MySQL `SHOW TABLES` 有两张新表；旧订单还在 |
| 翟梓涵 | 锁定 JSON / 预测字段 | 本文件即合同；建 `bigdata/`、`dashboard/data/` 空目录；把 mock JSON 放进仓库 | 群里确认字段不再改 |
| 朱雅琪 | 读懂 station_list 增量 | 不改代码；对照 `database.cpp` `stationList` 现有键 | 能说出要加哪些键 |
| 牛昀轶 | 大屏信息架构 | 在纸面/备注列出 6 区对应 JSON 路径 | 与 mock JSON 对得上 |
| 马晓钰 | 卡片加标签的位置 | 看 `stationcardwidget.cpp` 空闲/价格行，定标签插口 | 不改导航、不改定位 |

### 9/13（日）— 各模块可独立演示（允许 mock）

| 人员 | 当天要实现的功能 | 具体实现方法 | 当晚验收 |
|------|------------------|--------------|----------|
| 邓雅心 | NO.109 + NO.110 | 跑通 `etl_export.py`：聚合 + 补 14 天时序 + 导出 CSV + `hdfs dfs -put` | `SELECT COUNT(*) FROM station_load_hourly` 明显大于原订单数；HDFS 有文件 |
| 翟梓涵 | NO.111 + NO.112 | 装 Spark 3.4.1；`spark-submit` 读 HDFS、MLlib 训练，写出 `load_forecast` 与 `latest.json`；`hdfs_sync.sh` 能一键跑 | 4040 能打开；JSON 能被浏览器打开；表里有 1/6/24 三档 |
| 牛昀轶 | NO.113 | `dashboard/index.html` + ECharts，fetch mock JSON | `python3 -m http.server 8080` 六区都有图 |
| 朱雅琪 | NO.116（可 mock） | `stationList` 每条对象写入 congestion 等字段；无表数据时用 mid/0 兜底 | token 调 station_list，JSON 里能看到新键 |
| 马晓钰 | NO.118 + NO.119 | 开关 + 排序 + 卡片标签；先信接口里已有字段 | 开关开/关列表顺序变化；缺字段不崩 |

### 9/14（一）— 换成真数据，打通三端

| 人员 | 当天要实现的功能 | 具体实现方法 | 当晚验收 |
|------|------------------|--------------|----------|
| 邓雅心 | 修 ETL 空洞/重复小时 | 同一 (station_id, hour_ts) 唯一键；重跑可 `--replace` | 无重复主键；抽样一站 24 点曲线像高峰 |
| 翟梓涵 | 真 JSON 覆盖 mock；修预测异常 | `spark-submit` 读 HDFS；util 裁剪到 0–100；idle 裁剪到 [0,total] | 大屏刷新后数字与 SQL 对得上 |
| 牛昀轶 | NO.114 + NO.115 | fetch 真 `latest.json`；预警列表可点 | 预警数、高峰站与 JSON 一致 |
| 朱雅琪 | NO.117 查真表 | `stationList` LEFT JOIN 最新批次 load_forecast | 用户端标签与库里 congestion 一致 |
| 马晓钰 | 推荐与距离对比 | 准备两个站：近而 high、稍远而 low，开推荐后 low 在前 | 截图能讲清「减少排队」 |

### 9/15（二）— 只验收、录屏、修崩，不新开功能

| 人员 | 当天要实现的功能 | 具体实现方法 | 当晚验收 |
|------|------------------|--------------|----------|
| 全员 | NO.120 演示 | 按「HDFS → spark-submit/4040 → 大屏 → 用户端推荐」录屏；各人只讲自己文件 | 四段都能现场点开 |
| 翟梓涵 | 合 PR、看冲突 | 只合能编译、能跑的；冲突按文件边界裁 | main 可编译；9000 与 8020 同时活 |
| 邓雅心 | 备份一句口播 | 说明小时表来自订单拆分 + 补齐，不是手造乱数 | 老师问数据从哪来能答 |

---

## 15 号砍掉（问到就说「下一迭代」）

- 实时气象 API、故障诊断、需求调度、风控评分  
- 三节点 Hadoop、YARN 上跑 Spark、Hive  
- 用 sklearn 代替 Spark 作验收主路径（Spark 起不来时只能口头说明，不能当交卷方案）  
- 新开 `MsgType`、新开 Flask 网关、改充电 9000  
- 把 ECharts 塞进 Qt 管理端（大屏就是独立 Web）
