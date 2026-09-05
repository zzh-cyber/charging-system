#include "profilepage.h"
#include "windowhelper.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QPainter>
#include <QPixmap>
#include <QResizeEvent>
#include <QStringList>



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
title->setObjectName(
    QStringLiteral("profileTitle"));

    title->setStyleSheet(
        "font-size:24px;"
        "font-weight:700;"
        "color:#1f2329;");

    auto *subtitle =
        new QLabel(
            QStringLiteral(
                "个人资料与钱包管理"),
            this);
    subtitle->setObjectName(
    QStringLiteral("profileSubtitle"));

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
    userCard->setObjectName(
    QStringLiteral("profileUserCard"));


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
    avatar->setObjectName(
    QStringLiteral("profileAvatar"));

    avatar->setScaledContents(true);


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
    // 点击编辑昵称：自绘对话框（不用 QInputDialog，其嵌套事件循环在
    // WSL/X11 上经常接不住中文输入法）→ 本地校验 2~20 → 发信号
    // ------------------------------------------------------------------------
    connect(
        m_editNickButton,
        &QPushButton::clicked,
        this,
        [this]() {

            QDialog dlg(this);
            dlg.setWindowTitle(QStringLiteral("修改昵称"));
            dlg.setModal(true);
            dlg.setAttribute(Qt::WA_InputMethodEnabled, true);

            auto *label = new QLabel(
                QStringLiteral("请输入新的昵称（2～20 个字符）："),
                &dlg);

            auto *edit = new QLineEdit(&dlg);
            edit->setText(m_nickname);
            edit->setMaxLength(20);
            edit->setAttribute(Qt::WA_InputMethodEnabled, true);
            edit->setInputMethodHints(Qt::ImhNone);
            edit->setFocus();

            auto *buttons = new QDialogButtonBox(
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                &dlg);
            buttons->button(QDialogButtonBox::Ok)->setText(
                QStringLiteral("确定"));
            buttons->button(QDialogButtonBox::Cancel)->setText(
                QStringLiteral("取消"));

            auto *box = new QVBoxLayout(&dlg);
            box->addWidget(label);
            box->addWidget(edit);
            box->addWidget(buttons);

            QObject::connect(
                buttons, &QDialogButtonBox::accepted,
                &dlg, &QDialog::accept);
            QObject::connect(
                buttons, &QDialogButtonBox::rejected,
                &dlg, &QDialog::reject);

            if (dlg.exec() != QDialog::Accepted)
                return;

            const QString nickname =
                edit->text().trimmed();

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
    walletCard->setObjectName(
    QStringLiteral("profileWalletCard"));


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
    walletTitle->setObjectName(
    QStringLiteral("profileWalletTitle"));


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
    amountTitle->setObjectName(
    QStringLiteral("profileAmountTitle"));


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
        btn->setObjectName(
           QStringLiteral("profileQuickButton"));


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
    customLabel->setObjectName(
    QStringLiteral("profileCustomLabel"));


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
applyResponsiveStyle();

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
void ProfilePage::resizeEvent(
    QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    applyResponsiveStyle();
}


void ProfilePage::applyResponsiveStyle()
{
    QWidget *scaleBase =
        window()
            ? window()
            : this;

    const int titleFont =
        scaledUi(scaleBase, 24);

    const int nicknameFont =
        scaledUi(scaleBase, 18);

    const int balanceFont =
        scaledUi(scaleBase, 30);

    const int normalFont =
        scaledUi(scaleBase, 14);

    const int smallFont =
        scaledUi(scaleBase, 13);

    const int tipFont =
        scaledUi(scaleBase, 12);

    // ============================================================
    // 页面整体边距
    // ============================================================
    if (auto *pageLayout =
            qobject_cast<QVBoxLayout *>(
                layout())) {

        const int margin =
            scaledUi(scaleBase, 18);

        pageLayout->setContentsMargins(
            margin,
            margin,
            margin,
            margin);

        pageLayout->setSpacing(
            scaledUi(scaleBase, 14));
    }

    // ============================================================
    // 页面标题
    // ============================================================
    if (auto *title =
            findChild<QLabel *>(
                QStringLiteral(
                    "profileTitle"))) {

        title->setStyleSheet(
            QStringLiteral(
                "font-size:%1px;"
                "font-weight:700;"
                "color:#1f2329;")
                .arg(titleFont));
    }

    if (auto *subtitle =
            findChild<QLabel *>(
                QStringLiteral(
                    "profileSubtitle"))) {

        subtitle->setStyleSheet(
            QStringLiteral(
                "color:#86909c;"
                "font-size:%1px;")
                .arg(smallFont));
    }

    // ============================================================
    // 用户资料卡片
    // ============================================================
    if (auto *userCard =
            findChild<QFrame *>(
                QStringLiteral(
                    "profileUserCard"))) {

        userCard->setStyleSheet(
            QStringLiteral(
                "QFrame#profileUserCard{"
                "background:#ffffff;"
                "border:1px solid #d6e4ff;"
                "border-radius:%1px;"
                "}")
                .arg(
                    scaledUi(
                        scaleBase,
                        14)));

        if (auto *userLayout =
                qobject_cast<QHBoxLayout *>(
                    userCard->layout())) {

            const int margin =
                scaledUi(
                    scaleBase,
                    18);

            userLayout->setContentsMargins(
                margin,
                margin,
                margin,
                margin);

            userLayout->setSpacing(
                scaledUi(
                    scaleBase,
                    14));
        }
    }

    // ============================================================
    // 头像
    // ============================================================
    if (auto *avatar =
            findChild<QLabel *>(
                QStringLiteral(
                    "profileAvatar"))) {

        const int avatarSize =
            scaledUi(scaleBase, 54);

        avatar->setFixedSize(
            avatarSize,
            avatarSize);
            avatar->setStyleSheet(
        QStringLiteral(
            "background:#e8f0ff;"
            "color:#1d4ed8;"
            "border:none;"
            "border-radius:%1px;"
            "font-size:%2px;"
            "font-weight:700;")
            .arg(avatarSize / 2)
            .arg(
                scaledUi(
                    scaleBase,
                    22)));

    }

    // ============================================================
    // 昵称和手机号
    // ============================================================
    if (m_nicknameLabel) {

        m_nicknameLabel->setStyleSheet(
            QStringLiteral(
                "border:none;"
                "font-size:%1px;"
                "font-weight:700;"
                "color:#1f2329;")
                .arg(nicknameFont));
    }

    if (m_phoneLabel) {

        m_phoneLabel->setStyleSheet(
            QStringLiteral(
                "border:none;"
                "font-size:%1px;"
                "color:#86909c;")
                .arg(smallFont));
    }

    // ============================================================
    // 编辑昵称按钮
    // ============================================================
    if (m_editNickButton) {

        m_editNickButton->setStyleSheet(
            QStringLiteral(
                "QPushButton{"
                "background:#f5f8ff;"
                "color:#1d4ed8;"
                "border:1px solid #d6e4ff;"
                "border-radius:%1px;"
                "padding:%2px %3px;"
                "font-size:%4px;"
                "font-weight:600;"
                "}"
                "QPushButton:hover{"
                "background:#e8f0ff;"
                "border-color:#1d4ed8;"
                "}")
                .arg(
                    scaledUi(
                        scaleBase,
                        8))
                .arg(
                    scaledUi(
                        scaleBase,
                        6))
                .arg(
                    scaledUi(
                        scaleBase,
                        14))
                .arg(smallFont));
    }

    // ============================================================
    // 钱包卡片
    // ============================================================
    if (auto *walletCard =
            findChild<QFrame *>(
                QStringLiteral(
                    "profileWalletCard"))) {

        walletCard->setStyleSheet(
            QStringLiteral(
                "QFrame#profileWalletCard{"
                "background:#ffffff;"
                "border:1px solid #d6e4ff;"
                "border-radius:%1px;"
                "}")
                .arg(
                    scaledUi(
                        scaleBase,
                        14)));

        if (auto *walletLayout =
                qobject_cast<QVBoxLayout *>(
                    walletCard->layout())) {

            const int margin =
                scaledUi(scaleBase, 18);

            walletLayout->setContentsMargins(
                margin,
                margin,
                margin,
                margin);

            walletLayout->setSpacing(
                scaledUi(scaleBase, 12));
        }
    }

    // ============================================================
    // 钱包标题
    // ============================================================
    if (auto *walletTitle =
            findChild<QLabel *>(
                QStringLiteral(
                    "profileWalletTitle"))) {

        walletTitle->setStyleSheet(
            QStringLiteral(
                "border:none;"
                "color:#4e5969;"
                "font-size:%1px;")
                .arg(normalFont));
    }

    // ============================================================
    // 余额
    // ============================================================
    if (m_balanceLabel) {

        m_balanceLabel->setStyleSheet(
            QStringLiteral(
                "border:none;"
                "color:#1d4ed8;"
                "font-size:%1px;"
                "font-weight:700;")
                .arg(balanceFont));
    }

    // ============================================================
    // 充值相关标题
    // ============================================================
    const QStringList labelNames = {
        QStringLiteral("profileAmountTitle"),
        QStringLiteral("profileCustomLabel")
    };

    for (const QString &name :
         labelNames) {

        if (auto *label =
                findChild<QLabel *>(name)) {

            label->setStyleSheet(
                QStringLiteral(
                    "border:none;"
                    "font-size:%1px;"
                    "font-weight:600;")
                    .arg(normalFont));
        }
    }

    // ============================================================
    // 快捷充值按钮
    // ============================================================
    const auto quickButtons =
        findChildren<QPushButton *>(
            QStringLiteral(
                "profileQuickButton"));

    for (QPushButton *button :
         quickButtons) {

        button->setStyleSheet(
            QStringLiteral(
                "QPushButton{"
                "background:#f5f8ff;"
                "color:#1d4ed8;"
                "border:1px solid #d6e4ff;"
                "border-radius:%1px;"
                "padding:%2px;"
                "font-size:%3px;"
                "font-weight:600;"
                "}"
                "QPushButton:hover{"
                "background:#e8f0ff;"
                "border-color:#1d4ed8;"
                "}")
                .arg(
                    scaledUi(
                        scaleBase,
                        8))
                .arg(
                    scaledUi(
                        scaleBase,
                        9))
                .arg(normalFont));
    }

    // ============================================================
    // 充值金额输入框
    // ============================================================
    if (m_amountSpin) {

        m_amountSpin->setMinimumWidth(
            scaledUi(
                scaleBase,
                160));
    }

    // ============================================================
    // 底部提示
    // ============================================================
    if (m_tipLabel) {

        m_tipLabel->setStyleSheet(
            QStringLiteral(
                "border:none;"
                "color:#86909c;"
                "font-size:%1px;")
                .arg(tipFont));
    }
}
