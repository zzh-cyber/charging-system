const { createApp, nextTick } = Vue;

createApp({
  data: () => ({
    generated_at: "",
    kpis: {},
    quality: {},
    load_today: [],
    load_forecast_24h: [],
    stations: [],
    alerts: [],
    error: "",
    charts: {},
    kpiOrder: [
      "today_kwh",
      "today_revenue",
      "idle_piles",
      "busy_piles",
      "fault_piles",
      "peak_hour",
      "alert_count",
    ],
    qualityOrder: [
      "ods_rows",
      "null_start_time",
      "negative_kwh",
      "dup_order_no",
      "orphan_station",
      "dwd_rows",
    ],
    labels: {
      today_kwh: "今日充电量 (kWh)",
      today_revenue: "今日营收 (元)",
      idle_piles: "空闲桩",
      busy_piles: "在用桩",
      fault_piles: "故障桩",
      peak_hour: "高峰小时",
      alert_count: "预警数",
    },
    qualityLabels: {
      ods_rows: "ODS 总行数",
      null_start_time: "空 start_time",
      negative_kwh: "负电量",
      dup_order_no: "重复订单号",
      orphan_station: "孤儿电站",
      dwd_rows: "DWD 清洗后",
    },
  }),
  methods: {
    format(v, k) {
      if (v === undefined || v === null || v === "") return "--";
      if (k === "peak_hour") return String(v);
      if (k === "today_revenue") return "¥" + Number(v).toFixed(2);
      return Number(v).toLocaleString();
    },
    stationUtil(s) {
      const h1 = s && s.forecast && s.forecast.h1;
      if (h1 && h1.util != null) return Number(h1.util);
      if (s && s.total) return ((s.total - (s.idle || 0)) / s.total) * 100;
      return 0;
    },
    async refresh() {
      try {
        const r = await fetch("/api/dashboard");
        if (!r.ok) throw Error("HTTP " + r.status);
        const data = await r.json();
        this.generated_at = data.generated_at || "";
        this.kpis = data.kpis || {};
        this.quality = data.quality || {};
        this.load_today = data.load_today || [];
        this.load_forecast_24h = data.load_forecast_24h || [];
        this.stations = data.stations || [];
        this.alerts = data.alerts || [];
        this.error = "";
        await nextTick();
        this.draw();
      } catch (e) {
        this.error = "数据加载失败：" + e.message;
      }
    },
    draw() {
      const line = (ref, x, series) => {
        if (!this.$refs[ref]) return;
        if (this.charts[ref]) this.charts[ref].dispose();
        const c = (this.charts[ref] = echarts.init(this.$refs[ref]));
        c.setOption({
          tooltip: { trigger: "axis" },
          grid: { left: 40, right: 15, top: 15, bottom: 30 },
          xAxis: { type: "category", data: x },
          yAxis: { type: "value" },
          series: [
            {
              type: "line",
              smooth: true,
              data: series,
              areaStyle: { opacity: 0.15 },
              itemStyle: { color: "#42d9ff" },
            },
          ],
        });
      };
      line(
        "load",
        this.load_today.map((x) => x.hour + "时"),
        this.load_today.map((x) => x.kwh)
      );
      line(
        "forecast",
        this.load_forecast_24h.map((x) => "+" + (x.offset != null ? x.offset : x.hour) + "h"),
        this.load_forecast_24h.map((x) => x.kwh)
      );
      if (this.$refs.stations) {
        if (this.charts.stations) this.charts.stations.dispose();
        const c = (this.charts.stations = echarts.init(this.$refs.stations));
        c.setOption({
          grid: { left: 120, right: 30, top: 10, bottom: 25 },
          xAxis: { type: "value", max: 100 },
          yAxis: {
            type: "category",
            data: this.stations.map((x) => x.name),
          },
          series: [
            {
              type: "bar",
              data: this.stations.map((x) => this.stationUtil(x)),
              itemStyle: { color: "#ff9f5b" },
              label: { show: true, formatter: "{c}%" },
            },
          ],
        });
      }
    },
    highlight(id) {
      const idx = this.stations.findIndex((s) => s.station_id === id);
      if (idx < 0) return;
      const copy = this.stations.map((s) => JSON.parse(JSON.stringify(s)));
      const f = copy[idx].forecast && copy[idx].forecast.h1;
      if (f) f.util = Math.min(100, Number(f.util || 0) + 0.01);
      this.stations = copy;
      this.draw();
    },
  },
  mounted() {
    this.refresh();
  },
}).mount("#app");
