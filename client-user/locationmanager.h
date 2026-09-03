#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QString>

// ============================================================================
// 地址定位管理器
// 使用高德开放平台 Web 服务 API，将“城市 + 详细地址”解析为经纬度。
// ============================================================================

class LocationManager : public QObject
{
    Q_OBJECT

public:
    explicit LocationManager(QObject *parent = nullptr);

    // 地址 → 经纬度
    void geocode(const QString &address,
                 const QString &region = QString());

    double latitude() const
    {
        return m_latitude;
    }

    double longitude() const
    {
        return m_longitude;
    }

    bool hasLocation() const
    {
        return m_hasLocation;
    }

signals:
    // 注意参数顺序：纬度 lat，经度 lng
    void locationChanged(double lat, double lng);

    void locationError(const QString &message);

private:
    QNetworkAccessManager *m_manager = nullptr;

    double m_latitude = 0.0;
    double m_longitude = 0.0;
    bool m_hasLocation = false;

    // 防止旧请求覆盖新请求
    quint64 m_requestId = 0;

    // 高德开放平台 Web 服务 Key
    QString m_mapKey;
};
