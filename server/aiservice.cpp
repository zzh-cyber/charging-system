#include "aiservice.h"

#include <QDateTime>
#include <QEventLoop>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QMutex>
#include <QMutexLocker>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

AiService::AiService(QObject *parent)
    : QObject(parent)
{
}

QString AiService::ask(qint64 userId, const QString &question, QString *error)
{
    if (!rateLimiterAllow(userId)) {
        if (error) *error = QStringLiteral("提问太频繁，请稍后再试");
        return {};
    }

    // 命中常见问题直接回预设答案，省 API 费用
    const QString canned = fallbackAnswer(question);
    if (!canned.isEmpty())
        return canned;

    // 调大模型
    const QString answer = callApi(question, error);
    if (!answer.isEmpty())
        return answer;

    // API 失败兜底
    return QStringLiteral(
        "抱歉，E小充暂时无法连接，请稍后再试。您可以试试输入「充电」「预约」「充值」等关键词获取帮助。");
}

bool AiService::rateLimiterAllow(qint64 userId)
{
    static QMutex mutex;
    static QHash<qint64, QList<qint64>> history;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    constexpr qint64 windowMs = 60 * 1000;  // 60 秒窗口
    constexpr int maxRequests = 10;         // 每窗口最多 10 次

    QMutexLocker lock(&mutex);
    QList<qint64> &ts = history[userId];
    while (!ts.isEmpty() && now - ts.first() > windowMs)
        ts.removeFirst();
    if (ts.size() >= maxRequests)
        return false;
    ts.append(now);
    return true;
}

QString AiService::fallbackAnswer(const QString &question)
{
    struct Rule { const char *kw; const char *answer; };
    static const Rule rules[] = {
        { "预约", "在首页找到充电站，点进电桩列表，选择「闲置」状态的电桩点「预约」即可。" },
        { "充值", "在「我的」页面点「充值」，输入金额确认即可，余额实时到账。" },
        { "退款", "已结算订单暂不支持退款；未结算的可在订单页处理，或联系人工客服。" },
        { "找不到", "在首页顶部选择区域或手动输入地址重新定位，系统会按距离展示附近充电站。" },
        { "故障", "如电桩故障请换一根闲置桩；系统已记录该桩状态，管理员会尽快处理。" },
        { "结算", "充电结束后按电量和固化单价自动计费，确认后从钱包余额扣款。" },
        { "价格", "各站价格不同，可在充电站卡片上查看「元/度」价格。" },
        { "登录", "输入 11 位手机号即可免密登录，首次登录自动注册。" },
        { "余额", "在「我的」页面可查看钱包余额和充值。" },
    };
    for (const Rule &r : rules) {
        if (question.contains(QString::fromUtf8(r.kw)))
            return QString::fromUtf8(r.answer);
    }
    return {};
}

QString AiService::callApi(const QString &question, QString *error)
{
    // API Key 不写死在代码里：各成员启动前在 shell 里 export AI_API_KEY=<key>
    const QString apiKey  = qEnvironmentVariable("AI_API_KEY");
    const QString baseUrl = qEnvironmentVariable("AI_BASE_URL", QStringLiteral("https://dashscope.aliyuncs.com/compatible-mode"));
    const QString model   = qEnvironmentVariable("AI_MODEL", QStringLiteral("qwen-plus"));

    if (apiKey.isEmpty()) {
        if (error) *error = QStringLiteral("未配置 AI_API_KEY 环境变量");
        return {};
    }

    QNetworkRequest req(QUrl(baseUrl + QStringLiteral("/v1/chat/completions")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("Authorization", ("Bearer " + apiKey).toUtf8());

    QJsonObject sysMsg;
    sysMsg[QStringLiteral("role")] = QStringLiteral("system");
    sysMsg[QStringLiteral("content")] = QStringLiteral(
        "你是新能源汽车充电管理平台的 AI 客服，名叫「E小充」。请始终以「E小充」自称，"
        "不要自称 AI、AI 助手或大模型。本系统是运行在 Linux 桌面的 Qt 桌面应用程序，"
        "不是手机 App、微信小程序或网页。请用简洁、友好的中文回答用户关于充电站、充电桩、"
        "预约充电、计费结算、余额充值、订单查询等的问题，直接说明在系统界面里如何操作，"
        "不要提及「App」「小程序」「网页」「下载安装」等词语。回答控制在 200 字以内。");

    QJsonObject userMsg;
    userMsg[QStringLiteral("role")] = QStringLiteral("user");
    userMsg[QStringLiteral("content")] = question;

    QJsonObject body;
    body[QStringLiteral("model")] = model;
    body[QStringLiteral("messages")] = QJsonArray{ sysMsg, userMsg };

    QNetworkReply *reply = m_nam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(30000);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning().noquote() << "[AiService] 调用大模型失败:"
                             << reply->error()
                             << reply->errorString()
                             << "HTTP"
                             << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (error) *error = reply->errorString();
        reply->deleteLater();
        return {};
    }

    const QJsonObject resp = QJsonDocument::fromJson(reply->readAll()).object();
    reply->deleteLater();

    const QString answer = resp.value(QStringLiteral("choices")).toArray()
                               .first().toObject()
                               .value(QStringLiteral("message")).toObject()
                               .value(QStringLiteral("content")).toString();
    if (answer.isEmpty()) {
        if (error) *error = QStringLiteral("AI 返回内容为空");
        return {};
    }
    return answer;
}
