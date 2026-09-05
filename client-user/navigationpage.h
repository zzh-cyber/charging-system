#pragma once

#include <QWidget>
#include "routerequest.h"

class QLabel;

class NavigationPage : public QWidget
{
    Q_OBJECT

public:
    explicit NavigationPage(
        QWidget *parent = nullptr);

    void setNavigationData(const RouteRequest &request);

signals:
    void back();

private:
    QLabel *m_stationLabel;
    QLabel *m_startLabel;
    QLabel *m_targetLabel;
    QLabel *m_distanceLabel;
};
