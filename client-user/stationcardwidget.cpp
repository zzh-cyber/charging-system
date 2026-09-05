#include "stationcardwidget.h"
#include "windowhelper.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QResizeEvent>

#include <cmath>

namespace {

bool readNumber(
    const QJsonObject &obj,
    const QString &key,
    double &value)
{
    const QJsonValue v =
        obj.value(key);

    if (!v.isDouble())
        return false;

    value = v.toDouble();

    return std::isfinite(value);
}

bool validCoordinate(
    double lat,
    double lng)
{
    return std::isfinite(lat) &&
           std::isfinite(lng) &&
           lat >= -90.0 &&
           lat <= 90.0 &&
           lng >= -180.0 &&
           lng <= 180.0;
}

} // namespace

StationCardWidget::StationCardWidget(
    const QJsonObject &station,
    QWidget *parent)
    : QFrame(parent)
{
    setObjectName(
        QStringLiteral("stationCard"));
    setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Preferred);


    setStyleSheet(
        "#stationCard{"
        "background:#ffffff;"
        "border:1px solid #d6e4ff;"
        "border-radius:10px;"
        "}"
        "#stationCard:hover{"
        "border-color:#1d4ed8;"
        "}");

    // =====================================================================
    // 业务主键
    // =====================================================================
    m_stationId =
        station.value(
            QStringLiteral("id"))
            .toVariant()
            .toLongLong();

    m_name =
        station.value(
            QStringLiteral("name"))
            .toString()
            .trimmed();

    if (m_name.isEmpty())
        m_name = QStringLiteral("--");

    const QString address =
        station.value(
            QStringLiteral("address"))
            .toString()
            .trimmed();

    // =====================================================================
    // 字段安全读取
    // =====================================================================
    double price = 0.0;

    const bool hasPrice =
        readNumber(
            station,
            QStringLiteral("price"),
            price) &&
        price >= 0.0;

    double totalValue = 0.0;
    double idleValue = 0.0;

    const bool hasTotal =
        readNumber(
            station,
            QStringLiteral("total"),
            totalValue) &&
        totalValue >= 0.0;

    const bool hasIdle =
        readNumber(
            station,
            QStringLiteral("idle"),
            idleValue) &&
        idleValue >= 0.0;

    const int total =
        static_cast<int>(totalValue);

    const int idle =
        static_cast<int>(idleValue);

    // =====================================================================
    // 经纬度
    // =====================================================================
    double lat = 0.0;
    double lng = 0.0;

    const bool hasLat =
        readNumber(
            station,
            QStringLiteral("latitude"),
            lat);

    const bool hasLng =
        readNumber(
            station,
            QStringLiteral("longitude"),
            lng);

    if (hasLat &&
        hasLng &&
        validCoordinate(lat, lng)) {

        m_latitude = lat;
        m_longitude = lng;
        m_hasCoordinate = true;
    }

    // =====================================================================
    // 距离
    // =====================================================================
    double distance = 0.0;

    if (readNumber(
            station,
            QStringLiteral("distance"),
            distance) &&
        distance >= 0.0) {

        m_distance = distance;
        m_hasDistance = true;
    }

    // =====================================================================
    // 是否已满
    // =====================================================================
    const bool full =
        hasIdle &&
        hasTotal &&
        idle == 0;

    // =====================================================================
    // UI
    // =====================================================================
    auto *mainLayout =
        new QVBoxLayout(this);

    mainLayout->setContentsMargins(
        14, 12, 14, 12);

    mainLayout->setSpacing(7);

    // 第一行：站名 + 价格
    auto *topRow =
        new QHBoxLayout;

    auto *nameLabel =
        new QLabel(
            m_name,
            this);
    nameLabel->setObjectName(
    QStringLiteral("stationNameLabel"));


    nameLabel->setStyleSheet(
        "font-size:17px;"
        "font-weight:bold;"
        "color:#1d2129;");

    QString priceText =
        QStringLiteral("--");

    if (hasPrice) {
        priceText =
            QStringLiteral(
                "￥%1/度")
                .arg(
                    price,
                    0,
                    'f',
                    2);
    }

    auto *priceLabel =
        new QLabel(
            priceText,
            this);
    priceLabel->setObjectName(
    QStringLiteral("stationPriceLabel"));


    priceLabel->setStyleSheet(
        "font-size:15px;"
        "font-weight:bold;"
        "color:#ff6a00;");

    topRow->addWidget(
        nameLabel,
        1);

    topRow->addWidget(
        priceLabel);

    // 地址
    auto *addressLabel =
        new QLabel(
            address.isEmpty()
                ? QStringLiteral("--")
                : address,
            this);
    addressLabel->setObjectName(
    QStringLiteral("stationAddressLabel"));


    addressLabel->setWordWrap(true);

    addressLabel->setStyleSheet(
        "font-size:12px;"
        "color:#86909c;");

    // 空闲状态
    QString idleText;

    if (!hasIdle ||
        !hasTotal) {

        idleText =
            QStringLiteral(
                "空闲 --/--");

    } else if (full) {

        idleText =
            QStringLiteral("已满");

    } else {

        idleText =
            QStringLiteral(
                "空闲 %1/%2")
                .arg(idle)
                .arg(total);
    }

    auto *idleLabel =
        new QLabel(
            idleText,
            this);
    idleLabel->setObjectName(
        QStringLiteral("stationIdleLabel"));

    idleLabel->setProperty(
        "stationFull",
        full);


    if (full) {

        idleLabel->setStyleSheet(
            "font-size:13px;"
            "font-weight:bold;"
            "color:#999999;");

    } else {

        idleLabel->setStyleSheet(
            "font-size:13px;"
            "font-weight:bold;"
            "color:#16a34a;");
    }

    // 距离
    const QString distanceText =
        m_hasDistance
            ? QStringLiteral(
                  "距您 %1 km")
                  .arg(
                      m_distance,
                      0,
                      'f',
                      1)
            : QStringLiteral(
                  "距您 -- km");

    auto *distanceLabel =
        new QLabel(
            distanceText,
            this);
    distanceLabel->setObjectName(
        QStringLiteral("stationDistanceLabel"));

    distanceLabel->setStyleSheet(
        "font-size:12px;"
        "color:#4e5969;");

    // 底部信息
    auto *infoRow =
        new QHBoxLayout;

    infoRow->addWidget(
        idleLabel);

    infoRow->addStretch();

    infoRow->addWidget(
        distanceLabel);

    // =====================================================================
    // 操作按钮
    // =====================================================================
    auto *buttonRow =
        new QHBoxLayout;

    buttonRow->addStretch();

    auto *pileButton =
        new QPushButton(
            QStringLiteral(
                "查看充电桩"),
            this);
    pileButton->setObjectName(
    QStringLiteral("stationActionButton"));


    auto *navigationButton =
        new QPushButton(
            QStringLiteral("导航"),
            this);
    navigationButton->setObjectName(
    QStringLiteral("stationActionButton"));


    const QString buttonStyle =
        "QPushButton{"
        "border:1px solid #1d4ed8;"
        "border-radius:6px;"
        "padding:5px 12px;"
        "color:#1d4ed8;"
        "background:#ffffff;"
        "}"
        "QPushButton:hover{"
        "background:#eef4ff;"
        "}"
        "QPushButton:disabled{"
        "border-color:#d9d9d9;"
        "color:#bfbfbf;"
        "background:#f5f5f5;"
        "}";

    pileButton->setStyleSheet(
        buttonStyle);

    navigationButton->setStyleSheet(
        buttonStyle);

    pileButton->setCursor(
        Qt::PointingHandCursor);

    navigationButton->setCursor(
        Qt::PointingHandCursor);

    // 已满时弱化“查看充电桩”
    pileButton->setEnabled(
        !full &&
        m_stationId > 0);

    // 没有目标坐标不能导航
    navigationButton->setEnabled(
        m_stationId > 0 &&
        m_hasCoordinate);

    buttonRow->addWidget(
        pileButton);

    buttonRow->addWidget(
        navigationButton);

    mainLayout->addLayout(
        topRow);

    mainLayout->addWidget(
        addressLabel);

    mainLayout->addLayout(
        infoRow);

    mainLayout->addLayout(
        buttonRow);

    // =====================================================================
    // 信号
    // =====================================================================
    connect(
        pileButton,
        &QPushButton::clicked,
        this,
        [this]() {

            emit stationSelected(
                m_stationId,
                m_name);
        });

    connect(
        navigationButton,
        &QPushButton::clicked,
        this,
        [this]() {

            emit navigationRequested(
                m_stationId,
                m_name,
                m_latitude,
                m_longitude,
                m_distance);
        });
applyResponsiveStyle();

}
void StationCardWidget::resizeEvent(
    QResizeEvent *event)
{
    QFrame::resizeEvent(event);

    applyResponsiveStyle();
}


void StationCardWidget::applyResponsiveStyle()
{
    QWidget *scaleBase =
        window()
            ? window()
            : this;

    const int nameFont =
        scaledUi(scaleBase, 17);

    const int priceFont =
        scaledUi(scaleBase, 15);

    const int smallFont =
        scaledUi(scaleBase, 12);

    const int idleFont =
        scaledUi(scaleBase, 13);

    const int buttonFont =
        scaledUi(scaleBase, 15);

    // ---------------------------------------------------------
    // 卡片内部边距和间距
    // ---------------------------------------------------------
    if (auto *mainLayout =
            qobject_cast<QVBoxLayout *>(
                layout())) {

        mainLayout->setContentsMargins(
            scaledUi(scaleBase, 14),
            scaledUi(scaleBase, 12),
            scaledUi(scaleBase, 14),
            scaledUi(scaleBase, 12));

        mainLayout->setSpacing(
            scaledUi(scaleBase, 7));
    }

    // ---------------------------------------------------------
    // 站名
    // ---------------------------------------------------------
    if (auto *label =
            findChild<QLabel *>(
                QStringLiteral(
                    "stationNameLabel"))) {

        label->setStyleSheet(
            QStringLiteral(
                "font-size:%1px;"
                "font-weight:bold;"
                "color:#1d2129;")
                .arg(nameFont));
    }

    // ---------------------------------------------------------
    // 价格
    // ---------------------------------------------------------
    if (auto *label =
            findChild<QLabel *>(
                QStringLiteral(
                    "stationPriceLabel"))) {

        label->setStyleSheet(
            QStringLiteral(
                "font-size:%1px;"
                "font-weight:bold;"
                "color:#ff6a00;")
                .arg(priceFont));
    }

    // ---------------------------------------------------------
    // 地址
    // ---------------------------------------------------------
    if (auto *label =
            findChild<QLabel *>(
                QStringLiteral(
                    "stationAddressLabel"))) {

        label->setStyleSheet(
            QStringLiteral(
                "font-size:%1px;"
                "color:#86909c;")
                .arg(smallFont));
    }

    // ---------------------------------------------------------
    // 空闲 / 已满
    // ---------------------------------------------------------
    if (auto *label =
            findChild<QLabel *>(
                QStringLiteral(
                    "stationIdleLabel"))) {

        const bool full =
            label->property(
                "stationFull")
                .toBool();

        label->setStyleSheet(
            QStringLiteral(
                "font-size:%1px;"
                "font-weight:bold;"
                "color:%2;")
                .arg(idleFont)
                .arg(
                    full
                        ? QStringLiteral(
                              "#999999")
                        : QStringLiteral(
                              "#16a34a")));
    }

    // ---------------------------------------------------------
    // 距离
    // ---------------------------------------------------------
    if (auto *label =
            findChild<QLabel *>(
                QStringLiteral(
                    "stationDistanceLabel"))) {

        label->setStyleSheet(
            QStringLiteral(
                "font-size:%1px;"
                "color:#4e5969;")
                .arg(smallFont));
    }

    // ---------------------------------------------------------
    // 两个操作按钮
    // ---------------------------------------------------------
    const auto buttons =
        findChildren<QPushButton *>(
            QStringLiteral(
                "stationActionButton"));

    for (QPushButton *button :
         buttons) {

        button->setStyleSheet(
            QStringLiteral(
                "QPushButton{"
                "border:1px solid #1d4ed8;"
                "border-radius:%1px;"
                "padding:%2px %3px;"
                "font-size:%4px;"
                "color:#1d4ed8;"
                "background:#ffffff;"
                "}"
                "QPushButton:hover{"
                "background:#eef4ff;"
                "}"
                "QPushButton:disabled{"
                "border-color:#d9d9d9;"
                "color:#bfbfbf;"
                "background:#f5f5f5;"
                "}")
                .arg(
                    scaledUi(
                        scaleBase,
                        6))
                .arg(
                    scaledUi(
                        scaleBase,
                        5))
                .arg(
                    scaledUi(
                        scaleBase,
                        12))
                .arg(buttonFont));
    }
}
