#include "locationmanager.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

LocationManager::LocationManager(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
{
    // TODO：替换成你自己的高德开放平台 Web 服务 Key
    m_mapKey = QStringLiteral("fb0c67955c4186d685902dd8eb477782");
}

void LocationManager::geocode(const QString &address,
                              const QString &region)
{
    const QString addr = address.trimmed();
    const QString city = region.trimmed();

    // ------------------------------------------------------------------------
    // 参数检查
    // ------------------------------------------------------------------------
    if (addr.isEmpty()) {
        emit locationError(
            QStringLiteral("请输入详细地址"));
        return;
    }

    if (m_mapKey.trimmed().isEmpty() ||
        m_mapKey == QStringLiteral("你的高德Web服务Key")) {

        emit locationError(
            QStringLiteral("请先配置高德地图 Web 服务 Key"));
        return;
    }

    // 当前请求编号
    const quint64 requestId = ++m_requestId;

    // ------------------------------------------------------------------------
    // 高德地理编码 API
    // ------------------------------------------------------------------------
    QUrl url(
        QStringLiteral(
            "https://restapi.amap.com/v3/geocode/geo"));

    QUrlQuery query;

    query.addQueryItem(
        QStringLiteral("address"),
        addr);

    if (!city.isEmpty()) {
        query.addQueryItem(
            QStringLiteral("city"),
            city);
    }

    query.addQueryItem(
        QStringLiteral("key"),
        m_mapKey);

    query.addQueryItem(
        QStringLiteral("output"),
        QStringLiteral("JSON"));

    url.setQuery(query);

    QNetworkRequest request(url);

    QNetworkReply *reply =
        m_manager->get(request);

    // ------------------------------------------------------------------------
    // 5 秒超时
    // ------------------------------------------------------------------------
    QTimer *timer = new QTimer(reply);
    timer->setSingleShot(true);
    timer->start(5000);

    connect(timer,
            &QTimer::timeout,
            reply,
            [this, reply, requestId]() {

        if (requestId != m_requestId)
            return;

        if (reply->isRunning()) {

            reply->setProperty(
                "amapTimedOut",
                true);

            reply->abort();

            emit locationError(
                QStringLiteral(
                    "地址解析超时，请稍后重试"));
        }
    });

    // ------------------------------------------------------------------------
    // 请求完成
    // ------------------------------------------------------------------------
    connect(reply,
            &QNetworkReply::finished,
            this,
            [this, reply, timer, requestId]() {

        timer->stop();

        // 已经不是最新请求，直接丢弃
        if (requestId != m_requestId) {
            reply->deleteLater();
            return;
        }

        const bool timedOut =
            reply->property(
                     "amapTimedOut")
                .toBool();

        // --------------------------------------------------------------------
        // 网络错误
        // --------------------------------------------------------------------
        if (reply->error() !=
            QNetworkReply::NoError) {

            if (!timedOut) {
                emit locationError(
                    QStringLiteral(
                        "网络请求失败：%1")
                        .arg(
                            reply->errorString()));
            }

            reply->deleteLater();
            return;
        }

        const QByteArray data =
            reply->readAll();

        reply->deleteLater();

        // --------------------------------------------------------------------
        // JSON 解析
        // --------------------------------------------------------------------
        QJsonParseError parseError;

        const QJsonDocument doc =
            QJsonDocument::fromJson(
                data,
                &parseError);

        if (parseError.error !=
                QJsonParseError::NoError ||
            !doc.isObject()) {

            emit locationError(
                QStringLiteral(
                    "地址解析失败：服务器返回数据格式错误"));
            return;
        }

        const QJsonObject root =
            doc.object();

        // --------------------------------------------------------------------
        // 高德：
        // status = "1" 成功
        // status = "0" 失败
        // --------------------------------------------------------------------
        const QString status =
            root.value(
                    QStringLiteral("status"))
                .toString();

        if (status != QStringLiteral("1")) {

            QString info =
                root.value(
                        QStringLiteral("info"))
                    .toString();

            const QString infoCode =
                root.value(
                        QStringLiteral("infocode"))
                    .toString();

            if (info.isEmpty()) {
                info =
                    QStringLiteral("未知错误");
            }

            if (!infoCode.isEmpty()) {

                emit locationError(
                    QStringLiteral(
                        "地址解析失败：%1（错误码：%2）")
                        .arg(info, infoCode));
            } else {

                emit locationError(
                    QStringLiteral(
                        "地址解析失败：%1")
                        .arg(info));
            }

            return;
        }

        // --------------------------------------------------------------------
        // 获取 geocodes
        // --------------------------------------------------------------------
        const QJsonArray geocodes =
            root.value(
                    QStringLiteral("geocodes"))
                .toArray();

        if (geocodes.isEmpty()) {

            emit locationError(
                QStringLiteral(
                    "未找到该地址，请输入更详细的地址"));
            return;
        }

        const QJsonObject result =
            geocodes.first().toObject();

        // 高德 location 格式：
        //
        // 经度,纬度
        // 116.397499,39.908722
        //
        const QString location =
            result.value(
                      QStringLiteral("location"))
                .toString();

        if (location.isEmpty()) {

            emit locationError(
                QStringLiteral(
                    "地址解析失败：未获取到经纬度"));
            return;
        }

        const QStringList parts =
            location.split(',');

        if (parts.size() != 2) {

            emit locationError(
                QStringLiteral(
                    "地址解析失败：经纬度格式错误"));
            return;
        }

        bool lngOk = false;
        bool latOk = false;

        // 高德顺序：
        // parts[0] = 经度 lng
        // parts[1] = 纬度 lat
        const double lng =
            parts.at(0).toDouble(&lngOk);

        const double lat =
            parts.at(1).toDouble(&latOk);

        if (!lngOk ||
            !latOk ||
            lat < -90.0 ||
            lat > 90.0 ||
            lng < -180.0 ||
            lng > 180.0) {

            emit locationError(
                QStringLiteral(
                    "地址解析失败：经纬度数据无效"));
            return;
        }

        // --------------------------------------------------------------------
        // 保存结果并通知 MainWindow
        // --------------------------------------------------------------------
        m_latitude = lat;
        m_longitude = lng;
        m_hasLocation = true;

        emit locationChanged(
            lat,
            lng);
    });
}
