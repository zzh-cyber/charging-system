#include "profilepage.h"

#include <QDoubleSpinBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QPainter>
#include <QPixmap>


ProfilePage::ProfilePage(
    QWidget *parent)
    : QWidget(parent)
{
    auto *layout =
        new QVBoxLayout(this);

    layout->setContentsMargins(
        18, 18, 18, 18);

    layout->setSpacing(14);

    // ========================================================================
    // 标题
    // ========================================================================
    auto *title =
        new QLabel(
            QStringLiteral("我的"),
            this);

    title->setStyleSheet(
        "font-size:24px;"
        "font-weight:700;"
        "color:#1f2329;");

    auto *subtitle =
        new QLabel(
            QStringLiteral(
                "个人资料与钱包管理"),
            this);

    subtitle->setStyleSheet(
        "color:#86909c;"
        "font-size:13px;");

    layout->addWidget(title);
    layout->addWidget(subtitle);

    // ========================================================================
    // 用户资料卡片
    // ========================================================================
    auto *userCard =
        new QFrame(this);

    userCard->setStyleSheet(
        "QFrame{"
        "background:#ffffff;"
        "border:1px solid #d6e4ff;"
        "border-radius:14px;"
        "}");

    auto *userLayout =
        new QHBoxLayout(userCard);

    userLayout->setContentsMargins(
        18, 18, 18, 18);

    userLayout->setSpacing(14);

    // 简单头像
    // 默认灰色头像
auto *avatar = new QLabel(userCard);

avatar->setFixedSize(64, 64);
avatar->setAlignment(Qt::AlignCenter);
avatar->setStyleSheet(
    "border:none;"
    "background:transparent;");

// 创建头像画布
QPixmap avatarPixmap(64, 64);
avatarPixmap.fill(Qt::transparent);

QPainter painter(&avatarPixmap);
painter.setRenderHint(QPainter::Antialiasing, true);

// 灰色圆形背景
painter.setBrush(QColor("#e5e6eb"));
painter.setPen(Qt::NoPen);
painter.drawEllipse(0, 0, 64, 64);

// 头像头部
painter.setBrush(QColor("#86909c"));
painter.drawEllipse(23, 14, 18, 18);

// 头像身体
painter.drawEllipse(15, 34, 34, 26);

painter.end();

avatar->setPixmap(avatarPixmap);



    avatar->setFixedSize(
        54, 54);

    avatar->setAlignment(
        Qt::AlignCenter);

    avatar->setStyleSheet(
        "background:#e8f0ff;"
        "color:#1d4ed8;"
        "border:none;"
        "border-radius:27px;"
        "font-size:22px;"
        "font-weight:700;");

    auto *infoLayout =
        new QVBoxLayout;

    infoLayout->setSpacing(4);

    m_nicknameLabel =
        new QLabel(
            QStringLiteral("用户"),
            userCard);

    m_nicknameLabel->setStyleSheet(
        "border:none;"
        "font-size:18px;"
        "font-weight:700;"
        "color:#1f2329;");

    m_phoneLabel =
        new QLabel(
            QStringLiteral("手机号：--"),
            userCard);

    m_phoneLabel->setStyleSheet(
        "border:none;"
        "font-size:13px;"
        "color:#86909c;");

    infoLayout->addWidget(
        m_nicknameLabel);

    infoLayout->addWidget(
        m_phoneLabel);

    // ------------------------------------------------------------------------
    // 编辑昵称按钮（NO.18）
    // ------------------------------------------------------------------------
    m_editNickButton =
        new QPushButton(
            QStringLiteral("编辑"),
            userCard);

    m_editNickButton->setCursor(
        Qt::PointingHandCursor);

    m_editNickButton->setStyleSheet(
        "QPushButton{"
        "background:#f5f8ff;"
        "color:#1d4ed8;"
        "border:1px solid #d6e4ff;"
        "border-radius:8px;"
        "padding:6px 14px;"
        "font-size:13px;"
        "font-weight:600;"
        "}"
        "QPushButton:hover{"
        "background:#e8f0ff;"
        "border-color:#1d4ed8;"
        "}");

    userLayout->addWidget(
        avatar);

    userLayout->addLayout(
        infoLayout,
        1);

    userLayout->addWidget(
        m_editNickButton,
        0,
        Qt::AlignVCenter);

    layout->addWidget(
        userCard);

    // ------------------------------------------------------------------------
    // 点击编辑昵称：弹输入框 → 本地校验 2~20 → 发信号交 MainWindow 处理
    // ------------------------------------------------------------------------
    connect(
        m_editNickButton,
        &QPushButton::clicked,
        this,
        [this]() {

            bool ok = false;

            const QString input =
                QInputDialog::getText(
                    this,
                    QStringLiteral("修改昵称"),
                    QStringLiteral("请输入新的昵称（2～20 个字符）："),
                    QLineEdit::Normal,
                    m_nickname,
                    &ok);

            if (!ok)
                return;

            const QString nickname =
                input.trimmed();

            if (nickname.size() < 2 ||
                nickname.size() > 20) {

                QMessageBox::warning(
                    this,
                    QStringLiteral("昵称无效"),
                    QStringLiteral("昵称长度需为 2～20 个字符"));

                return;
            }

            if (nickname == m_nickname)
                return;

            emit nicknameChangeRequested(
                nickname);
        });

    // ========================================================================
    // 钱包卡片
    // ========================================================================
    auto *walletCard =
        new QFrame(this);

    walletCard->setStyleSheet(
        "QFrame{"
        "background:#ffffff;"
        "border:1px solid #d6e4ff;"
        "border-radius:14px;"
        "}");

    auto *walletLayout =
        new QVBoxLayout(walletCard);

    walletLayout->setContentsMargins(
        18, 18, 18, 18);

    walletLayout->setSpacing(12);

    auto *walletTitle =
        new QLabel(
            QStringLiteral(
                "钱包余额"),
            walletCard);

    walletTitle->setStyleSheet(
        "border:none;"
        "color:#4e5969;"
        "font-size:14px;");

    m_balanceLabel =
        new QLabel(
            QStringLiteral(
                "￥0.00"),
            walletCard);

    m_balanceLabel->setStyleSheet(
        "border:none;"
        "color:#1d4ed8;"
        "font-size:30px;"
        "font-weight:700;");

    walletLayout->addWidget(
        walletTitle);

    walletLayout->addWidget(
        m_balanceLabel);

    // ========================================================================
    // 快捷充值
    // ========================================================================
    auto *amountTitle =
        new QLabel(
            QStringLiteral(
                "选择充值金额"),
            walletCard);

    amountTitle->setStyleSheet(
        "border:none;"
        "font-size:14px;"
        "font-weight:600;");

    walletLayout->addWidget(
        amountTitle);

    auto *quickLayout =
        new QGridLayout;

    quickLayout->setHorizontalSpacing(8);
    quickLayout->setVerticalSpacing(8);

    const int amounts[] = {
        50,
        100,
        200,
        500
    };

    for (int i = 0; i < 4; ++i) {

        const int amount =
            amounts[i];

        auto *btn =
            new QPushButton(
                QStringLiteral(
                    "￥%1")
                    .arg(amount),
                walletCard);

        btn->setCursor(
            Qt::PointingHandCursor);

        btn->setStyleSheet(
            "QPushButton{"
            "background:#f5f8ff;"
            "color:#1d4ed8;"
            "border:1px solid #d6e4ff;"
            "border-radius:8px;"
            "padding:9px;"
            "font-size:14px;"
            "font-weight:600;"
            "}"
            "QPushButton:hover{"
            "background:#e8f0ff;"
            "border-color:#1d4ed8;"
            "}");

        connect(
            btn,
            &QPushButton::clicked,
            this,
            [this, amount]() {

                m_amountSpin->setValue(
                    amount);
            });

        quickLayout->addWidget(
            btn,
            i / 2,
            i % 2);
    }

    walletLayout->addLayout(
        quickLayout);

    // ========================================================================
    // 自定义充值金额
    // ========================================================================
    auto *customRow =
        new QHBoxLayout;

    auto *customLabel =
        new QLabel(
            QStringLiteral(
                "充值金额"),
            walletCard);

    customLabel->setStyleSheet(
        "border:none;"
        "font-size:14px;");

    m_amountSpin =
        new QDoubleSpinBox(
            walletCard);

    m_amountSpin->setRange(
        1.0,
        10000.0);

    m_amountSpin->setDecimals(2);
    m_amountSpin->setSingleStep(10.0);
    m_amountSpin->setValue(100.0);

    m_amountSpin->setPrefix(
        QStringLiteral("￥"));

    m_amountSpin->setMinimumWidth(
        160);

    customRow->addWidget(
        customLabel);

    customRow->addStretch();

    customRow->addWidget(
        m_amountSpin);

    walletLayout->addLayout(
        customRow);

    // ========================================================================
    // 充值按钮
    // ========================================================================
    m_rechargeButton =
        new QPushButton(
            QStringLiteral(
                "立即充值"),
            walletCard);

    m_rechargeButton->setCursor(
        Qt::PointingHandCursor);

    walletLayout->addWidget(
        m_rechargeButton);

    // ========================================================================
    // 状态提示
    // ========================================================================
    m_tipLabel =
        new QLabel(
            QStringLiteral(
                "充值金额将通过系统充值接口处理"),
            walletCard);

    m_tipLabel->setAlignment(
        Qt::AlignCenter);

    m_tipLabel->setStyleSheet(
        "border:none;"
        "color:#86909c;"
        "font-size:12px;");

    walletLayout->addWidget(
        m_tipLabel);

    layout->addWidget(
        walletCard);

    layout->addStretch();

    // ========================================================================
    // 点击充值
    // ========================================================================
    connect(
        m_rechargeButton,
        &QPushButton::clicked,
        this,
        [this]() {

            const double amount =
                m_amountSpin->value();

            if (amount <= 0.0) {

                m_tipLabel->setText(
                    QStringLiteral(
                        "请输入有效充值金额"));

                return;
            }

            emit rechargeRequested(
                amount);
        });
}

void ProfilePage::setUserInfo(
    const QString &nickname,
    const QString &phone,
    double balance)
{
    setNickname(nickname);

    m_phoneLabel->setText(
        QStringLiteral(
            "手机号：%1")
            .arg(phone));

    setBalance(balance);
}

void ProfilePage::setNickname(
    const QString &nickname)
{
    m_nickname = nickname;

    m_nicknameLabel->setText(
        nickname.isEmpty()
            ? QStringLiteral("用户")
            : nickname);
}

void ProfilePage::setBalance(
    double balance)
{
    m_balance =
        balance;

    m_balanceLabel->setText(
        QStringLiteral(
            "￥%1")
            .arg(
                m_balance,
                0,
                'f',
                2));
}
