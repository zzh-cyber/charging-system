#include "sessionmanager.h"

#include <QMutexLocker>
#include <QUuid>

SessionManager &SessionManager::instance()//单例模式，保证只有一个实例
{
    static SessionManager inst;
    return inst;
}

QString SessionManager::create(qint64 userId, const QString &role)//创建会话，生成 token 并写入会话表
{
    const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);

    Session s;
    s.userId = userId;
    s.role = role;
    s.lastActive = QDateTime::currentDateTimeUtc();

    QMutexLocker lock(&m_mutex);
    m_sessions.insert(token, s);
    return token;
}

bool SessionManager::validate(const QString &token, Session &out)//验证 token 是否有效，并获取用户信息，后续请求的身份从这里来
{
    if (token.isEmpty())
        return false;

    QMutexLocker lock(&m_mutex);
    auto it = m_sessions.find(token);
    if (it == m_sessions.end())
        return false;

    if (it->lastActive.secsTo(QDateTime::currentDateTimeUtc()) > kTimeoutSec) {
        m_sessions.erase(it);
        return false;
    }

    it->lastActive = QDateTime::currentDateTimeUtc();
    out = it.value();
    return true;
}

void SessionManager::revoke(const QString &token)//注销会话，从会话表中删除
{
    if (token.isEmpty())
        return;
    QMutexLocker lock(&m_mutex);
    m_sessions.remove(token);
}

void SessionManager::revokeByUser(qint64 userId, const QString &role)//注销用户所有会话，从会话表中删除
{
    QMutexLocker lock(&m_mutex);
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ) {
        const bool matchUser = (it->userId == userId);
        const bool matchRole = role.isEmpty() || (it->role == role);
        if (matchUser && matchRole)
            it = m_sessions.erase(it);
        else
            ++it;
    }
}
