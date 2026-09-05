#include "mainwindow.h"

#include "chargepage.h"
#include "locationmanager.h"
#include "loginwindow.h"
#include "netclient.h"
#include "pilelistpage.h"
#include "profilepage.h"
#include "stationlistpage.h"
#include "protocol.h"
#include "navigationpage.h"
#include "windowhelper.h"


#include <QButtonGroup>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QJsonObject>
#include <QMessageBox>
#include <QTimer>
#include <QResizeEvent>


// 服务器地址（与登录页保持一致）
static constexpr const char *kServerHost = "127.0.0.1";
static constexpr quint16     kServerPort = 9000;

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
    , m_userId(userId)
    , m_balance(balance)
{
    
    setWindowTitle(
        QStringLiteral("充电用户端"));

    // 响应式窗口：按屏幕分辨率自适应手机比例并居中
    applyPhoneWindow(this);
    applyResponsiveStyle();


    if (!m_net->isConnected()) {
        m_net->connectToServer(
            kServerHost,
            kServerPort);
    }


    // 必须在任何业务请求之前写入 token。
    // 否则构造函数里的 unfinished_order 会因无 token 收到 code=9，
    // 被误判为「登录已失效」。
    m_net->setToken(token);

    connect(m_net, &NetClient::sessionInvalid,
            this, &MainWindow::onSessionInvalid);

    // ------------------------------------------------------------------------
    // 首页子栈：
    // 充电站列表 ⇄ 桩列表
    // ------------------------------------------------------------------------
    m_homeStack->addWidget(
        m_stationPage);

    m_homeStack->addWidget(
        m_pilePage);
    m_homeStack->addWidget(
        m_navigationPage);

        // 把当前登录用户 ID 交给桩列表页
        // 预约 reserve 时需要 user_id
    m_pilePage->setUserId(m_userId);


    // =========================================================================
    // 首页容器
    // =========================================================================
    auto *homePage =
        new QWidget(this);

    auto *homeLayout =
        new QVBoxLayout(homePage);

    homeLayout->setContentsMargins(
        0, 0, 0, 0);

    homeLayout->setSpacing(0);

    // =========================================================================
    // 地址定位区域
    // =========================================================================
    auto *locationPanel =
        new QWidget(homePage);

    auto *locationMainLayout =
        new QVBoxLayout(locationPanel);

    locationMainLayout->setContentsMargins(
        12, 10, 12, 8);

    locationMainLayout->setSpacing(6);

    // 第一行：
    // 城市 + 地址 + 定位按钮
    auto *locationRow =
        new QHBoxLayout;

    locationRow->setSpacing(8);

    // ------------------------------------------------------------------------
    // 城市选择
    // ------------------------------------------------------------------------
    m_regionCombo =
        new QComboBox(locationPanel);

    m_regionCombo->addItem(
        QStringLiteral("北京市"));

    m_regionCombo->addItem(
        QStringLiteral("上海市"));

    m_regionCombo->addItem(
        QStringLiteral("广州市"));

    m_regionCombo->addItem(
        QStringLiteral("深圳市"));

    m_regionCombo->addItem(
        QStringLiteral("杭州市"));

    m_regionCombo->addItem(
        QStringLiteral("南京市"));

    m_regionCombo->setMinimumWidth(85);

    // ------------------------------------------------------------------------
    // 地址输入
    // ------------------------------------------------------------------------
    m_addressEdit =
        new QLineEdit(locationPanel);

    m_addressEdit->setPlaceholderText(
        QStringLiteral(
            "请输入详细地址"));

    // ------------------------------------------------------------------------
    // 定位按钮
    // ------------------------------------------------------------------------
    m_locationBtn =
        new QPushButton(
            QStringLiteral("定位"),
            locationPanel);

    m_locationBtn->setCursor(
        Qt::PointingHandCursor);

    locationRow->addWidget(
        m_regionCombo);

    locationRow->addWidget(
        m_addressEdit,
        1);

    locationRow->addWidget(
        m_locationBtn);

    // ------------------------------------------------------------------------
    // 定位状态提示
    // ------------------------------------------------------------------------
    m_locationTip =
        new QLabel(
            QStringLiteral(
                "请输入当前位置"),
            locationPanel);

    m_locationTip->setStyleSheet(
        "color:#86909c;"
        "padding-left:2px;");

    locationMainLayout->addLayout(
        locationRow);

    locationMainLayout->addWidget(
        m_locationTip);

    // 首页 =
    // 定位区域 + 原来的 m_homeStack
    homeLayout->addWidget(
        locationPanel);

    homeLayout->addWidget(
        m_homeStack,
        1);

    // ------------------------------------------------------------------------
    // 内容区
    // ------------------------------------------------------------------------
    m_contentStack->addWidget(
        homePage);               // 0 首页

    m_contentStack->addWidget(
        m_chargePage);           // 1 充电

    m_contentStack->addWidget(
        m_profilePage);          // 2 我的

    // ------------------------------------------------------------------------
    // 登录用户信息传给“我的”
    // ------------------------------------------------------------------------
    m_profilePage->setUserInfo(
        m_nickname,
        m_phone,
        m_balance);

    // =========================================================================
    // 底部导航
    // =========================================================================
    auto *navBar =
        new QWidget(this);

    navBar->setObjectName(
        "navBar");

    auto *navLayout =
        new QHBoxLayout(navBar);

    navLayout->setContentsMargins(
        0, 0, 0, 0);

    navLayout->setSpacing(0);

    struct NavItem
    {
        QString name;
        int index;
    };

    const NavItem items[] = {
        {
            QStringLiteral("首页"),
            0
        },
        {
            QStringLiteral("充电"),
            1
        },
        {
            QStringLiteral("我的"),
            2
        },
    };

    for (const auto &item : items) {

        auto *btn =
            new QPushButton(
                item.name,
                navBar);

        btn->setObjectName(
            "navBtn");

        btn->setCheckable(true);

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

    m_navGroup->setExclusive(true);

    m_navGroup->button(0)
        ->setChecked(true);

    connect(
        m_navGroup,
        &QButtonGroup::idClicked,
        this,
        [this](int id) {

            m_contentStack
                ->setCurrentIndex(id);
        });

    // =========================================================================
    // 总布局
    // =========================================================================
    auto *layout =
        new QVBoxLayout(this);

    layout->setContentsMargins(
        0, 0, 0, 0);

    layout->setSpacing(0);

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
    // 一键导航
    // =========================================================================
    connect(
        m_stationPage,
        &StationListPage::navigationRequested,
        this,
        [this, locationPanel](
            qint64 stationId,
            const QString &name,
            double startLat,
            double startLng,
            double targetLat,
            double targetLng,
            double distance) {

            Q_UNUSED(stationId);

            m_navigationPage
                ->setNavigationData(
                    name,
                    startLat,
                    startLng,
                    targetLat,
                    targetLng,
                    distance);

            m_homeStack->setCurrentWidget(
                m_navigationPage);

            // 导航页隐藏上方定位栏
            locationPanel->hide();
        });
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
    
    // ==================================================
    // 第8步：预约成功 → 自动进入充电页面
    // ==================================================
    connect(m_pilePage,
            &PileListPage::reservationSucceeded,
            this,
            [this](const QString &orderNo) {

        // 把预约产生的订单号交给充电页
        m_chargePage->setReservedOrder(orderNo);

        // 切换内容区到【充电】
        m_contentStack->setCurrentIndex(1);

        // 同步选中底部“充电”按钮
        if (m_navGroup->button(1)) {
            m_navGroup->button(1)->setChecked(true);
        }
    });
    // ==================================================
    // 第9步：开始充电 → start_charge
    // ==================================================
    connect(m_chargePage,
            &ChargePage::startChargeRequested,
            this,
            [this](const QString &orderNo) {

        if (orderNo.trimmed().isEmpty()) {
            QMessageBox::warning(
                this,
                QStringLiteral("开始充电失败"),
                QStringLiteral("订单号无效"));
            return;
        }

        // 使用现有 start_charge 协议
        QJsonObject data;
        data["order_no"] = orderNo;

        const QJsonObject resp =
            m_net->request(
                Protocol::makeRequest(
                    Protocol::MsgType::StartCharge,
                    data));

        const int code =
            resp.value("code").toInt();

        const QString msg =
            resp.value("msg").toString();

        if (code != Protocol::Ok) {

            QMessageBox::warning(
                this,
                QStringLiteral("开始充电失败"),
                msg);

            return;
        }

        // 服务端 start_charge 成功，
        // 将充电页切换到“充电中”状态
        m_chargePage->setChargingState();

        QMessageBox::information(
            this,
            QStringLiteral("提示"),
            QStringLiteral("开始充电成功"));
    });
    
    connect(
    m_chargePage,
    &ChargePage::settleRequested,
    this,
    [this](const QString &orderNo,
           double kwh) {

        if (orderNo.trimmed().isEmpty() ||
            kwh <= 0.0) {

            QMessageBox::warning(
                this,
                QStringLiteral("结算失败"),
                QStringLiteral(
                    "订单或充电量无效"));

            return;
        }

        QJsonObject data;

        data["order_no"] =
            orderNo;

        data["kwh"] =
            kwh;

        const QJsonObject resp =
            m_net->request(
                Protocol::makeRequest(
                    Protocol::MsgType::Settle,
                    data));

        const int code =
            resp.value("code").toInt();

        const QString msg =
            resp.value("msg").toString();

        if (code != Protocol::Ok) {

            QMessageBox::warning(
                this,
                QStringLiteral("结算失败"),
                msg);

            return;
        }

        const QJsonObject result =
            resp.value("data")
                .toObject();

        const double amount =
            result.value("amount")
                .toDouble();

        const double newBalance =
            result.value("balance")
                .toDouble();

        // 充电页展示结果
        m_chargePage->setSettledResult(
            kwh,
            amount);

        // 服务端已经扣余额，
        // 客户端只更新显示
        m_balance =
            newBalance;

        m_profilePage->setBalance(
            m_balance);

        QMessageBox::information(
            this,
            QStringLiteral("结算成功"),
            QStringLiteral(
                "本次费用：￥%1\n"
                "当前余额：￥%2")
                .arg(
                    amount,
                    0,
                    'f',
                    2)
                .arg(
                    newBalance,
                    0,
                    'f',
                    2));
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
            data["nickname"] = nickname;

            const QJsonObject resp =
                m_net->request(
                    Protocol::makeRequest(
                        Protocol::MsgType::UpdateProfile,
                        data));

            const int code =
                resp.value("code").toInt();

            const QString msg =
                resp.value("msg").toString();

            if (code != Protocol::Ok) {

                // code=9 由全局 onSessionInvalid 统一回登录页，这里只提示其它错误
                if (code != Protocol::SessionInvalid) {

                    QMessageBox::warning(
                        this,
                        QStringLiteral("修改昵称失败"),
                        msg);
                }

                return;
            }

            const QString newNick =
                resp.value("data")
                    .toObject()
                    .value("nickname")
                    .toString(nickname);

            m_nickname = newNick;

            m_profilePage->setNickname(
                newNick);

            QMessageBox::information(
                this,
                QStringLiteral("修改成功"),
                QStringLiteral("昵称已更新为：%1")
                    .arg(newNick));
        });

    connect(
        m_profilePage,
        &ProfilePage::rechargeRequested,
        this,
        [this](double amount) {

            if (m_userId <= 0 ||
                amount <= 0.0) {

                QMessageBox::warning(
                    this,
                    QStringLiteral("充值失败"),
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
                resp.value("code").toInt();

            const QString msg =
                resp.value("msg").toString();

            if (code != Protocol::Ok) {

                QMessageBox::warning(
                    this,
                    QStringLiteral("充值失败"),
                    msg);

                return;
            }

            const double newBalance =
                resp.value("data")
                    .toObject()
                    .value("balance")
                    .toDouble();

            m_balance =
                newBalance;

            m_profilePage->setBalance(
                m_balance);

            QMessageBox::information(
                this,
                QStringLiteral("充值成功"),
                QStringLiteral(
                    "当前余额：￥%1")
                    .arg(
                        newBalance,
                        0,
                        'f',
                        2));
        });

        if (m_userId > 0) {

        const QJsonObject resp =
            m_net->request(
                Protocol::makeRequest(
                    Protocol::MsgType::UnfinishedOrder));

        if (resp.value("code").toInt()
            == Protocol::Ok) {

            const QJsonObject order =
                resp.value("data")
                    .toObject()
                    .value("order")
                    .toObject();

            const QString orderNo =
                order.value("order_no")
                    .toString();

            const QString status =
                order.value("status")
                    .toString();

            if (!orderNo.isEmpty()) {

                // 先把订单交给充电页并置好状态：
                // charging / pending_payment 都要显示结算入口（待支付可再次结算补款）。
                m_chargePage->setReservedOrder(
                    orderNo);

                const bool charging =
                    status == QStringLiteral("charging");
                const bool pending =
                    status == QStringLiteral("pending_payment");

                if (charging || pending) {
                    m_chargePage
                        ->setChargingState();
                }

                // NO.20：有未完成订单时弹窗提示，并切到充电页。
                // 用 singleShot(0) 延后到构造完成、窗口显示后再弹，
                // 避免在构造函数里进嵌套事件循环。
                QString tip;
                if (pending)
                    tip = QStringLiteral(
                        "您有一个待支付的订单，请前往充电页完成支付。");
                else if (charging)
                    tip = QStringLiteral(
                        "您有一个正在充电的订单，已为您跳转到充电页。");
                else
                    tip = QStringLiteral(
                        "您有一个已预约的订单，请前往充电页开始充电。");

                QTimer::singleShot(0, this,
                    [this, tip]() {
                        m_contentStack->setCurrentIndex(1);
                        if (auto *b = m_navGroup->button(1))
                            b->setChecked(true);
                        QMessageBox::information(
                            this,
                            QStringLiteral("未完成订单"),
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
        [this](double lat,
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
        });

    // =========================================================================
    // 定位失败
    // =========================================================================
    connect(
        m_locationManager,
        &LocationManager::locationError,
        this,
        [this](const QString &message) {

            m_locationBtn->setEnabled(
                true);

            m_locationTip->setText(
                message);
        });
}
void MainWindow::resizeEvent(
    QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    applyResponsiveStyle();
}

void MainWindow::applyResponsiveStyle()
{
    const double scale =
        uiScaleForWindow(this);

    const int normalFont =
        qRound(14 * scale);

    const int controlFont =
        qRound(15 * scale);

    const int inputPaddingV =
        qRound(10 * scale);

    const int inputPaddingH =
        qRound(12 * scale);

    const int buttonPaddingV =
        qRound(10 * scale);

    const int buttonPaddingH =
        qRound(16 * scale);

    const int navPadding =
        qRound(14 * scale);

    setStyleSheet(
        QStringLiteral(
            "QWidget{font-size:%1px;}"
            "QLineEdit,QComboBox,QDoubleSpinBox{"
            "font-size:%2px;"
            "padding:%3px %4px;"
            "}"
            "QPushButton{"
            "font-size:%2px;"
            "padding:%5px %6px;"
            "}"
            "QPushButton#navBtn{"
            "font-size:%2px;"
            "padding:%7px 0;"
            "}")
            .arg(normalFont)
            .arg(controlFont)
            .arg(inputPaddingV)
            .arg(inputPaddingH)
            .arg(buttonPaddingV)
            .arg(buttonPaddingH)
            .arg(navPadding));
}



void MainWindow::setSessionToken(const QString &token)
{
    if (m_net)
        m_net->setToken(token);
}

void MainWindow::onSessionInvalid(const QString &msg)
{
    if (m_kickedToLogin)
        return;
    m_kickedToLogin = true;

    QMessageBox::warning(
        this,
        QStringLiteral("登录已失效"),
        msg.isEmpty() ? QStringLiteral("请重新登录") : msg);

    auto *login = new LoginWindow;
    login->setAttribute(Qt::WA_DeleteOnClose);
    login->show();
    close();
}
