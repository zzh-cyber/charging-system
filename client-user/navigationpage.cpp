#include "navigationpage.h"
#include "windowhelper.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QResizeEvent>


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
    backButton->setObjectName(
    QStringLiteral("navigationBackButton"));


    backButton->setCursor(
        Qt::PointingHandCursor);

    auto *title =
        new QLabel(
            QStringLiteral("一键导航"),
            this);
    title->setObjectName(
    QStringLiteral("navigationTitle"));

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
    stationTitle->setObjectName(
    QStringLiteral("navigationStationTitle"));


    stationTitle->setStyleSheet(
        "color:#86909c;"
        "font-size:13px;");

    m_stationLabel =
        new QLabel(this);
    m_stationLabel->setObjectName(
    QStringLiteral("navigationStationLabel"));


    m_stationLabel->setStyleSheet(
        "font-size:19px;"
        "font-weight:bold;"
        "color:#1d2129;");

    // =====================================================================
    // 路线摘要
    // =====================================================================
    auto *routeCard =
        new QFrame(this);
    routeCard->setObjectName(
    QStringLiteral("navigationRouteCard"));


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
    startTitle->setObjectName(
    QStringLiteral("navigationSmallTitle"));


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
    arrow->setObjectName(
    QStringLiteral("navigationArrow"));


    arrow->setAlignment(
        Qt::AlignCenter);

    arrow->setStyleSheet(
        "font-size:22px;"
        "color:#1d4ed8;");

    auto *targetTitle =
        new QLabel(
            QStringLiteral("目的地"),
            routeCard);
    targetTitle->setObjectName(
    QStringLiteral("navigationSmallTitle"));


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
applyResponsiveStyle();

}

void NavigationPage::setNavigationData(const RouteRequest &request)
{
    m_stationLabel->setText(request.toName);

    m_startLabel->setText(
        QStringLiteral(
            "%1, %2")
            .arg(
                request.fromLat,
                0,
                'f',
                6)
            .arg(
                request.fromLng,
                0,
                'f',
                6));

    m_targetLabel->setText(
        QStringLiteral(
            "%1, %2")
            .arg(
                request.toLat,
                0,
                'f',
                6)
            .arg(
                request.toLng,
                0,
                'f',
                6));

    const double distance = request.distance;
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
void NavigationPage::resizeEvent(
    QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    applyResponsiveStyle();
}


void NavigationPage::applyResponsiveStyle()
{
    QWidget *scaleBase =
        window()
            ? window()
            : this;

    const int titleFont =
        scaledUi(scaleBase, 20);

    const int stationFont =
        scaledUi(scaleBase, 19);

    const int normalFont =
        scaledUi(scaleBase, 14);

    const int smallFont =
        scaledUi(scaleBase, 12);

    const int distanceFont =
        scaledUi(scaleBase, 15);

    // 页面整体边距
    if (auto *mainLayout =
            qobject_cast<QVBoxLayout *>(
                layout())) {

        mainLayout->setContentsMargins(
            scaledUi(scaleBase, 16),
            scaledUi(scaleBase, 14),
            scaledUi(scaleBase, 16),
            scaledUi(scaleBase, 16));

        mainLayout->setSpacing(
            scaledUi(scaleBase, 14));
    }

    // 返回按钮
    if (auto *backButton =
            findChild<QPushButton *>(
                QStringLiteral(
                    "navigationBackButton"))) {

        backButton->setStyleSheet(
            QStringLiteral(
                "QPushButton{"
                "background:transparent;"
                "color:#1d4ed8;"
                "border:1px solid #1d4ed8;"
                "border-radius:%1px;"
                "padding:%2px %3px;"
                "font-size:%4px;"
                "font-weight:600;"
                "}"
                "QPushButton:hover{"
                "background:#1d4ed8;"
                "color:#ffffff;"
                "}")
                .arg(
                    scaledUi(scaleBase, 8))
                .arg(
                    scaledUi(scaleBase, 6))
                .arg(
                    scaledUi(scaleBase, 14))
                .arg(normalFont));
    }

    // 页面标题
    if (auto *title =
            findChild<QLabel *>(
                QStringLiteral(
                    "navigationTitle"))) {

        title->setStyleSheet(
            QStringLiteral(
                "font-size:%1px;"
                "font-weight:bold;")
                .arg(titleFont));
    }

    // “导航至”
    if (auto *stationTitle =
            findChild<QLabel *>(
                QStringLiteral(
                    "navigationStationTitle"))) {

        stationTitle->setStyleSheet(
            QStringLiteral(
                "color:#86909c;"
                "font-size:%1px;")
                .arg(
                    scaledUi(scaleBase, 13)));
    }

    // 目标站名
    if (m_stationLabel) {

        m_stationLabel->setStyleSheet(
            QStringLiteral(
                "font-size:%1px;"
                "font-weight:bold;"
                "color:#1d2129;")
                .arg(stationFont));
    }

    // 路线卡片
    if (auto *routeCard =
            findChild<QFrame *>(
                QStringLiteral(
                    "navigationRouteCard"))) {

        routeCard->setStyleSheet(
            QStringLiteral(
                "QFrame#navigationRouteCard{"
                "background:#ffffff;"
                "border:1px solid #d6e4ff;"
                "border-radius:%1px;"
                "}")
                .arg(
                    scaledUi(scaleBase, 10)));

        if (auto *routeLayout =
                qobject_cast<QVBoxLayout *>(
                    routeCard->layout())) {

            routeLayout->setContentsMargins(
                scaledUi(scaleBase, 16),
                scaledUi(scaleBase, 14),
                scaledUi(scaleBase, 16),
                scaledUi(scaleBase, 14));

            routeLayout->setSpacing(
                scaledUi(scaleBase, 12));
        }
    }

    // “当前位置 / 目的地”
    const auto smallTitles =
        findChildren<QLabel *>(
            QStringLiteral(
                "navigationSmallTitle"));

    for (QLabel *label : smallTitles) {

        label->setStyleSheet(
            QStringLiteral(
                "color:#86909c;"
                "font-size:%1px;")
                .arg(smallFont));
    }

    // 起点坐标
    if (m_startLabel) {

        m_startLabel->setStyleSheet(
            QStringLiteral(
                "font-size:%1px;")
                .arg(normalFont));
    }

    // 终点坐标
    if (m_targetLabel) {

        m_targetLabel->setStyleSheet(
            QStringLiteral(
                "font-size:%1px;")
                .arg(normalFont));
    }

    // 箭头
    if (auto *arrow =
            findChild<QLabel *>(
                QStringLiteral(
                    "navigationArrow"))) {

        arrow->setStyleSheet(
            QStringLiteral(
                "font-size:%1px;"
                "color:#1d4ed8;")
                .arg(
                    scaledUi(scaleBase, 22)));
    }

    // 距离
    if (m_distanceLabel) {

        m_distanceLabel->setStyleSheet(
            QStringLiteral(
                "font-size:%1px;"
                "font-weight:bold;"
                "color:#1d4ed8;")
                .arg(distanceFont));
    }
}
