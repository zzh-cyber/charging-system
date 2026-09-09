#pragma once

// ============================================================================
// AiChatDialog - AI 客服聊天对话框（微信风格：头像 + 消息气泡）
// 用户消息靠右（蓝色气泡），AI 消息靠左（白色气泡）。
// ============================================================================

#include <QDialog>
#include <QJsonObject>
#include <QPoint>

class QEvent;
class QKeyEvent;
class QLineEdit;
class QPaintEvent;
class QPushButton;
class QScrollArea;
class QVBoxLayout;
class NetClient;

class AiChatDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AiChatDialog(NetClient *net, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *ev) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void sendQuestion();
    void onResponse(const QJsonObject &resp);

private:
    void addBubble(const QString &who, const QString &text);
    QWidget *makeAvatar(const QString &label, const QString &bgColor);
    void askQuestion(const QString &q);

    NetClient   *m_net;
    QScrollArea *m_scroll;
    QVBoxLayout *m_messageLayout;
    QLineEdit   *m_input;
    QPushButton *m_sendBtn;
    QPushButton *m_closeBtn;
    QWidget     *m_header;
    QPoint       m_dragPos;
    bool         m_dragging = false;
};
