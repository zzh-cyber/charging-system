#pragma once

#include <QStringList>
#include <QtGlobal>
#include <QVector>

struct RouteStep
{
    QString instruction;
    QString road;
    QString action;
    double distanceMeters = 0.0;
};

struct RouteResult
{
    double distanceMeters = 0.0;
    qint64 durationSeconds = 0;
    QStringList points;
    QVector<RouteStep> steps;
};
