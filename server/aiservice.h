#pragma once

// ============================================================================
// AiService - AI 客服（方案 B：调用外部大模型 API）
// ----------------------------------------------------------------------------
// 通过 OpenAI 兼容接口（DeepSeek / 通义千问等）调用大模型。
// 关键配置从环境变量读取：
//   AI_API_KEY   必填，模型 API 密钥
//   AI_BASE_URL  可选，默认 https://api.deepseek.com
//   AI_MODEL     可选，默认 deepseek-chat
//
// 防护机制：
//   1) 限流：每用户每 60 秒最多 10 次提问，防止刷接口；
//   2) 兜底：命中常见关键词直接回预设答案（省 API 费用），API 失败给默认话术。
// ============================================================================

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

class AiService : public QObject
{
    Q_OBJECT
public:
    explicit AiService(QObject *parent = nullptr);

    // 提问：先限流，命中关键词直接回预设答案，否则调大模型；失败给兜底
    QString ask(qint64 userId, const QString &question, QString *error = nullptr);

private:
    QString callApi(const QString &question, QString *error);  // 调大模型
    QString fallbackAnswer(const QString &question);           // 关键词兜底（无命中返回空）
    bool rateLimiterAllow(qint64 userId);                      // 每用户限流

    QNetworkAccessManager m_nam;
};
