#pragma once

// ============================================================================
// ClientHandler - 单个客户端连接的处理器（运行在独立线程中）
// ----------------------------------------------------------------------------
// 每来一个连接，TcpServer 就创建一个 ClientHandler 并 moveToThread 到新线程，
// 由此实现"多线程并发处理"（对应实训的多线程考点）。
// 处理器在自己的线程里：建 socket、开数据库连接、收包、分发、回包。
// ============================================================================

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

class QTcpSocket;
class Database;

class ClientHandler : public QObject
{
    Q_OBJECT
public:
    explicit ClientHandler(qintptr socketDescriptor, QObject *parent = nullptr);
    ~ClientHandler() override;

public slots:
    void start();   // 线程启动后调用：初始化 socket 与数据库连接

signals:
    void finished(); // 连接结束，通知外层回收线程

private slots:
    void onReadyRead();
    void onDisconnected();

private:
    void dispatch(const QJsonObject &req);   // 按 type 分发
    void reply(const QJsonObject &resp);     // 发送响应

    // 纯自研"AI 代执行"：识别用户操作意图（目前支持帮用户预约充电桩）
    // 并直接调用业务层执行。命中操作时返回自然语言结果/引导；未命中返回空，
    // 由调用方回落为普通文本问答。
    // actionOut 为出参：预约成功时填入结构化信息
    //   { action:"reserve_ok", order_no, station_name, pile_code }
    // 供客户端在聊天之外把"充电"页同步到"已预约"状态。
    // userLat/userLng/hasLocation：用户当前位置（可选）。当用户说"最近的充电站/附近"
    // 且没点名站点时，用它检索距离最近、且有空闲电桩的充电站来预约。
    QString tryAiAction(qint64 userId, const QString &question,
                        QJsonObject &actionOut,
                        double userLat = 0.0, double userLng = 0.0,
                        bool hasLocation = false);

    qintptr     m_descriptor;//系统交给你的 socket 编号
    QTcpSocket *m_socket = nullptr;//本线程里的 QTcpSocket
    Database   *m_db = nullptr;//本线程自己的 Database
    QByteArray  m_buffer;//粘包用的字节缓存
};
