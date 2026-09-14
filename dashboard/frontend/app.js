const { createApp, nextTick } = Vue;

createApp({
  data: () => ({
    generated_at: "",
    kpis: {},
    quality: {},
    load_today: [],
    load_forecast_24h: [],
    load_hour_avg: [],
    weekday_weekend: {},
    regions: [],
    pile_types: [],
    charge_heatmap: {}, summary: {}, station_rank: [], overstay_records: [], device_warnings: [], active_tickets: [],
    stations: [],
    alerts: [],
    dispatch: [],
    faults: [],
    now: "",
    error: "",
    charts: {},
    kpiOrder: [
      "today_kwh",
      "today_revenue",
      "total_kwh",
      "total_revenue",
      "active_stations",
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
      total_kwh: "累计充电量 (kWh)",
      total_revenue: "累计收益 (元)",
      active_stations: "活跃站点",
      idle_piles: "空闲桩",
      busy_piles: "在用桩",
      fault_piles: "故障桩",
      peak_hour: "高峰小时",
      alert_count: "预警数",
    },
    kpiUnits: { today_kwh: "kWh", today_revenue: "元", total_kwh: "kWh", total_revenue: "元", active_stations: "个", idle_piles: "个", busy_piles: "个", fault_piles: "个" },
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
      if (k === "today_revenue" || k === "total_revenue") return "¥" + Number(v).toFixed(2);
      return Number(v).toLocaleString();
    },
    stationUtil(s) {
      // The card's percentage is current occupancy, so it must agree with
      // the displayed idle/total count. Forecast utilization belongs to the
      // prediction views and can intentionally differ.
      if (s && Number(s.total) > 0 && s.idle != null)
        return ((Number(s.total) - Number(s.idle)) / Number(s.total)) * 100;
      const h1 = s && s.forecast && s.forecast.h1;
      if (h1 && h1.util != null) return Number(h1.util);
      return 0;
    },
    congestion(s) {
      const value = this.stationUtil(s);
      return value >= 80 ? "high" : value >= 50 ? "mid" : "low";
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
        this.load_hour_avg = data.load_hour_avg || [];
        this.weekday_weekend = data.weekday_weekend || {};
        this.regions = data.regions || [];
        this.pile_types = Array.isArray(data.pile_types) ? data.pile_types : [];
        this.charge_heatmap = data.charge_heatmap || {};
        this.summary = data.summary || {};
        this.station_rank = Array.isArray(data.station_rank) ? data.station_rank : [];
        this.overstay_records = Array.isArray(data.overstay_records) ? data.overstay_records : [];
        this.device_warnings = Array.isArray(data.device_warnings) ? data.device_warnings : [];
        this.active_tickets = Array.isArray(data.active_tickets) ? data.active_tickets : [];
        this.stations = data.stations || [];
        this.alerts = data.alerts || [];
        this.dispatch = data.dispatch || [];
        this.faults = data.faults || [];
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
          textStyle: { color: "#7899b6" },
          tooltip: { trigger: "axis" },
          grid: { left: 48, right: 18, top: 22, bottom: 28 },
          xAxis: { type: "category", data: x, axisLine: { lineStyle: { color: "#244d70" } }, axisLabel: { color: "#6688a5" } },
          yAxis: { type: "value", splitLine: { lineStyle: { color: "rgba(62,130,180,.15)" } }, axisLabel: { color: "#6688a5" } },
          series: [
            {
              type: "line",
              smooth: true,
              data: series,
              lineStyle: { width: 2, color: "#22d7ff" },
              areaStyle: { color: { type: "linear", x: 0, y: 0, x2: 0, y2: 1, colorStops: [{ offset: 0, color: "rgba(25,190,255,.55)" }, { offset: 1, color: "rgba(25,100,255,.02)" }] } },
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
      if (this.$refs.piles) {
        if (this.charts.piles) this.charts.piles.dispose();
        const values = [Number(this.kpis.idle_piles || 0), Number(this.kpis.busy_piles || 0), Number(this.kpis.fault_piles || 0)];
        const total = values.reduce((a, b) => a + b, 0);
        const c = (this.charts.piles = echarts.init(this.$refs.piles));
        c.setOption({ tooltip: { trigger: "item" }, legend: { bottom: 0, textStyle: { color: "#7899b6" } }, title: { text: String(total), subtext: "充电桩总数", left: "center", top: "32%", textStyle: { color: "#dcf6ff", fontSize: 24 }, subtextStyle: { color: "#6688a5" } }, series: [{ type: "pie", radius: ["55%", "75%"], center: ["50%", "45%"], label: { show: false }, data: [{ value: values[0], name: "空闲", itemStyle: { color: "#13dabb" } }, { value: values[1], name: "在用", itemStyle: { color: "#438cff" } }, { value: values[2], name: "故障", itemStyle: { color: "#ff6559" } }] }] });
      }
      line(
        "forecast",
        this.load_forecast_24h.map((x) => "+" + (x.offset != null ? x.offset : x.hour) + "h"),
        this.load_forecast_24h.map((x) => x.kwh)
      );
      if (this.$refs.stations) {
        if (this.charts.stations) this.charts.stations.dispose();
        const c = (this.charts.stations = echarts.init(this.$refs.stations));
        c.setOption({
          grid: { left: 112, right: 35, top: 5, bottom: 15 },
          xAxis: { type: "value", max: 100, show: false },
          yAxis: {
            type: "category",
            data: this.stations.slice(0, 8).map((x) => x.name), axisLabel: { color: "#83a7c4", width: 95, overflow: "truncate" }, axisLine: { show: false }, axisTick: { show: false }
          },
          series: [
            {
              type: "bar",
              data: this.stations.slice(0, 8).map((x) => this.stationUtil(x)), barWidth: 8,
              itemStyle: { color: { type: "linear", x: 0, y: 0, x2: 1, y2: 0, colorStops: [{ offset: 0, color: "#1679ff" }, { offset: 1, color: "#19e0dc" }] }, borderRadius: 4 },
              label: { show: true, formatter: "{c}%" },
            },
          ],
        });
      }
      if (this.$refs.heatmap) {
        if (this.charts.heatmap) this.charts.heatmap.dispose();
        const c = (this.charts.heatmap = echarts.init(this.$refs.heatmap));
        const contract = this.charge_heatmap || {};
        const source = Array.isArray(contract.series_data) && contract.series_data.length ? contract.series_data : null;
        if (source) {
          const xs = Array.isArray(contract.x_axis) ? contract.x_axis : [];
          const ys = Array.isArray(contract.y_axis) ? contract.y_axis : [];
          const c = (this.charts.heatmap = echarts.init(this.$refs.heatmap));
          c.setOption({ grid:{left:42,right:10,top:8,bottom:42}, xAxis:{type:"category",data:xs.map(x=>x+"时"),axisLabel:{color:"#6688a5"}}, yAxis:{type:"category",data:ys,axisLabel:{color:"#7899b6"}}, visualMap:{min:Number(contract.min_val)||0,max:Number(contract.max_val)||1,orient:"horizontal",left:"center",bottom:0,show:true,itemWidth:70,itemHeight:7,textStyle:{color:"#6688a5",fontSize:9},inRange:{color:["#10244a","#146fa4","#20d9d0","#ffd35a","#ff7048"]}}, series:[{type:"heatmap",data:source,itemStyle:{borderColor:"#0a1631",borderWidth:2,borderRadius:2}}]});
        } else {
        const source = this.load_hour_avg.length ? this.load_hour_avg : this.load_today;
        const dayNames = ["周一", "周二", "周三", "周四", "周五", "周六", "周日"];
        const groups = {};
        source.forEach((x) => { const day = x.weekday || x.day || x.date || "全天"; (groups[day] ||= []).push(x); });
        const days = Object.keys(groups);
        const hours = [...new Set(source.map((x) => Number(x.hour)))].sort((a, b) => a - b);
        const points = source.map((x) => [hours.indexOf(Number(x.hour)), days.indexOf(x.weekday || x.day || x.date || "全天"), Number(x.kwh || 0)]);
        const values = points.map((x) => x[2]);
        const max = Math.max(...values, 1);
        c.setOption({
          tooltip: { formatter: (p) => `${days[p.value[1]]} ${hours[p.value[0]]}时<br/>充电量：${p.value[2]} kWh` },
          grid: { left: 38, right: 10, top: 8, bottom: 42 },
          xAxis: { type: "category", data: hours, axisLabel: { color: "#6688a5", interval: 3 }, axisLine: { lineStyle: { color: "#244d70" } } },
          yAxis: { type: "category", data: days.map((d) => dayNames[Number(d) - 1] || d), axisLabel: { color: "#7899b6" }, axisLine: { show: false }, axisTick: { show: false } },
          visualMap: { min: 0, max, orient: "horizontal", left: "center", bottom: 0, itemWidth: 70, itemHeight: 7, text: ["高", "低"], textStyle: { color: "#6688a5", fontSize: 9 }, calculable: false, inRange: { color: ["#384c9e", "#287cff", "#22dce5", "#ffe36e"] } },
          series: [{ type: "heatmap", data: points, label: { show: false }, itemStyle: { borderColor: "#081d39", borderWidth: 3, borderRadius: 2 } }],
        });
        }
      }
      if (this.$refs.mix) {
        if (this.charts.mix) this.charts.mix.dispose();
        const c = (this.charts.mix = echarts.init(this.$refs.mix));
        const data = this.pile_types.map((x) => ({ name: x.type === "直流" ? "快充（直流）" : x.type === "交流" ? "慢充（交流）" : x.type, value: Number(x.busy || 0) + Number(x.idle || 0) }));
        const total = data.reduce((sum, x) => sum + x.value, 0);
        c.setOption({ color: ["#26dded", "#568eff", "#b28cff", "#72e2c3"], tooltip: { trigger: "item", formatter: "{b}<br/>{d}%（{c} 桩）" }, legend: { bottom: 0, icon: "circle", itemWidth: 7, itemHeight: 7, textStyle: { color: "#7899b6", fontSize: 11 } }, title: { text: String(total), subtext: "桩总数", left: "center", top: "30%", textStyle: { color: "#e7f8ff", fontSize: 22 }, subtextStyle: { color: "#7899b6", fontSize: 10 } }, series: [{ type: "pie", radius: ["48%", "72%"], center: ["50%", "43%"], startAngle: 90, avoidLabelOverlap: true, label: { show: false }, emphasis: { label: { show: true, color: "#e7f8ff", formatter: "{d}%" } }, data, itemStyle: { borderColor: "#081d39", borderWidth: 2 } }] });
      }
    },
    visibleStations() {
      if (this.stations.length >= 8) return this.stations.slice(0, 8);
      const names = ["深圳市民中心", "北京南站", "上海陆家嘴", "广州天河", "杭州西湖", "南京新街口", "成都天府", "重庆江北"];
      const result = this.stations.slice();
      for (let i = result.length; i < 8; i++) result.push({ station_id: "fallback-" + i, name: names[i], idle: 0, total: 0 });
      return result;
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
    const tick = () => { this.now = new Date().toLocaleString("zh-CN", { hour12: false }); };
    tick(); this.clockTimer = setInterval(tick, 1000);
    let resizeTimer; window.addEventListener("resize", () => { clearTimeout(resizeTimer); resizeTimer = setTimeout(() => Object.values(this.charts).forEach((c) => c && c.resize()), 120); });
    this.refresh();
  },
}).mount("#app");
