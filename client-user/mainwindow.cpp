#include "mainwindow.h"

#include "aichatdialog.h"
#include "appmessagebox.h"
#include "chargepage.h"
#include "floatingball.h"
#include "locationmanager.h"
#include "loginwindow.h"
#include "mainwindowstyle.h"
#include "navigationpage.h"
#include "netclient.h"
#include "pilelistpage.h"
#include "profilepage.h"
#include "protocol.h"
#include "stationlistpage.h"
#include "uitheme.h"
#include "windowhelper.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QSettings>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>


// ============================================================================
// 服务器地址
// ============================================================================
static constexpr const char *kServerHost =
    "127.0.0.1";

static constexpr quint16 kServerPort =
    9000;


// ============================================================================
// 构造函数
// ============================================================================
MainWindow::MainWindow(
    qint64 userId,
    const QString &nickname,
    const QString &phone,
    double balance,
    const QString &token,
    QWidget *parent)
    : QWidget(parent)
    , m_net(new NetClient(this))
    , m_contentStack(new QStackedWidget)
    , m_homeStack(new QStackedWidget)
    , m_navGroup(new QButtonGroup(this))
    , m_stationPage(new StationListPage(m_net))
    , m_pilePage(new PileListPage(m_net))
    , m_navigationPage(new NavigationPage)
    , m_chargePage(new ChargePage)
    , m_profilePage(new ProfilePage)
    , m_locationManager(new LocationManager(this))
    , m_regionCombo(nullptr)
    , m_addressEdit(nullptr)
    , m_locationBtn(nullptr)
    , m_locationTip(nullptr)
    , m_nickname(nickname)
    , m_phone(phone)
    , m_balance(balance)
    , m_userId(userId)
{
    setObjectName(
        QStringLiteral(
            "userMainWindow"));

    setWindowTitle(
        QStringLiteral(
            "充电用户端"));


    // 响应式窗口：
    // 按屏幕分辨率自适应手机比例并居中
    applyPhoneWindow(
        this);


    // =========================================================================
    // Stack objectName
    // =========================================================================
    m_contentStack->setObjectName(
        QStringLiteral(
            "contentStack"));

    m_homeStack->setObjectName(
        QStringLiteral(
            "homeStack"));


    // =========================================================================
    // 连接业务服务器
    // =========================================================================
    if (!m_net->isConnected()) {

        m_net->connectToServer(
            kServerHost,
            kServerPort);
    }


    // =========================================================================
    // 必须在任何业务请求之前写入 token
    //
    // 否则构造函数里的 unfinished_order 会因无 token 收到 code=9，
    // 被误判为「登录已失效」。
    // =========================================================================
    m_net->setToken(
        token);


    connect(
        m_net,
        &NetClient::sessionInvalid,
        this,
        &MainWindow::onSessionInvalid);


    // =========================================================================
    // NO.24：
    // 充电过程中网络断开 / 重连
    // =========================================================================
    connect(
        m_net,
        &NetClient::disconnected,
        m_chargePage,
        &ChargePage::handleNetworkDisconnected);


    connect(
        m_net,
        &NetClient::reconnected,
        m_chargePage,
        &ChargePage::handleNetworkReconnected);


    // =========================================================================
    // 首页子栈：
    // 充电站列表 ⇄ 桩列表 ⇄ 路线规划
    // =========================================================================
    m_homeStack->addWidget(
        m_stationPage);

    m_homeStack->addWidget(
        m_pilePage);

    m_homeStack->addWidget(
        m_navigationPage);


    // 把当前登录用户 ID 交给桩列表页
    // 预约 reserve 时需要 user_id
    m_pilePage->setUserId(
        m_userId);


    // =========================================================================
    // 首页容器
    // =========================================================================
    auto *homePage =
        new QWidget(this);

    homePage->setObjectName(
        QStringLiteral(
            "homePage"));


    auto *homeLayout =
        new QVBoxLayout(
            homePage);

    homeLayout->setContentsMargins(
        0,
        8,
        0,
        0);

    homeLayout->setSpacing(
        6);


    // =========================================================================
    // 地址定位区域
    // =========================================================================
    auto *locationPanel =
        new QFrame(
            homePage);

    locationPanel->setObjectName(
        QStringLiteral(
            "locationPanel"));

    locationPanel->setAttribute(
        Qt::WA_StyledBackground,
        true);


    UiTheme::applyCardShadow(
        locationPanel,
        24,
        5);


    auto *locationMainLayout =
        new QVBoxLayout(
            locationPanel);

    locationMainLayout->setObjectName(
        QStringLiteral(
            "locationMainLayout"));

    locationMainLayout->setContentsMargins(
        12,
        6,
        12,
        6);

    locationMainLayout->setSpacing(
        0);


    // =========================================================================
    // 第一行：
    // 城市 + 地址 + 定位
    // =========================================================================
    auto *locationRow =
        new QHBoxLayout;

    locationRow->setSpacing(
        8);


    // -------------------------------------------------------------------------
    // 城市选择
    // -------------------------------------------------------------------------
    m_regionCombo =
        new QComboBox(
            locationPanel);

    m_regionCombo->setObjectName(
        QStringLiteral(
            "regionCombo"));


    // 保留原有城市和顺序
    m_regionCombo->addItem(
        QStringLiteral(
            "北京市"));

    m_regionCombo->addItem(
        QStringLiteral(
            "上海市"));

    m_regionCombo->addItem(
        QStringLiteral(
            "广州市"));

    m_regionCombo->addItem(
        QStringLiteral(
            "深圳市"));

    m_regionCombo->addItem(
        QStringLiteral(
            "杭州市"));

    m_regionCombo->addItem(
        QStringLiteral(
            "南京市"));

    m_regionCombo->setMinimumWidth(
        85);


    // -------------------------------------------------------------------------
    // 地址输入
    // -------------------------------------------------------------------------
    m_addressEdit =
        new QLineEdit(
            locationPanel);

    m_addressEdit->setObjectName(
        QStringLiteral(
            "addressEdit"));

    m_addressEdit->setPlaceholderText(
        QStringLiteral(
            "请输入详细地址"));


    // 搜索图标
    m_addressEdit->addAction(
        QIcon(
            QStringLiteral(
                ":/icons/search.svg")),
        QLineEdit::LeadingPosition);


    // -------------------------------------------------------------------------
    // 定位按钮
    // -------------------------------------------------------------------------
    m_locationBtn =
        new QPushButton(
            QStringLiteral(
                "定位"),
            locationPanel);

    m_locationBtn->setObjectName(
        QStringLiteral(
            "locationButton"));

    m_locationBtn->setCursor(
        Qt::PointingHandCursor);


    locationRow->addWidget(
        m_regionCombo);

    locationRow->addWidget(
        m_addressEdit,
        1);

    locationRow->addWidget(
        m_locationBtn);


    // -------------------------------------------------------------------------
    // 定位状态提示
    // -------------------------------------------------------------------------
    m_locationTip =
        new QLabel(
            QStringLiteral(
                "请输入当前位置"),
            locationPanel);

    m_locationTip->setObjectName(
        QStringLiteral(
            "locationTip"));

    m_locationTip->hide();


    // =========================================================================
    // NO.1：
    // 恢复最近一次成功定位
    // =========================================================================
    {
        QSettings settings(
            QStringLiteral(
                "ChargingSystem"),
            QStringLiteral(
                "ChargingUser"));


        const QString savedRegion =
            settings.value(
                        QStringLiteral(
                            "location/region"))
                .toString()
                .trimmed();


        const QString savedAddress =
            settings.value(
                        QStringLiteral(
                            "location/address"))
                .toString()
                .trimmed();


        if (!savedRegion.isEmpty()) {

            const int index =
                m_regionCombo->findText(
                    savedRegion);


            if (index >= 0) {

                m_regionCombo->setCurrentIndex(
                    index);
            }
        }


        if (!savedAddress.isEmpty()) {

            m_addressEdit->setText(
                savedAddress);
        }


        bool latOk =
            false;

        bool lngOk =
            false;


        const double savedLat =
            settings.value(
                        QStringLiteral(
                            "location/lat"))
                .toDouble(
                    &latOk);


        const double savedLng =
            settings.value(
                        QStringLiteral(
                            "location/lng"))
                .toDouble(
                    &lngOk);


        if (latOk &&
            lngOk &&
            savedLat >= -90.0 &&
            savedLat <= 90.0 &&
            savedLng >= -180.0 &&
            savedLng <= 180.0) {

            // 恢复最近一次有效坐标。
            // StationListPage 自己会在显示时加载附近站点。
            m_stationPage->setLocation(
                savedLat,
                savedLng);


            m_locationTip->setText(
                QStringLiteral(
                    "已恢复上次定位"));

        } else if (
            !savedAddress.isEmpty()) {

            m_locationTip->setText(
                QStringLiteral(
                    "已恢复上次地址，请点击定位"));
        }
    }


    locationMainLayout->addLayout(
        locationRow);

    locationMainLayout->addWidget(
        m_locationTip);


    // 定位卡保留页面留白，下面的站点列表则铺满内容宽度。
    auto *locationWrapper =
        new QWidget(homePage);

    auto *locationWrapperLayout =
        new QHBoxLayout(locationWrapper);

    locationWrapperLayout->setContentsMargins(
        14,
        0,
        14,
        0);

    locationWrapperLayout->addWidget(
        locationPanel);

    homeLayout->addWidget(
        locationWrapper);

    homeLayout->addWidget(
        m_homeStack,
        1);


    // =========================================================================
    // 内容区
    // =========================================================================
    m_contentStack->addWidget(
        homePage);             // 0 首页

    m_contentStack->addWidget(
        m_chargePage);         // 1 充电

    m_contentStack->addWidget(
        m_profilePage);        // 2 我的


    // =========================================================================
    // NO.17：
    // 告诉 ProfilePage 当前登录的是哪个用户
    //
    // 用于按 userId 分开保存本地头像
    // =========================================================================
    m_profilePage->setUserId(
        m_userId);


    // =========================================================================
    // 登录用户信息传给“我的”
    // =========================================================================
    m_profilePage->setUserInfo(
        m_nickname,
        m_phone,
        m_balance);


    // =========================================================================
    // 底部导航
    // =========================================================================
    auto *navBar =
        new QFrame(this);

    navBar->setObjectName(
        QStringLiteral(
            "navBar"));

    navBar->setAttribute(
        Qt::WA_StyledBackground,
        true);


    auto *navLayout =
        new QHBoxLayout(
            navBar);

    navLayout->setContentsMargins(
        12,
        7,
        12,
        7);

    navLayout->setSpacing(
        4);


    struct NavItem
    {
        QString name;
        int index;
    };


    const NavItem items[] = {

        {
            QStringLiteral(
                "首页"),
            0
        },

        {
            QStringLiteral(
                "充电"),
            1
        },

        {
            QStringLiteral(
                "我的"),
            2
        }
    };


    for (const auto &item : items) {

        auto *btn =
            new QToolButton(
                navBar);


        btn->setText(
            item.name);


        // 参考移动端导航栏：图标固定在文字上方。
        // 仍加入原来的按钮组，页面索引和跳转逻辑完全不变。
        btn->setToolButtonStyle(
            Qt::ToolButtonTextUnderIcon);


        btn->setObjectName(
            QStringLiteral(
                "navBtn"));


        btn->setCheckable(
            true);


        btn->setCursor(
            Qt::PointingHandCursor);


        btn->setSizePolicy(
            QSizePolicy::Expanding,
            QSizePolicy::Preferred);


        navLayout->addWidget(
            btn,
            1);


        m_navGroup->addButton(
            btn,
            item.index);
    }


    m_navGroup->setExclusive(
        true);


    if (m_navGroup->button(0)) {

        m_navGroup->button(0)
            ->setChecked(
                true);
    }


    // 初始：首页绿色图标
    MainWindowStyle::updateNavIcons(
        m_navGroup,
        0,
        24);


    // =========================================================================
    // 点击底部导航
    // =========================================================================
    connect(
        m_navGroup,
        &QButtonGroup::idClicked,
        this,
        [this](int id) {

            m_contentStack
                ->setCurrentIndex(
                    id);
        });


    // =========================================================================
    // 页面变化时同步底栏图标
    //
    // 这样预约成功自动跳充电页、
    // 余额不足跳我的页面时，
    // 底栏图标也会自动同步。
    // =========================================================================
    connect(
        m_contentStack,
        &QStackedWidget::currentChanged,
        this,
        [this](int index) {

            if (index < 0 ||
                index > 2) {

                return;
            }


            if (QAbstractButton *button =
                    m_navGroup->button(
                        index)) {

                button->setChecked(
                    true);
            }


            const int iconSize =
                qRound(
                    24 *
                    uiScaleForWindow(
                        this));


            MainWindowStyle::updateNavIcons(
                m_navGroup,
                index,
                iconSize);
        });


    // =========================================================================
    // 总布局
    // =========================================================================
    auto *layout =
        new QVBoxLayout(
            this);

    layout->setContentsMargins(
        0,
        0,
        0,
        0);

    layout->setSpacing(
        0);


    layout->addWidget(
        m_contentStack,
        1);

    layout->addWidget(
        navBar);


    // =========================================================================
    // 点击充电站 → 桩列表
    // =========================================================================
    connect(
        m_stationPage,
        &StationListPage::stationSelected,
        this,
        [this, locationPanel](
            qint64 id,
            const QString &name) {

            m_pilePage->loadStation(
                id,
                name);


            m_homeStack->setCurrentWidget(
                m_pilePage);


            // 桩列表页面暂时隐藏定位栏
            locationPanel->hide();
        });


    // =========================================================================
    // 路线规划
    // =========================================================================
    connect(
        m_stationPage,
        &StationListPage::navigationRequested,
        this,
        [this, locationPanel](
            const RouteRequest &request) {

            m_navigationPage
                ->setNavigationData(
                    request);


            m_homeStack->setCurrentWidget(
                m_navigationPage);


            // 路线规划页隐藏上方定位栏
            locationPanel->hide();
        });


    // =========================================================================
    // 路线规划返回
    // =========================================================================
    connect(
        m_navigationPage,
        &NavigationPage::back,
        this,
        [this, locationPanel]() {

            m_homeStack->setCurrentWidget(
                m_stationPage);


            locationPanel->show();
        });


    // =========================================================================
    // 桩列表返回
    // =========================================================================
    connect(
        m_pilePage,
        &PileListPage::back,
        this,
        [this, locationPanel]() {

            m_homeStack->setCurrentWidget(
                m_stationPage);


            locationPanel->show();
        });


    // =========================================================================
    // 第8步：
    // 预约成功 → 自动进入充电页面
    // =========================================================================
    connect(
        m_pilePage,
        &PileListPage::reservationSucceeded,
        this,
        [this](const QString &orderNo) {

            // 把预约产生的订单号交给充电页
            m_chargePage->setReservedOrder(
                orderNo);


            // 切换内容区到【充电】
            m_contentStack->setCurrentIndex(
                1);


            // 同步选中底部“充电”按钮
            if (m_navGroup->button(1)) {

                m_navGroup->button(1)
                    ->setChecked(
                        true);
            }
        });


    // =========================================================================
    // 第9步：
    // 开始充电 → start_charge
    // =========================================================================
    connect(
        m_chargePage,
        &ChargePage::startChargeRequested,
        this,
        [this](const QString &orderNo) {

            if (orderNo.trimmed().isEmpty()) {

                AppMessageBox::warning(
                    this,
                    QStringLiteral(
                        "开始充电失败"),
                    QStringLiteral(
                        "订单号无效"));

                return;
            }


            QJsonObject data;

            data["order_no"] =
                orderNo;


            const QJsonObject resp =
                m_net->request(
                    Protocol::makeRequest(
                        Protocol::MsgType::StartCharge,
                        data));


            const int code =
                resp.value(
                        "code")
                    .toInt();


            const QString msg =
                resp.value(
                        "msg")
                    .toString();


            if (code != Protocol::Ok) {

                AppMessageBox::warning(
                    this,
                    QStringLiteral(
                        "开始充电失败"),
                    msg);

                return;
            }


            // -----------------------------------------------------------------
            // 服务端 start_charge 成功
            // -----------------------------------------------------------------
            const QJsonObject chargeData =
                resp.value(
                        "data")
                    .toObject();


            const QString startTime =
                chargeData.value(
                              "start_time")
                    .toString();


            const double powerKw =
                chargeData.value(
                              "power_kw")
                    .toDouble();


            const double unitPrice =
                chargeData.value(
                              "unit_price")
                    .toDouble();


            // 模拟车辆 SOC 参数
            const double startSoc =
                chargeData.value(
                              "start_soc")
                    .toDouble(
                        -1.0);


            const double batteryCapacityKwh =
                chargeData.value(
                              "battery_capacity_kwh")
                    .toDouble(
                        0.0);


            const double targetSoc =
                chargeData.value(
                              "target_soc")
                    .toDouble(
                        100.0);


            m_chargePage->setChargingState(
                startTime,
                powerKw,
                unitPrice,
                startSoc,
                batteryCapacityKwh,
                targetSoc);


            AppMessageBox::information(
                this,
                QStringLiteral(
                    "提示"),
                QStringLiteral(
                    "开始充电成功"));
        });


    // =========================================================================
    // 结束充电 → finish_charge
    // =========================================================================
    connect(
        m_chargePage,
        &ChargePage::finishChargeRequested,
        this,
        [this](const QString &orderNo) {

            if (orderNo.trimmed().isEmpty()) {

                AppMessageBox::warning(
                    this,
                    QStringLiteral(
                        "结束充电失败"),
                    QStringLiteral(
                        "订单号无效"));

                return;
            }


            QJsonObject data;

            data["order_no"] =
                orderNo;


            const QJsonObject resp =
                m_net->request(
                    Protocol::makeRequest(
                        Protocol::MsgType::FinishCharge,
                        data));


            const int code =
                resp.value(
                        "code")
                    .toInt();


            const QString msg =
                resp.value(
                        "msg")
                    .toString();


            if (code != Protocol::Ok) {

                AppMessageBox::warning(
                    this,
                    QStringLiteral(
                        "结束充电失败"),
                    msg);

                return;
            }


            const QJsonObject result =
                resp.value(
                        "data")
                    .toObject();


            const qint64 durationSeconds =
                result.value(
                          "duration_seconds")
                    .toVariant()
                    .toLongLong();


            const double kwh =
                result.value(
                          "kwh")
                    .toDouble();


            const double amount =
                result.value(
                          "amount")
                    .toDouble();


            m_chargePage
                ->setPendingPaymentResult(
                    durationSeconds,
                    kwh,
                    amount);
        });


    // =========================================================================
    // 确认支付 → pay_charge
    // =========================================================================
    connect(
        m_chargePage,
        &ChargePage::payChargeRequested,
        this,
        [this](const QString &orderNo) {

            if (orderNo.trimmed().isEmpty()) {

                AppMessageBox::warning(
                    this,
                    QStringLiteral(
                        "支付失败"),
                    QStringLiteral(
                        "订单号无效"));

                return;
            }


            QJsonObject data;

            data["order_no"] =
                orderNo;


            const QJsonObject resp =
                m_net->request(
                    Protocol::makeRequest(
                        Protocol::MsgType::PayCharge,
                        data));


            const int code =
                resp.value(
                        "code")
                    .toInt();


            const QString msg =
                resp.value(
                        "msg")
                    .toString();


            if (code != Protocol::Ok) {

                // =============================================================
                // 余额不足
                // 订单继续保持 pending_payment
                // =============================================================
                if (code == 7) {

                    const bool goRecharge =
                        AppMessageBox::question(
                            this,
                            QStringLiteral(
                                "余额不足"),
                            QStringLiteral(
                                "当前余额不足，是否前往充值？"),
                            QStringLiteral(
                                "去充值"),
                            QStringLiteral(
                                "暂不充值"));


                    if (goRecharge) {

                        // 切换到“我的”
                        m_contentStack->setCurrentIndex(
                            2);


                        // 同步底部“我的”按钮
                        if (m_navGroup->button(2)) {

                            m_navGroup
                                ->button(2)
                                ->setChecked(
                                    true);
                        }


                        // 自动滚动到充值区域
                        m_profilePage
                            ->openRechargeSection();
                    }


                    return;
                }


                // 其它支付失败
                AppMessageBox::warning(
                    this,
                    QStringLiteral(
                        "支付失败"),
                    msg);

                return;
            }


            const QJsonObject result =
                resp.value(
                        "data")
                    .toObject();


            const qint64 durationSeconds =
                result.value(
                          "duration_seconds")
                    .toVariant()
                    .toLongLong();


            const double kwh =
                result.value(
                          "kwh")
                    .toDouble();


            const double amount =
                result.value(
                          "amount")
                    .toDouble();


            const double newBalance =
                result.value(
                          "balance")
                    .toDouble();


            m_balance =
                newBalance;


            m_profilePage->setBalance(
                m_balance);


            m_chargePage->setPaidResult(
                durationSeconds,
                kwh,
                amount,
                newBalance);
        });


    // =========================================================================
    // 修改昵称 → update_profile（NO.18）
    // =========================================================================
    connect(
        m_profilePage,
        &ProfilePage::nicknameChangeRequested,
        this,
        [this](const QString &nickname) {

            QJsonObject data;

            data["nickname"] =
                nickname;


            const QJsonObject resp =
                m_net->request(
                    Protocol::makeRequest(
                        Protocol::MsgType::UpdateProfile,
                        data));


            const int code =
                resp.value(
                        "code")
                    .toInt();


            const QString msg =
                resp.value(
                        "msg")
                    .toString();


            if (code != Protocol::Ok) {

                // code=9 由全局 onSessionInvalid
                // 统一回登录页
                if (code !=
                    Protocol::SessionInvalid) {

                    AppMessageBox::warning(
                        this,
                        QStringLiteral(
                            "修改昵称失败"),
                        msg);
                }


                return;
            }


            const QString newNick =
                resp.value(
                        "data")
                    .toObject()
                    .value(
                        "nickname")
                    .toString(
                        nickname);


            m_nickname =
                newNick;


            m_profilePage->setNickname(
                newNick);


            AppMessageBox::information(
                this,
                QStringLiteral(
                    "修改成功"),
                QStringLiteral(
                    "昵称已更新为：%1")
                    .arg(
                        newNick));
        });


    // =========================================================================
    // 修改头像 → update_profile
    // （NO.17，对接服务端 NO.75）
    // =========================================================================
    connect(
        m_profilePage,
        &ProfilePage::avatarChangeRequested,
        this,
        [this](
            const QString &avatar,
            const QPixmap &image) {

            QJsonObject data;

            data["avatar"] =
                avatar;


            const QJsonObject resp =
                m_net->request(
                    Protocol::makeRequest(
                        Protocol::MsgType::UpdateProfile,
                        data));


            const int code =
                resp.value(
                        "code")
                    .toInt();


            const QString msg =
                resp.value(
                        "msg")
                    .toString();


            if (code != Protocol::Ok) {

                // 失败保留旧图；
                // code=9 由全局 onSessionInvalid 回登录页
                if (code !=
                    Protocol::SessionInvalid) {

                    AppMessageBox::warning(
                        this,
                        QStringLiteral(
                            "修改头像失败"),
                        msg);
                }


                return;
            }


            m_profilePage->commitAvatar(
                image);


            AppMessageBox::information(
                this,
                QStringLiteral(
                    "修改成功"),
                QStringLiteral(
                    "头像已更新"));
        });


    // =========================================================================
    // NO.16：
    // 退出登录
    // =========================================================================
    connect(
        m_profilePage,
        &ProfilePage::logoutRequested,
        this,
        [this]() {

            const bool confirmed =
                AppMessageBox::question(
                    this,
                    QStringLiteral(
                        "退出登录"),
                    QStringLiteral(
                        "确定要退出当前账号吗？"),
                    QStringLiteral(
                        "退出登录"),
                    QStringLiteral(
                        "取消"));


            if (!confirmed) {

                return;
            }


            // 清除当前 Session Token
            if (m_net) {

                m_net->clearToken();
            }


            // 当前 MainWindow 关闭后，
            // Station / Pile / Charge / Profile /
            // Navigation 页面对象都会一起销毁，
            // 不保留上一账号页面缓存。
            auto *login =
                new LoginWindow;


            login->setAttribute(
                Qt::WA_DeleteOnClose);


            login->show();


            close();
        });


    // =========================================================================
    // 充值
    // =========================================================================
    connect(
        m_profilePage,
        &ProfilePage::rechargeRequested,
        this,
        [this](double amount) {

            if (m_userId <= 0 ||
                amount <= 0.0) {

                AppMessageBox::warning(
                    this,
                    QStringLiteral(
                        "充值失败"),
                    QStringLiteral(
                        "用户或充值金额无效"));

                return;
            }


            QJsonObject data;

            data["amount"] =
                amount;


            const QJsonObject resp =
                m_net->request(
                    Protocol::makeRequest(
                        Protocol::MsgType::Recharge,
                        data));


            const int code =
                resp.value(
                        "code")
                    .toInt();


            const QString msg =
                resp.value(
                        "msg")
                    .toString();


            if (code != Protocol::Ok) {

                AppMessageBox::warning(
                    this,
                    QStringLiteral(
                        "充值失败"),
                    msg);

                return;
            }


            const double newBalance =
                resp.value(
                        "data")
                    .toObject()
                    .value(
                        "balance")
                    .toDouble();


            m_balance =
                newBalance;


            m_profilePage->setBalance(
                m_balance);


            AppMessageBox::information(
                this,
                QStringLiteral(
                    "充值成功"),
                QStringLiteral(
                    "当前余额：￥%1")
                    .arg(
                        newBalance,
                        0,
                        'f',
                        2));
        });


    // =========================================================================
    // 恢复未完成订单
    // =========================================================================
    if (m_userId > 0) {

        const QJsonObject resp =
            m_net->request(
                Protocol::makeRequest(
                    Protocol::MsgType::UnfinishedOrder));


        if (resp.value(
                    "code")
                .toInt()
            == Protocol::Ok) {

            const QJsonObject order =
                resp.value(
                        "data")
                    .toObject()
                    .value(
                        "order")
                    .toObject();


            const QString orderNo =
                order.value(
                         "order_no")
                    .toString();


            const QString status =
                order.value(
                         "status")
                    .toString();


            if (!orderNo.isEmpty()) {

                // 先把订单交给充电页
                m_chargePage->setReservedOrder(
                    orderNo);


                const bool charging =
                    status ==
                    QStringLiteral(
                        "charging");


                const bool pending =
                    status ==
                    QStringLiteral(
                        "pending_payment");


                // -------------------------------------------------------------
                // 正在充电
                // -------------------------------------------------------------
                if (charging) {

                    const QString startTime =
                        order.value(
                                 "start_time")
                            .toString();


                    const double powerKw =
                        order.value(
                                 "power_kw")
                            .toDouble();


                    const double unitPrice =
                        order.value(
                                 "unit_price")
                            .toDouble();


                    // 恢复服务端保存的 SOC 参数
                    const double startSoc =
                        order.value(
                                 "start_soc")
                            .toDouble(
                                -1.0);


                    const double batteryCapacityKwh =
                        order.value(
                                 "battery_capacity_kwh")
                            .toDouble(
                                0.0);


                    const double targetSoc =
                        order.value(
                                 "target_soc")
                            .toDouble(
                                100.0);


                    m_chargePage->setChargingState(
                        startTime,
                        powerKw,
                        unitPrice,
                        startSoc,
                        batteryCapacityKwh,
                        targetSoc);
                }


                // -------------------------------------------------------------
                // 待支付
                // -------------------------------------------------------------
                else if (pending) {

                    m_chargePage
                        ->setPendingPaymentResult(

                            order.value(
                                     "duration_seconds")
                                .toVariant()
                                .toLongLong(),

                            order.value(
                                     "kwh")
                                .toDouble(),

                            order.value(
                                     "amount")
                                .toDouble());
                }


                // -------------------------------------------------------------
                // NO.20：
                // 未完成订单提示
                // -------------------------------------------------------------
                QString tip;


                if (pending) {

                    tip =
                        QStringLiteral(
                            "您有一个待支付的订单，请前往充电页完成支付。");

                } else if (charging) {

                    tip =
                        QStringLiteral(
                            "您有一个正在充电的订单，已为您跳转到充电页。");

                } else {

                    tip =
                        QStringLiteral(
                            "您有一个已预约的订单，请前往充电页开始充电。");
                }


                // 构造完成、窗口显示后再弹
                QTimer::singleShot(
                    0,
                    this,
                    [this, tip]() {

                        m_contentStack
                            ->setCurrentIndex(
                                1);


                        if (auto *button =
                                m_navGroup
                                    ->button(1)) {

                            button->setChecked(
                                true);
                        }


                        AppMessageBox::information(
                            this,
                            QStringLiteral(
                                "未完成订单"),
                            tip);
                    });
            }
        }
    }


    // =========================================================================
    // 点击定位按钮
    // =========================================================================
    connect(
        m_locationBtn,
        &QPushButton::clicked,
        this,
        [this]() {

            const QString region =
                m_regionCombo
                    ->currentText()
                    .trimmed();


            const QString address =
                m_addressEdit
                    ->text()
                    .trimmed();


            if (address.isEmpty()) {

                m_locationTip->setText(
                    QStringLiteral(
                        "请输入详细地址"));

                return;
            }


            m_locationBtn->setEnabled(
                false);


            m_locationTip->setText(
                QStringLiteral(
                    "正在定位…"));


            m_locationManager->geocode(
                address,
                region);
        });


    // 回车也可以定位
    connect(
        m_addressEdit,
        &QLineEdit::returnPressed,
        m_locationBtn,
        &QPushButton::click);


    // =========================================================================
    // 定位成功
    // =========================================================================
    connect(
        m_locationManager,
        &LocationManager::locationChanged,
        this,
        [this](
            double lat,
            double lng) {

            m_locationBtn->setEnabled(
                true);


            m_locationTip->setText(
                QStringLiteral(
                    "定位成功"));


            // 把经纬度交给附近充电站页面
            m_stationPage->setLocation(
                lat,
                lng);


            // -----------------------------------------------------------------
            // NO.1：
            // 仅保存成功解析过的地址和坐标
            // -----------------------------------------------------------------
            QSettings settings(
                QStringLiteral(
                    "ChargingSystem"),
                QStringLiteral(
                    "ChargingUser"));


            settings.setValue(
                QStringLiteral(
                    "location/region"),
                m_regionCombo
                    ->currentText()
                    .trimmed());


            settings.setValue(
                QStringLiteral(
                    "location/address"),
                m_addressEdit
                    ->text()
                    .trimmed());


            settings.setValue(
                QStringLiteral(
                    "location/lat"),
                lat);


            settings.setValue(
                QStringLiteral(
                    "location/lng"),
                lng);


            settings.sync();
        });


    // =========================================================================
    // 定位失败
    // =========================================================================
    connect(
        m_locationManager,
        &LocationManager::locationError,
        this,
        [this](
            const QString &message) {

            m_locationBtn->setEnabled(
                true);


            m_locationTip->setText(
                message);
        });

    // =========================================================================
    // AI 客服：悬浮圆球（点击打开聊天对话框）
    // =========================================================================
    m_navBar = navBar;
    m_aiBall = new FloatingBall(this);
    m_aiBall->raise();
    connect(m_aiBall, &FloatingBall::clicked, this, [this]() {
        auto *dlg = new AiChatDialog(m_net, m_locationManager, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        // E小充 AI 代执行预约成功：把预约订单交给"充电"页并切过去，
        // 让用户直接看到"预约成功，可以开始充电"（与手动预约成功的行为一致）
        connect(dlg, &AiChatDialog::reserveSucceeded, this, [this](const QString &orderNo) {
            m_chargePage->setReservedOrder(orderNo);
            m_contentStack->setCurrentIndex(1);
            if (m_navGroup->button(1))
                m_navGroup->button(1)->setChecked(true);
        });
        dlg->show();
    });

    // 所有控件创建完以后再套新样式
    applyResponsiveStyle();

    // 悬浮球精确定位（以底部导航条上缘为基准，避开【我的】）
    positionAiBall();
    // 等窗口首轮布局完成后再校正一次坐标
    QTimer::singleShot(0, this, [this]() { positionAiBall(); });
}


// ============================================================================
// Resize
// ============================================================================
void MainWindow::resizeEvent(
    QResizeEvent *event)
{
    QWidget::resizeEvent(
        event);


    applyResponsiveStyle();

    if (m_aiBall)
        positionAiBall();
}

void MainWindow::positionAiBall()
{
    if (!m_aiBall)
        return;

    const int x = width() - m_aiBall->width() - 14;

    int y = height() - m_aiBall->height() - 24;   // 兜底：贴近窗口底边
    if (m_navBar) {
        const int navTop = m_navBar->mapTo(this, QPoint(0, 0)).y();
        if (navTop > 0)
            y = navTop - m_aiBall->height() - 14; // 悬浮在底部导航条之上，不遮挡 tab
    }
    m_aiBall->move(x, y);
}


// ============================================================================
// 响应式样式
// ============================================================================
void MainWindow::applyResponsiveStyle()
{
    const double scale =
        uiScaleForWindow(
            this);


    const int controlFont =
        qRound(
            13 * scale);


    const int tipFont =
        qRound(
            11 * scale);


    const int navFont =
        qRound(
            12 * scale);


    const int controlHeight =
        qRound(
            34 * scale);


    const int navHeight =
        qRound(
            84 * scale);


    const int navButtonHeight =
        qRound(
            72 * scale);


    const int iconSize =
        qRound(
            24 * scale);


    // =========================================================================
    // 页面基础背景
    // =========================================================================
    setStyleSheet(
        QStringLiteral(

            "QWidget#userMainWindow{"
            "background:%1;"
            "color:%2;"
            "}"

            "QWidget#homePage{"
            "background:%1;"
            "}"

            "QStackedWidget#contentStack,"
            "QStackedWidget#homeStack{"
            "background:%1;"
            "border:none;"
            "}")

            .arg(
                UiTheme::pageBackground())

            .arg(
                UiTheme::textPrimary()));


    // =========================================================================
    // 定位白卡
    // =========================================================================
    if (auto *locationPanel =
            findChild<QFrame *>(
                QStringLiteral(
                    "locationPanel"))) {

        locationPanel->setStyleSheet(
            MainWindowStyle::
                locationPanelStyle());
    }


    if (auto *locationMainLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "locationMainLayout"))) {

        locationMainLayout->setContentsMargins(
            qRound(12 * scale),
            qRound(6 * scale),
            qRound(12 * scale),
            qRound(6 * scale));

        locationMainLayout->setSpacing(0);
    }


    // =========================================================================
    // 城市
    // =========================================================================
    if (m_regionCombo) {

        m_regionCombo->setStyleSheet(
            MainWindowStyle::
                regionComboStyle(
                    controlFont));


        m_regionCombo->setMinimumHeight(
            controlHeight);


        m_regionCombo->setMinimumWidth(
            qRound(
                88 * scale));
    }


    // =========================================================================
    // 地址输入框
    // =========================================================================
    if (m_addressEdit) {

        m_addressEdit->setStyleSheet(
            MainWindowStyle::
                addressEditStyle(
                    controlFont));


        m_addressEdit->setMinimumHeight(
            controlHeight);
    }


    // =========================================================================
    // 定位按钮
    // =========================================================================
    if (m_locationBtn) {

        m_locationBtn->setStyleSheet(
            MainWindowStyle::
                locationButtonStyle(
                    controlFont));


        m_locationBtn->setMinimumHeight(
            controlHeight);
    }


    // =========================================================================
    // 定位提示
    // =========================================================================
    if (m_locationTip) {

        m_locationTip->setStyleSheet(
            MainWindowStyle::
                locationTipStyle(
                    tipFont));
    }


    // =========================================================================
    // 深色底部导航
    // =========================================================================
    if (auto *navBar =
            findChild<QFrame *>(
                QStringLiteral(
                    "navBar"))) {

        navBar->setStyleSheet(
            MainWindowStyle::
                navBarStyle());


        navBar->setMinimumHeight(
            navHeight);
    }


    // =========================================================================
    // 导航按钮
    // =========================================================================
    if (m_navGroup) {

        for (QAbstractButton *abstractButton
             : m_navGroup->buttons()) {

            auto *button =
                qobject_cast<QToolButton *>(
                    abstractButton);


            if (!button) {

                continue;
            }


            button->setStyleSheet(
                MainWindowStyle::
                    navButtonStyle(
                        navFont));


            button->setMinimumHeight(
                navButtonHeight);
        }


        int activeId =
            m_navGroup->checkedId();


        if (activeId < 0 &&
            m_contentStack) {

            activeId =
                m_contentStack
                    ->currentIndex();
        }


        if (activeId < 0 ||
            activeId > 2) {

            activeId =
                0;
        }


        MainWindowStyle::
            updateNavIcons(
                m_navGroup,
                activeId,
                iconSize);
    }
}


// ============================================================================
// 设置 Session Token
// ============================================================================
void MainWindow::setSessionToken(
    const QString &token)
{
    if (m_net) {

        m_net->setToken(
            token);
    }
}


// ============================================================================
// 登录失效
// ============================================================================
void MainWindow::onSessionInvalid(
    const QString &msg)
{
    if (m_kickedToLogin) {

        return;
    }


    m_kickedToLogin =
        true;


    AppMessageBox::warning(
        this,
        QStringLiteral(
            "登录已失效"),

        msg.isEmpty()
            ? QStringLiteral(
                  "请重新登录")
            : msg);


    auto *login =
        new LoginWindow;


    login->setAttribute(
        Qt::WA_DeleteOnClose);


    login->show();


    close();
}
