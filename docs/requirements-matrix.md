# 需求进度管理表

> **项目名称**：新能源汽车充电管理系统（Linux + Qt）  
> **小组人数**：5 人（组长 + A/B/C/D）  
> **大分类（三项，立项定稿）**：① 充电服务器端　② PC 服务器端　③ 数据库端  
> **编制日期**：2026-09-02  
> **评审节点**：9/4 需求分析评审 · 9/7 中期评审 · 9/10 阶段答辩 · 9/11 24:00 提交  
> **鉴权定稿（老师要求）**：TCP + 长度前缀 JSON；**除 `login` / `admin_login` 外，每个业务请求必须在 JSON 顶层携带 `token`**；服务器用 token 反查真实用户/管理员身份，**禁止客户端自报 `user_id` 充当登录身份**（同账号多端靠不同 token 区分）。

---

## 状态图例

| 符号 | 含义 |
|:----:|------|
| ○ | 完成（通过评审或测试） |
| △ | 进行中 |
| × | 未着手 |
| N/A | 不适用（没有此项活动） |

---

## 协议与 Token 约定（全组统一）

| 项 | 约定 |
|----|------|
| 传输 | TCP 长连接，帧格式 `4 字节大端长度 + UTF-8 JSON`（非 HTTP / 非 JWT） |
| 请求形态 | `{ "type":"<接口名>", "token":"<登录下发的 UUID>", "data":{ ... } }` |
| 登录例外 | `login` / `admin_login` **不带** token；成功响应在 `data.token` 下发随机会话 token |
| 身份来源 | 服务器 `SessionManager` 用 token 得到 `userId` + `role`；业务里的“当前用户”一律取自会话，**不读客户端伪造的 `data.user_id`** |
| `data.user_id` 何时可出现 | 仅管理端操作**目标用户**时（如冻结某用户）作为操作对象字段，不是登录身份 |
| 失效 | token 无效/过期返回 `code=9`（SessionInvalid），客户端清空 token 并回登录页 |
| 落地进度 | 第 1～4 步已合入 main：登录下发 token、按会话认人、分发层鉴权门、客户端 `code=9` 清 token 并回登录页 |

---

# 东软电动汽车充电桩应用管理平台需求矩阵（功能分析前三类）

> ○：完成（通过评审或测试）   △：进行中   ×：未着手   N/A：不适用（没有此项活动）

| NO. | 大分类 | 中分类 | 小分类 | 详细说明 | 负责人 | 预计日期 | 状态 | 困难 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 充电用户端 | 附近充电站查询 | 支持通过下拉选择区域或手动输入地址确定用户位置。 | 实现流程：在首页放置区域下拉框、地址输入框和“定位”按钮；下拉框加载预设区域，手动输入时先去除首尾空格并检查地址非空。将用户选择结果保存到 LocationModel，点击定位后发出 locationChanged 信号，供后续地理编码和附近电站查询统一使用。软件仅模拟 GPS，不读取真实定位权限；重新进入页面时从 QSettings 恢复最近一次地址。 | 马晓钰 | 2026-09-02 | △ |  |
| 2 |  |  | 调用腾讯地图 Web API 将输入地址转换为经纬度坐标。 | 使用 QNetworkAccessManager 调用腾讯地图地址解析接口，请求参数至少包含 address、region、key 和 output=json；为每次请求生成 requestId，并设置 5 秒超时。解析返回 JSON 中的 status、message、result.location.lat/lng，成功后保存 double 类型经纬度；地址无结果、配额受限、网络超时或 JSON 字段缺失时显示明确提示并允许重试，禁止把无效坐标继续发送给服务器。 | 马晓钰 | 2026-09-03 | △ |  |
| 3 |  |  | 向 PC 服务器请求当前位置附近的 5/10 个最近充电站。 | 客户端经已登录的 NetClient 发送 {type:'station_list', token, data:{lat, lng}}（token 由 NetClient 自动附带，禁止漏带）。服务器先校验 token 有效且角色为 user，再校验经纬度范围，查询启用电站并按距离排序返回 id、name、address、price、total、idle、distance 等；token 无效返回 code=9。 | 朱雅琪 | 2026-09-04 | ○ |  |
| 4 |  |  | 按距离由近及远展示充电站，并支持 5 条/10 条结果切换。 | 收到服务器结果后先校验 stationId 唯一、distance 为非负数，再按 distance 升序稳定排序；用户切换 5 条或 10 条时重新发送查询并保留当前地址。若服务器未返回 distance，客户端可用两点经纬度按 Haversine 公式兜底计算，统一保留 1 位小数；距离相同时按空闲桩数降序、站名升序排列，保证结果稳定。 | 翟梓涵 | 2026-09-04 | △ |  |
| 5 |  |  | 充电站卡片展示站名、充电价格、总桩数、空闲数和距离。 | 用 QScrollArea + 自定义 StationCardWidget 生成卡片列表，每张卡片绑定 stationId，不以界面序号作为业务主键。字段分别显示站名、价格（元/度，保留 2 位）、“空闲数/总数”和距离（km）；空闲数为 0 时卡片显示“已满”并弱化操作按钮，价格或数量缺失时显示“--”而不是崩溃。列表刷新前复用/销毁旧卡片，避免重复信号连接和内存泄漏。 | 马晓钰 | 2026-09-04 | ○ |  |
| 6 |  |  | 点击充电站可查看该站全部电桩的编号、类型、状态和功率。 | 点击卡片后携带 station_id 进入详情页并发送 {type:'pile_list', token, data:{station_id}}。服务器校验 token 后按 station_id 查询电桩，返回 id、code、type、power_kw、status；客户端表格/卡片展示，状态映射为闲置/在用/故障并用颜色区分，只有闲置桩显示“选择充电”。电站不存在或无电桩时返回业务错误码并展示空状态。 | 马晓钰 | 2026-09-05 | ○ |  |
| 7 |  |  | 查询过程提供加载、空数据、超时和断网处理。 | 页面用 QStackedWidget 管理 loading/content/empty/error 四种状态，发送请求后立即显示加载状态并暂时禁用重复提交。5 秒无响应则取消等待并提示重试；Socket 断开时统一交给 ConnectionManager 自动重连，最多重试 3 次且采用递增间隔。可缓存最近一次成功列表用于只读展示，但必须标注“缓存数据”，充电操作前仍需重新校验桩状态。 | 邓雅心 | 2026-09-05 | ○ |  |
| 8 |  | 一键导航 | 点击充电站距离信息后进入该站导航页面。 | 将距离 QLabel 设置为可点击控件或在卡片上提供“导航”按钮，点击时从 StationModel 读取目标 stationId、站名和经纬度，并从 LocationModel 读取当前位置。先检查起点与终点坐标完整性，防止只传显示文本；确认后切换至 NavigationPage，并在页头展示目标站名、起终点摘要和返回按钮。 | 马晓钰 | 2026-09-04 | × |  |
| 9 |  |  | 使用 QWebEngineView 加载腾讯地图路线规划页面。 | 在 NavigationPage 中嵌入 QWebEngineView，启用必要的 JavaScript 和本地存储，禁止不需要的弹窗与文件下载。首次进入显示占位页，拼接完成路线 URL 后调用 setUrl；连接 loadStarted、loadProgress、loadFinished 信号显示加载进度。地图 API Key 从配置文件或环境配置读取，不硬编码在源文件提交记录中。 | 马晓钰 | 2026-09-05 | × |  |
| 10 |  |  | 导航请求同时传入当前位置起点和目标电站终点。 | 构造路线参数时使用纬度、经度的固定顺序，起点包含当前位置名称与坐标，终点包含电站名称与坐标；对名称进行 URL 编码并将坐标保留 6 位小数。导航请求格式统一封装为 RouteRequest{fromName,fromLat,fromLng,toName,toLat,toLng,mode}，在发起加载前记录 requestId，避免快速切换电站时旧页面覆盖新目标。 | 朱雅琪 | 2026-09-05 | × |  |
| 11 |  |  | 支持驾车和步行两种出行方式切换。 | 导航页提供“驾车/步行”切换按钮或 QComboBox，默认驾车；选择变化后只更新 RouteRequest.mode，并重新加载对应路线，保留起终点不变。界面上突出当前模式，加载期间禁用连续切换；若腾讯地图不支持当前路线或返回无可用路径，则显示原因并允许返回电站详情页。 | 马晓钰 | 2026-09-05 | × |  |
| 12 |  |  | 导航加载失败时提供重试、外部浏览器打开和返回操作。 | 监听 QWebEngineView::loadFinished(false)、网络错误和证书异常，失败时显示错误面板而非空白页面；“重试”使用同一 RouteRequest 再次加载，“外部打开”经用户确认后用 QDesktopServices 打开路线 URL，“返回”恢复原电站列表滚动位置。记录失败时间、目标 stationId 和错误码，便于联调排查，但日志中不写入 API Key。 | 翟梓涵 | 2026-09-06 | × |  |
| 13 |  | 用户信息维护 | 用户输入 11 位手机号即可发起免密登录。 | 登录页使用 QLineEdit 限制仅输入数字、最大长度 11，并用正则 ^1\\d{10}$ 做提交校验；不合法时在输入框下显示原因。合法后发送 {type:'login', data:{phone}}（**登录请求不带 token**），提交期间禁用按钮。手机号仅用于本实训模拟登录，界面明确提示不发送验证码；日志对中间四位脱敏。 | 马晓钰 | 2026-09-02 | ○ |  |
| 14 |  |  | 服务器查询手机号并对已存在的正常用户直接完成登录。 | 服务器对 phone 做格式校验，参数化查询 user 表。存在且 status='normal' 时由 SessionManager 生成随机 UUID token（写入内存会话表，含 userId、role=user、lastActive），响应 data 返回 id、phone、nickname、avatar、balance、**token**；冻结用户返回 code=6，不创建会话。同一账号多次登录可持有多个 token（区分并发会话）。禁止用客户端传来的 user_id 当作已登录身份。 | 朱雅琪 | 2026-09-03 | ○ |  |
| 15 |  |  | 手机号不存在时自动创建新用户并立即登录。 | 事务内检查手机号唯一性后插入 user（默认昵称“用户”+后4位、balance=0、status=normal），再创建会话并返回与普通登录相同结构（含 token）。唯一键冲突则回退为查询已有用户并登录发 token。 | 朱雅琪 | 2026-09-03 | ○ |  |
| 16 |  |  | 登录成功后展示默认头像、昵称和钱包余额。 | 登录成功后客户端调用 NetClient::setToken 保存 data.token，后续所有业务请求自动在顶层附带 token。界面展示昵称、余额等；退出登录时 clearToken、清空页面缓存并回登录页。收到 code=9 时同样清 token 并强制重新登录。 | 马晓钰 | 2026-09-03 | ○ |  |
| 17 |  |  | 用户可从本地选择图片并更换头像。 | 通过 QFileDialog 选择 jpg/png，校验大小与格式后发送 {type:'update_profile', token, data:{avatar}}（**data 不含 user_id**）。服务器用 token 反查 userId 再写库；成功刷新头像，失败保留旧图；token 无效返回 code=9。 | 马晓钰 | 2026-09-04 | × |  |
| 18 |  |  | 用户可编辑并保存新的昵称。 | 昵称限制 2～20 可见字符；发送 {type:'update_profile', token, data:{nickname}}。服务器根据 token 取 userId，参数化 UPDATE，禁止信任客户端自报的 user_id；客户端仅在成功后更新界面，并处理 code=9。 | 翟梓涵 | 2026-09-04 | × |  |
| 19 |  |  | 用户输入充值金额后模拟支付并实时更新钱包余额。 | 金额校验后发送 {type:'recharge', token, data:{amount}}（**不传 user_id**）。服务器用 token 确定入账用户，事务内写 wallet_transactions 并增加 balance；成功返回新余额，客户端刷新展示。 | 翟梓涵 | 2026-09-05 | ○ |  |
| 20 |  | 电动汽车充电 | 进入充电页面前自动检查当前用户是否有未完成订单。 | 发送 {type:'unfinished_order', token, data:{}}（身份仅来自 token）。服务器查该会话用户 status IN ('reserved','charging','pending_payment') 的最新订单；存在则弹窗并进入充电/结算页。未登录或 token 失效禁止继续选桩。 | 翟梓涵 | 2026-09-05 | △ |  |
| 21 |  |  | 用户只能选择状态为闲置的电桩并确认预约。 | 仅闲置桩可点；确认发送 {type:'reserve', token, data:{pile_id}}（**不传 user_id**）。服务器校验 token 后再预约；成功跳转充电页，重复点击由按钮锁定防抖。 | 马晓钰 | 2026-09-05 | ○ |  |
| 22 |  |  | 服务器原子化锁定空闲电桩并创建预约订单。 | 分发层用 token 得到 sess.userId 后开启事务：条件更新 pile 为 busy/reserved，插入 charge_order（user_id 取自会话而非报文）。同时检查该用户无未完成订单；失败整单回滚。客户端伪造 data.user_id 无效。 | 朱雅琪 | 2026-09-06 | ○ |  |
| 23 |  |  | 用户确认开始后由服务器向电桩模拟程序发送启动指令。 | 客户端发送 {type:'start_charge', token, data:{order_no}}。服务器校验 token，并校验订单属于 sess.userId 且状态为 reserved，再启动充电并改状态；归属不符或 token 无效则拒绝。 | 朱雅琪 | 2026-09-06 | △ |  |
| 24 |  |  | 充电页面实时显示时长、功率、已充电量和预估金额。 | 本地 QTimer 刷新时长，定时发送带 token 的状态/业务查询（如 settle 前的展示数据）；界面只读展示。网络中断保留最后值并标注重连；连续失败停止本地金额增长。 | 马晓钰 | 2026-09-06 | △ |  |
| 25 |  |  | 停止充电时按充电量和电站单价计算最终费用。 | 服务器以订单固化单价与上报电量计算 amount；结算接口须带 token，并校验订单归属 sess.userId。响应返回电量、单价、时长、金额供客户端核对。 | 翟梓涵 | 2026-09-07 | △ |  |
| 26 |  |  | 完成扣款、订单结算和电桩释放，并处理余额不足。 | 客户端发送 {type:'settle', token, data:{order_no,kwh}}。服务器事务内按会话用户扣款、写流水、释放电桩；余额不足置 pending_payment。禁止仅凭 order_no 操作他人订单。 | 朱雅琪 | 2026-09-07 | △ |  |
| 27 | PC服务器端 | 管理员登录 | 管理员在登录界面输入账号和密码并提交验证。 | 账号密码输入；发送 {type:'admin_login', data:{username,password}}（**不带 token**）。成功后 NetClient::setToken(data.token)，进入管理后台；退出 clearToken。密码不写客户端日志。 | 牛昀轶 | 2026-09-02 | ○ |  |
| 28 |  |  | 服务器校验数据库管理员账号并兼容默认账号 admin/123456。 | admin 表存 password_hash+salt；登录校验成功后更新 last_login_at，返回 id、username，并由 SessionManager 创建 role=admin 的 token 写入 data.token。账号停用或密码错误统一 AUTH_FAILED。默认 admin/123456 仅作初始化哈希入库，库中不保留明文。 | 朱雅琪 | 2026-09-03 | ○ |  |
| 29 |  |  | 登录成功后建立管理员会话并控制后台操作权限。 | SessionManager 保存 token→(adminId, role=admin, lastActive)；**除 admin_login 外，所有 admin_* 请求必须带 token**，分发层校验 token 且 role 为 admin 后才进业务。30 分钟无操作滑动过期。写操作（重启、冻结、新增电站）须鉴权通过并写 operation_logs。服务器鉴权门已开闸；管理端收到 code=9 会清 token 并回到登录窗。 | 朱雅琪 | 2026-09-03 | ○ |  |
| 30 |  | 销售业绩 | 管理端可切换查看近 7 日和近 30 日营收数据。 | 切换 7/30 日后发送 {type:'revenue_trend_query', token, data:{days:7\|30}}（须管理员 token）。横轴按自然日补 0；切换模块返回时保持原时间维度。 | 牛昀轶 | × |  |  |
| 31 |  |  | 服务器汇总已结算订单生成趋势和营收指标。 | 须管理员 token；只统计已结算订单按日汇总营收，返回趋势序列及今日/本月/历史总营收，金额保留 2 位。 | 朱雅琪 | 2026-09-05 | × |  |
| 32 |  |  | 页面展示今日营收、本月营收和总营收三项核心指标。 | 使用三个统一样式的 KPI 卡片绑定 todayRevenue、monthRevenue、totalRevenue，按人民币格式显示并使用千分位；无数据时显示 ¥0.00。刷新趋势时三项指标同步更新，响应 requestId 不匹配时丢弃旧数据；点击刷新按钮可重新请求，失败时保留旧值并标注最后成功更新时间。 | 牛昀轶 | 2026-09-05 | × |  |
| 33 |  |  | 使用 Qt Charts 绘制营收变化趋势折线图。 | 使用 QChart、QLineSeries 和 QDateTimeAxis/QValueAxis 构建折线图，横轴按日期、纵轴从 0 起并根据最大营收留出余量；切换数据时清空旧 series 后一次性填充，避免重复叠线。为数据点提供悬停提示“日期 + 金额”，30 日模式适当减少横轴标签密度；窗口缩放时图表自适应，异常值和空数据不导致坐标轴无效。 | 牛昀轶 | 2026-09-06 | × |  |
| 34 |  | 电桩状态 | 以表格展示全部电桩当前的在用、闲置和故障状态。 | 进入页面后请求 {type:'admin_pile_list', token, data:{}}（管理员 token）。QTableView 展示编号、所属站、类型、功率、状态等；状态映射为中文；支持筛选排序。无 token 或非 admin 返回 code=9。 | 牛昀轶 | 2026-09-04 | △ |  |
| 35 |  |  | 服务器统计各状态电桩的数量和占比。 | 执行 SELECT status,COUNT(*) FROM piles WHERE enabled=1 GROUP BY status 获取数量，total 为各状态之和；在用可包含 charging，闲置对应 idle，故障对应 fault，预约中若单独存在则在接口中明确返回。占比按 count/total×100 计算并保留 1 位，total=0 时全部为 0；响应同时返回统计时间，便于判断数据新鲜度。 | 朱雅琪 | 2026-09-05 | × |  |
| 36 |  |  | 状态页自动刷新并用数量、占比和颜色反映设备健康度。 | 用 QTimer 每 10 秒触发一次状态汇总请求，上一请求未完成时跳过本次，避免堆积；页面顶部显示各状态数量和占比，表格中的闲置/在用/故障分别使用绿色/蓝色/红色标签。刷新时按 pileId 更新模型而非重建整个窗口，保留筛选条件和滚动位置；连接断开时暂停定时器并显示最后更新时间。 | 牛昀轶 | 2026-09-06 | × |  |
| 37 |  | 充电桩管理 | 列表展示电桩编号、所属电站、类型、功率、状态及累计使用数据。 | 使用 QTableView 显示 pileCode、stationName、chargeType、powerKw、status、chargeCount、chargeDurationHours；累计次数只统计已开始的有效订单，累计时长由已结束订单 duration_seconds 求和并换算小时。提供电站、类型、状态筛选和编号搜索，列标题固定、长文本悬停显示完整值，双击可查看详情。 | 牛昀轶 | 2026-09-05 | ○ |  |
| 38 |  |  | 服务器支持电桩列表的联表查询、筛选和分页。 | 接口 {type:'admin_pile_list', token, data:{...筛选可选}}：先校验管理员 token，再联表查询并聚合累计次数/时长；筛选用绑定参数。无 token 拒绝。 | 朱雅琪 | 2026-09-06 | △ |  |
| 39 |  |  | 管理员可选择电桩并发起远程重启操作。 | 选中行后确认发送 {type:'admin_pile_restart', token, data:{pile_id}}。成功刷新该行状态；充电中默认禁止重启。 | 牛昀轶 | 2026-09-06 | ○ |  |
| 40 |  |  | 服务器向电桩模拟程序发送重启指令并记录执行结果。 | 校验管理员 token 与电桩状态后写 device_commands，发送 restart；结果写 operation_logs（adminId 取自会话，非客户端自报）。 | 翟梓涵 | 2026-09-07 | △ |  |
| 41 |  | 充电站管理 | 列表展示电站 ID、站名、地址、经纬度、总桩数和在线率。 | 加载时发送 {type:'admin_station_list', token, data:{}}；QTableView 展示站名、地址、坐标、总桩数、在线率等。须管理员 token。 | 牛昀轶 | 2026-09-05 | ○ |  |
| 42 |  |  | 点击电站行可查看站内全部电桩的实时状态明细。 | 双击或点击“详情”后以 stationId 请求站内电桩，服务器返回 pileCode、类型、功率、状态、当前订单号、最后心跳时间。详情页每 10 秒刷新，使用 pileId 对现有模型做增量更新；若电站已被删除返回 STATION_NOT_FOUND。页面保留返回入口，并可从某一电桩继续跳转到电桩管理模块。 | 翟梓涵 | 2026-09-06 | × |  |
| 43 |  |  | 管理员可填写站名、地址、经纬度和电桩数量新增电站。 | 对话框校验字段后发送 {type:'admin_station_add', token, data:{name,address,longitude,latitude,price,...}}；须管理员 token。成功关闭窗口并刷新列表。 | 牛昀轶 | 2026-09-06 | × |  |
| 44 |  |  | 服务器在一个事务中创建电站及其初始电桩。 | 校验管理员 token 与字段后，事务内插入 station 及初始 piles；失败回滚；成功写 operation_logs（操作者 adminId 来自会话）。 | 翟梓涵 | 2026-09-07 | × |  |
| 45 |  | 用户管理 | 列表展示用户 ID、手机号、昵称、余额、注册时间和账号状态。 | 进入页面发送 {type:'admin_user_list', token, data:{keyword?}}；表格展示脱敏手机号、昵称、余额、状态等。须管理员 token。 | 牛昀轶 | 2026-09-06 | ○ |  |
| 46 |  |  | 服务器按手机号进行参数化模糊搜索并分页返回用户。 | 接口 admin_user_list：先校验管理员 token，再参数化 LIKE 搜索；无匹配返回空数组。界面只展示脱敏号码。 | 翟梓涵 | 2026-09-07 | △ |  |
| 47 |  |  | 管理员可对选中的用户执行冻结或解冻。 | 确认后发送 {type:'admin_user_freeze', token, data:{user_id,frozen:bool}}。此处 data.user_id 是**操作对象**（被冻结用户），操作者身份仍来自 token，二者不可混淆。成功只刷新对应行。 | 牛昀轶 | △ |  |  |
| 48 |  |  | 冻结状态在登录和充电业务中即时生效并记录审计日志。 | 更新 user.status 并写 operation_logs；冻结后 SessionManager::revokeByUser 使该用户全部 token 立即失效。后续登录返回冻结；预约/开始充电也校验状态。正在充电不强制断电，但禁止新订单。 | 翟梓涵 | 2026-09-08 | △ |  |
| 49 | 数据库端 | 数据库连接与初始化 | 设计并创建覆盖用户、电站、电桩、订单、管理员和流水的关系模型。 | 先绘制 ER 关系：users 1:N orders，stations 1:N piles，piles 1:N orders，users 1:N wallet_transactions，admins 1:N operation_logs。编写版本化 schema.sql（MySQL/InnoDB）创建 users、stations、piles、orders、admins、wallet_transactions、device_commands、operation_logs，统一使用 BIGINT AUTO_INCREMENT 主键、DATETIME 时间戳、DECIMAL(10,2) 金额，字符集 utf8mb4；配置主键、外键、UNIQUE、NOT NULL、CHECK/ENUM 约束及常用索引，并写 schema_version 便于升级。 | 翟梓涵 | 2026-09-01 | ○ |  |
| 50 |  |  | 封装 MySQL 连接、事务和参数化查询的统一访问层。 | 使用 QSqlDatabase(QMYSQL 驱动) 建立命名连接，每个服务器工作线程创建并只使用自己的连接，程序退出前按正确顺序关闭并 removeDatabase。连接时统一使用 InnoDB 引擎（支持外键与事务）、utf8mb4 字符集，并按需设置 innodb_lock_wait_timeout；DAO 层统一使用 QSqlQuery::prepare/bindValue，返回业务对象或明确错误，不把 SQL 散落在界面代码中。启动时检测 schema_version，缺表则按脚本初始化。 | 邓雅心 | 2026-09-02 | ○ |  |
| 51 |  | 用户与账户数据管理 | 用户表保存手机号、昵称、头像、余额、状态和时间信息。 | users 字段建议为 id、phone、nickname、avatar_path、balance_cents、status、created_at、updated_at、last_login_at；phone 设置 UNIQUE NOT NULL，balance_cents 默认 0 且 CHECK>=0，status 限定 normal/frozen。为 phone 建唯一索引、created_at 和 status 建查询索引；新增、修改昵称、头像、冻结和登录更新时间均通过 UserDao 完成，并用单元测试验证重复手机号和负余额会被拒绝。 | 邓雅心 | 2026-09-03 | ○ |  |
| 52 |  |  | 钱包流水记录每次充值和充电扣款并可追溯余额变化。 | wallet_transactions 包含 id、transaction_no、user_id、type(recharge/charge_pay/refund)、amount_cents、balance_before、balance_after、order_id、created_at；transaction_no UNIQUE，user_id 和 order_id 建索引。所有余额变化必须先写/同步写流水并与 users.balance_cents 处于同一事务，禁止只改余额；查询时按 created_at 倒序分页，可通过前后余额校验账目连续性。 | 邓雅心 | 2026-09-04 | ○ |  |
| 53 |  | 电站与电桩数据管理 | 充电站表保存位置、坐标、电价和启用状态。 | stations 字段建议为 id、station_code、name、address、latitude、longitude、price_cents_per_kwh、enabled、created_at、updated_at；station_code UNIQUE，坐标设置范围 CHECK，电价必须大于 0。为 enabled、name 建索引，附近查询读取所有启用站点后按距离排序；新增电站使用 StationDao 参数化插入，更新位置或价格时同步 updated_at，不物理删除已有订单引用的站点。 | 邓雅心 | 2026-09-03 | ○ |  |
| 54 |  |  | 电桩表保存所属电站、类型、功率、状态和在线信息。 | piles 字段建议为 id、pile_code、station_id、charge_type(fast/slow)、power_kw、status(idle/reserved/charging/fault/offline)、enabled、current_user_id、last_online_at；pile_code UNIQUE，station_id 外键引用 stations。为 station_id+status 建复合索引支持空闲数统计，状态更新统一通过条件 UPDATE 实现并发控制；设备心跳更新 last_online_at，超过阈值由服务层判定 offline。 | 邓雅心 | 2026-09-04 | ○ |  |
| 55 |  | 订单与交易数据管理 | 充电订单表完整记录预约、充电、计费和结算状态。 | orders 包含 id/order_no、user_id、station_id、pile_id、status、unit_price_cents、start_time、end_time、duration_seconds、energy_wh、amount_cents、pay_request_id、created_at、updated_at；order_no 与 pay_request_id（非空时）唯一，三个外键均建索引。状态仅允许 reserved→charging→pending_payment/paid/cancelled 的合法转换，服务层先校验旧状态再条件更新，防止重复开始或重复结算。 | 邓雅心 | 2026-09-05 | ○ |  |
| 56 |  |  | 为未完成订单、营收趋势和设备累计数据提供高效查询。 | 建立 orders(user_id,status)、orders(end_time,status)、orders(pile_id,start_time) 索引；封装查询：按用户找未完成订单、按日期汇总 paid 金额、按电桩统计 chargeCount 和 duration。可创建只读视图 v_station_summary 与 v_pile_usage 简化管理端联表，但业务写入仍通过 DAO；使用 EXPLAIN QUERY PLAN 检查常用查询命中索引，并准备空数据、跨月和大量订单测试数据。 | 邓雅心 | 2026-09-06 | △ |  |
| 57 |  | 管理员与运维日志管理 | 管理员表安全保存登录凭据、角色、状态和最后登录时间。 | admins 包含 id、username、password_hash、salt、role、status、last_login_at、created_at、updated_at；username UNIQUE，禁止保存明文密码。初始化脚本仅在表为空时创建 admin，密码 123456 经随机盐和安全哈希后入库；修改密码时重新生成盐。AdminDao 只返回验证所需字段，普通列表接口不返回 password_hash/salt，登录失败不写敏感内容。 | 邓雅心 | 2026-09-04 | ○ |  |
| 58 |  |  | 记录远程重启、用户冻结和电站新增等关键操作及设备指令。 | operation_logs 保存 admin_id、action、target_type、target_id、before_value、after_value、result、reason、created_at；device_commands 保存 command_no、pile_id、command、status、request_at、response_at、error_code。command_no UNIQUE，按 admin_id/created_at、pile_id/status 建索引。日志只追加不在界面提供修改，详细值采用 JSON 文本并过滤密码、token、API Key 等敏感字段。 | 邓雅心 | 2026-09-06 | × |  |
| 59 |  | 数据安全、并发与备份 | 使用事务、外键、幂等键与 token 会话保证并发与身份安全。 | 预约/充值/结算用显式事务与条件 UPDATE；身份一律由 token→SessionManager 解析，拒绝客户端伪造 user_id。启用 InnoDB 外键；锁等待/死锁有限重试。并发测：双预约仅一人成功；伪造他人 user_id 不能操作其钱包/订单。 | 邓雅心 | 2026-09-07 | △ |  |
| 60 |  |  | 实现数据库备份、恢复验证、权限控制和异常审计。 | 每天收尾或发布前使用 mysqldump 生成带日期的备份文件，备份前后执行 CHECK TABLE 校验关键表，并在临时库中恢复抽查关键表数量；仅保留最近若干版本并记录备份日志。数据库文件和头像目录设置为应用账号可读写、其他用户不可写，配置/API Key 与数据库分离；捕获查询和事务异常时记录错误码、模块和 requestId，不记录密码或完整 token，并准备恢复演练步骤。 | 邓雅心 | 2026-09-09 | × |  |
