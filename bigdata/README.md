# bigdata/

第二阶段「大数据可视化大屏 + 充电负荷智能预测」的脚本目录。
对应老师六步里的（1）模拟数据 →（2）探查 →（3）清洗 →（4）分层 →（6）预测。

负责人与文件边界见 [`docs/phase2-matrix.md`](../docs/phase2-matrix.md)。

## 怎么跑

用**系统 `python3`**（3.10.12），和 `tests/*.py` 一致，`pymysql` 已装：

```bash
cd ~/charging-system
python3 bigdata/gen_ods.py
```

数据库口令沿用服务端约定：优先读环境变量 `CHARGING_DB_PASSWORD`，没设则回退到
实训默认值（见 `server/config.h`）。下面这条可以不用写：

```bash
CHARGING_DB_PASSWORD=xxx python3 bigdata/gen_ods.py
```

## 现有脚本

| 脚本 | 需求 | 说明 |
|------|------|------|
| `common.py` | — | 共用底座：连库 + CSV 写作约定 |
| `gen_ods.py` | NO.108 | 从演示库导出 ODS 维表（`station` / `pile`）到 `out/` |
| `out/` | — | 导出产物，**已被 `.gitignore` 忽略**（靠根目录的 `out/` 规则） |

## 演示库是只读的

**绝不往 `charging_system` 写模拟数据或脏数据。** 脏数据只存在于 ODS 层的
CSV / HDFS 里，演示库要始终干净（Qt 用户端在连着它）。

唯一的例外是 `load_forecast` 表 —— 它是预测结果的落地表，由 MLlib 脚本写。

## CSV 约定（Spark 端读取时依赖）

| 项 | 约定 |
|----|------|
| 编码 / 行尾 | UTF-8 **无 BOM**、`\n` |
| 表头 | 有。列名 = 数据库列名，顺序 = 表定义顺序 |
| NULL | **空字段**（不是 `None`、不是 `NULL` 字面量） |
| DECIMAL | 原样十进制字符串，保留精度（如 `1.20`、`114.061000`） |
| DATETIME | `YYYY-MM-DD HH:MM:SS` |
| ENUM / INT / TINYINT | 原样（如 `fast`、`1`） |

> ⚠️ **Spark 读 CSV 时务必显式指定 schema 和 `nullValue=""`。**
> 默认的 `inferSchema` 会把空字段当空串塞进 string 列，数值列则可能整列推断错。
> 例：
>
> ```python
> df = (spark.read
>       .option("header", True)
>       .option("nullValue", "")
>       .schema("id long, station_id long, code string, ...")
>       .csv("file:///home/dyx/charging-system/bigdata/out/ods_pile.csv"))
> ```

> ⚠️⚠️ **坑：在 Spark 里读本地文件必须加 `file://` 前缀。**
> 本机 `fs.defaultFS = hdfs://localhost:8020`，所以 Spark 会把**相对路径和
> `/` 开头的路径都当成 HDFS 路径**。写成 `spark.read.csv("bigdata/out/ods_pile.csv")`
> 会报 `PATH_NOT_FOUND: hdfs://localhost:8020/user/dyx/bigdata/out/ods_pile.csv`。
> 要么加 `file://` 前缀（如上），要么先把文件 `hdfs dfs -put` 上去再从 HDFS 读
> —— 正式流程走后者。

`gen_ods.py` 自带两处自检，对不上会非 0 退出：
结果集列序 == `information_schema` 列序；写出行数 == 库里行数。

## HDFS 目录约定

```text
/user/charging/ods/charge_order/     /user/charging/dwd/charge_order/
/user/charging/ods/station/          /user/charging/dws/station_hour/
/user/charging/ods/pile/             /user/charging/ads/kpis/
/user/charging/qa/report.json        /user/charging/ads/forecast/
```

HDFS 走 **8020**（`hdfs://localhost:8020`），**不要用 9000** —— 那是充电业务的地盘。
Hadoop 不随 WSL 开机自启，重启后先 `~/hadoopctl.sh start`。
