#pragma once

#include <QObject>

#include "routerequest.h"
#include "routeresult.h"

class QNetworkAccessManager;
class QNetworkReply;

class RoutePlanner : public QObject
{
    Q_OBJECT

public:
    explicit RoutePlanner(
        QObject *parent = nullptr);

    quint64 planRoute(
        const RouteRequest &request);

    void cancelPending();

signals:
    void routeReady(
        quint64 requestId,
        const RouteResult &result);

    void routeError(
        quint64 requestId,
        const QString &message);

private:
    QNetworkAccessManager *m_networkManager = nullptr;
    QNetworkReply *m_reply = nullptr;
    quint64 m_requestId = 0;
};
