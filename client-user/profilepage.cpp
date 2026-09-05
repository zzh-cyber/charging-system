#include "profilepage.h"

#include "uitheme.h"
#include "windowhelper.h"

#include <QColor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QVBoxLayout>


// ============================================================================
// 构造函数
// ============================================================================
ProfilePage::ProfilePage(
    QWidget *parent)
    : QWidget(parent)
{
    setObjectName(
        QStringLiteral(
            "profilePage"));


    // ========================================================================
    // 页面根布局
    // ========================================================================
    auto *rootLayout =
        new QVBoxLayout(this);

    rootLayout->setContentsMargins(
        0,
        0,
        0,
        0);

    rootLayout->setSpacing(
        0);


    // ========================================================================
    // 滚动区域
    // ========================================================================
    auto *scrollArea =
        new QScrollArea(this);

    scrollArea->setObjectName(
        QStringLiteral(
            "profileScrollArea"));

    scrollArea->setWidgetResizable(
        true);

    scrollArea->setFrameShape(
        QFrame::NoFrame);

    scrollArea->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAlwaysOff);


    auto *content =
        new QWidget;

    content->setObjectName(
        QStringLiteral(
            "profileContent"));


    auto *layout =
        new QVBoxLayout(
            content);

    layout->setObjectName(
        QStringLiteral(
            "profileContentLayout"));

    layout->setContentsMargins(
        18,
        18,
        18,
        18);

    layout->setSpacing(
        14);


    // ========================================================================
    // 页面标题
    // ========================================================================
    auto *title =
        new QLabel(
            QStringLiteral(
                "我的"),
            content);

    title->setObjectName(
        QStringLiteral(
            "profileTitle"));


    auto *subtitle =
        new QLabel(
            QStringLiteral(
                "个人资料与钱包管理"),
            content);

    subtitle->setObjectName(
        QStringLiteral(
            "profileSubtitle"));

    subtitle->setWordWrap(
        true);


    layout->addWidget(
        title);

    layout->addWidget(
        subtitle);


    // ========================================================================
    // 用户资料卡
    // ========================================================================
    auto *userCard =
        new QFrame(content);

    userCard->setObjectName(
        QStringLiteral(
            "profileUserCard"));

    UiTheme::applyCardShadow(
        userCard,
        18,
        4);


    auto *userLayout =
        new QHBoxLayout(
            userCard);

    userLayout->setObjectName(
        QStringLiteral(
            "profileUserLayout"));

    userLayout->setContentsMargins(
        18,
        18,
        18,
        18);

    userLayout->setSpacing(
        14);


    // ========================================================================
    // 头像
    // ========================================================================
    auto *avatar =
        new QLabel(
            userCard);

    avatar->setObjectName(
        QStringLiteral(
            "profileAvatar"));

    avatar->setAlignment(
        Qt::AlignCenter);

    avatar->setScaledContents(
        true);


    QPixmap avatarPixmap(
        64,
        64);

    avatarPixmap.fill(
        Qt::transparent);


    QPainter painter(
        &avatarPixmap);

    painter.setRenderHint(
        QPainter::Antialiasing,
        true);


    painter.setBrush(
        QColor(
            "#E7EFEA"));

    painter.setPen(
        Qt::NoPen);

    painter.drawEllipse(
        0,
        0,
        64,
        64);


    painter.setBrush(
        QColor(
            "#315B4D"));

    painter.drawEllipse(
        23,
        13,
        18,
        18);

    painter.drawEllipse(
        14,
        34,
        36,
        27);

    painter.end();


    avatar->setPixmap(
        avatarPixmap);


    // ========================================================================
    // 用户信息
    // ========================================================================
    auto *infoLayout =
        new QVBoxLayout;

    infoLayout->setSpacing(
        5);


    auto *userCaption =
        new QLabel(
            QStringLiteral(
                "账户信息"),
            userCard);

    userCaption->setObjectName(
        QStringLiteral(
            "profileUserCaption"));


    // 保留原默认值：用户
    m_nicknameLabel =
        new QLabel(
            QStringLiteral(
                "用户"),
            userCard);

    m_nicknameLabel->setObjectName(
        QStringLiteral(
            "profileNicknameLabel"));


    // 保留原默认值：手机号：--
    m_phoneLabel =
        new QLabel(
            QStringLiteral(
                "手机号：--"),
            userCard);

    m_phoneLabel->setObjectName(
        QStringLiteral(
            "profilePhoneLabel"));

    m_phoneLabel->setWordWrap(
        true);


    infoLayout->addWidget(
        userCaption);

    infoLayout->addWidget(
        m_nicknameLabel);

    infoLayout->addWidget(
        m_phoneLabel);


    // ========================================================================
    // 编辑昵称按钮
    // ========================================================================
    m_editNickButton =
        new QPushButton(
            QStringLiteral(
                "编辑昵称"),
            userCard);

    m_editNickButton->setObjectName(
        QStringLiteral(
            "profileEditButton"));

    m_editNickButton->setCursor(
        Qt::PointingHandCursor);


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


    // ========================================================================
    // 修改昵称
    // 原业务逻辑保持不变
    // ========================================================================
    connect(
        m_editNickButton,
        &QPushButton::clicked,
        this,
        [this]() {

            QDialog dialog(this);

            dialog.setObjectName(
                QStringLiteral(
                    "nicknameDialog"));

            dialog.setWindowTitle(
                QStringLiteral(
                    "修改昵称"));

            dialog.setModal(
                true);

            dialog.setAttribute(
                Qt::WA_InputMethodEnabled,
                true);


            auto *dialogLayout =
                new QVBoxLayout(
                    &dialog);

            dialogLayout->setContentsMargins(
                22,
                20,
                22,
                20);

            dialogLayout->setSpacing(
                12);


            auto *dialogTitle =
                new QLabel(
                    QStringLiteral(
                        "修改昵称"),
                    &dialog);

            dialogTitle->setObjectName(
                QStringLiteral(
                    "nicknameDialogTitle"));


            auto *label =
                new QLabel(
                    QStringLiteral(
                        "请输入新的昵称（2～20 个字符）："),
                    &dialog);

            label->setObjectName(
                QStringLiteral(
                    "nicknameDialogTip"));

            label->setWordWrap(
                true);


            auto *edit =
                new QLineEdit(
                    &dialog);

            edit->setObjectName(
                QStringLiteral(
                    "nicknameEdit"));

            edit->setText(
                m_nickname);

            edit->setMaxLength(
                20);

            edit->setAttribute(
                Qt::WA_InputMethodEnabled,
                true);

            edit->setInputMethodHints(
                Qt::ImhNone);

            edit->setFocus();


            auto *buttons =
                new QDialogButtonBox(
                    QDialogButtonBox::Ok |
                    QDialogButtonBox::Cancel,
                    &dialog);


            buttons->button(
                QDialogButtonBox::Ok)
                ->setText(
                    QStringLiteral(
                        "确定"));


            buttons->button(
                QDialogButtonBox::Cancel)
                ->setText(
                    QStringLiteral(
                        "取消"));


            dialogLayout->addWidget(
                dialogTitle);

            dialogLayout->addWidget(
                label);

            dialogLayout->addWidget(
                edit);

            dialogLayout->addWidget(
                buttons);


            dialog.setStyleSheet(
                QStringLiteral(

                    "QDialog#nicknameDialog{"
                    "background:#F6F4EF;"
                    "}"

                    "QLabel#nicknameDialogTitle{"
                    "color:#202824;"
                    "font-size:18px;"
                    "font-weight:800;"
                    "}"

                    "QLabel#nicknameDialogTip{"
                    "color:#7A837E;"
                    "font-size:13px;"
                    "}"

                    "QLineEdit#nicknameEdit{"
                    "background:#FFFFFF;"
                    "color:#202824;"
                    "border:1px solid #E7E3DA;"
                    "border-radius:10px;"
                    "padding:9px 12px;"
                    "font-size:14px;"
                    "}"

                    "QLineEdit#nicknameEdit:focus{"
                    "border:1px solid #315B4D;"
                    "}"

                    "QDialogButtonBox QPushButton{"
                    "min-width:72px;"
                    "padding:8px 14px;"
                    "border-radius:9px;"
                    "font-weight:600;"
                    "}"

                    "QDialogButtonBox QPushButton:hover{"
                    "background:#E9F0EC;"
                    "}"));


            QObject::connect(
                buttons,
                &QDialogButtonBox::accepted,
                &dialog,
                &QDialog::accept);


            QObject::connect(
                buttons,
                &QDialogButtonBox::rejected,
                &dialog,
                &QDialog::reject);


            if (dialog.exec() !=
                QDialog::Accepted) {

                return;
            }


            const QString nickname =
                edit->text()
                    .trimmed();


            // 保留原昵称校验：2 ～ 20
            if (nickname.size() < 2 ||
                nickname.size() > 20) {

                QMessageBox::warning(
                    this,
                    QStringLiteral(
                        "昵称无效"),
                    QStringLiteral(
                        "昵称长度需为 2～20 个字符"));

                return;
            }


            if (nickname ==
                m_nickname) {

                return;
            }


            emit nicknameChangeRequested(
                nickname);
        });


    // ========================================================================
    // 钱包卡
    // ========================================================================
    auto *walletCard =
        new QFrame(
            content);

    walletCard->setObjectName(
        QStringLiteral(
            "profileWalletCard"));

    UiTheme::applyCardShadow(
        walletCard,
        18,
        4);


    auto *walletLayout =
        new QVBoxLayout(
            walletCard);

    walletLayout->setObjectName(
        QStringLiteral(
            "profileWalletLayout"));

    walletLayout->setContentsMargins(
        18,
        18,
        18,
        18);

    walletLayout->setSpacing(
        13);


    // ========================================================================
    // 钱包顶部
    // ========================================================================
    auto *walletHeader =
        new QHBoxLayout;


    auto *walletTitle =
        new QLabel(
            QStringLiteral(
                "钱包余额"),
            walletCard);

    walletTitle->setObjectName(
        QStringLiteral(
            "profileWalletTitle"));


    auto *walletBadge =
        new QLabel(
            QStringLiteral(
                "账户余额"),
            walletCard);

    walletBadge->setObjectName(
        QStringLiteral(
            "profileWalletBadge"));

    walletBadge->setAlignment(
        Qt::AlignCenter);


    walletHeader->addWidget(
        walletTitle);

    walletHeader->addStretch();

    walletHeader->addWidget(
        walletBadge);


    walletLayout->addLayout(
        walletHeader);


    // ========================================================================
    // 当前余额
    // ========================================================================
    auto *balanceCaption =
        new QLabel(
            QStringLiteral(
                "当前余额"),
            walletCard);

    balanceCaption->setObjectName(
        QStringLiteral(
            "profileBalanceCaption"));


    // 保留原默认值：￥0.00
    m_balanceLabel =
        new QLabel(
            QStringLiteral(
                "￥0.00"),
            walletCard);

    m_balanceLabel->setObjectName(
        QStringLiteral(
            "profileBalanceLabel"));


    walletLayout->addWidget(
        balanceCaption);

    walletLayout->addWidget(
        m_balanceLabel);


    // ========================================================================
    // 分隔线
    // ========================================================================
    auto *divider =
        new QFrame(
            walletCard);

    divider->setObjectName(
        QStringLiteral(
            "profileDivider"));

    divider->setFrameShape(
        QFrame::HLine);


    walletLayout->addWidget(
        divider);


    // ========================================================================
    // 快捷充值
    // ========================================================================
    auto *amountTitle =
        new QLabel(
            QStringLiteral(
                "选择充值金额"),
            walletCard);

    amountTitle->setObjectName(
        QStringLiteral(
            "profileAmountTitle"));


    auto *amountSubtitle =
        new QLabel(
            QStringLiteral(
                "可选择常用金额，也可以输入自定义金额"),
            walletCard);

    amountSubtitle->setObjectName(
        QStringLiteral(
            "profileAmountSubtitle"));

    amountSubtitle->setWordWrap(
        true);


    walletLayout->addWidget(
        amountTitle);

    walletLayout->addWidget(
        amountSubtitle);


    auto *quickLayout =
        new QGridLayout;

    quickLayout->setObjectName(
        QStringLiteral(
            "profileQuickLayout"));

    quickLayout->setHorizontalSpacing(
        8);

    quickLayout->setVerticalSpacing(
        8);


    // 保留原快捷金额
    const int amounts[] = {
        50,
        100,
        200,
        500
    };


    for (int i = 0;
         i < 4;
         ++i) {

        const int amount =
            amounts[i];


        auto *button =
            new QPushButton(
                QStringLiteral(
                    "￥%1")
                    .arg(
                        amount),
                walletCard);

        button->setObjectName(
            QStringLiteral(
                "profileQuickButton"));

        button->setCursor(
            Qt::PointingHandCursor);


        connect(
            button,
            &QPushButton::clicked,
            this,
            [this, amount]() {

                m_amountSpin->setValue(
                    amount);
            });


        quickLayout->addWidget(
            button,
            i / 2,
            i % 2);
    }


    walletLayout->addLayout(
        quickLayout);


    // ========================================================================
    // 自定义充值金额
    // ========================================================================
    auto *customCard =
        new QFrame(
            walletCard);

    customCard->setObjectName(
        QStringLiteral(
            "profileCustomCard"));


    auto *customLayout =
        new QHBoxLayout(
            customCard);

    customLayout->setObjectName(
        QStringLiteral(
            "profileCustomLayout"));

    customLayout->setContentsMargins(
        13,
        10,
        13,
        10);

    customLayout->setSpacing(
        10);


    auto *customLabel =
        new QLabel(
            QStringLiteral(
                "充值金额"),
            customCard);

    customLabel->setObjectName(
        QStringLiteral(
            "profileCustomLabel"));


    m_amountSpin =
        new QDoubleSpinBox(
            customCard);

    m_amountSpin->setObjectName(
        QStringLiteral(
            "profileAmountSpin"));


    // 保留原充值范围
    m_amountSpin->setRange(
        1.0,
        10000.0);

    // 保留原小数位
    m_amountSpin->setDecimals(
        2);

    // 保留原步进
    m_amountSpin->setSingleStep(
        10.0);

    // 保留原默认充值金额
    m_amountSpin->setValue(
        100.0);

    // 保留原前缀
    m_amountSpin->setPrefix(
        QStringLiteral(
            "￥"));

    m_amountSpin->setMinimumWidth(
        160);


    customLayout->addWidget(
        customLabel);

    customLayout->addStretch();

    customLayout->addWidget(
        m_amountSpin);


    walletLayout->addWidget(
        customCard);


    // ========================================================================
    // 充值按钮
    // ========================================================================
    m_rechargeButton =
        new QPushButton(
            QStringLiteral(
                "立即充值"),
            walletCard);

    m_rechargeButton->setObjectName(
        QStringLiteral(
            "profileRechargeButton"));

    m_rechargeButton->setCursor(
        Qt::PointingHandCursor);


    walletLayout->addWidget(
        m_rechargeButton);


    // ========================================================================
    // 状态提示
    // ========================================================================
    // 保留原提示文案
    m_tipLabel =
        new QLabel(
            QStringLiteral(
                "充值金额将通过系统充值接口处理"),
            walletCard);

    m_tipLabel->setObjectName(
        QStringLiteral(
            "profileTipLabel"));

    m_tipLabel->setAlignment(
        Qt::AlignCenter);

    m_tipLabel->setWordWrap(
        true);


    walletLayout->addWidget(
        m_tipLabel);


    layout->addWidget(
        walletCard);


    // ========================================================================
    // 页面说明卡
    // ========================================================================
    auto *noteCard =
        new QFrame(
            content);

    noteCard->setObjectName(
        QStringLiteral(
            "profileNoteCard"));


    auto *noteLayout =
        new QVBoxLayout(
            noteCard);

    noteLayout->setObjectName(
        QStringLiteral(
            "profileNoteLayout"));

    noteLayout->setContentsMargins(
        16,
        14,
        16,
        14);

    noteLayout->setSpacing(
        6);


    auto *noteTitle =
        new QLabel(
            QStringLiteral(
                "账户说明"),
            noteCard);

    noteTitle->setObjectName(
        QStringLiteral(
            "profileNoteTitle"));


    auto *noteText =
        new QLabel(
            QStringLiteral(
                "账户余额会在充值成功或充电支付完成后同步更新。"),
            noteCard);

    noteText->setObjectName(
        QStringLiteral(
            "profileNoteText"));

    noteText->setWordWrap(
        true);


    noteLayout->addWidget(
        noteTitle);

    noteLayout->addWidget(
        noteText);


    layout->addWidget(
        noteCard);

    layout->addStretch();


    scrollArea->setWidget(
        content);

    rootLayout->addWidget(
        scrollArea);


    // ========================================================================
    // 点击充值
    // 原业务逻辑保持不变
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


// ============================================================================
// 更新完整用户信息
// ============================================================================
void ProfilePage::setUserInfo(
    const QString &nickname,
    const QString &phone,
    double balance)
{
    // 原逻辑保持不变
    setNickname(
        nickname);


    // 原手机号显示逻辑保持不变
    m_phoneLabel->setText(
        QStringLiteral(
            "手机号：%1")
            .arg(
                phone));


    // 原余额更新逻辑保持不变
    setBalance(
        balance);
}


// ============================================================================
// 更新昵称
// ============================================================================
void ProfilePage::setNickname(
    const QString &nickname)
{
    m_nickname =
        nickname;


    // 保留原逻辑：
    // 服务器昵称为空时显示“用户”
    m_nicknameLabel->setText(
        nickname.isEmpty()
            ? QStringLiteral(
                  "用户")
            : nickname);
}


// ============================================================================
// 更新余额
// ============================================================================
void ProfilePage::setBalance(
    double balance)
{
    m_balance =
        balance;


    // 保留原金额格式
    m_balanceLabel->setText(
        QStringLiteral(
            "￥%1")
            .arg(
                m_balance,
                0,
                'f',
                2));
}


// ============================================================================
// Resize
// ============================================================================
void ProfilePage::resizeEvent(
    QResizeEvent *event)
{
    QWidget::resizeEvent(
        event);

    applyResponsiveStyle();
}


// ============================================================================
// 响应式样式
// ============================================================================
void ProfilePage::applyResponsiveStyle()
{
    QWidget *scaleBase =
        window()
            ? window()
            : this;


    const int titleFont =
        scaledUi(
            scaleBase,
            24);

    const int nicknameFont =
        scaledUi(
            scaleBase,
            19);

    const int balanceFont =
        scaledUi(
            scaleBase,
            31);

    const int normalFont =
        scaledUi(
            scaleBase,
            14);

    const int smallFont =
        scaledUi(
            scaleBase,
            12);

    const int tinyFont =
        scaledUi(
            scaleBase,
            11);

    const int buttonFont =
        scaledUi(
            scaleBase,
            13);

    const int cardRadius =
        scaledUi(
            scaleBase,
            18);

    const int smallRadius =
        scaledUi(
            scaleBase,
            10);


    setStyleSheet(
        QStringLiteral(

            // ================================================================
            // 页面
            // ================================================================
            "QWidget#profilePage{"
            "background:transparent;"
            "color:#202824;"
            "}"

            "QWidget#profileContent{"
            "background:transparent;"
            "}"

            "QScrollArea#profileScrollArea{"
            "background:transparent;"
            "border:none;"
            "}"

            // ================================================================
            // 页面标题
            // ================================================================
            "QLabel#profileTitle{"
            "background:transparent;"
            "color:#202824;"
            "font-size:%1px;"
            "font-weight:800;"
            "}"

            "QLabel#profileSubtitle{"
            "background:transparent;"
            "color:#7A837E;"
            "font-size:%2px;"
            "}"

            // ================================================================
            // 用户卡
            // ================================================================
            "QFrame#profileUserCard{"
            "background:#FFFFFF;"
            "border:1px solid #E7E3DA;"
            "border-radius:%3px;"
            "}"

            "QLabel#profileAvatar{"
            "background:transparent;"
            "border:none;"
            "}"

            "QLabel#profileUserCaption{"
            "background:transparent;"
            "color:#7A837E;"
            "font-size:%4px;"
            "}"

            "QLabel#profileNicknameLabel{"
            "background:transparent;"
            "color:#202824;"
            "font-size:%5px;"
            "font-weight:800;"
            "}"

            "QLabel#profilePhoneLabel{"
            "background:transparent;"
            "color:#7A837E;"
            "font-size:%2px;"
            "}"

            // ================================================================
            // 编辑昵称按钮
            // ================================================================
            "QPushButton#profileEditButton{"
            "background:#E9F0EC;"
            "color:#315B4D;"
            "border:1px solid #D6E1DA;"
            "border-radius:%6px;"
            "font-size:%7px;"
            "font-weight:700;"
            "padding:8px 13px;"
            "}"

            "QPushButton#profileEditButton:hover{"
            "background:#DFE9E3;"
            "}"

            // ================================================================
            // 钱包卡
            // ================================================================
            "QFrame#profileWalletCard{"
            "background:#FFFFFF;"
            "border:1px solid #E7E3DA;"
            "border-radius:%3px;"
            "}"

            "QLabel#profileWalletTitle{"
            "background:transparent;"
            "color:#202824;"
            "font-size:%8px;"
            "font-weight:800;"
            "}"

            "QLabel#profileWalletBadge{"
            "background:#E9F0EC;"
            "color:#315B4D;"
            "border:none;"
            "border-radius:%6px;"
            "font-size:%4px;"
            "font-weight:700;"
            "padding:4px 9px;"
            "}"

            "QLabel#profileBalanceCaption{"
            "background:transparent;"
            "color:#7A837E;"
            "font-size:%2px;"
            "}"

            "QLabel#profileBalanceLabel{"
            "background:transparent;"
            "color:#315B4D;"
            "font-size:%9px;"
            "font-weight:800;"
            "}"

            // ================================================================
            // 分隔线
            // ================================================================
            "QFrame#profileDivider{"
            "background:#E7E3DA;"
            "border:none;"
            "max-height:1px;"
            "}"

            // ================================================================
            // 充值文字
            // ================================================================
            "QLabel#profileAmountTitle{"
            "background:transparent;"
            "color:#202824;"
            "font-size:%10px;"
            "font-weight:700;"
            "}"

            "QLabel#profileAmountSubtitle{"
            "background:transparent;"
            "color:#7A837E;"
            "font-size:%4px;"
            "}"

            // ================================================================
            // 快捷充值
            // ================================================================
            "QPushButton#profileQuickButton{"
            "background:#FAF8F3;"
            "color:#315B4D;"
            "border:1px solid #E1E5DF;"
            "border-radius:%6px;"
            "font-size:%10px;"
            "font-weight:700;"
            "padding:10px;"
            "}"

            "QPushButton#profileQuickButton:hover{"
            "background:#E9F0EC;"
            "border-color:#C9D8CF;"
            "}"

            "QPushButton#profileQuickButton:pressed{"
            "background:#DFE9E3;"
            "}"

            // ================================================================
            // 自定义金额
            // ================================================================
            "QFrame#profileCustomCard{"
            "background:#FAF8F3;"
            "border:1px solid #E7E3DA;"
            "border-radius:%6px;"
            "}"

            "QLabel#profileCustomLabel{"
            "background:transparent;"
            "color:#202824;"
            "font-size:%10px;"
            "font-weight:600;"
            "}"

            "QDoubleSpinBox#profileAmountSpin{"
            "background:#FFFFFF;"
            "color:#202824;"
            "border:1px solid #E1DDD4;"
            "border-radius:%6px;"
            "font-size:%10px;"
            "padding:8px 10px;"
            "}"

            "QDoubleSpinBox#profileAmountSpin:focus{"
            "border:1px solid #315B4D;"
            "}"

            // ================================================================
            // 充值按钮
            // ================================================================
            "QPushButton#profileRechargeButton{"
            "background:#315B4D;"
            "color:#FFFFFF;"
            "border:none;"
            "border-radius:%6px;"
            "font-size:%10px;"
            "font-weight:700;"
            "padding:11px 18px;"
            "}"

            "QPushButton#profileRechargeButton:hover{"
            "background:#284C41;"
            "}"

            "QPushButton#profileRechargeButton:pressed{"
            "background:#203F36;"
            "}"

            // ================================================================
            // 提示
            // ================================================================
            "QLabel#profileTipLabel{"
            "background:transparent;"
            "color:#7A837E;"
            "font-size:%4px;"
            "}"

            // ================================================================
            // 账户说明
            // ================================================================
            "QFrame#profileNoteCard{"
            "background:#FAF8F3;"
            "border:1px solid #E7E3DA;"
            "border-radius:%3px;"
            "}"

            "QLabel#profileNoteTitle{"
            "background:transparent;"
            "color:#202824;"
            "font-size:%10px;"
            "font-weight:700;"
            "}"

            "QLabel#profileNoteText{"
            "background:transparent;"
            "color:#7A837E;"
            "font-size:%4px;"
            "}")

        .arg(
            titleFont)

        .arg(
            smallFont)

        .arg(
            cardRadius)

        .arg(
            tinyFont)

        .arg(
            nicknameFont)

        .arg(
            smallRadius)

        .arg(
            buttonFont)

        .arg(
            normalFont)

        .arg(
            balanceFont)

        .arg(
            normalFont));


    // ========================================================================
    // 页面内容边距
    // ========================================================================
    if (auto *contentLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "profileContentLayout"))) {

        contentLayout->setContentsMargins(
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 18));

        contentLayout->setSpacing(
            scaledUi(
                scaleBase,
                14));
    }


    // ========================================================================
    // 用户资料卡
    // ========================================================================
    if (auto *userLayout =
            findChild<QHBoxLayout *>(
                QStringLiteral(
                    "profileUserLayout"))) {

        userLayout->setContentsMargins(
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 18));

        userLayout->setSpacing(
            scaledUi(
                scaleBase,
                14));
    }


    // ========================================================================
    // 头像大小
    // ========================================================================
    if (auto *avatar =
            findChild<QLabel *>(
                QStringLiteral(
                    "profileAvatar"))) {

        const int avatarSize =
            scaledUi(
                scaleBase,
                58);

        avatar->setFixedSize(
            avatarSize,
            avatarSize);
    }


    // ========================================================================
    // 钱包卡
    // ========================================================================
    if (auto *walletLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "profileWalletLayout"))) {

        walletLayout->setContentsMargins(
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 18),
            scaledUi(scaleBase, 18));

        walletLayout->setSpacing(
            scaledUi(
                scaleBase,
                13));
    }


    // ========================================================================
    // 快捷充值网格
    // ========================================================================
    if (auto *quickLayout =
            findChild<QGridLayout *>(
                QStringLiteral(
                    "profileQuickLayout"))) {

        quickLayout->setHorizontalSpacing(
            scaledUi(
                scaleBase,
                8));

        quickLayout->setVerticalSpacing(
            scaledUi(
                scaleBase,
                8));
    }


    // ========================================================================
    // 自定义金额区
    // ========================================================================
    if (auto *customLayout =
            findChild<QHBoxLayout *>(
                QStringLiteral(
                    "profileCustomLayout"))) {

        customLayout->setContentsMargins(
            scaledUi(scaleBase, 13),
            scaledUi(scaleBase, 10),
            scaledUi(scaleBase, 13),
            scaledUi(scaleBase, 10));

        customLayout->setSpacing(
            scaledUi(
                scaleBase,
                10));
    }


    if (m_amountSpin) {

        m_amountSpin->setMinimumWidth(
            scaledUi(
                scaleBase,
                160));

        m_amountSpin->setMinimumHeight(
            scaledUi(
                scaleBase,
                40));
    }


    // ========================================================================
    // 账户说明卡
    // ========================================================================
    if (auto *noteLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "profileNoteLayout"))) {

        noteLayout->setContentsMargins(
            scaledUi(scaleBase, 16),
            scaledUi(scaleBase, 14),
            scaledUi(scaleBase, 16),
            scaledUi(scaleBase, 14));

        noteLayout->setSpacing(
            scaledUi(
                scaleBase,
                6));
    }
}
