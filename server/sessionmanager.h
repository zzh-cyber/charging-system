#pragma once

// ============================================================================
// SessionManager - 进程内会话表（随机 token → 用户/管理员身份）
// ----------------------------------------------------------------------------
// 每个 ClientHandler 跑在独立线程里，登录与后续请求会并发读写本表，
// 因此所有访问必须经过 QMutex。这是跨 Handler 的共享可变状态。
//
// 当前阶段只提供 create/validate/revoke，dispatch 尚未强制校验。
// ============================================================================

#include <QDateTime>
#include <QHash>
#include <QMutex>
#include <QString>
#include <QtGlobal>

struct Session {
    qint64    userId = 0;
    QString   role;          // "user" / "admin"
    QDateTime lastActive;
};

class SessionManager
{
public:
    static SessionManager &instance();

    // 登录成功后调用：生成 UUID token 并写入会话表
    QString create(qint64 userId, const QString &role);

    // token 有效则回填 out、刷新 lastActive；无效/过期返回 false
    bool validate(const QString &token, Session &out);

    void revoke(const QString &token);
    void revokeByUser(qint64 userId, const QString &role = {});

private:
    SessionManager() = default;
    Q_DISABLE_COPY_MOVE(SessionManager)

    QHash<QString, Session> m_sessions;
    QMutex m_mutex;

    static constexpr int kTimeoutSec = 30 * 60;  // 30 分钟滑动过期
};
