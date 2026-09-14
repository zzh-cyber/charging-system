# bigdata/

第二阶段「大数据可视化大屏 + 充电负荷智能预测」的脚本目录。
对应老师六步里的（1）模拟数据 →（2）探查 →（3）清洗 →（4）分层 →（6）预测。

负责人与文件边界见 [`docs/phase2-matrix.md`](../docs/phase2-matrix.md)。

## 怎么跑

用**系统 `python3`**（3.10.12），和 `tests/*.py` 一致，`pymysql` 已装：

```bash
cd ~/charging-system

python3 bigdata/gen_ods.py                    # 只导维表（NO.108）
python3 bigdata/gen_ods.py --orders           # 维表 + 模拟订单 + 脏数据（NO.109/110）
./bigdata/put_ods.sh                          # 三个 CSV 上传 HDFS（NO.111）
./bigdata/put_ods.sh --status                 # 只探活 NameNode，不上传

# 验收：Spark 回读 HDFS，逐条核对 dq_expected.json 契约（NO.111）
source ~/.hadoop_env.sh
spark-submit --master 'local[2]' bigdata/check_ods_hdfs.py
```

默认一次 `--orders` 会产出 **20000 行**订单（含 1593 行脏数据，占 8.0%），
窗口 30 天，`seed=20260913`，模拟当前时刻 `2026-09-13 21:00:00`。

数据库口令沿用服务端约定：优先读环境变量 `CHARGING_DB_PASSWORD`，没设则回退到
实训默认值（见 `server/config.h`）。下面这条可以不用写：

```bash
CHARGING_DB_PASSWORD=xxx python3 bigdata/gen_ods.py
```

### 常用参数

```bash
python3 bigdata/gen_ods.py --orders \
    --days 14 --rows 30000 \        # 天数与总量（矩阵区间：14~30 天 / 1~3 万条）
    --seed 20260914 \               # 换种子换一份数据；同种子逐字节相同
    --sim-now 21:00:00 \            # 「模拟当前时刻」，必须落在 --end-date 当天
    --end-date 2026-09-13 \
    --dirty-ratio 0.08 \            # 脏数据比例，矩阵区间 0.05~0.10
    --holidays 2026-10-01           # 窗口内的节假日；默认空
```

**可复现**：同一组 `(--seed --end-date --sim-now --days --rows --dirty-ratio)`
下输出逐字节相同。解析后的参数会原样写进 `dq_expected.json` 备查。

> ⚠️ 窗口默认 **30 天**而不是矩阵下限的 14 天，是实测调出来的：72 站 / 399 桩
> 装 2 万单，14 天时 17.7% 的「站×小时」占用率 ≥80%、72 个站有 70 个触顶，
> 等于全城永远堵着，热点和 `congestion` 分级就失去意义。30 天时桩平均工时
> 占用率 10.3%，与真实演示库此刻的 15.6% 量级一致。要别的密度调 `--days` /
> `--rows` 即可，**不用改代码**。

## 现有脚本

| 脚本 | 需求 | 说明 |
|------|------|------|
| `common.py` | — | 共用底座：连库 + CSV 写作约定 |
| `gen_ods.py` | NO.108/109/110 | 导维表；`--orders` 时并造模拟订单与脏数据 |
| `simulate.py` | NO.109/110 | 订单合成 + 脏数据注入的纯逻辑（不连库、不写文件） |
| `put_ods.sh` | NO.111 | 三个 CSV 上传 HDFS；Hadoop 没起则非 0 退出 |
| `check_ods_hdfs.py` | NO.111 | 验收：Spark 回读 HDFS，逐条核对契约 |
| `report.py` | NO.109/110 | **可视化检验**：生成 HTML 图表报告，肉眼验分布与脏数据 |
| `dq_expected.json` | NO.110 | **接口契约**：注入数 / 期望探查数 / DWD 行数，见下 |
| `out/` | — | 导出产物，**已被 `.gitignore` 忽略**（靠根目录的 `out/` 规则） |

`gen_ods.py` 自带几处自检，对不上会非 0 退出、**并且不写任何订单文件**：
结果集列序 == `information_schema` 列序；写出行数 == 库里行数；
九类脏数据的**注入数逐项等于复刻探查数**。

## 演示库是只读的

**绝不往 `charging_system` 写模拟数据或脏数据。** 脏数据只存在于 ODS 层的
CSV / HDFS 里，演示库要始终干净（Qt 用户端在连着它）。
`bigdata/` 下所有脚本对演示库**只有 `SELECT`**。

唯一的例外是 `load_forecast` 表 —— 它是预测结果的落地表，由 MLlib 脚本写。

> ⚠️ **站 73 被排除了。** 演示库里的 73 号站是测试建的：`station_code` 是 UUID、
> 名字叫「1」、单价 2.00，却带着 6 个空闲桩而占用率为 0 —— 按矩阵里
> `recommend_score = forecast_idle_1h × 10 − forecast_util_1h` 它会拿到 60 分排到
> 「智能推荐」最前面，把演示用户导到一个测试站；`load_forecast` 对 `station` 有真
> FK，它还会被写进预测。
>
> 所以 `--exclude-stations`（默认 `73`）**一个参数同时作用于维表导出和订单生成**，
> 桩按 `station_id` 连带过滤。结果维表是 **72 站 / 399 桩**（原 73 / 405），正好对上
> 矩阵 NO.60a 的「约 72 座站、约 399 根桩」。逃生舱：`--keep-all-stations`。
>
> 这条**改动了昨天 NO.108 的输出**（73/405 → 72/399）。
> 连带影响：合法桩集变成 **1–399**，孤儿桩的假 id 必须从过滤后的集合推导。

## CSV 约定（Spark 端读取时依赖）

| 项 | 约定 |
|----|------|
| 编码 / 行尾 | UTF-8 **无 BOM**、`\n` |
| 表头 | 有。列名 = 数据库列名，顺序 = 表定义顺序 |
| NULL | **空字段**（不是 `None`、不是 `NULL` 字面量） |
| DECIMAL | 原样十进制字符串，保留精度（如 `1.20`、`114.061000`） |
| DATETIME | `YYYY-MM-DD HH:MM:SS` |
| ENUM / INT / TINYINT | 原样（如 `fast`、`1`） |

## 订单表的列与自洽关系

`out/ods_charge_order.csv` = `charge_order` 的 **19 列原样** + 末尾一列 `dq_tag`，
共 **20 列**。前 19 列的表头逐字等于 `SHOW CREATE TABLE charge_order`
（`sql/schema.sql`），**ODS 只多这一列**。

干净行满足这几条关系，可以直接拿来断言：

```
kwh              == (target_soc - start_soc)/100 * battery_capacity_kwh   ← kwh 是权威值
duration_seconds == end_time - start_time
amount           == round(kwh * 该站的 price, 2)                          ← 单价取该站的，不是随机的
pile.station_id  == 该行的 station_id
end_time         <= sim_now（2026-09-13 21:00:00），全量成立
```

> 顺序是「先定 `kwh` 再由它反推时长」，不是反的。反过来先定时长再算 `kwh` 会造出
> 180kW × 1.5h × 0.92 = 248 度这种行 —— 电池才 40~80 度、SOC 只涨一两格，
> 拿 `battery_capacity_kwh` 一对就露馅。

`status` 分布与各字段的填法：

| status | reserve_time | start_time | end_time | kwh / amount / duration |
|---|---|---|---|---|
| `settled` | 有 | 有 | 有 | 实值 |
| `pending_payment` | 有 | 有 | 有 | 实值（`pay_request_id` 为空） |
| `charging` | 有 | 有 | **= `sim_now`** | 已充部分（按已充时长折算） |
| `cancelled` | 有 | 六成为空 | 同上 | 同上 |
| `reserved` | 有 | 空 | 空 | **`0.00` / `0.00` / `0`，不是空** |

> `charging` 的 `end_time` 写死 `sim_now`（**快照截断，不是会话结束**）。
> 留空的话 Spark 得 `coalesce(current_timestamp())`，而流水线这几天会跑好几次 ——
> 13 号开的单到 15 号跑就变成 48 小时会话，`kwh` 摊到 48 个整点，大屏冒出窗口外的桶。
> `reserved` / `cancelled` 的 `kwh` 写 `0` 而不是空，否则 `isNull(kwh)` 会多吞掉几千行合法数据。

## 九种 `dq_tag` 及其含义（NO.110）

**每个脏行只注入一个缺陷** —— 这是「探查数 == 注入数」能对上的前提。脏行自带
`dq_tag` 列标注类型；**干净行写哨兵值 `OK`，不是空**（空的在 Spark 里是 `null`，
会诱导下游用 `dq_tag is not null` 当清洗规则 —— 那是拿注入答案自证）。

| `dq_tag` | 怎么造的 | 关键约束（别踩） |
|---|---|---|
| `NULL_START_TIME` | 只把 `start_time` 置空 | 其余全合法（`settled`、`duration>0`、`kwh>0`） |
| `NULL_KWH` | `kwh` 与 `amount` 都置空 | **`start_time`/`end_time`/`duration` 保持正常** |
| `NEG_KWH` | `kwh` 取负，`amount` 随之取负 | 避免「正 `amount` 负 `kwh`」再触发别的异常 |
| `SOC_RANGE` | 只把 **`target_soc`** 置 `-5` 或 `105` | `start_soc` 保持合法，`kwh` 正常 |
| `TIME_REVERSED` | `end_time` 提前到 `start_time` 之前 | 两端都非空 |
| `DUP_ORDER_NO` | 复制一条干净行 | 每对恰好 2 行；`id` 取新值；`created_at`/`updated_at` 都更早 |
| `ORPHAN_STATION` | `station_id` 换成维表里没有的（9001+） | **`pile_id` 保持真实** |
| `ORPHAN_PILE` | `pile_id` 换成维表里没有的（90001+） | **`station_id` 保持真实** |
| `STATUS_CONFLICT` | `settled` + `duration_seconds = 0` + `kwh > 0` | **`end_time - start_time` 仍是正常的 1200s** |

最后三条「关键约束」是有意设计的，都是为了不让一行命中两个检查项：

- `NULL_KWH` 行若 `start_time` 也为空，`isNull(start_time)` 会把两类一起吞掉，
  **两个计数同时错**。
- `STATUS_CONFLICT` 若顺手把 `start_time` 写成等于 `end_time`（为了让
  `duration=0` 自洽），下游只要把时间倒置判成 `end <= start`（比矩阵的 `<` 更严）
  就会**把这行数两次**。所以保持 1200s 正常，只置 `duration_seconds = 0`。
- `SOC_RANGE` 只动 `target_soc` 一列，计数才不取决于 SQL 里写了哪一列。

其余口径：

- **孤儿拆两类**，是为了让两个 anti-join 各自只抓一类、**不重复计数**。
  代价是**两个 anti-join 都必须写** —— 只 join `pile` 的话 `ORPHAN_STATION`
  会漏进 DWD，DWS 冒出 `station_id=9001`，而 `load_forecast` 对 `station` 有真 FK，
  写预测时会直接报错。
- `order_no` 用**结构化唯一键** `SIM{yyyymmdd}{序号:06d}`，不是随机串 —— 随机串迟早
  碰撞，`groupBy.count > 1` 会超过注入数，验收当天翻车。前缀 `SIM` 也让「业务库
  没被碰」一眼可查。
- **各类固定配额**（本次每类 177 行），不按比例随机撒 —— 随机会让某些类掉到个位数，
  且数字随 seed 漂。
- **脏比例口径**：`--rows 20000` 是**含脏行**的总量；`DUP_ORDER_NO` 一类算 1 行
  （多出来的那行）。
- `--holidays` 默认空：默认窗口 8/15–9/13 内**没有节假日**，所以 DWS 的 `is_holiday`
  恒为 0、模型会忽略这一维。机制已实现，只是窗口里没有样本。

## `dq_expected.json` 怎么用

**这是给 NO.113（探查）和 NO.114（清洗）的接口契约**，放在 `bigdata/` 而不是
`out/` —— `out/` 被 `.gitignore` 整个忽略，放那儿队友看不到。

| 字段 | 用途 |
|---|---|
| `params` | 本次生成的种子与参数，复现时照抄 |
| `injected` | 实际注入了多少行（按 `dq_tag` 分） |
| `expected_probe` | **NO.113 的探查脚本应当返回的数字**，键名与矩阵 mock 对齐 |
| `derived` | 干净/脏行数、`dwd_rows`、kwh 总量 |
| `dimension` | 合法站/桩 id 范围，用来推孤儿判定 |
| `hot_stations` | 8 个热点站及其桩数，用来校峰值占用率 |
| `contract` | 给 Spark 侧的约定，逐条照做 |

**9/14 验收就对着 `expected_probe` 逐项比**。生成时打印的那张表也是它：

```
  注入数 == 期望探查数
  dq_tag               注入    业务天然    期望探查
  NULL_START_TIME       177      1832        2009
  NULL_KWH              177         0         177
  ...
```

> ⚠️ **`null_start_time` 是两个来源之和**：注入的 177 行 **+ 业务上天然没有
> `start_time` 的 1832 行**（`reserved`、以及还没开始的 `cancelled`）。后者不是脏数据，
> 但 `isNull(start_time)` 一样会数到它们 —— 所以契约里两者分开写，**别把 2009 当成
> 注入数**，那样会以为注入多了 1832 行。
>
**`dwd_rows = 16929` 是怎么来的**（不是 `20000 - 1593`，看 `derived.dwd_by_tag`）：

```
DWD = 干净且已开始的 16575 行
    + SOC_RANGE         177 行      ← 【不丢弃】
    + STATUS_CONFLICT   177 行      ← 【不丢弃】
    = 16929 行，kwh 合计 608421.91 度
```

> ⚠️ **`SOC_RANGE` 必须留在 DWD 里。** 矩阵场景表给它的口径是「SOC **裁剪或置空**」
> 不是丢弃，NO.114 的丢弃清单里也没有它。顺手过滤掉是明确错的 ——
> DWD 会少 **177 行**，`dwd_rows` 和 `dwd_kwh_total` 双双对不上契约。

> ⚠️⚠️ **`STATUS_CONFLICT` 的口径矩阵自相矛盾，等组长裁定。**
>
> | 出处 | 写的什么 |
> |---|---|
> | 60–72 行场景表 ·「清洗规则（DWD）」列 | 状态矛盾「丢弃或按规则重算（**本阶段丢弃**）」 |
> | NO.114 · 详细说明 | 「丢弃空 start / 负 kwh / 时间颠倒 / 孤儿；重复 order_no 留最新」——**没列它** |
>
> 本实现跟 NO.114 的行级清单走（**保留**）：`dwd_rows = 16929`、
> `dwd_kwh_total = 608421.91`。
> 若裁定改为丢弃：**16752 行 / 602100.09 度**。
> **两个数都已写在 `dq_expected.json` 的 `derived` 里** ——
> `dwd_rows` / `dwd_kwh_total` 是保留版，`dwd_rows_if_status_conflict_dropped` /
> `dwd_kwh_if_status_conflict_dropped` 是丢弃版。
> 9/14 对数字时按裁定的那个取，别因为记账口径不同就判成「探查与注入不一致」。

> ⚠️ **有 56 行的 `end_time` 等于 `sim_now`**（`derived.truncated_charging_rows`，
> 其中干净行 54 条 = `truncated_charging_clean`）。那是**跨过模拟当前时刻、按已充
> 时长折算过 `kwh`** 的 `charging` 会话 —— 它们**不满足**上面那条
> `kwh == (target-start)/100*capacity` 整段关系式（`kwh` 只是已充的那部分）。
> 做这类自洽断言时要按 `status == 'charging' and end_time == sim_now` 排掉，
> 否则会看到几十行「对不上」而误以为自己写错了清洗。

> ⚠️ **矩阵 mock 里的数字是占位的，会动。** mock 写的是
> `ods_rows: 20000 / dwd_rows: 18500 / today_kwh: 1280.5`，真值是
> `20000 / 16929 / 约 4 万度级`。mock 本来就是「结构对齐用」的占位数 ——
> 前端按本文件的真值替换即可。

## HDFS 目录约定

```text
/user/charging/ods/charge_order/     /user/charging/dwd/charge_order/
/user/charging/ods/station/          /user/charging/dws/station_hour/
/user/charging/ods/pile/             /user/charging/ads/kpis/
/user/charging/qa/report.json        /user/charging/ads/forecast/
```

`put_ods.sh` 负责 ODS 三份。**每个目录里只放一个 CSV** —— 目录里混进第二个文件的话，
Spark 读整个目录会把行数翻倍（也不会读 `.` / `_` 开头的隐藏文件）。

上传前先探活 NameNode，连不上就非 0 退出并提示 `~/hadoopctl.sh start`。
探活用 `hdfs dfs -ls /` 而不是 `jps` —— 看到进程不代表 NameNode 已经离开安全模式。
脚本**不会**去 `format` NameNode（那会清空 HDFS 上已有的数据）。

HDFS 走 **8020**（`hdfs://localhost:8020`），**不要用 9000** —— 那是充电业务的地盘。
Hadoop 不随 WSL 开机自启，重启后先 `~/hadoopctl.sh start`。

> `put_ods.sh` 刻意**不叫** `hdfs_sync.sh` —— 那个文件名在需求矩阵里划给翟梓涵的
> 全流水线脚本（qa→clean→dws→ads→train），不占他的位置。

## `check_ods_hdfs.py` 验收脚本

`spark-submit --master 'local[2]' bigdata/check_ods_hdfs.py`，全过返回 0。
它把 `dq_expected.json` 的每一条都从**下游方向**验一遍：

1. 行数 / 列数 / 表头与 `charge_order` 定义逐字一致
2. 九类 `dq_tag` 分布 == 契约 `injected`（含干净行 `OK`）
3. 小时直方图 —— 确认早高峰落在 8–9、晚高峰落在 17–20
4. 反证：解析后换渲染时区，整条曲线挪 8 位（就是上面第 4 条的实测）
5. 干净行自洽：kwh / 时长 / `end_time <= sim_now`（排掉被截断的会话）
6. 维表 72 / 399、不含站 73、干净行无孤儿
7. **在 Spark 里按矩阵四条规则清洗**，核对 `dwd_rows` 与 `dwd_kwh_total`

> 边界：这是**模拟数据这一侧的验收脚本**，不是流水线。第 7 段跑清洗只是为了
> 从下游方向反证契约自洽（否则 `dwd_rows` 没人数过，9/14 容易各算各的），
> 不是替翟梓涵实现 NO.114 —— DWD 怎么写、放哪个文件由他定。
> 如果觉得越界，删掉这个文件即可，`dq_expected.json` 本身是独立的。

## `report.py` 可视化检验报告

`python3 bigdata/report.py` → `bigdata/out/ods_report.html`，浏览器打开即可。
加 `--desktop` 会再复制一份到 Windows 桌面，双击就能看（WSL 路径不好点）。

**纯标准库生成，零新依赖，零外部资源** —— 本机没装 matplotlib / pandas / numpy，
也不打算为此引新依赖；图表是内联 SVG，中文交给浏览器字体渲染。
它读的是 `bigdata/out/` 里的**本地 CSV**，所以 **Hadoop 没起也能跑**。

| 图 | 对应验收点 |
|----|-----------|
| 24 小时分布（工作日 vs 周末，按天均） | NO.109 早高峰 8–9、晚高峰 17–20、周末差异 |
| 逐时电量负荷曲线 | 「大屏今日负荷曲线」会长成什么样 |
| 站 × 小时 占用率热力图（72 站全画） | 少数站很挤、多数站很空；预警有东西可报 |
| 每小时日均告警站数 | NO.116 `alert_count` 的量级 |
| 九类 `dq_tag`：实际注入 vs 期望探查 | NO.110 的「探查分类与注入一致」 |
| 清洗丢弃明细 + 站点单量 | NO.114 四条规则各丢多少、总量对不对 |

几个刻意做对的细节，改的时候别改回去：

- **热力图 72 个站全画、按忙闲降序**，不设滚动框。结论就是整体「上红下白」的
  渐变，只画最忙的 40 个站等于把「多数站很空」那半边藏起来。
- **配色 gamma 是 1.6（>1）**。别改成 <1：实测日均占用率中位数才 13%，
  gamma 0.65 会把它渲染成 29% 的红，整张图糊成一片红，结论就看不见了。
- **≥80% 的格子额外加深色描边**。颜色深浅是连续的，而「触发 NO.116 预警」是硬
  阈值，得让人一眼数得出来是哪几格。
- **占用率是「先按天算、再对天数取平均」**，不是整个窗口取最大。30 天里挑最极端
  的一小时必然虚高，看什么都像「全城永远堵着」。鼠标悬停能看到该格的窗口内最高值。
- **独立复刻 DWD 规则**（不 import `simulate.py`），两边各自实现还能对上才算证据。
- 底部那句「丢弃 + 保留 == ODS 总行数」是不变式断言，少一条规则或多算一次都会变红。

## 给 Spark 侧的四条注意事项

1. **`dq_tag` 在 DWD 之后必须 `drop` 掉**，而且**不许拿它当清洗依据**。
   DWD 要按矩阵的四条规则真清洗（丢弃 `start_time` 为空 / `kwh` 为空 /
   `kwh < 0` / 时间颠倒 / 孤儿；重复单号留 `updated_at` 最新一条），
   拿 `dq_tag` 过滤等于拿注入答案自证 —— 被问「你怎么知道清洗规则对」就答不上来了。
2. **`header=True` + 显式 schema，不要用 `inferSchema`。** 默认的 `inferSchema`
   会把空字段当空串塞进 string 列，数值列则可能整列推断错。
3. **按列名取列**：Spark 列名**大小写敏感**，中文站名是 UTF-8。
4. **每个作业都设 `spark.sql.session.timeZone=Asia/Shanghai`**
   （`--conf` 或会话创建时 `.config(...)`，两种都行）。

   ⚠️ 这条的踩法和直觉不一样，是实测出来的：CSV 里是**无时区的裸时间串**，
   Spark 按**会话时区**解析、也按**会话时区**渲染 —— 同一个作业里两头一致，
   所以「读一次、同一次算」**看不出任何差别**（8 点还是 8 点）。
   危险的是**跨作业不一致**：

   - DWS 用一个时区写 parquet / 写 JDBC，ADS 用另一个时区读回来 ——
     时间整体挪 **8 小时**。实测：上海解析的数据在 UTC 会话里，8 点被渲染成 0 点，
     **早高峰整段错位到凌晨**，大屏曲线全糊。
   - 拿裸时间串去跟 `current_timestamp()` 这类**真实时刻**比大小，同理差 8 小时。
     （我们的 `end_time <= sim_now` 是裸串比裸串，不受影响。）

   所以：**定一个值，每个作业都写死同一个**，别一个脚本设一个不设。

```python
df = (spark.read
      .option("header", True)
      .option("nullValue", "")
      .schema("id long, order_no string, user_id long, station_id long, "
              "pile_id long, status string, unit_price decimal(10,2), ...")
      .csv("/user/charging/ods/charge_order/ods_charge_order.csv"))
# 正式流程从 HDFS 读；读本地文件才需要 file:// 前缀（见下）
```

> ⚠️ **坑：在 Spark 里读本地文件必须加 `file://` 前缀。**
> 本机 `fs.defaultFS = hdfs://localhost:8020`，所以 Spark 会把**相对路径和
> `/` 开头的路径都当成 HDFS 路径**。写成 `spark.read.csv("bigdata/out/ods_pile.csv")`
> 会报 `PATH_NOT_FOUND: hdfs://localhost:8020/user/dyx/bigdata/out/ods_pile.csv`。
> 要么加 `file://` 前缀，要么先把文件 `hdfs dfs -put` 上去再从 HDFS 读
> —— 正式流程走后者。
