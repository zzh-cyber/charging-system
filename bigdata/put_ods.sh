#!/usr/bin/env bash
#
# 把 ODS 三个 CSV 上传到 HDFS（NO.111）。
#
# 目录约定（需求矩阵里的）：
#   /user/charging/ods/station/
#   /user/charging/ods/pile/
#   /user/charging/ods/charge_order/
#
# 用法：
#   bigdata/put_ods.sh            # 探活 + 上传 + 列目录
#   bigdata/put_ods.sh --status   # 只探活，不上传
#
# 刻意不叫 hdfs_sync.sh —— 那个文件名在需求矩阵里划给翟梓涵的全流水线
# 脚本（qa→clean→dws→ads→train），不占他的位置。
#
# 依赖：~/.hadoop_env.sh 里配好的 HADOOP_HOME 等；Hadoop 没起来时
#       非 0 退出并提示怎么起。

set -euo pipefail

# hdfs 命令默认打一堆 INFO（SASL 之类），压到 WARN，免得把真正的报错淹掉
export HADOOP_ROOT_LOGGER="${HADOOP_ROOT_LOGGER:-WARN,console}"

HDFS_ROOT="${HDFS_ROOT:-/user/charging/ods}"
STATUS_ONLY=0
[[ "${1:-}" == "--status" ]] && STATUS_ONLY=1

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$REPO_ROOT/bigdata/out"

# ---- 环境 ----
if [[ -f "$HOME/.hadoop_env.sh" ]]; then
    # shellcheck disable=SC1091
    source "$HOME/.hadoop_env.sh"
else
    echo "⚠️  找不到 ~/.hadoop_env.sh，先按《新环境配置》配好 Hadoop 环境变量" >&2
    exit 1
fi

if ! command -v hdfs >/dev/null 2>&1; then
    echo "⚠️  PATH 里没有 hdfs 命令，Hadoop 环境变量没生效" >&2
    exit 1
fi

# ---- NameNode 探活 ----
# 用 -ls / 而不是 jps：jps 看到进程不代表 NameNode 已经离开安全模式。
echo "探活 NameNode ..."
if ! timeout 25 hdfs dfs -ls / >/dev/null 2>&1; then
    cat >&2 <<'EOF'
❌ HDFS 连不上，NameNode 没在跑。

   先启动 Hadoop：
       ~/hadoopctl.sh start
       ~/hadoopctl.sh status     # 确认 NameNode / DataNode 都起来了

   注意 Hadoop 不会随 WSL 开机自启，重启 WSL 后都要手动跑一次。
   本脚本不会去 format NameNode —— 那会清空 HDFS 上已有的数据。
EOF
    exit 1
fi
echo "✅ NameNode 正常"

if [[ $STATUS_ONLY -eq 1 ]]; then
    hdfs dfs -ls -R "$HDFS_ROOT" 2>/dev/null || echo "（$HDFS_ROOT 还不存在）"
    exit 0
fi

# ---- 检查产物 ----
declare -A SRC=(
    [station]="ods_station.csv"
    [pile]="ods_pile.csv"
    [charge_order]="ods_charge_order.csv"
)
missing=0
for sub in "${!SRC[@]}"; do
    if [[ ! -f "$OUT_DIR/${SRC[$sub]}" ]]; then
        echo "❌ 缺少 $OUT_DIR/${SRC[$sub]}" >&2
        missing=1
    fi
done
if [[ $missing -eq 1 ]]; then
    echo "" >&2
    echo "   先跑：python3 bigdata/gen_ods.py --orders" >&2
    exit 1
fi

# ---- 上传 ----
# -put -f 覆盖同名文件。每个目录里只放一个 CSV —— 目录里混进第二个文件
# 的话，Spark 读整个目录会把行数翻倍。
for sub in station pile charge_order; do
    hdfs dfs -mkdir -p "$HDFS_ROOT/$sub"
    hdfs dfs -put -f "$OUT_DIR/${SRC[$sub]}" "$HDFS_ROOT/$sub/"
    echo "✅ $HDFS_ROOT/$sub/${SRC[$sub]}"
done

# ---- 验收：列目录 ----
echo
echo "== hdfs dfs -ls -R $HDFS_ROOT =="
hdfs dfs -ls -R "$HDFS_ROOT"

echo
echo "== 各文件行数（含表头，所以是数据行数 +1）=="
for sub in station pile charge_order; do
    n=$(hdfs dfs -cat "$HDFS_ROOT/$sub/${SRC[$sub]}" | wc -l)
    printf "   %-22s %s 行\n" "$sub" "$n"
done

echo
echo "✅ ODS 已上传。需求矩阵 NO.111 的验收点：hdfs dfs -ls -R $HDFS_ROOT"
