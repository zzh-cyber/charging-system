#include "adminloginwindow.h"
#include "adminmainwindow.h"

#include "netclient.h"
#include "protocol.h"

#include <QApplication>
#include <QFormLayout>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

static constexpr const char *kServerHost = "127.0.0.1";
static constexpr quint16 kServerPort = 9000;

AdminLoginWindow::AdminLoginWindow(QWidget *parent)
    : QWidget(parent)
    , m_net(new NetClient(this))
    , m_mainWindow(nullptr)
{
    setWindowTitle(QStringLiteral("充电桩运营管理后台 - 登录"));
    resize(380, 240);

    auto *title = new QLabel(
        QStringLiteral("充电桩运营管理后台"),
        this
    );

    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(
        "font-size:18px;font-weight:bold;"
    );

    m_userEdit = new QLineEdit(this);
    m_userEdit->setPlaceholderText(
        QStringLiteral("管理员账号")
    );
    m_userEdit->setText("admin");

    m_pwdEdit = new QLineEdit(this);
    m_pwdEdit->setPlaceholderText(
        QStringLiteral("密码")
    );
    m_pwdEdit->setEchoMode(QLineEdit::Password);

    m_loginBtn = new QPushButton(
        QStringLiteral("登录"),
        this
    );

    auto *form = new QFormLayout;

    form->addRow(
        QStringLiteral("账号"),
        m_userEdit
    );

    form->addRow(
        QStringLiteral("密码"),
        m_pwdEdit
    );

    auto *layout = new QVBoxLayout(this);

    layout->addStretch();
    layout->addWidget(title);
    layout->addSpacing(10);
    layout->addLayout(form);
    layout->addWidget(m_loginBtn);
    layout->addStretch();

    connect(
        m_loginBtn,
        &QPushButton::clicked,
        this,
        &AdminLoginWindow::onLoginClicked
    );
}

AdminLoginWindow::~AdminLoginWindow()
{
}

void AdminLoginWindow::onLoginClicked()
{
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

    if (!m_net->isConnected() &&
        !m_net->connectToServer(kServerHost, kServerPort)) {

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

    QMessageBox::information(
        this,
        QStringLiteral("登录成功"),
        QStringLiteral("欢迎，管理员 %1").arg(user)
    );

    // ============================================================
    // 登录成功，进入管理员主界面
    // ============================================================

    if (!m_mainWindow) {
        m_mainWindow = new AdminMainWindow(m_net);
    }

    m_mainWindow->show();
    m_mainWindow->raise();
    m_mainWindow->activateWindow();

    this->hide();
}