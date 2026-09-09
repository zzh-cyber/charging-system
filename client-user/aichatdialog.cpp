#include "aichatdialog.h"

#include "netclient.h"
#include "protocol.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

// 生成默认用户头像（灰色圆底 + 白色人形）
static QPixmap defaultUserAvatar(int size)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(QStringLiteral("#c8c9cc")));
    p.drawEllipse(0, 0, size, size);
    p.setBrush(QColor(QStringLiteral("#ffffff")));
    p.drawEllipse(QPointF(size * 0.5, size * 0.38), size * 0.17, size * 0.17);
    p.drawEllipse(QRectF(size * 0.22, size * 0.62, size * 0.56, size * 0.36));
    return pm;
}

// 加载 AI 客服头像图片（资源内嵌），失败时返回空 QPixmap
static QPixmap aiAvatarPixmap(int size)
{
    const QPixmap pm(QStringLiteral(":/images/ai_avatar.png"));
    if (pm.isNull())
        return {};
    return pm.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

AiChatDialog::AiChatDialog(NetClient *net, QWidget *parent)
    : QDialog(parent)
    , m_net(net)
{
    // 无边框 + 透明背景，自绘圆角（去掉系统标题栏）
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground);
    resize(400, 560);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ---- 顶栏 ----
    m_header = new QFrame(this);
    m_header->setFixedHeight(48);
    m_header->setStyleSheet(QStringLiteral(
        "QFrame{background:#171D27;border-top-left-radius:16px;border-top-right-radius:16px;}"));
    m_header->installEventFilter(this);
    auto *headerLayout = new QHBoxLayout(m_header);
    headerLayout->setContentsMargins(16, 0, 12, 0);
    headerLayout->setSpacing(0);
    auto *title = new QLabel(QStringLiteral("E小充"), m_header);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QStringLiteral("color:white;font-size:16px;font-weight:bold;"));
    headerLayout->addWidget(title, 1);

    m_closeBtn = new QPushButton(QStringLiteral("×"), m_header);
    m_closeBtn->setFixedSize(28, 28);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setStyleSheet(QStringLiteral(
        "QPushButton{border:none;color:white;font-size:20px;border-radius:14px;}"
        "QPushButton:hover{background:rgba(255,255,255,0.25);}"));
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::close);
    headerLayout->addWidget(m_closeBtn);

    root->addWidget(m_header);

    // ---- 消息区（滚动）----
    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setStyleSheet(QStringLiteral("QScrollArea{background:#F4F6F5;border:none;}"));

    auto *container = new QWidget;
    container->setStyleSheet(QStringLiteral("background:#F4F6F5;"));
    m_messageLayout = new QVBoxLayout(container);
    m_messageLayout->setContentsMargins(12, 12, 12, 12);
    m_messageLayout->setSpacing(12);
    m_messageLayout->addStretch();
    m_scroll->setWidget(container);
    root->addWidget(m_scroll, 1);

    // ---- 常用问题快捷按钮 ----
    auto *quickBar = new QWidget(this);
    quickBar->setStyleSheet(QStringLiteral("background:#F4F6F5;"));
    auto *quickLayout = new QHBoxLayout(quickBar);
    quickLayout->setContentsMargins(12, 0, 12, 8);
    quickLayout->setSpacing(8);

    const QString quickBtnStyle = QStringLiteral(
        "QPushButton{background:#FFFFFF;color:#171D27;border:1px solid #45D86B;"
        "min-height:32px;border-radius:16px;padding:0 16px;font-size:13px;font-weight:600;}"
        "QPushButton:hover{background:#E2F9E7;border-color:#74EC8B;}");

    auto *btnReserve = new QPushButton(QStringLiteral("如何预约充电？"), quickBar);
    btnReserve->setCursor(Qt::PointingHandCursor);
    btnReserve->setStyleSheet(quickBtnStyle);

    auto *btnRecharge = new QPushButton(QStringLiteral("如何给钱包充值？"), quickBar);
    btnRecharge->setCursor(Qt::PointingHandCursor);
    btnRecharge->setStyleSheet(quickBtnStyle);

    connect(btnReserve, &QPushButton::clicked, this,
            [this]() { askQuestion(QStringLiteral("如何预约充电？")); });
    connect(btnRecharge, &QPushButton::clicked, this,
            [this]() { askQuestion(QStringLiteral("如何给钱包充值？")); });

    quickLayout->addWidget(btnReserve);
    quickLayout->addWidget(btnRecharge);
    quickLayout->addStretch();
    root->addWidget(quickBar);

    // ---- 输入区 ----
    auto *inputBar = new QFrame(this);
    inputBar->setStyleSheet(QStringLiteral(
        "QFrame{background:#FFFFFF;border-top:1px solid #E7EBE9;"
        "border-bottom-left-radius:16px;border-bottom-right-radius:16px;}"));
    auto *inputLayout = new QHBoxLayout(inputBar);
    inputLayout->setContentsMargins(12, 10, 12, 10);
    inputLayout->setSpacing(8);

    m_input = new QLineEdit(inputBar);
    m_input->setPlaceholderText(QStringLiteral("输入你的问题…"));
    m_input->setStyleSheet(QStringLiteral(
        "QLineEdit{border:1px solid #E7EBE9;border-radius:18px;padding:8px 14px;"
        "background:#F7F9F8;font-size:14px;color:#151C24;}"
        "QLineEdit::placeholder{color:#A2AAB2;}"
        "QLineEdit:focus{border-color:#45D86B;background:#FFFFFF;}"));

    m_sendBtn = new QPushButton(QStringLiteral("发送"), inputBar);
    m_sendBtn->setCursor(Qt::PointingHandCursor);
    m_sendBtn->setStyleSheet(QStringLiteral(
        "QPushButton{background:#171D27;color:#FFFFFF;border:none;"
        "min-height:36px;border-radius:18px;padding:0 22px;font-size:15px;font-weight:700;}"
        "QPushButton:hover{background:#252E3A;}"
        "QPushButton:disabled{background:#A2AAB2;color:#F4F6F5;}"));

    inputLayout->addWidget(m_input, 1);
    inputLayout->addWidget(m_sendBtn);
    root->addWidget(inputBar);

    connect(m_sendBtn, &QPushButton::clicked, this, &AiChatDialog::sendQuestion);
    connect(m_input, &QLineEdit::returnPressed, this, &AiChatDialog::sendQuestion);
    connect(m_net, &NetClient::responseReceived, this, &AiChatDialog::onResponse);

    addBubble(QStringLiteral("E小充"), QStringLiteral("你好，我是 E小充，有什么可以帮你？"));
}

void AiChatDialog::sendQuestion()
{
    const QString q = m_input->text().trimmed();
    if (q.isEmpty())
        return;
    askQuestion(q);
    m_input->clear();
}

void AiChatDialog::askQuestion(const QString &q)
{
    if (q.isEmpty())
        return;

    addBubble(QStringLiteral("user"), q);
    m_sendBtn->setEnabled(false);

    m_net->send(Protocol::makeRequest(
        Protocol::MsgType::AiChat,
        QJsonObject{ { QStringLiteral("question"), q } }));
}

void AiChatDialog::onResponse(const QJsonObject &resp)
{
    if (resp.value(QStringLiteral("type")).toString() != Protocol::MsgType::AiChat)
        return;

    m_sendBtn->setEnabled(true);

    QString answer = resp.value(QStringLiteral("data")).toObject()
                         .value(QStringLiteral("answer")).toString();
    if (answer.isEmpty())
        answer = resp.value(QStringLiteral("msg")).toString();
    addBubble(QStringLiteral("E小充"), answer);
}

QWidget *AiChatDialog::makeAvatar(const QString &label, const QString &bgColor)
{
    auto *avatar = new QLabel(label);
    avatar->setFixedSize(36, 36);
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setStyleSheet(QStringLiteral(
        "QLabel{background:%1;color:white;border-radius:18px;"
        "font-size:13px;font-weight:bold;}").arg(bgColor));
    return avatar;
}

void AiChatDialog::addBubble(const QString &who, const QString &text)
{
    const bool isUser = (who == QStringLiteral("user"));

    auto *row = new QWidget;
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(8);

    auto *bubble = new QLabel(text);
    bubble->setWordWrap(true);
    bubble->setMaximumWidth(250);
    bubble->setTextInteractionFlags(Qt::TextSelectableByMouse);

    if (isUser) {
        bubble->setStyleSheet(QStringLiteral(
            "QLabel{background:#171D27;color:#FFFFFF;border-radius:12px;"
            "padding:10px 14px;font-size:14px;}"));
        rowLayout->addStretch();
        rowLayout->addWidget(bubble);
        auto *userAvatar = new QLabel;
        userAvatar->setFixedSize(36, 36);
        userAvatar->setPixmap(defaultUserAvatar(36));
        rowLayout->addWidget(userAvatar, 0, Qt::AlignTop);
    } else {
        bubble->setStyleSheet(QStringLiteral(
            "QLabel{background:#FFFFFF;color:#151C24;border:1px solid #E7EBE9;border-radius:12px;"
            "padding:10px 14px;font-size:14px;}"));
        const QPixmap apm = aiAvatarPixmap(36);
        if (apm.isNull()) {
            // 兜底：图片缺失时用原来的绿色圆 + AI 文字
            rowLayout->addWidget(makeAvatar(QStringLiteral("E小充"), QStringLiteral("#171D27")), 0, Qt::AlignTop);
        } else {
            auto *aiAvatar = new QLabel;
            aiAvatar->setFixedSize(36, 36);
            aiAvatar->setPixmap(apm);
            rowLayout->addWidget(aiAvatar, 0, Qt::AlignTop);
        }
        rowLayout->addWidget(bubble);
        rowLayout->addStretch();
    }

    // 插到末尾的 stretch 之前，让消息从上往下排
    m_messageLayout->insertWidget(m_messageLayout->count() - 1, row);

    // 滚动到底部
    QScrollBar *bar = m_scroll->verticalScrollBar();
    QTimer::singleShot(0, bar, [bar]() { bar->setValue(bar->maximum()); });
}

void AiChatDialog::keyPressEvent(QKeyEvent *event)
{
    // 在输入框按 Enter 时，消息发送由 QLineEdit::returnPressed 触发；
    // 这里把 Enter/Return 吞掉，避免 QDialog 默认把它当作 accept() 而关闭窗口。
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        event->accept();
        return;
    }
    QDialog::keyPressEvent(event);
}

void AiChatDialog::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // 自绘圆角背景 + 浅色描边，让窗口四角圆润
    QPainterPath path;
    path.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 16, 16);
    p.fillPath(path, QColor(QStringLiteral("#F4F6F5")));
    p.setPen(QPen(QColor(QStringLiteral("#45D86B")), 1.5));
    p.drawPath(path);
}

bool AiChatDialog::eventFilter(QObject *obj, QEvent *ev)
{
    // 顶栏拖动（无边框窗口需要手动实现）；关闭按钮是顶栏子控件，点击它不会走到这里
    if (obj == m_header) {
        if (ev->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent *>(ev);
            if (me->button() == Qt::LeftButton) {
                m_dragging = true;
                m_dragPos = me->globalPosition().toPoint() - frameGeometry().topLeft();
            }
        } else if (ev->type() == QEvent::MouseMove && m_dragging) {
            auto *me = static_cast<QMouseEvent *>(ev);
            move(me->globalPosition().toPoint() - m_dragPos);
        } else if (ev->type() == QEvent::MouseButtonRelease) {
            m_dragging = false;
        }
    }
    return QDialog::eventFilter(obj, ev);
}
