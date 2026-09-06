#pragma once

#include <QWidget>
#include "routerequest.h"

class QLabel;
class QResizeEvent;

class NavigationPage : public QWidget
{
    Q_OBJECT

public:
    explicit NavigationPage(
        QWidget *parent = nullptr);

    void setNavigationData(const RouteRequest &request);

protected:
    void resizeEvent(
        QResizeEvent *event) override;

signals:
    void back();

private:
    void applyResponsiveStyle();

    QLabel *m_stationLabel;
    QLabel *m_startLabel;
    QLabel *m_targetLabel;
    QLabel *m_distanceLabel;
};
