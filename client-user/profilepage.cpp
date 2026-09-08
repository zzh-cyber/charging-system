#include "profilepage.h"

#include "appmessagebox.h"
#include "uitheme.h"
#include "windowhelper.h"

#include <QColor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSize>
#include <QStandardPaths>
#include <QVBoxLayout>


// ============================================================================
// 构造函数
// ============================================================================
ProfilePage::ProfilePage(
    QWidget *parent)
    : QWidget(parent)
{
    setObjectName(
        QStringLiteral("profilePage"));


    // =========================================================================
    // 页面根布局
    // =========================================================================
    auto *rootLayout =
        new QVBoxLayout(this);

    rootLayout->setContentsMargins(
        0,
        0,
        0,
        0);

    rootLayout->setSpacing(
        0);


    // =========================================================================
    // 滚动区域
    // =========================================================================
    auto *scrollArea =
        new QScrollArea(this);

    scrollArea->setObjectName(
        QStringLiteral("profileScrollArea"));

    scrollArea->setWidgetResizable(
        true);

    scrollArea->setFrameShape(
        QFrame::NoFrame);

    scrollArea->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAlwaysOff);


    auto *content =
        new QWidget;

    content->setObjectName(
        QStringLiteral("profileContent"));


    auto *layout =
        new QVBoxLayout(content);

    layout->setObjectName(
        QStringLiteral("profileContentLayout"));

    layout->setContentsMargins(
        18,
        18,
        18,
        18);

    layout->setSpacing(
        14);


    // =========================================================================
    // 页面标题
    // =========================================================================
    auto *title =
        new QLabel(
            QStringLiteral("我的"),
            content);

    title->setObjectName(
        QStringLiteral("profileTitle"));


    auto *subtitle =
        new QLabel(
            QStringLiteral("个人资料与钱包管理"),
            content);

    subtitle->setObjectName(
        QStringLiteral("profileSubtitle"));

    subtitle->setWordWrap(
        true);


    layout->addWidget(
        title);

    layout->addWidget(
        subtitle);


    // =========================================================================
    // 用户资料卡
    // =========================================================================
    auto *userCard =
        new QFrame(content);

    userCard->setObjectName(
        QStringLiteral("profileUserCard"));

    UiTheme::applyCardShadow(
        userCard,
        18,
        4);


    auto *userLayout =
        new QHBoxLayout(userCard);

    userLayout->setObjectName(
        QStringLiteral("profileUserLayout"));

    userLayout->setContentsMargins(
        18,
        18,
        18,
        18);

    userLayout->setSpacing(
        14);


    // =========================================================================
    // NO.17：头像
    //
    // 点击头像后选择本地图片。
    // 最终头像会复制到应用数据目录，
    // 按 userId 分开保存。
    // =========================================================================
    m_avatarButton =
        new QPushButton(
            userCard);

    m_avatarButton->setObjectName(
        QStringLiteral(
            "profileAvatar"));

    m_avatarButton->setCursor(
        Qt::PointingHandCursor);

    m_avatarButton->setFlat(
        true);

    m_avatarButton->setFocusPolicy(
        Qt::NoFocus);

    m_avatarButton->setToolTip(
        QStringLiteral(
            "点击更换头像"));


    // 初始默认头像
    m_avatarPixmap =
        createDefaultAvatar(
            256);

    refreshAvatar();


    connect(
        m_avatarButton,
        &QPushButton::clicked,
        this,
        &ProfilePage::chooseAvatar);


    // =========================================================================
    // 用户信息
    // =========================================================================
    auto *infoLayout =
        new QVBoxLayout;

    infoLayout->setSpacing(
        5);


    auto *userCaption =
        new QLabel(
            QStringLiteral("账户信息"),
            userCard);

    userCaption->setObjectName(
        QStringLiteral("profileUserCaption"));


    // 保留原默认值：用户
    m_nicknameLabel =
        new QLabel(
            QStringLiteral("用户"),
            userCard);

    m_nicknameLabel->setObjectName(
        QStringLiteral("profileNicknameLabel"));


    // 保留原默认值：手机号：--
    m_phoneLabel =
        new QLabel(
            QStringLiteral("手机号：--"),
            userCard);

    m_phoneLabel->setObjectName(
        QStringLiteral("profilePhoneLabel"));

    m_phoneLabel->setWordWrap(
        true);


    infoLayout->addWidget(
        userCaption);

    infoLayout->addWidget(
        m_nicknameLabel);

    infoLayout->addWidget(
        m_phoneLabel);


    // =========================================================================
    // 编辑昵称按钮
    // =========================================================================
    m_editNickButton =
        new QPushButton(
            QStringLiteral("编辑昵称"),
            userCard);

    m_editNickButton->setObjectName(
        QStringLiteral("profileEditButton"));

    m_editNickButton->setCursor(
        Qt::PointingHandCursor);


    userLayout->addWidget(
        m_avatarButton);

    userLayout->addLayout(
        infoLayout,
        1);

    userLayout->addWidget(
        m_editNickButton,
        0,
        Qt::AlignVCenter);


    layout->addWidget(
        userCard);


    // =========================================================================
    // 修改昵称
    //
    // 业务逻辑保持不变：
    // - 初始值仍使用当前昵称
    // - 最长 20
    // - 校验仍为 2～20
    // - 确认后仍 emit nicknameChangeRequested
    //
    // UI 改为：
    // - 无系统标题栏
    // - 透明外层
    // - 米白圆角卡片
    // =========================================================================
    connect(
        m_editNickButton,
        &QPushButton::clicked,
        this,
        [this]() {

            QDialog dialog(this);

            dialog.setObjectName(
                QStringLiteral("nicknameDialog"));

            dialog.setWindowTitle(
                QStringLiteral("修改昵称"));

            dialog.setModal(
                true);

            dialog.setWindowFlags(
                Qt::Dialog |
                Qt::FramelessWindowHint);

            dialog.setAttribute(
                Qt::WA_TranslucentBackground,
                true);

            dialog.setAttribute(
                Qt::WA_InputMethodEnabled,
                true);

            dialog.setMinimumWidth(
                390);


            auto *outerLayout =
                new QVBoxLayout(
                    &dialog);

            outerLayout->setContentsMargins(
                18,
                18,
                18,
                18);

            outerLayout->setSpacing(
                0);


            auto *dialogCard =
                new QFrame(
                    &dialog);

            dialogCard->setObjectName(
                QStringLiteral(
                    "nicknameDialogCard"));

            dialogCard->setAttribute(
                Qt::WA_StyledBackground,
                true);

            UiTheme::applyCardShadow(
                dialogCard,
                24,
                6);


            auto *dialogLayout =
                new QVBoxLayout(
                    dialogCard);

            dialogLayout->setContentsMargins(
                22,
                20,
                22,
                20);

            dialogLayout->setSpacing(
                13);


            auto *dialogTitle =
                new QLabel(
                    QStringLiteral("修改昵称"),
                    dialogCard);

            dialogTitle->setObjectName(
                QStringLiteral(
                    "nicknameDialogTitle"));


            auto *label =
                new QLabel(
                    QStringLiteral(
                        "请输入新的昵称（2～20 个字符）："),
                    dialogCard);

            label->setObjectName(
                QStringLiteral(
                    "nicknameDialogTip"));

            label->setWordWrap(
                true);


            auto *edit =
                new QLineEdit(
                    dialogCard);

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

            edit->setMinimumHeight(
                42);


            auto *buttons =
                new QDialogButtonBox(
                    QDialogButtonBox::Ok |
                    QDialogButtonBox::Cancel,
                    dialogCard);

            buttons->setObjectName(
                QStringLiteral(
                    "nicknameDialogButtons"));


            auto *okButton =
                buttons->button(
                    QDialogButtonBox::Ok);

            okButton->setText(
                QStringLiteral("确定"));

            okButton->setObjectName(
                QStringLiteral(
                    "nicknameOkButton"));

            okButton->setCursor(
                Qt::PointingHandCursor);

            okButton->setMinimumWidth(
                90);

            okButton->setMinimumHeight(
                40);


            auto *cancelButton =
                buttons->button(
                    QDialogButtonBox::Cancel);

            cancelButton->setText(
                QStringLiteral("取消"));

            cancelButton->setObjectName(
                QStringLiteral(
                    "nicknameCancelButton"));

            cancelButton->setCursor(
                Qt::PointingHandCursor);

            cancelButton->setMinimumWidth(
                90);

            cancelButton->setMinimumHeight(
                40);


            dialogLayout->addWidget(
                dialogTitle);

            dialogLayout->addWidget(
                label);

            dialogLayout->addWidget(
                edit);

            dialogLayout->addSpacing(
                4);

            dialogLayout->addWidget(
                buttons);


            outerLayout->addWidget(
                dialogCard);


            dialog.setStyleSheet(
                QStringLiteral(

                    "QDialog#nicknameDialog{"
                    "background:transparent;"
                    "}"

                    "QFrame#nicknameDialogCard{"
                    "background:#F7F4EE;"
                    "border:1px solid #E2DBD0;"
                    "border-radius:20px;"
                    "}"

                    "QLabel#nicknameDialogTitle{"
                    "background:transparent;"
                    "border:none;"
                    "color:#202824;"
                    "font-size:19px;"
                    "font-weight:800;"
                    "}"

                    "QLabel#nicknameDialogTip{"
                    "background:transparent;"
                    "border:none;"
                    "color:#7A837E;"
                    "font-size:13px;"
                    "}"

                    "QLineEdit#nicknameEdit{"
                    "background:#FFFFFF;"
                    "color:#202824;"
                    "border:1px solid #E1DDD4;"
                    "border-radius:11px;"
                    "padding:9px 12px;"
                    "font-size:14px;"
                    "selection-background-color:#315B4D;"
                    "selection-color:#FFFFFF;"
                    "}"

                    "QLineEdit#nicknameEdit:focus{"
                    "border:1px solid #315B4D;"
                    "}"

                    "QDialogButtonBox#nicknameDialogButtons{"
                    "background:transparent;"
                    "border:none;"
                    "}"

                    "QPushButton#nicknameCancelButton{"
                    "background:#ECE8E0;"
                    "color:#202824;"
                    "border:1px solid #DDD6CA;"
                    "border-radius:12px;"
                    "font-size:14px;"
                    "font-weight:650;"
                    "padding:8px 16px;"
                    "}"

                    "QPushButton#nicknameCancelButton:hover{"
                    "background:#E4DFD5;"
                    "}"

                    "QPushButton#nicknameCancelButton:pressed{"
                    "background:#DCD6CB;"
                    "}"

                    "QPushButton#nicknameOkButton{"
                    "background:#315B4D;"
                    "color:#FFFFFF;"
                    "border:none;"
                    "border-radius:12px;"
                    "font-size:14px;"
                    "font-weight:700;"
                    "padding:8px 16px;"
                    "}"

                    "QPushButton#nicknameOkButton:hover{"
                    "background:#284C41;"
                    "}"

                    "QPushButton#nicknameOkButton:pressed{"
                    "background:#203F36;"
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


            edit->setFocus(
                Qt::OtherFocusReason);

            edit->selectAll();


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

                AppMessageBox::warning(
                    this,
                    QStringLiteral("昵称无效"),
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


    // =========================================================================
    // 钱包卡
    // =========================================================================
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


    // =========================================================================
    // 钱包顶部
    // =========================================================================
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


    // =========================================================================
    // 当前余额
    // =========================================================================
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


    // =========================================================================
    // 分隔线
    // =========================================================================
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


    // =========================================================================
    // 快捷充值
    // =========================================================================
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
                QStringLiteral("￥%1")
                    .arg(amount),
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


    // =========================================================================
    // 自定义充值金额
    // =========================================================================
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
        QStringLiteral("￥"));

    m_amountSpin->setMinimumWidth(
        160);


    customLayout->addWidget(
        customLabel);

    customLayout->addStretch();

    customLayout->addWidget(
        m_amountSpin);


    walletLayout->addWidget(
        customCard);


    // =========================================================================
    // 充值按钮
    // =========================================================================
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


    // =========================================================================
    // 状态提示
    // =========================================================================
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


    // =========================================================================
    // 页面说明卡
    // =========================================================================
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


    // =========================================================================
    // NO.16：退出登录
    // =========================================================================
    auto *logoutButton =
        new QPushButton(
            QStringLiteral(
                "退出登录"),
            content);

    logoutButton->setObjectName(
        QStringLiteral(
            "profileLogoutButton"));

    logoutButton->setCursor(
        Qt::PointingHandCursor);


    connect(
        logoutButton,
        &QPushButton::clicked,
        this,
        [this]() {

            emit logoutRequested();
        });


    layout->addWidget(
        logoutButton);

    layout->addStretch();


    scrollArea->setWidget(
        content);

    rootLayout->addWidget(
        scrollArea);


    // =========================================================================
    // 点击充值
    // 原业务逻辑保持不变
    // =========================================================================
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
// NO.17：设置当前用户 ID
//
// 按 userId 区分不同用户的本地头像。
// ============================================================================
void ProfilePage::setUserId(
    qint64 userId)
{
    if (m_userId ==
        userId) {

        return;
    }


    m_userId =
        userId;


    loadAvatar();
}


// ============================================================================
// NO.17：头像保存路径
//
// 不保存用户原始图片的位置，而是把处理后的头像复制到应用自己的目录。
// 用户以后移动或删除原图片，也不会影响头像。
// ============================================================================
QString ProfilePage::avatarFilePath() const
{
    if (m_userId <= 0) {

        return QString();
    }


    QString appDataPath =
        QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation);


    if (appDataPath.isEmpty()) {

        appDataPath =
            QDir::homePath() +
            QStringLiteral(
                "/.charging-user");
    }


    const QString avatarDirectory =
        appDataPath +
        QStringLiteral(
            "/avatars");


    return avatarDirectory +
           QStringLiteral(
               "/user_%1.png")
               .arg(
                   m_userId);
}


QString ProfilePage::avatarKey() const
{
    if (m_userId <= 0) {

        return QString();
    }


    return QStringLiteral(
               "avatars/user_%1.png")
        .arg(
            m_userId);
}


// ============================================================================
// NO.17：默认头像
//
// 保留原页面的绿色默认头像设计。
// ============================================================================
QPixmap ProfilePage::createDefaultAvatar(
    int size) const
{
    if (size <= 0) {

        size =
            256;
    }


    QPixmap pixmap(
        size,
        size);

    pixmap.fill(
        Qt::transparent);


    QPainter painter(
        &pixmap);

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
        size,
        size);


    const double scale =
        static_cast<double>(
            size) /
        64.0;


    painter.setBrush(
        QColor(
            "#315B4D"));


    // 头部
    painter.drawEllipse(
        QRectF(
            23.0 * scale,
            13.0 * scale,
            18.0 * scale,
            18.0 * scale));


    // 身体
    painter.drawEllipse(
        QRectF(
            14.0 * scale,
            34.0 * scale,
            36.0 * scale,
            27.0 * scale));


    painter.end();


    return pixmap;
}


// ============================================================================
// NO.17：把用户图片裁成圆形头像
//
// KeepAspectRatioByExpanding：
// - 保证头像区域完全填满
// - 不拉伸原图片
//
// 超出的部分从中心裁剪。
// ============================================================================
QPixmap ProfilePage::createCircularAvatar(
    const QPixmap &source,
    int size) const
{
    if (source.isNull() ||
        size <= 0) {

        return QPixmap();
    }


    const QPixmap scaled =
        source.scaled(
            size,
            size,
            Qt::KeepAspectRatioByExpanding,
            Qt::SmoothTransformation);


    const int cropX =
        qMax(
            0,
            (scaled.width() -
             size) /
                2);


    const int cropY =
        qMax(
            0,
            (scaled.height() -
             size) /
                2);


    QPixmap cropped =
        scaled.copy(
            cropX,
            cropY,
            size,
            size);


    QPixmap result(
        size,
        size);

    result.fill(
        Qt::transparent);


    QPainter painter(
        &result);

    painter.setRenderHint(
        QPainter::Antialiasing,
        true);

    painter.setRenderHint(
        QPainter::SmoothPixmapTransform,
        true);


    QPainterPath path;

    path.addEllipse(
        QRectF(
            0.0,
            0.0,
            static_cast<double>(
                size),
            static_cast<double>(
                size)));


    painter.setClipPath(
        path);


    painter.drawPixmap(
        0,
        0,
        cropped);


    painter.end();


    return result;
}


// ============================================================================
// NO.17：刷新头像显示
// ============================================================================
void ProfilePage::refreshAvatar()
{
    if (!m_avatarButton) {

        return;
    }


    if (m_avatarPixmap.isNull()) {

        m_avatarPixmap =
            createDefaultAvatar(
                256);
    }


    m_avatarButton->setIcon(
        QIcon(
            m_avatarPixmap));


    const int buttonWidth =
        m_avatarButton->width();

    const int buttonHeight =
        m_avatarButton->height();


    if (buttonWidth > 0 &&
        buttonHeight > 0) {

        const int iconSize =
            qMin(
                buttonWidth,
                buttonHeight);


        m_avatarButton->setIconSize(
            QSize(
                iconSize,
                iconSize));
    }
}


// ============================================================================
// NO.17：加载当前账号头像
// ============================================================================
void ProfilePage::loadAvatar()
{
    if (m_userId <= 0) {

        m_avatarPixmap =
            createDefaultAvatar(
                256);


        refreshAvatar();

        return;
    }


    const QString filePath =
        avatarFilePath();


    QPixmap pixmap;


    if (!filePath.isEmpty()) {

        pixmap.load(
            filePath);
    }


    if (pixmap.isNull()) {

        // 本地没有保存头像，使用默认头像
        m_avatarPixmap =
            createDefaultAvatar(
                256);

    } else {

        // 即使旧文件本身不是圆形，
        // 加载时也统一裁剪一次。
        m_avatarPixmap =
            createCircularAvatar(
                pixmap,
                512);
    }


    refreshAvatar();
}


// ============================================================================
// NO.17：选择并更换头像
// ============================================================================
void ProfilePage::chooseAvatar()
{
    if (m_userId <= 0) {

        AppMessageBox::warning(
            this,
            QStringLiteral(
                "无法更换头像"),
            QStringLiteral(
                "当前用户信息无效，请重新登录后再试"));

        return;
    }


    QString startDirectory =
        QStandardPaths::writableLocation(
            QStandardPaths::PicturesLocation);


    if (startDirectory.isEmpty()) {

        startDirectory =
            QDir::homePath();
    }


    const QString filePath =
        QFileDialog::getOpenFileName(
            this,
            QStringLiteral(
                "选择头像"),
            startDirectory,
            QStringLiteral(
                "图片文件 (*.png *.jpg *.jpeg *.webp *.bmp);;"
                "所有文件 (*)"));


    // 用户取消
    if (filePath.isEmpty()) {

        return;
    }


    const qint64 fileSize =
        QFileInfo(filePath).size();


    if (fileSize <= 0 ||
        fileSize > 5 * 1024 * 1024) {

        AppMessageBox::warning(
            this,
            QStringLiteral(
                "头像文件过大"),
            QStringLiteral(
                "请选择 5MB 以内的 jpg/png 图片"));

        return;
    }


    // QImageReader 可以根据 EXIF 自动处理手机照片方向
    QImageReader reader(
        filePath);

    reader.setAutoTransform(
        true);


    const QImage image =
        reader.read();


    if (image.isNull()) {

        AppMessageBox::warning(
            this,
            QStringLiteral(
                "头像读取失败"),
            QStringLiteral(
                "请选择有效的图片文件"));

        return;
    }


    const QPixmap source =
        QPixmap::fromImage(
            image);


    const QPixmap avatar =
        createCircularAvatar(
            source,
            512);


    if (avatar.isNull()) {

        AppMessageBox::warning(
            this,
            QStringLiteral(
                "头像处理失败"),
            QStringLiteral(
                "无法处理所选图片，请换一张图片重试"));

        return;
    }


    const QString key =
        avatarKey();


    if (key.isEmpty()) {

        AppMessageBox::warning(
            this,
            QStringLiteral(
                "无法更换头像"),
            QStringLiteral(
                "当前用户信息无效，请重新登录后再试"));

        return;
    }


    // 先发给服务器，成功后再落盘刷新；失败保留旧图。
    emit avatarChangeRequested(
        key,
        avatar);
}


// ============================================================================
// NO.17：服务器确认后保存本地头像并刷新界面
// ============================================================================
void ProfilePage::commitAvatar(
    const QPixmap &avatar)
{
    if (avatar.isNull() ||
        m_userId <= 0) {

        return;
    }


    const QString savePath =
        avatarFilePath();


    if (savePath.isEmpty()) {

        AppMessageBox::warning(
            this,
            QStringLiteral(
                "头像保存失败"),
            QStringLiteral(
                "无法确定头像保存位置"));

        return;
    }


    const QFileInfo fileInfo(
        savePath);


    QDir directory;


    if (!directory.mkpath(
            fileInfo.absolutePath())) {

        AppMessageBox::warning(
            this,
            QStringLiteral(
                "头像保存失败"),
            QStringLiteral(
                "无法创建头像保存目录"));

        return;
    }


    // 始终转成标准 PNG 保存。
    //
    // 即使用户原图以后被移动或删除，
    // 应用自己的头像文件仍然存在。
    if (!avatar.save(
            savePath,
            "PNG")) {

        AppMessageBox::warning(
            this,
            QStringLiteral(
                "头像保存失败"),
            QStringLiteral(
                "服务器已更新，但本地文件未能保存"));

        return;
    }


    m_avatarPixmap =
        avatar;


    refreshAvatar();
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
        QStringLiteral("手机号：%1")
            .arg(phone));


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
            ? QStringLiteral("用户")
            : nickname);
}


// ============================================================================
// 从其它页面跳转到充值区域
// ============================================================================
void ProfilePage::openRechargeSection()
{
    if (!m_amountSpin) {

        return;
    }


    if (auto *scrollArea =
            findChild<QScrollArea *>(
                QStringLiteral(
                    "profileScrollArea"))) {

        scrollArea->ensureWidgetVisible(
            m_amountSpin,
            30,
            80);
    }


    m_amountSpin->setFocus(
        Qt::OtherFocusReason);

    m_amountSpin->selectAll();
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
        QStringLiteral("￥%1")
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

            // ================================================================
            // NO.17 头像按钮
            // ================================================================
            "QPushButton#profileAvatar{"
            "background:transparent;"
            "border:none;"
            "padding:0px;"
            "}"

            "QPushButton#profileAvatar:hover{"
            "background:transparent;"
            "border:none;"
            "}"

            "QPushButton#profileAvatar:pressed{"
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
            "}"

            // ================================================================
            // 退出登录
            // ================================================================
            "QPushButton#profileLogoutButton{"
            "background:#F8EFEC;"
            "color:#B65F59;"
            "border:1px solid #E9CFCA;"
            "border-radius:%6px;"
            "font-size:%10px;"
            "font-weight:700;"
            "padding:11px 18px;"
            "}"

            "QPushButton#profileLogoutButton:hover{"
            "background:#F2E2DE;"
            "}"

            "QPushButton#profileLogoutButton:pressed{"
            "background:#ECD9D4;"
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


    // =========================================================================
    // 页面内容边距
    // =========================================================================
    if (auto *contentLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "profileContentLayout"))) {

        contentLayout->setContentsMargins(
            scaledUi(
                scaleBase,
                18),
            scaledUi(
                scaleBase,
                18),
            scaledUi(
                scaleBase,
                18),
            scaledUi(
                scaleBase,
                18));

        contentLayout->setSpacing(
            scaledUi(
                scaleBase,
                14));
    }


    // =========================================================================
    // 用户资料卡
    // =========================================================================
    if (auto *userLayout =
            findChild<QHBoxLayout *>(
                QStringLiteral(
                    "profileUserLayout"))) {

        userLayout->setContentsMargins(
            scaledUi(
                scaleBase,
                18),
            scaledUi(
                scaleBase,
                18),
            scaledUi(
                scaleBase,
                18),
            scaledUi(
                scaleBase,
                18));

        userLayout->setSpacing(
            scaledUi(
                scaleBase,
                14));
    }


    // =========================================================================
    // NO.17：头像大小
    // =========================================================================
    if (m_avatarButton) {

        const int avatarSize =
            scaledUi(
                scaleBase,
                58);


        m_avatarButton->setFixedSize(
            avatarSize,
            avatarSize);


        m_avatarButton->setIconSize(
            QSize(
                avatarSize,
                avatarSize));
    }


    // =========================================================================
    // 钱包卡
    // =========================================================================
    if (auto *walletLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "profileWalletLayout"))) {

        walletLayout->setContentsMargins(
            scaledUi(
                scaleBase,
                18),
            scaledUi(
                scaleBase,
                18),
            scaledUi(
                scaleBase,
                18),
            scaledUi(
                scaleBase,
                18));

        walletLayout->setSpacing(
            scaledUi(
                scaleBase,
                13));
    }


    // =========================================================================
    // 快捷充值网格
    // =========================================================================
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


    // =========================================================================
    // 自定义金额区
    // =========================================================================
    if (auto *customLayout =
            findChild<QHBoxLayout *>(
                QStringLiteral(
                    "profileCustomLayout"))) {

        customLayout->setContentsMargins(
            scaledUi(
                scaleBase,
                13),
            scaledUi(
                scaleBase,
                10),
            scaledUi(
                scaleBase,
                13),
            scaledUi(
                scaleBase,
                10));

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


    // =========================================================================
    // 账户说明卡
    // =========================================================================
    if (auto *noteLayout =
            findChild<QVBoxLayout *>(
                QStringLiteral(
                    "profileNoteLayout"))) {

        noteLayout->setContentsMargins(
            scaledUi(
                scaleBase,
                16),
            scaledUi(
                scaleBase,
                14),
            scaledUi(
                scaleBase,
                16),
            scaledUi(
                scaleBase,
                14));

        noteLayout->setSpacing(
            scaledUi(
                scaleBase,
                6));
    }
}
