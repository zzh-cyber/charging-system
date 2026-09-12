#!/usr/bin/env bash
# 有 NameNode 时把迷你 ODS 推上 HDFS，再跑 qa+clean。禁止 namenode -format。
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck disable=SC1090
[ -f "$HOME/.hadoop_env.sh" ] && . "$HOME/.hadoop_env.sh"

if ! jps 2>/dev/null | grep -q NameNode; then
  echo "NameNode 未运行。先执行: ~/opt/module/hadoop-3.2.1/sbin/start-hadoop-local.sh"
  exit 1
fi

hdfs dfs -mkdir -p /user/charging/ods/charge_order /user/charging/ods/station /user/charging/ods/pile
hdfs dfs -put -f "$ROOT/bigdata/ods/charge_order.csv" /user/charging/ods/charge_order/
hdfs dfs -put -f "$ROOT/bigdata/ods/station.csv" /user/charging/ods/station/
hdfs dfs -put -f "$ROOT/bigdata/ods/pile.csv" /user/charging/ods/pile/
hdfs dfs -ls -R /user/charging/ods

spark-submit "$ROOT/bigdata/pipeline.py" --stage qa
spark-submit "$ROOT/bigdata/pipeline.py" --stage clean
echo "本地结果: $ROOT/bigdata/work/qa/report.json  与  $ROOT/bigdata/work/dwd/"
