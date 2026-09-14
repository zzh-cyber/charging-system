#!/usr/bin/env bash
# NO.118 一键流水线：冻结 ODS 上 HDFS → qa → clean → dws → ads → train
#
# 禁止 namenode -format。禁止 put 迷你 ODS（bigdata/ods/ 那 16 行会盖掉 2 万单）。
#
# 用法：
#   source ~/.hadoop_env.sh
#   ./bigdata/hdfs_sync.sh
#
# 环境变量：
#   HDFS_SYNC_SKIP_PUT=1   已 put 过冻结 ODS 时跳过上传
#   HDFS_SYNC_ODS=hdfs     默认 hdfs；可改 auto 只读本地 out/
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck disable=SC1090
[ -f "$HOME/.hadoop_env.sh" ] && . "$HOME/.hadoop_env.sh"

export SPARK_LOCAL_IP="${SPARK_LOCAL_IP:-127.0.0.1}"
export HADOOP_ROOT_LOGGER="${HADOOP_ROOT_LOGGER:-WARN,console}"
# 答辩默认 hdfs。Hadoop 没起时可用：HDFS_SYNC_ODS=local ./bigdata/hdfs_sync.sh
ODS_KIND="${HDFS_SYNC_ODS:-hdfs}"

if ! command -v spark-submit >/dev/null 2>&1; then
  echo "PATH 里没有 spark-submit。先: source ~/.hadoop_env.sh" >&2
  exit 1
fi

if [[ "$ODS_KIND" == "hdfs" ]]; then
  if ! command -v hdfs >/dev/null 2>&1; then
    echo "PATH 里没有 hdfs。先: source ~/.hadoop_env.sh" >&2
    exit 1
  fi
  echo "探活 NameNode ..."
  if ! timeout 25 hdfs dfs -ls / >/dev/null 2>&1; then
    cat >&2 <<'EOF'
NameNode 未就绪。先启动 Hadoop，例如：
  ~/opt/module/hadoop-3.2.1/sbin/start-hadoop-local.sh
  或 ~/hadoopctl.sh start
本脚本不会执行 namenode -format。
EOF
    exit 1
  fi
  echo "NameNode 正常"

  if [[ "${HDFS_SYNC_SKIP_PUT:-0}" != "1" ]]; then
    echo "上传冻结 ODS（bigdata/out，不是迷你 ods/）..."
    "$ROOT/bigdata/put_ods.sh"
  else
    echo "跳过 put（HDFS_SYNC_SKIP_PUT=1）"
  fi
else
  echo "ODS=$ODS_KIND，不 put HDFS（答辩请用默认，读 /user/charging/ods）"
fi

run_stage() {
  local stage="$1"
  echo
  echo "======== spark-submit --stage $stage --ods $ODS_KIND ========"
  spark-submit \
    --driver-memory 2g \
    --conf spark.ui.port=4040 \
    "$ROOT/bigdata/pipeline.py" \
    --stage "$stage" \
    --ods "$ODS_KIND"
}

run_stage qa
run_stage clean
run_stage dws
run_stage ads
run_stage train

count_lines() {
  local f="$1"
  if [[ -f "$f" ]]; then
    # 减表头
    wc -l < "$f" | awk '{print $1-1}'
  else
    echo "（无文件）"
  fi
}

echo
echo "======== NO.118 收口 ========"
echo "禁止 format NameNode：本脚本未执行。"
echo "Spark UI（作业运行中）: http://${SPARK_LOCAL_IP}:4040"
echo "  注意：每个 stage 结束 UI 会关掉，答辩截图请在某段 spark-submit 尚未退出时打开。"
echo
echo "各层行数（本地 work/，不含表头）："
printf "  DWD charge_order     %s\n" "$(count_lines "$ROOT/bigdata/work/dwd/charge_order/charge_order.csv")"
printf "  DWS station_hour     %s\n" "$(count_lines "$ROOT/bigdata/work/dws/station_hour/station_hour.csv")"
printf "  ADS train            %s\n" "$(count_lines "$ROOT/bigdata/work/ads/train/train.csv")"
printf "  ADS forecast         %s\n" "$(count_lines "$ROOT/bigdata/work/ads/forecast/forecast.csv")"
echo "  QA  report           $ROOT/bigdata/work/qa/report.json"
echo "  ADS dashboard        $ROOT/bigdata/work/ads/kpis/dashboard.json"
echo
if command -v mysql >/dev/null 2>&1; then
  mysql -u charging_user -p123456 charging_system -N -B -e \
    "SELECT CONCAT('load_forecast ', COUNT(*), ' 行') FROM load_forecast;" \
    2>/dev/null || echo "  load_forecast （查询失败，可稍后手动 SELECT COUNT）"
fi
echo
echo "大屏："
echo "  DASHBOARD_DATA_FILE=$ROOT/bigdata/work/ads/kpis/dashboard.json python3 $ROOT/dashboard/app.py"
echo "  http://127.0.0.1:8081"
echo "充电后台端口 9000 可同时开，本脚本不占用。"
echo "✅ qa → clean → dws → ads → train 跑完"
