const { createApp, nextTick } = Vue;

const dashboardTooltip = (options = {}) => ({
  confine: false,
  appendTo: () => document.body,
  transitionDuration: 0.15,
  extraCssText: "z-index: 1000; pointer-events: none; max-width: 320px; white-space: normal; word-break: break-word;",
  ...options,
});

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
    yesterday_load: [],
    targets: {},
    data_source: "",
    latest_data_time: "",
    freshness_status: "",
    period: "1",
    window_start: "",
    window_end: "",
    periods: [
      { value: "1", label: "今日" },
      { value: "7", label: "近7日" },
      { value: "30", label: "近30日" },
    ],
    now: "",
    error: "",
    loading: false, faultModal: false,
    drawer: "", selectedStation: null, selectedFault: null, eventTab: "alerts", note: "",
    charts: {},
    kpiOrder: [
      "today_kwh",
      "today_revenue", "order_count",
      "total_kwh",
      "total_revenue",
      "active_stations",
      "idle_piles",
      "busy_piles",
      "fault_piles",
      "peak_hour",
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
      alert_count: "预警数", order_count: "有效订单",
    },
    kpiUnits: { today_kwh: "kWh", today_revenue: "元", order_count: "单", total_kwh: "kWh", total_revenue: "元", active_stations: "个", idle_piles: "个", busy_piles: "个", fault_piles: "个" },
    qualityLabels: {
      ods_rows: "ODS 总行数",
      null_start_time: "空 start_time",
      negative_kwh: "负电量",
      dup_order_no: "重复订单号",
      orphan_station: "孤儿电站",
      dwd_rows: "DWD 清洗后",
    },
  }),
  computed: {
    loadTitle() {
      return { "1": "今日充电负荷趋势", "7": "近7日平均负荷趋势", "30": "近30日平均负荷趋势" }[this.period];
    },
    availabilityPct() {
      const idle = Number(this.kpis.idle_piles || 0);
      const busy = Number(this.kpis.busy_piles || 0);
      const fault = Number(this.kpis.fault_piles || 0);
      const total = idle + busy + fault;
      if (!total) return null;
      return ((total - fault) / total) * 100;
    },
    freshnessLine() {
      const src = this.data_source || "unknown";
      const stamp = this.latest_data_time || this.generated_at || "";
      let delay = "";
      if (this.delay_hours != null) delay = "延迟 " + Number(this.delay_hours).toFixed(1) + " 小时";
      let hours = null;
      if (stamp) {
        const t = Date.parse(String(stamp).replace(" ", "T"));
        if (!Number.isNaN(t)) {
          hours = Math.max(0, (Date.now() - t) / 3600000);
          if (this.delay_hours == null) delay = hours < 1 ? "延迟 " + Math.round(hours * 60) + " 分钟" : "延迟 " + hours.toFixed(1) + " 小时";
        }
      }
      let status = this.freshness_status;
      if (src === "mock") status = "mock";
      else if (hours != null && hours > 24) status = "stale";
      const label = { ads: "ADS", mock: "mock", ok: "新鲜", stale: "陈旧" }[status] || status;
      return [src, stamp || "--", delay, label].filter(Boolean).join(" · ");
    },
    freshnessTone() {
      if ((this.data_source || this.freshness_status) === "mock") return "stale";
      const stamp = this.latest_data_time || this.generated_at || "";
      const t = Date.parse(String(stamp).replace(" ", "T"));
      if (!Number.isNaN(t) && Date.now() - t > 24 * 3600000) return "stale";
      return "ok";
    },
    averageOrderValue() { const n=Number(this.kpis.order_count||0), r=Number(this.kpis.today_revenue||0); return n ? (r/n).toFixed(2) : "--"; },
    weekdayWeekendRatio() { const w=Number(this.weekday_weekend.weekday_kwh||0), e=Number(this.weekday_weekend.weekend_kwh||0); return w+e ? (w/(w+e)*100).toFixed(1)+"% / "+(e/(w+e)*100).toFixed(1)+"%" : "--"; },
    eventTabs() { return [{key:"alerts",label:"拥堵预警",items:this.alerts},{key:"device_warnings",label:"设备告警",items:this.device_warnings},{key:"overstay_records",label:"超时占位",items:this.overstay_records},{key:"active_tickets",label:"工单",items:this.active_tickets}]; },
    activeEvents() { return (this.eventTabs.find(x=>x.key===this.eventTab)||{}).items||[]; },
  },
  watch: {
    eventTab() { this.$nextTick(() => { const el = document.querySelector('.event-body'); if (el) el.scrollTop = 0; }); },
  },
  methods: {
    targetPct(actual, target) {
      if (actual === undefined || actual === null || target === undefined || target === null || Number(target) === 0) return null;
      return (Number(actual) / Number(target)) * 100;
    },
    formatTarget(v) {
      if (v === undefined || v === null || v === "") return "--";
      return Number(v).toLocaleString("zh-CN", { maximumFractionDigits: 2 });
    },
    kpiLabel(key) {
      if (key === "today_kwh") {
        return { "1": "今日充电量 (kWh)", "7": "近7日充电量 (kWh)", "30": "近30日充电量 (kWh)" }[this.period];
      }
      if (key === "today_revenue") {
        return { "1": "今日营收 (元)", "7": "近7日营收 (元)", "30": "近30日营收 (元)" }[this.period];
      }
      return this.labels[key] || key;
    },
    kpiDelta(key) {
      const map = { today_kwh: "yesterday_kwh", today_revenue: "yesterday_revenue" };
      const prevKey = map[key];
      if (!prevKey) return "";
      const prev = this.kpis[prevKey];
      const cur = this.kpis[key];
      if (prev === undefined || prev === null || cur === undefined || cur === null) {
        return this.period === "30" ? "上期不足" : "";
      }
      const d = Number(cur) - Number(prev);
      const sign = d > 0 ? "+" : "";
      const unit = key === "today_revenue" ? "元" : "kWh";
      return (this.period === "1" ? "较昨日 " : "较上期 ") + sign + d.toFixed(1) + " " + unit;
    },
    trendData(key) {
      const previousKeys = { today_kwh: "yesterday_kwh", today_revenue: "yesterday_revenue" };
      const previousKey = previousKeys[key];
      if (!previousKey || this.period !== "1") return null;
      const currentRaw = this.kpis[key];
      const previousRaw = this.kpis[previousKey];
      const current = currentRaw === null || currentRaw === undefined || currentRaw === "" ? NaN : Number(currentRaw);
      const previous = previousRaw === null || previousRaw === undefined || previousRaw === "" ? NaN : Number(previousRaw);
      const previousLabel = key === "today_revenue"
        ? "昨日营收：" + (Number.isFinite(previous) ? "¥" + previous.toLocaleString("zh-CN", { maximumFractionDigits: 2 }) : "--")
        : "昨日充电量：" + (Number.isFinite(previous) ? previous.toLocaleString("zh-CN", { maximumFractionDigits: 2 }) + " kWh" : "--");
      if (!Number.isFinite(current) || !Number.isFinite(previous) || previous === 0)
        return { text: "--", tone: "flat", title: "较昨日 --\n" + previousLabel };
      const rate = (current - previous) / previous * 100;
      const arrow = rate > 0 ? "↑" : rate < 0 ? "↓" : "—";
      const sign = rate > 0 ? "+" : "";
      return {
        text: `${arrow} ${Math.abs(rate).toFixed(1)}%`,
        tone: rate > 0 ? "up" : rate < 0 ? "down" : "flat",
        title: `较昨日 ${sign}${rate.toFixed(1)}%\n${previousLabel}`,
      };
    },
    format(v, k) {
      if (v === undefined || v === null || v === "") return "--";
      if (k === "peak_hour") return String(v);
      if (k === "today_revenue" || k === "total_revenue") return "¥" + Number(v).toFixed(2);
      return Number(v).toLocaleString();
    },
    setPeriod(value) {
      this.period = String(value);
      this.refresh();
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
    stationPower(s) { return Number(s.power_kw ?? s.load_kw ?? s.forecast?.h1?.kwh ?? 0).toFixed(1); },
    rankedStations() {
      return (this.stations || []).slice().sort((a, b) => this.stationUtil(b) - this.stationUtil(a));
    },
    severity(a) { return String(a.severity || '').toLowerCase().includes('high') || a.severity === '红' ? 'red' : String(a.severity || '').toLowerCase().includes('medium') || a.severity === '橙' ? 'orange' : 'yellow'; },
    severityLabel(a) { return {red:'红色',orange:'橙色',yellow:'黄色'}[this.severity(a)]; },
    showStation(s) { if (!s || String(s.station_id||'').startsWith('fallback-')) return; this.selectedStation=s; this.drawer='station'; },
    showFault(f) { this.selectedFault=f; this.drawer='fault'; },
    showQuality() { this.drawer='quality'; },
    closeDrawer() { this.drawer=''; },
    display(v,unit='') { return v===undefined||v===null||v===''?'--':Number.isFinite(Number(v))?Number(v).toLocaleString('zh-CN',{maximumFractionDigits:2})+unit:String(v); },
    faultDuration(f) { const t=Date.parse(String((f||{}).fault_time||'').replace(' ','T')); if(Number.isNaN(t))return '--'; const mins=Math.max(0,Math.floor((Date.now()-t)/60000)); return mins<60?mins+' 分钟':(mins/60).toFixed(1)+' 小时'; },
    statusLabel(status) { return ({fault:'故障',idle:'空闲',busy:'在用',offline:'离线'})[status] || status || '--'; },
    timeOnly(value) { const match=String(value||'').match(/(?:T|\s)(\d{2}:\d{2})/); return match?match[1]:'--'; },
    async refresh() {
      if (this.loading) return;
      this.loading = true;
      try {
        const r = await fetch("/api/dashboard?period=" + this.period);
        if (!r.ok) throw Error("HTTP " + r.status);
        const data = await r.json();
        this.generated_at = data.generated_at || "";
        this.kpis = data.kpis || {};
        this.quality = data.quality || {};
        this.load_today = data.load_today || [];
        this.yesterday_load = data.yesterday_load || [];
        this.window_start = data.window_start || "";
        this.window_end = data.window_end || "";
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
        this.targets = data.targets || {};
        this.data_source = data.data_source || "";
        this.latest_data_time = data.latest_data_time || data.generated_at || "";
        this.freshness_status = data.freshness_status || "";
        this.delay_hours = data.delay_hours;
        this.note = data.note || "";
        this.faults = (data.faults || []).filter((f) => !f.summary);
        this.error = "";
        await nextTick();
        this.draw();
      } catch (e) {
        this.error = "数据加载失败：" + e.message;
      } finally { this.loading = false; }
    },
    draw() {
      const line = (ref, x, series) => {
        if (!this.$refs[ref]) return;
        if (this.charts[ref]) this.charts[ref].dispose();
        const c = (this.charts[ref] = echarts.init(this.$refs[ref]));
        c.setOption({
          textStyle: { color: "#7899b6" },
          tooltip: dashboardTooltip({ trigger: "axis" }),
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
      if (this.$refs.load) {
        if (this.charts.load) this.charts.load.dispose();
        const curName = { "1": "今日", "7": "近7日均", "30": "近30日均" }[this.period];
        const prevName = this.period === "1" ? "昨日" : "上一窗口";
        const series = [
          {
            name: curName,
            type: "line",
            smooth: true,
            data: this.load_today.map((x) => x.kwh),
            lineStyle: { width: 2, color: "#22d7ff" },
            areaStyle: { color: { type: "linear", x: 0, y: 0, x2: 0, y2: 1, colorStops: [{ offset: 0, color: "rgba(25,190,255,.55)" }, { offset: 1, color: "rgba(25,100,255,.02)" }] } },
            itemStyle: { color: "#42d9ff" },
          },
        ];
        if (this.yesterday_load && this.yesterday_load.length) {
          series.push({
            name: prevName,
            type: "line",
            smooth: true,
            data: this.yesterday_load.map((x) => x.kwh),
            lineStyle: { width: 2, type: "dashed", color: "#ffae45" },
            itemStyle: { color: "#ffae45" },
          });
        }
        if (this.load_hour_avg.length && this.load_hour_avg.every(x => x.hour != null && x.kwh != null)) {
          const averageByHour = new Map(this.load_hour_avg.map(x => [Number(x.hour), Number(x.kwh)]));
          series.push({ name:"历史小时均值", type:"line", smooth:true, symbol:"none", data:this.load_today.map(x=>averageByHour.get(Number(x.hour))??null), lineStyle:{width:1,type:"dashed",opacity:.32,color:"#8fb1cc"}, itemStyle:{opacity:.32,color:"#7899b6"} });
        }
        const c = (this.charts.load = echarts.init(this.$refs.load));
        c.setOption({
          textStyle: { color: "#7899b6" },
          tooltip: dashboardTooltip({ trigger: "axis" }),
          legend: { top: 0, right: 8, textStyle: { color: "#7899b6", fontSize: 11 } },
          grid: { left: 48, right: 18, top: 28, bottom: 28 },
          xAxis: { type: "category", data: this.load_today.map((x) => x.hour + "时"), axisLine: { lineStyle: { color: "#244d70" } }, axisLabel: { color: "#6688a5" } },
          yAxis: { type: "value", splitLine: { lineStyle: { color: "rgba(62,130,180,.15)" } }, axisLabel: { color: "#6688a5" } },
          series,
        });
      }
      if (this.$refs.piles) {
        if (this.charts.piles) this.charts.piles.dispose();
        const values = [Number(this.kpis.idle_piles || 0), Number(this.kpis.busy_piles || 0), Number(this.kpis.fault_piles || 0)];
        const total = values.reduce((a, b) => a + b, 0);
        const c = (this.charts.piles = echarts.init(this.$refs.piles));
        c.setOption({ tooltip: dashboardTooltip({ trigger: "item" }), legend: { bottom: 0, textStyle: { color: "#7899b6" } }, title: { text: String(total), subtext: "充电桩总数", left: "center", top: "32%", textStyle: { color: "#dcf6ff", fontSize: 24 }, subtextStyle: { color: "#6688a5" } }, series: [{ type: "pie", radius: ["55%", "75%"], center: ["50%", "45%"], label: { show: false }, data: [{ value: values[0], name: "空闲", itemStyle: { color: "#13dabb" } }, { value: values[1], name: "在用", itemStyle: { color: "#438cff" } }, { value: values[2], name: "故障", itemStyle: { color: "#ff6559" } }] }] });
      }
      if (this.$refs.forecast) {
        if (this.charts.forecast) this.charts.forecast.dispose();
        const rows = this.load_forecast_24h || [];
        const labels = rows.map((x) => "+" + (x.offset != null ? x.offset : x.hour) + "h");
        const values = rows.map((x) => Number(x.kwh));
        const flagged = rows.map((x,index) => (x.is_peak || x.peak_flag || x.warning || String(x.congestion || "").toLowerCase() === "high") ? index : -1).filter((index) => index >= 0);
        let peakIndex = flagged.reduce((best,index) => best < 0 || values[index] > values[best] ? index : best, -1);
        if (peakIndex < 0 && values.length) peakIndex = values.reduce((best, value, index) => Number.isFinite(value) && (best < 0 || value > values[best]) ? index : best, -1);
        const peak = peakIndex >= 0 ? { coord: [labels[peakIndex], values[peakIndex]], value: values[peakIndex] } : null;
        const c = (this.charts.forecast = echarts.init(this.$refs.forecast));
        c.setOption({
          textStyle: { color: "#7899b6" },
          tooltip: dashboardTooltip({ trigger: "axis", formatter: (items) => { const p=items[0], flagged=p&&p.dataIndex===peakIndex; return p ? `${p.axisValue}<br/>预测负荷：${p.value} kWh${flagged?'<br/><b style="color:#ff7568">峰值预警：是</b>':''}` : ''; } }),
          grid: { left: 48, right: 18, top: 42, bottom: 28 },
          xAxis: { type: "category", data: labels, axisLine: { lineStyle: { color: "#244d70" } }, axisLabel: { color: "#6688a5", interval: 3 } },
          yAxis: { type: "value", splitLine: { lineStyle: { color: "rgba(62,130,180,.15)" } }, axisLabel: { color: "#6688a5" } },
          series: [{ type:"line", smooth:true, data:values, symbolSize:4, lineStyle:{width:2,color:"#22d7ff"}, areaStyle:{color:{type:"linear",x:0,y:0,x2:0,y2:1,colorStops:[{offset:0,color:"rgba(25,190,255,.55)"},{offset:1,color:"rgba(25,100,255,.02)"}]}}, itemStyle:{color:"#42d9ff"}, markPoint:peak?{symbol:"circle",symbolSize:13,itemStyle:{color:"#ff6257",shadowBlur:14,shadowColor:"#ff493d"},label:{show:true,formatter:"高峰预警",position:"top",distance:7,color:"#fff",fontSize:10,padding:[3,6],backgroundColor:"rgba(207,55,48,.9)",borderRadius:3},data:[peak]}:undefined }]
        });
      }
      if (this.$refs.stations) {
        if (this.charts.stations) this.charts.stations.dispose();
        const c = (this.charts.stations = echarts.init(this.$refs.stations));
        c.setOption({
          grid: { left: 112, right: 35, top: 5, bottom: 15 },
          xAxis: { type: "value", max: 100, show: false },
          yAxis: {
            type: "category",
            data: this.rankedStations().slice(0, 8).map((x) => x.name), axisLabel: { color: "#83a7c4", width: 95, overflow: "truncate" }, axisLine: { show: false }, axisTick: { show: false }
          },
          series: [
            {
              type: "bar",
              data: this.rankedStations().slice(0, 8).map((x) => Number(this.stationUtil(x).toFixed(1))), barWidth: 8,
              itemStyle: { color: { type: "linear", x: 0, y: 0, x2: 1, y2: 0, colorStops: [{ offset: 0, color: "#1679ff" }, { offset: 1, color: "#19e0dc" }] }, borderRadius: 4 },
              label: { show: true, formatter: "{c}%" },
            },
          ],
        });
        c.on("click", (p) => { const station=this.rankedStations().slice(0,8)[p.dataIndex]; if(station)this.showStation(station); });
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
          c.setOption({ grid:{left:38,right:6,top:8,bottom:32,containLabel:true}, xAxis:{type:"category",data:xs.map(x=>x+"时"),axisLabel:{color:"#6688a5",fontSize:8,interval:(index,value)=>Number(String(value).replace('时',''))%4===0},axisTick:{show:false}}, yAxis:{type:"category",data:ys,axisLabel:{color:"#7899b6",fontSize:8},axisTick:{show:false}}, visualMap:{show:false,min:Number(contract.min_val)||0,max:Number(contract.max_val)||1,inRange:{color:["#10244a","#146fa4","#20d9d0","#ffd35a","#ff7048"]}}, series:[{type:"heatmap",data:source,itemStyle:{borderColor:"#0a1631",borderWidth:2,borderRadius:2}}]});
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
          tooltip: dashboardTooltip({ formatter: (p) => `${days[p.value[1]]} ${hours[p.value[0]]}时<br/>充电量：${p.value[2]} kWh` }),
          grid: { left: 38, right: 10, top: 8, bottom: 42 },
          xAxis: { type: "category", data: hours, axisLabel: { color: "#6688a5", fontSize: 8, interval: (index,value)=>Number(value)%4===0 }, axisLine: { lineStyle: { color: "#244d70" } } },
          yAxis: { type: "category", data: days.map((d) => dayNames[Number(d) - 1] || d), axisLabel: { color: "#7899b6" }, axisLine: { show: false }, axisTick: { show: false } },
          visualMap: { show: false, min: 0, max, inRange: { color: ["#384c9e", "#287cff", "#22dce5", "#ffe36e"] } },
          series: [{ type: "heatmap", data: points, label: { show: false }, itemStyle: { borderColor: "#081d39", borderWidth: 3, borderRadius: 2 } }],
        });
        }
      }
      if (this.$refs.mix) {
        if (this.charts.mix) this.charts.mix.dispose();
        const c = (this.charts.mix = echarts.init(this.$refs.mix));
        const data = this.pile_types.map((x) => ({ name: x.type === "直流" ? "快充（直流）" : x.type === "交流" ? "慢充（交流）" : x.type, value: Number(x.busy || 0) + Number(x.idle || 0) }));
        const total = data.reduce((sum, x) => sum + x.value, 0);
        c.setOption({ color: ["#26dded", "#568eff", "#b28cff", "#72e2c3"], tooltip: dashboardTooltip({ trigger: "item", formatter: "{b}<br/>{d}%（{c} 桩）" }), legend: { bottom: 0, left:"center", icon: "circle", itemWidth: 6, itemHeight: 6, itemGap:5, textStyle: { color: "#7899b6", fontSize: 9 } }, title: { text: String(total), subtext: "桩总数", left: "center", top: "29%", textStyle: { color: "#e7f8ff", fontSize: 17 }, subtextStyle: { color: "#7899b6", fontSize: 9 } }, series: [{ type: "pie", radius: ["43%", "64%"], center: ["52%", "42%"], startAngle: 90, avoidLabelOverlap: true, label: { show: false }, emphasis: { label: { show: true, color: "#e7f8ff", formatter: "{d}%" } }, data, itemStyle: { borderColor: "#081d39", borderWidth: 2 } }] });
      }
      if (this.$refs.regions) {
        if (this.charts.regions) this.charts.regions.dispose();
        const c = (this.charts.regions = echarts.init(this.$refs.regions));
        const rows = [...(this.regions || [])].sort((a,b)=>Number(b.kwh||0)-Number(a.kwh||0)).slice(0, 6);
        c.setOption({ grid:{left:72,right:74,top:5,bottom:8},xAxis:{type:"value",show:false},yAxis:{type:"category",inverse:true,data:rows.map(x=>x.city||x.region||"--"),axisLabel:{color:"#8fb2cc",fontSize:10,width:62,overflow:'truncate'},axisLine:{show:false},axisTick:{show:false}},tooltip:dashboardTooltip({trigger:"item",formatter:(p)=>{const r=rows[p.dataIndex]||{};return `${r.city||'--'}<br/>充电量：${this.display(r.kwh,' kWh')}<br/>营收：¥${this.display(r.amount)}<br/>收益：${this.display(r.yuan_per_kwh,' 元/kWh')}`}}),series:[{type:"bar",data:rows.map(x=>Number(x.kwh||0)),barWidth:8,itemStyle:{color:{type:'linear',x:0,y:0,x2:1,y2:0,colorStops:[{offset:0,color:'#176dca'},{offset:1,color:'#2bd5e8'}]},borderRadius:4},label:{show:true,position:'right',distance:7,color:'#ffbd65',fontSize:9,formatter:(p)=>'¥'+Number(rows[p.dataIndex]?.amount||0).toLocaleString('zh-CN',{maximumFractionDigits:0})}}]});
      }
    },
    visibleStations() {
      return this.rankedStations();
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
    enableDragScroll() {
      [this.$refs.dispatchScroll, this.$refs.faultScroll].filter(Boolean).forEach((el) => {
        let down = false, startY = 0, startTop = 0;
        el.addEventListener('mousedown', (e) => { down = true; startY = e.clientY; startTop = el.scrollTop; el.classList.add('dragging'); e.preventDefault(); });
        el.addEventListener('mousemove', (e) => { if (down) el.scrollTop = startTop - (e.clientY - startY); });
        ['mouseup','mouseleave'].forEach((name) => el.addEventListener(name, () => { down = false; el.classList.remove('dragging'); }));
      });
    },
  },
    mounted() {
    const tick = () => { this.now = new Date().toLocaleString("zh-CN", { hour12: false }); };
    tick(); this.clockTimer = setInterval(tick, 1000);
    let resizeTimer; window.addEventListener("resize", () => { clearTimeout(resizeTimer); resizeTimer = setTimeout(() => Object.values(this.charts).forEach((c) => c && c.resize()), 120); });
    this.refresh().then(() => this.$nextTick(() => { this.enableDragScroll(); const el = document.querySelector('.event-body'); if (el) el.scrollTop = 0; }));
  },
}).mount("#app");
