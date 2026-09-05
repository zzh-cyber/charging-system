#include "navigationpage.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>


NavigationPage::NavigationPage(
    QWidget *parent)
    : QWidget(parent)
{
    auto *mainLayout =
        new QVBoxLayout(this);

    mainLayout->setContentsMargins(
        16, 14, 16, 16);

    mainLayout->setSpacing(14);

    // =====================================================================
    // 页头
    // =====================================================================
    auto *header =
        new QHBoxLayout;

    auto *backButton =
        new QPushButton(
            QStringLiteral("← 返回"),
            this);

    backButton->setCursor(
        Qt::PointingHandCursor);

    auto *title =
        new QLabel(
            QStringLiteral("一键导航"),
            this);

    title->setStyleSheet(
        "font-size:20px;"
        "font-weight:bold;");

    header->addWidget(
        backButton);

    header->addWidget(
        title,
        1);

    connect(
        backButton,
        &QPushButton::clicked,
        this,
        &NavigationPage::back);

    // =====================================================================
    // 目标站
    // =====================================================================
    auto *stationTitle =
        new QLabel(
            QStringLiteral("导航至"),
            this);

    stationTitle->setStyleSheet(
        "color:#86909c;"
        "font-size:13px;");

    m_stationLabel =
        new QLabel(this);

    m_stationLabel->setStyleSheet(
        "font-size:19px;"
        "font-weight:bold;"
        "color:#1d2129;");

    // =====================================================================
    // 路线摘要
    // =====================================================================
    auto *routeCard =
        new QFrame(this);

    routeCard->setStyleSheet(
        "QFrame{"
        "background:#ffffff;"
        "border:1px solid #d6e4ff;"
        "border-radius:10px;"
        "}");

    auto *routeLayout =
        new QVBoxLayout(
            routeCard);

    routeLayout->setContentsMargins(
        16, 14, 16, 14);

    routeLayout->setSpacing(12);

    auto *startTitle =
        new QLabel(
            QStringLiteral("当前位置"),
            routeCard);

    startTitle->setStyleSheet(
        "color:#86909c;"
        "font-size:12px;");

    m_startLabel =
        new QLabel(
            routeCard);

    m_startLabel->setStyleSheet(
        "font-size:14px;");

    auto *arrow =
        new QLabel(
            QStringLiteral("↓"),
            routeCard);

    arrow->setAlignment(
        Qt::AlignCenter);

    arrow->setStyleSheet(
        "font-size:22px;"
        "color:#1d4ed8;");

    auto *targetTitle =
        new QLabel(
            QStringLiteral("目的地"),
            routeCard);

    targetTitle->setStyleSheet(
        "color:#86909c;"
        "font-size:12px;");

    m_targetLabel =
        new QLabel(
            routeCard);

    m_targetLabel->setStyleSheet(
        "font-size:14px;");

    m_distanceLabel =
        new QLabel(
            routeCard);

    m_distanceLabel->setStyleSheet(
        "font-size:15px;"
        "font-weight:bold;"
        "color:#1d4ed8;");

    routeLayout->addWidget(
        startTitle);

    routeLayout->addWidget(
        m_startLabel);

    routeLayout->addWidget(
        arrow);

    routeLayout->addWidget(
        targetTitle);

    routeLayout->addWidget(
        m_targetLabel);

    routeLayout->addWidget(
        m_distanceLabel);

    // =====================================================================
    // 整体
    // =====================================================================
    mainLayout->addLayout(
        header);

    mainLayout->addWidget(
        stationTitle);

    mainLayout->addWidget(
        m_stationLabel);

    mainLayout->addWidget(
        routeCard);

    mainLayout->addStretch();
}

void NavigationPage::setNavigationData(
    const QString &stationName,
    double startLat,
    double startLng,
    double targetLat,
    double targetLng,
    double distance)
{
    m_stationLabel->setText(
        stationName);

    m_startLabel->setText(
        QStringLiteral(
            "%1, %2")
            .arg(
                startLat,
                0,
                'f',
                6)
            .arg(
                startLng,
                0,
                'f',
                6));

    m_targetLabel->setText(
        QStringLiteral(
            "%1, %2")
            .arg(
                targetLat,
                0,
                'f',
                6)
            .arg(
                targetLng,
                0,
                'f',
                6));

    if (distance >= 0.0) {

        m_distanceLabel->setText(
            QStringLiteral(
                "直线距离约 %1 km")
                .arg(
                    distance,
                    0,
                    'f',
                    1));

    } else {

        m_distanceLabel->setText(
            QStringLiteral(
                "距离 -- km"));
    }
}
