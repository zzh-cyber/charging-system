#include "adminloginwindow.h"
#include "adminmainwindow.h"

#include "netclient.h"
#include "protocol.h"

#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QIcon>
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
QIcon fieldIcon(bool locked)
{
    QPixmap icon(22, 22);
    icon.fill(Qt::transparent);
    QPainter painter(&icon);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor("#A4AAA6"), 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    if (locked) {
        painter.drawRoundedRect(QRectF(5.5, 9.5, 11, 8.5), 2, 2);
        painter.drawArc(QRectF(7.5, 3.5, 7, 10), 0, 180 * 16);
    } else {
        painter.drawEllipse(QRectF(8, 3.5, 6, 6));
        painter.drawArc(QRectF(4.5, 10, 13, 9), 15 * 16, 150 * 16);
    }
    return QIcon(icon);
}

class BrandVisualWidget final : public QWidget
{
public:
    explicit BrandVisualWidget(QWidget *parent = nullptr) : QWidget(parent)
    {
        setObjectName(QStringLiteral("loginBrandVisual"));
        setMinimumHeight(250);
        m_car.load(QStringLiteral(":/login-car-transparent.png"));
        m_charger.load(QStringLiteral(":/login-charger-transparent.png"));
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF area = rect().adjusted(4, 2, -4, -2);

        QRadialGradient glow(area.center() + QPointF(area.width() * .18, area.height() * .15),
                             area.width() * .58);
        glow.setColorAt(0, QColor(53, 198, 107, 18));
        glow.setColorAt(1, QColor(53, 198, 107, 0));
        painter.fillRect(area, glow);

        painter.setPen(QPen(QColor(53, 198, 107, 24), 1));
        painter.drawArc(area.adjusted(20, 32, -10, 30), 12 * 16, 135 * 16);

        const QRectF scene = area.adjusted(6, 0, -6, -2);

        // 后景只保留一个充电桩，按原始比例缩放。
        const qreal chargerHeight = scene.height() * .86;
        const QSizeF chargerSize(chargerHeight * m_charger.width() / qreal(m_charger.height()),
                                 chargerHeight);
        const QRectF chargerRect(scene.right() - chargerSize.width() - scene.width() * .05,
                                 scene.bottom() - chargerSize.height() - scene.height() * .10,
                                 chargerSize.width(), chargerSize.height());
        painter.setOpacity(.94);
        painter.drawPixmap(chargerRect, m_charger, m_charger.rect());

        // 白色电动车位于左下前景，不拉伸，横向形成视觉主导。
        const qreal carWidth = scene.width() * .84;
        const qreal carHeight = carWidth * m_car.height() / qreal(m_car.width());
        const QRectF carRect(scene.left() - scene.width() * .035,
                             scene.bottom() - carHeight + scene.height() * .02,
                             carWidth, carHeight);
        painter.setOpacity(1.0);
        painter.drawPixmap(carRect, m_car, m_car.rect());
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
    resize(1000, 680);

    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *brandPanel = new QWidget(this);
    brandPanel->setObjectName(QStringLiteral("loginBrandPanel"));
    auto *brandLayout = new QVBoxLayout(brandPanel);
    brandLayout->setContentsMargins(48, 38, 40, 30);
    brandLayout->setSpacing(0);
    auto *brandName = new QLabel(QStringLiteral("充电桩综合运营管理系统"), brandPanel);
    brandName->setObjectName(QStringLiteral("loginBrandName"));
    auto *brandEnglish = new QLabel(QStringLiteral("EV OPERATIONS CONSOLE"), brandPanel);
    brandEnglish->setObjectName(QStringLiteral("loginBrandEnglish"));
    auto *statement = new QLabel(QStringLiteral("连接每一座站点，\n让每一次充电都可感知。"), brandPanel);
    statement->setObjectName(QStringLiteral("loginStatement"));
    auto *caption = new QLabel(QStringLiteral("Smart Charging · A Greener Tomorrow."), brandPanel);
    caption->setObjectName(QStringLiteral("loginBrandCaption"));
    caption->setWordWrap(true);
    brandLayout->addWidget(brandName);
    brandLayout->addWidget(brandEnglish);
    auto *accentLine = new QWidget(brandPanel);
    accentLine->setObjectName(QStringLiteral("loginAccentLine"));
    accentLine->setFixedSize(36, 3);
    brandLayout->addSpacing(30);
    brandLayout->addWidget(accentLine);
    brandLayout->addSpacing(22);
    brandLayout->addWidget(statement);
    brandLayout->addSpacing(10);
    brandLayout->addWidget(caption);
    brandLayout->addSpacing(18);
    brandLayout->addWidget(new BrandVisualWidget(brandPanel), 1);

    auto *loginPanel = new QWidget(this);
    loginPanel->setObjectName(QStringLiteral("loginFormPanel"));
    auto *rightLayout = new QVBoxLayout(loginPanel);
    rightLayout->setContentsMargins(36, 52, 36, 52);
    auto *form = new QWidget(loginPanel);
    form->setObjectName(QStringLiteral("loginForm"));
    form->setFixedWidth(380);
    auto *formLayout = new QVBoxLayout(form);
    formLayout->setContentsMargins(26, 28, 26, 26);
    formLayout->setSpacing(10);
    auto *formShadow = new QGraphicsDropShadowEffect(form);
    formShadow->setBlurRadius(34);
    formShadow->setOffset(0, 12);
    formShadow->setColor(QColor(0, 0, 0, 105));
    form->setGraphicsEffect(formShadow);
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
    m_userEdit->addAction(fieldIcon(false), QLineEdit::LeadingPosition);

    m_pwdEdit = new QLineEdit(this);
    m_pwdEdit->setPlaceholderText(
        QStringLiteral("密码")
    );
    m_pwdEdit->setEchoMode(QLineEdit::Password);
    m_pwdEdit->setMinimumHeight(48);
    m_pwdEdit->addAction(fieldIcon(true), QLineEdit::LeadingPosition);
    auto *passwordLabel = new QLabel(QStringLiteral("密码"), form);
    passwordLabel->setObjectName(QStringLiteral("loginFieldLabel"));

    m_loginBtn = new QPushButton(
        QStringLiteral("登录"),
        this
    );
    m_loginBtn->setMinimumHeight(48);
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
    root->addWidget(brandPanel, 57);
    root->addWidget(loginPanel, 43);

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
