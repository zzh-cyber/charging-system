#include "adminloginwindow.h"
#include "adminmainwindow.h"

#include "netclient.h"
#include "protocol.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>

static constexpr const char *kServerHost = "127.0.0.1";
static constexpr quint16 kServerPort = 9000;

namespace {
class BrandVisualWidget final : public QWidget
{
public:
    explicit BrandVisualWidget(QWidget *parent = nullptr) : QWidget(parent)
    {
        setObjectName(QStringLiteral("loginBrandVisual"));
        setMinimumHeight(310);
        m_car.load(QStringLiteral(":/login-car.jpg"));
        m_charger.load(QStringLiteral(":/login-charger.jpg"));
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF area = rect().adjusted(4, 6, -4, -6);

        QRadialGradient glow(area.center() + QPointF(area.width() * .18, 0), area.width() * .58);
        glow.setColorAt(0, QColor(91, 222, 136, 34));
        glow.setColorAt(1, QColor(91, 222, 136, 0));
        painter.fillRect(area, glow);

        painter.setPen(QPen(QColor(255, 255, 255, 13), 1));
        for (qreal x = area.left(); x < area.right(); x += 28)
            for (qreal y = area.top(); y < area.bottom(); y += 28)
                painter.drawPoint(QPointF(x, y));

        // 参考展厅式登录页：深色外框中放置一个柔和浅灰影棚，白底素材自然融入。
        const QRectF scene = area.adjusted(8, 5, -8, -5);
        QPainterPath scenePath;
        scenePath.addRoundedRect(scene, 18, 18);
        painter.save();
        painter.setClipPath(scenePath);
        QLinearGradient backdrop(scene.topLeft(), scene.bottomRight());
        backdrop.setColorAt(0, QColor("#F7F7F3"));
        backdrop.setColorAt(.62, QColor("#FFFFFF"));
        backdrop.setColorAt(1, QColor("#E9ECE7"));
        painter.fillPath(scenePath, backdrop);

        painter.setPen(QPen(QColor(22, 27, 23, 13), 1));
        for (qreal y = scene.top() + 28; y < scene.bottom(); y += 34)
            painter.drawLine(QPointF(scene.left(), y), QPointF(scene.right(), y));

        // 后景只保留一个充电桩，按原始比例缩放。
        const qreal chargerHeight = scene.height() * .82;
        const QSizeF chargerSize(chargerHeight * m_charger.width() / qreal(m_charger.height()),
                                 chargerHeight);
        const QRectF chargerRect(scene.right() - chargerSize.width() - scene.width() * .05,
                                 scene.bottom() - chargerSize.height() - scene.height() * .04,
                                 chargerSize.width(), chargerSize.height());
        painter.setOpacity(.96);
        painter.drawPixmap(chargerRect, m_charger, m_charger.rect());

        // 白色电动车位于左下前景，不拉伸，横向形成视觉主导。
        const qreal carWidth = scene.width() * .77;
        const qreal carHeight = carWidth * m_car.height() / qreal(m_car.width());
        const QRectF carRect(scene.left() - scene.width() * .035,
                             scene.bottom() - carHeight + scene.height() * .035,
                             carWidth, carHeight);
        painter.setOpacity(1.0);
        painter.drawPixmap(carRect, m_car, m_car.rect());
        painter.restore();

        painter.setPen(QPen(QColor("#57DB7E"), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(scene, 18, 18);
    }

private:
    QPixmap m_car;
    QPixmap m_charger;
};
}

AdminLoginWindow::AdminLoginWindow(QWidget *parent)
    : QWidget(parent)
    , m_net(new NetClient(this))
    , m_mainWindow(nullptr)
{
    setWindowTitle(QStringLiteral("充电桩运营管理后台 - 登录"));
    setObjectName(QStringLiteral("adminLoginWindow"));
    resize(1200, 800);

    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *brandPanel = new QWidget(this);
    brandPanel->setObjectName(QStringLiteral("loginBrandPanel"));
    auto *brandLayout = new QVBoxLayout(brandPanel);
    brandLayout->setContentsMargins(54, 42, 48, 42);
    brandLayout->setSpacing(0);
    auto *brandName = new QLabel(QStringLiteral("充电桩综合运营管理系统"), brandPanel);
    brandName->setObjectName(QStringLiteral("loginBrandName"));
    auto *brandEnglish = new QLabel(QStringLiteral("EV OPERATIONS CONSOLE"), brandPanel);
    brandEnglish->setObjectName(QStringLiteral("loginBrandEnglish"));
    auto *statement = new QLabel(QStringLiteral("连接每一座站点，\n让每一次充电都可感知。"), brandPanel);
    statement->setObjectName(QStringLiteral("loginStatement"));
    auto *caption = new QLabel(QStringLiteral("Smart Charging · Real-time Monitoring · Energy Operations"), brandPanel);
    caption->setObjectName(QStringLiteral("loginBrandCaption"));
    caption->setWordWrap(true);
    brandLayout->addWidget(brandName);
    brandLayout->addWidget(brandEnglish);
    brandLayout->addSpacing(48);
    brandLayout->addWidget(statement);
    brandLayout->addSpacing(10);
    brandLayout->addWidget(caption);
    brandLayout->addStretch();
    brandLayout->addWidget(new BrandVisualWidget(brandPanel), 2);

    auto *loginPanel = new QWidget(this);
    loginPanel->setObjectName(QStringLiteral("loginFormPanel"));
    auto *rightLayout = new QVBoxLayout(loginPanel);
    rightLayout->setContentsMargins(76, 64, 76, 64);
    auto *form = new QWidget(loginPanel);
    form->setObjectName(QStringLiteral("loginForm"));
    form->setMaximumWidth(430);
    auto *formLayout = new QVBoxLayout(form);
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->setSpacing(10);
    auto *eyebrow = new QLabel(QStringLiteral("ADMINISTRATOR ACCESS"), form);
    eyebrow->setObjectName(QStringLiteral("loginEyebrow"));
    auto *title = new QLabel(QStringLiteral("管理员登录"), form);
    title->setObjectName(QStringLiteral("loginTitle"));
    auto *welcome = new QLabel(QStringLiteral("欢迎回来，请登录以进入运营管理平台。"), form);
    welcome->setObjectName(QStringLiteral("loginWelcome"));
    auto *userLabel = new QLabel(QStringLiteral("账号"), form);
    userLabel->setObjectName(QStringLiteral("loginFieldLabel"));

    m_userEdit = new QLineEdit(this);
    m_userEdit->setPlaceholderText(
        QStringLiteral("管理员账号")
    );
    m_userEdit->setText("admin");
    m_userEdit->setMinimumHeight(48);

    m_pwdEdit = new QLineEdit(this);
    m_pwdEdit->setPlaceholderText(
        QStringLiteral("密码")
    );
    m_pwdEdit->setEchoMode(QLineEdit::Password);
    m_pwdEdit->setMinimumHeight(48);
    auto *passwordLabel = new QLabel(QStringLiteral("密码"), form);
    passwordLabel->setObjectName(QStringLiteral("loginFieldLabel"));

    m_loginBtn = new QPushButton(
        QStringLiteral("登录"),
        this
    );
    m_loginBtn->setMinimumHeight(50);
    auto *securityHint = new QLabel(QStringLiteral("仅限授权管理员访问"), form);
    securityHint->setObjectName(QStringLiteral("loginSecurityHint"));
    securityHint->setAlignment(Qt::AlignCenter);

    formLayout->addWidget(eyebrow);
    formLayout->addWidget(title);
    formLayout->addWidget(welcome);
    formLayout->addSpacing(32);
    formLayout->addWidget(userLabel);
    formLayout->addWidget(m_userEdit);
    formLayout->addSpacing(10);
    formLayout->addWidget(passwordLabel);
    formLayout->addWidget(m_pwdEdit);
    formLayout->addSpacing(18);
    formLayout->addWidget(m_loginBtn);
    formLayout->addWidget(securityHint);
    rightLayout->addStretch();
    rightLayout->addWidget(form, 0, Qt::AlignHCenter);
    rightLayout->addStretch();
    root->addWidget(brandPanel, 11);
    root->addWidget(loginPanel, 10);

    connect(
        m_loginBtn,
        &QPushButton::clicked,
        this,
        &AdminLoginWindow::onLoginClicked
    );
    connect(m_userEdit, &QLineEdit::returnPressed, this, &AdminLoginWindow::onLoginClicked);
    connect(m_pwdEdit, &QLineEdit::returnPressed, this, &AdminLoginWindow::onLoginClicked);

    connect(
        m_net,
        &NetClient::sessionInvalid,
        this,
        &AdminLoginWindow::onSessionInvalid
    );
}

AdminLoginWindow::~AdminLoginWindow()
{
}

void AdminLoginWindow::onLoginClicked()
{
    if (!m_loginBtn->isEnabled())
        return;
    const QString user = m_userEdit->text().trimmed();
    const QString pwd = m_pwdEdit->text();

    if (user.isEmpty() || pwd.isEmpty()) {
        QMessageBox::warning(
            this,
            QStringLiteral("提示"),
            QStringLiteral("请输入账号和密码")
        );
        return;
    }

    m_loginBtn->setEnabled(false);
    m_loginBtn->setText(QStringLiteral("登录中..."));
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    const auto restoreButton = [this] {
        m_loginBtn->setText(QStringLiteral("登录"));
        m_loginBtn->setEnabled(true);
    };

    if (!m_net->isConnected() &&
        !m_net->connectToServer(kServerHost, kServerPort)) {

        restoreButton();

        QMessageBox::critical(
            this,
            QStringLiteral("错误"),
            QStringLiteral("无法连接服务器，请确认服务器已启动")
        );

        return;
    }

    QJsonObject data;
    data["username"] = user;
    data["password"] = pwd;

    const QJsonObject resp =
        m_net->request(
            Protocol::makeRequest(
                Protocol::MsgType::AdminLogin,
                data
            )
        );

    if (resp.value("code").toInt() != Protocol::Ok) {
        restoreButton();
        QMessageBox::warning(
            this,
            QStringLiteral("登录失败"),
            resp.value("msg").toString()
        );
        return;
    }

    m_net->setToken(
        resp.value("data").toObject().value("token").toString()
    );

    // ============================================================
    // 登录成功，进入管理员主界面
    // ============================================================

    if (!m_mainWindow) {
        m_mainWindow = new AdminMainWindow(m_net);
    }
    const QRect loginGeometry = geometry();
    const bool wasMaximized = isMaximized();
    m_mainWindow->setGeometry(loginGeometry);
    if (wasMaximized)
        m_mainWindow->showMaximized();
    else
        m_mainWindow->showNormal();
    m_mainWindow->raise();
    m_mainWindow->activateWindow();

    this->hide();
}

void AdminLoginWindow::onSessionInvalid(const QString &msg)
{
    if (!m_mainWindow || !m_mainWindow->isVisible())
        return;

    QMessageBox::warning(
        m_mainWindow,
        QStringLiteral("登录已失效"),
        msg.isEmpty() ? QStringLiteral("请重新登录") : msg);

    const QRect mainGeometry = m_mainWindow->geometry();
    const bool wasMaximized = m_mainWindow->isMaximized();
    m_mainWindow->close();
    m_mainWindow->deleteLater();
    m_mainWindow = nullptr;

    m_pwdEdit->clear();
    m_loginBtn->setText(QStringLiteral("登录"));
    m_loginBtn->setEnabled(true);
    setGeometry(mainGeometry);
    if (wasMaximized)
        showMaximized();
    else
        showNormal();
    this->raise();
    this->activateWindow();
    m_pwdEdit->setFocus();
}
