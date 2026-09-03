# 需求进度管理表

> **项目名称**：新能源汽车充电管理系统（Linux + Qt）  
> **小组人数**：5 人（组长 + A/B/C/D）  
> **大分类（三项，立项定稿）**：① 充电服务器端　② PC 服务器端　③ 数据库端  
> **编制日期**：2026-09-02  
> **评审节点**：9/4 需求分析评审 · 9/7 中期评审 · 9/10 阶段答辩 · 9/11 24:00 提交

---

## 状态图例

| 符号 | 含义 |
|:----:|------|
| ○ | 完成（通过评审或测试） |
| △ | 进行中 |
| × | 未着手 |
| N/A | 不适用（没有此项活动） |

---

# 东软电动汽车充电桩应用管理平台需求矩阵（功能分析前三类）

> ○：完成（通过评审或测试）   △：进行中   ×：未着手   N/A：不适用（没有此项活动）

| NO. | 大分类 | 中分类 | 小分类 | 详细说明 | 负责人 | 预计日期 | 状态 | 困难 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 充电用户端 | 附近充电站查询 | 支持通过下拉选择区域或手动输入地址确定用户位置。 | 实现流程：在首页放置区域下拉框、地址输入框和“定位”按钮；下拉框加载预设区域，手动输入时先去除首尾空格并检查地址非空。将用户选择结果保存到 LocationModel，点击定位后发出 locationChanged 信号，供后续地理编码和附近电站查询统一使用。软件仅模拟 GPS，不读取真实定位权限；重新进入页面时从 QSettings 恢复最近一次地址。 | 马晓钰 | 2026-09-02 | × |  |
| 2 |  |  | 调用腾讯地图 Web API 将输入地址转换为经纬度坐标。 | 使用 QNetworkAccessManager 调用腾讯地图地址解析接口，请求参数至少包含 address、region、key 和 output=json；为每次请求生成 requestId，并设置 5 秒超时。解析返回 JSON 中的 status、message、result.location.lat/lng，成功后保存 double 类型经纬度；地址无结果、配额受限、网络超时或 JSON 字段缺失时显示明确提示并允许重试，禁止把无效坐标继续发送给服务器。 | 马晓钰 | 2026-09-03 | × |  |
| 3 |  |  | 向 PC 服务器请求当前位置附近的 5/10 个最近充电站。 | 客户端通过 QTcpSocket 发送长度前缀 + UTF-8 JSON 消息，格式为 {type:'station_nearby_query', requestId, token, data:{latitude, longitude, limit}}，limit 只允许 5 或 10。服务器校验坐标范围与登录令牌后查询启用状态的电站，联表统计总桩数和空闲桩数，计算距离并返回 stationId、name、address、price、totalCount、idleCount、distance、latitude、longitude；响应携带 code、message、requestId，便于客户端匹配异步请求。 | 朱雅琪 | 2026-09-04 | △ |  |
| 4 |  |  | 按距离由近及远展示充电站，并支持 5 条/10 条结果切换。 | 收到服务器结果后先校验 stationId 唯一、distance 为非负数，再按 distance 升序稳定排序；用户切换 5 条或 10 条时重新发送查询并保留当前地址。若服务器未返回 distance，客户端可用两点经纬度按 Haversine 公式兜底计算，统一保留 1 位小数；距离相同时按空闲桩数降序、站名升序排列，保证结果稳定。 | 翟梓涵 | 2026-09-04 | × |  |
| 5 |  |  | 充电站卡片展示站名、充电价格、总桩数、空闲数和距离。 | 用 QScrollArea + 自定义 StationCardWidget 生成卡片列表，每张卡片绑定 stationId，不以界面序号作为业务主键。字段分别显示站名、价格（元/度，保留 2 位）、“空闲数/总数”和距离（km）；空闲数为 0 时卡片显示“已满”并弱化操作按钮，价格或数量缺失时显示“--”而不是崩溃。列表刷新前复用/销毁旧卡片，避免重复信号连接和内存泄漏。 | 马晓钰 | 2026-09-04 | △ |  |
| 6 |  |  | 点击充电站可查看该站全部电桩的编号、类型、状态和功率。 | 点击卡片后携带 stationId 进入详情页并发送 {type:'pile_list_query', data:{stationId}}。服务器按 station_id 查询电桩，返回 pileId、pileCode、chargeType、status、powerKw；客户端使用表格或卡片展示，状态映射为闲置/在用/故障/预约中并用颜色区分，只有闲置桩显示“选择充电”按钮。若电站已删除或无电桩，返回业务错误码并展示空状态页。 | 马晓钰 | 2026-09-05 | ○ |  |
| 7 |  |  | 查询过程提供加载、空数据、超时和断网处理。 | 页面用 QStackedWidget 管理 loading/content/empty/error 四种状态，发送请求后立即显示加载状态并暂时禁用重复提交。5 秒无响应则取消等待并提示重试；Socket 断开时统一交给 ConnectionManager 自动重连，最多重试 3 次且采用递增间隔。可缓存最近一次成功列表用于只读展示，但必须标注“缓存数据”，充电操作前仍需重新校验桩状态。 | 邓雅心 | 2026-09-05 | △ |  |
| 8 |  | 一键导航 | 点击充电站距离信息后进入该站导航页面。 | 将距离 QLabel 设置为可点击控件或在卡片上提供“导航”按钮，点击时从 StationModel 读取目标 stationId、站名和经纬度，并从 LocationModel 读取当前位置。先检查起点与终点坐标完整性，防止只传显示文本；确认后切换至 NavigationPage，并在页头展示目标站名、起终点摘要和返回按钮。 | 马晓钰 | 2026-09-04 | × |  |
| 9 |  |  | 使用 QWebEngineView 加载腾讯地图路线规划页面。 | 在 NavigationPage 中嵌入 QWebEngineView，启用必要的 JavaScript 和本地存储，禁止不需要的弹窗与文件下载。首次进入显示占位页，拼接完成路线 URL 后调用 setUrl；连接 loadStarted、loadProgress、loadFinished 信号显示加载进度。地图 API Key 从配置文件或环境配置读取，不硬编码在源文件提交记录中。 | 马晓钰 | 2026-09-05 | × |  |
| 10 |  |  | 导航请求同时传入当前位置起点和目标电站终点。 | 构造路线参数时使用纬度、经度的固定顺序，起点包含当前位置名称与坐标，终点包含电站名称与坐标；对名称进行 URL 编码并将坐标保留 6 位小数。导航请求格式统一封装为 RouteRequest{fromName,fromLat,fromLng,toName,toLat,toLng,mode}，在发起加载前记录 requestId，避免快速切换电站时旧页面覆盖新目标。 | 朱雅琪 | 2026-09-05 | × |  |
| 11 |  |  | 支持驾车和步行两种出行方式切换。 | 导航页提供“驾车/步行”切换按钮或 QComboBox，默认驾车；选择变化后只更新 RouteRequest.mode，并重新加载对应路线，保留起终点不变。界面上突出当前模式，加载期间禁用连续切换；若腾讯地图不支持当前路线或返回无可用路径，则显示原因并允许返回电站详情页。 | 马晓钰 | 2026-09-05 | × |  |
| 12 |  |  | 导航加载失败时提供重试、外部浏览器打开和返回操作。 | 监听 QWebEngineView::loadFinished(false)、网络错误和证书异常，失败时显示错误面板而非空白页面；“重试”使用同一 RouteRequest 再次加载，“外部打开”经用户确认后用 QDesktopServices 打开路线 URL，“返回”恢复原电站列表滚动位置。记录失败时间、目标 stationId 和错误码，便于联调排查，但日志中不写入 API Key。 | 翟梓涵 | 2026-09-06 | × |  |
| 13 |  | 用户信息维护 | 用户输入 11 位手机号即可发起免密登录。 | 登录页使用 QLineEdit 限制仅输入数字、最大长度 11，并用正则 ^1\\d{10}$ 做提交校验；不合法时在输入框下显示原因。合法后发送 {type:'user_login', requestId, data:{phone}}，提交期间禁用按钮以防重复请求。手机号仅用于本实训模拟登录，界面明确提示不发送验证码；日志展示时对中间四位脱敏。 | 马晓钰 | 2026-09-02 | ○ |  |
| 14 |  |  | 服务器查询手机号并对已存在的正常用户直接完成登录。 | 服务器对 phone 再次做格式校验，使用参数化 SQL SELECT id,nickname,balance,avatar_path,status FROM users WHERE phone=? 查询，禁止字符串拼接。存在且 status='normal' 时生成随机会话 token，返回 userId、昵称、余额和头像地址；冻结用户返回 USER_FROZEN，不创建会话。登录成功写入 last_login_at，并将 token 与 userId、到期时间保存在会话管理器中。 | 朱雅琪 | 2026-09-03 | ○ |  |
| 15 |  |  | 手机号不存在时自动创建新用户并立即登录。 | 在数据库事务中再次检查手机号唯一性，未存在则插入 users：phone、nickname='用户'+手机号后4位、balance=0、status='normal'、created_at=当前时间、avatar_path=默认头像；依赖 phone UNIQUE 约束避免并发重复注册。插入成功后读取自增 userId、创建会话并返回与普通登录相同的数据结构；若唯一键冲突则回退为查询已有用户。 | 朱雅琪 | 2026-09-03 | ○ |  |
| 16 |  |  | 登录成功后展示默认头像、昵称和钱包余额。 | 用户中心收到登录响应后写入 UserSession，并按 userId 请求最新资料；头像文件不存在时加载资源文件中的灰色默认头像，昵称按原文显示，余额按“¥0.00”格式化。页面每次获得焦点时刷新余额，避免充值或结算后仍显示旧值；退出登录时清空 token、页面缓存和敏感输入并返回登录页。 | 马晓钰 | 2026-09-03 | △ |  |
| 17 |  |  | 用户可从本地选择图片并更换头像。 | 通过 QFileDialog 选择 jpg/png，读取前检查文件存在、大小不超过 2 MB，并用 QImage 验证确为图片；客户端裁剪为正方形并缩放至 256×256。将图片按 userId + 时间戳命名，以分块或 Base64 方式发送 {type:'avatar_update'}；服务器保存到受控目录并只把相对路径写入 users，更新成功后客户端刷新头像，失败则保留旧图。 | 马晓钰 | 2026-09-04 | × |  |
| 18 |  |  | 用户可编辑并保存新的昵称。 | 昵称编辑框限制 2～20 个可见字符，提交前去除首尾空格并拒绝全空白、控制字符；发送 {type:'nickname_update', token, data:{nickname}}。服务器根据 token 获取 userId，参数化执行 UPDATE users SET nickname=?,updated_at=? WHERE id=?，返回受影响行数和最终昵称；客户端仅在成功响应后更新 UserSession，并处理会话失效和数据库失败。 | 翟梓涵 | 2026-09-04 | × |  |
| 19 |  |  | 用户输入充值金额后模拟支付并实时更新钱包余额。 | 充值框使用 QDoubleValidator，金额必须大于 0、最多 2 位小数且单次不超过 10000 元；用户二次确认后发送唯一 transactionNo。服务器开启事务，先向 wallet_transactions 写入 recharge 记录，再执行 balance=balance+amount，transactionNo 设 UNIQUE 保证重试不重复入账；成功返回新余额和流水号，客户端更新余额并展示结果，失败回滚。 | 翟梓涵 | 2026-09-05 | × |  |
| 20 |  | 电动汽车充电 | 进入充电页面前自动检查当前用户是否有未完成订单。 | 页面切换前发送 {type:'order_unfinished_query', token}，服务器查询该用户 status IN ('reserved','charging','pending_payment') 的最新订单并返回 orderId、pileId、status、startTime、energy。存在未完成订单时弹窗提示并强制进入订单/结算页；查询完成前不允许选择新桩。若网络失败，按安全原则阻止新建订单并提示重新连接，避免产生重复充电。 | 翟梓涵 | 2026-09-05 | × |  |
| 21 |  |  | 用户只能选择状态为闲置的电桩并确认预约。 | 电桩详情页根据实时 status 控制按钮，故障、在用、预约中的桩不可点击；选择闲置桩后展示电站、桩编号、快/慢充、功率和单价确认框。确认发送 {type:'pile_reserve', token, data:{pileId}}，等待服务器成功响应后再跳转充电页；取消不产生订单，重复点击由 requestId 和按钮锁定共同防抖。 | 马晓钰 | 2026-09-05 | × |  |
| 22 |  |  | 服务器原子化锁定空闲电桩并创建预约订单。 | 服务器开启事务并执行条件更新 UPDATE piles SET status='reserved',current_user_id=? WHERE id=? AND status='idle'；受影响行数为 1 才插入 orders(status='reserved',user_id,pile_id,station_id,unit_price,created_at)。若为 0 返回 PILE_NOT_IDLE，并让客户端刷新列表；同时再次检查用户无未完成订单。提交后返回 orderId，任何一步失败均回滚，避免一桩多约和孤立订单。 | 朱雅琪 | 2026-09-06 | × |  |
| 23 |  |  | 用户确认开始后由服务器向电桩模拟程序发送启动指令。 | 客户端发送 {type:'charge_start', token, data:{orderId}}；服务器校验订单归属和 reserved 状态后，通过设备连接线程发送 {type:'device_command', command:'start', pileId, orderId}。收到电桩 ACK 后在同一事务中把订单改为 charging、记录 start_time，并把电桩改为 charging；10 秒无 ACK 标记命令超时、释放预约并返回失败，防止界面误显示已启动。 | 朱雅琪 | 2026-09-06 | × |  |
| 24 |  |  | 充电页面实时显示时长、功率、已充电量和预估金额。 | 充电页使用 1 秒 QTimer 更新本地时长，并每 3 秒发送 {type:'charge_status_query', data:{orderId}} 获取服务器权威数据：pileStatus、powerKw、energyKwh、startTime、estimatedAmount。界面用只读字段和进度区域显示，网络短暂中断时保留最后值并标注“正在重连”；连续失败超过阈值后停止本地金额增长，避免把估算值当作最终账单。 | 马晓钰 | 2026-09-06 | × |  |
| 25 |  |  | 停止充电时按充电量和电站单价计算最终费用。 | 服务器以电桩最终上报 energy_kwh 为准，读取订单创建时固化的 unit_price，计算 amount=round(energy_kwh×unit_price,2)，避免电站改价影响进行中订单；同时记录 end_time、duration_seconds。金额计算统一用分或定点小数，禁止直接用二进制浮点累计；响应中返回电量、单价、时长、金额供客户端逐项核对。 | 翟梓涵 | 2026-09-07 | × |  |
| 26 |  |  | 完成扣款、订单结算和电桩释放，并处理余额不足。 | 结算请求包含 orderId 和唯一 payRequestId。服务器事务内锁定/读取用户余额和订单，若余额足够则扣减 balance、写入 wallet_transactions(type='charge_pay')、更新订单为 paid，并将电桩恢复 idle；payRequestId 设唯一保证重复请求幂等。余额不足则订单置 pending_payment、电桩仍可释放但用户被禁止新建订单；客户端引导充值后再次结算，成功进入订单详情页。 | 朱雅琪 | 2026-09-07 | × |  |
| 27 | PC服务器端 | 管理员登录 | 管理员在登录界面输入账号和密码并提交验证。 | 使用 QLineEdit 构建账号、密码输入，密码设置 Password 回显模式；账号去除空格后不能为空，密码不在客户端日志中输出。点击登录发送 {type:'admin_login', requestId, data:{username,password}}，按钮在响应前禁用，并对连续失败给出统一提示，避免泄露账号是否存在；成功后进入宽屏管理后台，退出时清理会话。 | 牛昀轶 | 2026-09-02 | ○ |  |
| 28 |  |  | 服务器校验数据库管理员账号并兼容默认账号 admin/123456。 | 初始化数据库时若 admins 表为空，创建默认 admin，并保存加盐哈希而非明文；登录时按 username 参数化查询 password_hash、salt、status，用同一算法计算后恒定时间比较。验证成功更新 last_login_at 并返回管理员基本信息；账号停用或密码错误均返回 AUTH_FAILED。首次登录可提示修改默认密码，数据库中不得长期保留 123456 明文。 | 朱雅琪 | 2026-09-03 | ○ |  |
| 29 |  |  | 登录成功后建立管理员会话并控制后台操作权限。 | AuthService 生成高随机 token，记录 adminId、role、createdAt、expireAt；所有管理请求统一在分发层验证 token 和角色后再进入业务处理。30 分钟无操作可过期，客户端收到 TOKEN_EXPIRED 时返回登录页；远程重启、冻结用户、新增电站等写操作必须再次校验权限并写操作日志，避免仅靠按钮隐藏实现授权。 | 朱雅琪 | 2026-09-03 | × |  |
| 30 |  | 销售业绩 | 管理端可切换查看近 7 日和近 30 日营收数据。 | 页面提供 7 日/30 日切换按钮，默认近 7 日；切换后发送 {type:'revenue_trend_query', token, data:{days:7\|30}}，显示加载状态并取消过期请求。横轴按自然日从早到晚排列，即使某天无订单也补 0；当前选择写入页面状态，切换到其他模块再返回时保持原时间维度。 | 牛昀轶 | × |  |  |
| 31 |  |  | 服务器汇总已结算订单生成趋势和营收指标。 | 只统计 orders.status='paid'，按 date(end_time,'localtime') 分组汇总 amount；请求 7/30 日时生成完整日期序列并左连接汇总结果，返回 [{date,revenue}]。同一接口额外计算今日、本月、历史总营收，全部用 COALESCE(SUM(amount),0)；响应金额保留 2 位并带统计起止时间，确保管理端与数据库口径一致。 | 朱雅琪 | 2026-09-05 | × |  |
| 32 |  |  | 页面展示今日营收、本月营收和总营收三项核心指标。 | 使用三个统一样式的 KPI 卡片绑定 todayRevenue、monthRevenue、totalRevenue，按人民币格式显示并使用千分位；无数据时显示 ¥0.00。刷新趋势时三项指标同步更新，响应 requestId 不匹配时丢弃旧数据；点击刷新按钮可重新请求，失败时保留旧值并标注最后成功更新时间。 | 牛昀轶 | 2026-09-05 | × |  |
| 33 |  |  | 使用 Qt Charts 绘制营收变化趋势折线图。 | 使用 QChart、QLineSeries 和 QDateTimeAxis/QValueAxis 构建折线图，横轴按日期、纵轴从 0 起并根据最大营收留出余量；切换数据时清空旧 series 后一次性填充，避免重复叠线。为数据点提供悬停提示“日期 + 金额”，30 日模式适当减少横轴标签密度；窗口缩放时图表自适应，异常值和空数据不导致坐标轴无效。 | 牛昀轶 | 2026-09-06 | × |  |
| 34 |  | 电桩状态 | 以表格展示全部电桩当前的在用、闲置和故障状态。 | 后台进入电桩状态页后请求 {type:'pile_status_list', token}，使用 QTableView + QAbstractTableModel 展示电桩编号、所属站、类型、功率、状态和最近更新时间。状态由枚举值映射为中文，不直接显示数据库代码；表格支持按状态筛选和按更新时间排序，选中行可跳转到电桩管理详情。 | 牛昀轶 | 2026-09-04 | △ |  |
| 35 |  |  | 服务器统计各状态电桩的数量和占比。 | 执行 SELECT status,COUNT(*) FROM piles WHERE enabled=1 GROUP BY status 获取数量，total 为各状态之和；在用可包含 charging，闲置对应 idle，故障对应 fault，预约中若单独存在则在接口中明确返回。占比按 count/total×100 计算并保留 1 位，total=0 时全部为 0；响应同时返回统计时间，便于判断数据新鲜度。 | 朱雅琪 | 2026-09-05 | × |  |
| 36 |  |  | 状态页自动刷新并用数量、占比和颜色反映设备健康度。 | 用 QTimer 每 10 秒触发一次状态汇总请求，上一请求未完成时跳过本次，避免堆积；页面顶部显示各状态数量和占比，表格中的闲置/在用/故障分别使用绿色/蓝色/红色标签。刷新时按 pileId 更新模型而非重建整个窗口，保留筛选条件和滚动位置；连接断开时暂停定时器并显示最后更新时间。 | 牛昀轶 | 2026-09-06 | × |  |
| 37 |  | 充电桩管理 | 列表展示电桩编号、所属电站、类型、功率、状态及累计使用数据。 | 使用 QTableView 显示 pileCode、stationName、chargeType、powerKw、status、chargeCount、chargeDurationHours；累计次数只统计已开始的有效订单，累计时长由已结束订单 duration_seconds 求和并换算小时。提供电站、类型、状态筛选和编号搜索，列标题固定、长文本悬停显示完整值，双击可查看详情。 | 牛昀轶 | 2026-09-05 | ○ |  |
| 38 |  |  | 服务器支持电桩列表的联表查询、筛选和分页。 | 接口 {type:'pile_manage_query', data:{stationId,type,status,keyword,page,pageSize}} 校验 pageSize 上限；SQL 联结 piles、stations，并以子查询聚合 orders 的次数与时长。所有筛选值使用绑定参数，排序字段只从白名单选择；返回 total、page、pageSize、items，客户端翻页时携带同一筛选条件。为 station_id、status、pile_code 建索引保证查询性能。 | 朱雅琪 | 2026-09-06 | △ |  |
| 39 |  |  | 管理员可选择电桩并发起远程重启操作。 | 仅当选中一行时启用“远程重启”，点击后弹出确认框，显示电桩编号、当前状态和影响提示；确认发送 {type:'pile_restart', token, data:{pileId}}。按钮进入执行中状态并等待结果，成功后刷新该行状态，失败显示可理解的原因；对 charging 状态默认禁止重启，除非业务规则明确允许。 | 牛昀轶 | 2026-09-06 | ○ |  |
| 40 |  |  | 服务器向电桩模拟程序发送重启指令并记录执行结果。 | 服务器校验管理员权限、电桩存在且非充电中，创建 device_commands 记录(status='pending')，再由设备通信线程发送 restart 指令。收到 ACK 后更新命令为 success、记录响应时间，并刷新电桩 last_online_at；超时或断线改为 failed 并保存错误码。无论成功失败都写 operation_logs(adminId,action,targetId,result)，并通过原 requestId 返回管理端。 | 翟梓涵 | 2026-09-07 | △ |  |
| 41 |  | 充电站管理 | 列表展示电站 ID、站名、地址、经纬度、总桩数和在线率。 | 管理页加载后请求 station_manage_query，QTableView 展示 stationId、name、address、latitude、longitude、pileCount、onlineRate。在线率定义为最近心跳正常且非离线电桩数/总桩数，保留 1 位；总桩数为 0 时显示 0%。支持按站名/地址搜索及在线率排序，坐标保留 6 位以便核查地图定位。 | 牛昀轶 | 2026-09-05 | ○ |  |
| 42 |  |  | 点击电站行可查看站内全部电桩的实时状态明细。 | 双击或点击“详情”后以 stationId 请求站内电桩，服务器返回 pileCode、类型、功率、状态、当前订单号、最后心跳时间。详情页每 10 秒刷新，使用 pileId 对现有模型做增量更新；若电站已被删除返回 STATION_NOT_FOUND。页面保留返回入口，并可从某一电桩继续跳转到电桩管理模块。 | 翟梓涵 | 2026-09-06 | × |  |
| 43 |  |  | 管理员可填写站名、地址、经纬度和电桩数量新增电站。 | 新增对话框包含站名、详细地址、经纬度、默认电价、电桩数量；站名/地址必填，经度范围 -180～180、纬度 -90～90、电价大于 0、电桩数量为 1～100。地址可调用地理编码自动回填坐标，管理员仍可校正；提交前显示字段级错误并二次确认，发送 create_station 请求，成功后关闭窗口并刷新列表。 | 牛昀轶 | 2026-09-06 | × |  |
| 44 |  |  | 服务器在一个事务中创建电站及其初始电桩。 | 服务器校验字段并生成唯一 station_code，事务内先插入 stations，再按 pileCount 批量插入 piles，电桩编号采用站点编码+三位序号，默认 status='idle'、enabled=1；快充/慢充类型和功率可使用界面输入或系统默认模板。任一电桩插入失败则整体回滚，成功返回 stationId 和电桩编号列表，并写管理员新增操作日志。 | 翟梓涵 | 2026-09-07 | × |  |
| 45 |  | 用户管理 | 列表展示用户 ID、手机号、昵称、余额、注册时间和账号状态。 | 使用 QTableView 展示 userId、脱敏手机号、nickname、balance、createdAt、status；余额显示 2 位小数，注册时间按本地时间格式化，冻结状态使用醒目标识。搜索框输入手机号后 300 ms 防抖再请求，清空时恢复全部数据；支持分页和按注册时间排序，双击行可查看该用户订单与钱包流水摘要。 | 牛昀轶 | 2026-09-06 | ○ |  |
| 46 |  |  | 服务器按手机号进行参数化模糊搜索并分页返回用户。 | 接口 user_manage_query 接收 phoneKeyword、status、page、pageSize，先移除非数字字符并限制关键字长度；SQL 使用 WHERE phone LIKE ? ESCAPE '\\' 且绑定 '%关键字%'，禁止直接拼接。分别查询总数和当前页数据，返回 total/items；没有匹配时返回空数组而不是错误。管理员界面仅展示脱敏号码，确需完整号码的操作应单独授权。 | 翟梓涵 | 2026-09-07 | △ |  |
| 47 |  |  | 管理员可对选中的用户执行冻结或解冻。 | 根据当前 status 动态显示“冻结账号”或“解除冻结”，点击后弹出含用户昵称和脱敏手机号的确认框；发送 {type:'user_status_update', token, data:{userId,status:'frozen'\|'normal',reason}}。操作期间锁定按钮，成功后只刷新对应行；禁止冻结不存在用户，并要求冻结原因非空，便于审计。 | 牛昀轶 | ○ |  |  |
| 48 |  |  | 冻结状态在登录和充电业务中即时生效并记录审计日志。 | 服务器事务更新 users.status 和 updated_at，写 operation_logs 记录 adminId、userId、前后状态、原因。冻结后使该用户现有 token 失效，后续登录返回 USER_FROZEN；预约和开始充电接口也必须查询/缓存校验状态，不能只在登录时判断。若用户正在充电，不强制断电，但禁止新订单并允许完成当前订单结算；解冻后恢复正常访问。 | 翟梓涵 | 2026-09-08 | △ |  |
| 49 | 数据库端 | 数据库连接与初始化 | 设计并创建覆盖用户、电站、电桩、订单、管理员和流水的关系模型。 | 先绘制 ER 关系：users 1:N orders，stations 1:N piles，piles 1:N orders，users 1:N wallet_transactions，admins 1:N operation_logs。编写版本化 schema.sql（MySQL/InnoDB）创建 users、stations、piles、orders、admins、wallet_transactions、device_commands、operation_logs，统一使用 BIGINT AUTO_INCREMENT 主键、DATETIME 时间戳、DECIMAL(10,2) 金额，字符集 utf8mb4；配置主键、外键、UNIQUE、NOT NULL、CHECK/ENUM 约束及常用索引，并写 schema_version 便于升级。 | 翟梓涵 | 2026-09-01 | △ |  |
| 50 |  |  | 封装 MySQL 连接、事务和参数化查询的统一访问层。 | 使用 QSqlDatabase(QMYSQL 驱动) 建立命名连接，每个服务器工作线程创建并只使用自己的连接，程序退出前按正确顺序关闭并 removeDatabase。连接时统一使用 InnoDB 引擎（支持外键与事务）、utf8mb4 字符集，并按需设置 innodb_lock_wait_timeout；DAO 层统一使用 QSqlQuery::prepare/bindValue，返回业务对象或明确错误，不把 SQL 散落在界面代码中。启动时检测 schema_version，缺表则按脚本初始化。 | 邓雅心 | 2026-09-02 | ○ |  |
| 51 |  | 用户与账户数据管理 | 用户表保存手机号、昵称、头像、余额、状态和时间信息。 | users 字段建议为 id、phone、nickname、avatar_path、balance_cents、status、created_at、updated_at、last_login_at；phone 设置 UNIQUE NOT NULL，balance_cents 默认 0 且 CHECK>=0，status 限定 normal/frozen。为 phone 建唯一索引、created_at 和 status 建查询索引；新增、修改昵称、头像、冻结和登录更新时间均通过 UserDao 完成，并用单元测试验证重复手机号和负余额会被拒绝。 | 邓雅心 | 2026-09-03 | ○ |  |
| 52 |  |  | 钱包流水记录每次充值和充电扣款并可追溯余额变化。 | wallet_transactions 包含 id、transaction_no、user_id、type(recharge/charge_pay/refund)、amount_cents、balance_before、balance_after、order_id、created_at；transaction_no UNIQUE，user_id 和 order_id 建索引。所有余额变化必须先写/同步写流水并与 users.balance_cents 处于同一事务，禁止只改余额；查询时按 created_at 倒序分页，可通过前后余额校验账目连续性。 | 邓雅心 | 2026-09-04 | △ |  |
| 53 |  | 电站与电桩数据管理 | 充电站表保存位置、坐标、电价和启用状态。 | stations 字段建议为 id、station_code、name、address、latitude、longitude、price_cents_per_kwh、enabled、created_at、updated_at；station_code UNIQUE，坐标设置范围 CHECK，电价必须大于 0。为 enabled、name 建索引，附近查询读取所有启用站点后按距离排序；新增电站使用 StationDao 参数化插入，更新位置或价格时同步 updated_at，不物理删除已有订单引用的站点。 | 邓雅心 | 2026-09-03 | ○ |  |
| 54 |  |  | 电桩表保存所属电站、类型、功率、状态和在线信息。 | piles 字段建议为 id、pile_code、station_id、charge_type(fast/slow)、power_kw、status(idle/reserved/charging/fault/offline)、enabled、current_user_id、last_online_at；pile_code UNIQUE，station_id 外键引用 stations。为 station_id+status 建复合索引支持空闲数统计，状态更新统一通过条件 UPDATE 实现并发控制；设备心跳更新 last_online_at，超过阈值由服务层判定 offline。 | 邓雅心 | 2026-09-04 | ○ |  |
| 55 |  | 订单与交易数据管理 | 充电订单表完整记录预约、充电、计费和结算状态。 | orders 包含 id/order_no、user_id、station_id、pile_id、status、unit_price_cents、start_time、end_time、duration_seconds、energy_wh、amount_cents、pay_request_id、created_at、updated_at；order_no 与 pay_request_id（非空时）唯一，三个外键均建索引。状态仅允许 reserved→charging→pending_payment/paid/cancelled 的合法转换，服务层先校验旧状态再条件更新，防止重复开始或重复结算。 | 邓雅心 | 2026-09-05 | ○ |  |
| 56 |  |  | 为未完成订单、营收趋势和设备累计数据提供高效查询。 | 建立 orders(user_id,status)、orders(end_time,status)、orders(pile_id,start_time) 索引；封装查询：按用户找未完成订单、按日期汇总 paid 金额、按电桩统计 chargeCount 和 duration。可创建只读视图 v_station_summary 与 v_pile_usage 简化管理端联表，但业务写入仍通过 DAO；使用 EXPLAIN QUERY PLAN 检查常用查询命中索引，并准备空数据、跨月和大量订单测试数据。 | 邓雅心 | 2026-09-06 | × |  |
| 57 |  | 管理员与运维日志管理 | 管理员表安全保存登录凭据、角色、状态和最后登录时间。 | admins 包含 id、username、password_hash、salt、role、status、last_login_at、created_at、updated_at；username UNIQUE，禁止保存明文密码。初始化脚本仅在表为空时创建 admin，密码 123456 经随机盐和安全哈希后入库；修改密码时重新生成盐。AdminDao 只返回验证所需字段，普通列表接口不返回 password_hash/salt，登录失败不写敏感内容。 | 邓雅心 | 2026-09-04 | △ |  |
| 58 |  |  | 记录远程重启、用户冻结和电站新增等关键操作及设备指令。 | operation_logs 保存 admin_id、action、target_type、target_id、before_value、after_value、result、reason、created_at；device_commands 保存 command_no、pile_id、command、status、request_at、response_at、error_code。command_no UNIQUE，按 admin_id/created_at、pile_id/status 建索引。日志只追加不在界面提供修改，详细值采用 JSON 文本并过滤密码、token、API Key 等敏感字段。 | 邓雅心 | 2026-09-06 | × |  |
| 59 |  | 数据安全、并发与备份 | 使用事务、外键和幂等键保证并发预约、充值与结算的数据一致性。 | 预约、自动注册、充值、结算、新增电站均使用显式事务；通过条件 UPDATE 的受影响行数解决一桩多约，通过 phone/transaction_no/pay_request_id 唯一约束处理重复请求。启用 InnoDB 外键并为引用关系设置合理的 RESTRICT/SET NULL 行为，不对历史订单做级联删除；捕获锁等待超时（innodb_lock_wait_timeout）或死锁时有限重试，失败返回统一错误码。编写并发测试验证双预约只有一个成功、重复充值只入账一次。 | 邓雅心 | 2026-09-07 | △ |  |
| 60 |  |  | 实现数据库备份、恢复验证、权限控制和异常审计。 | 每天收尾或发布前使用 mysqldump 生成带日期的备份文件，备份前后执行 CHECK TABLE 校验关键表，并在临时库中恢复抽查关键表数量；仅保留最近若干版本并记录备份日志。数据库文件和头像目录设置为应用账号可读写、其他用户不可写，配置/API Key 与数据库分离；捕获查询和事务异常时记录错误码、模块和 requestId，不记录密码或完整 token，并准备恢复演练步骤。 | 邓雅心 | 2026-09-09 | × |  |
