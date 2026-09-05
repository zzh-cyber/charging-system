#include "dashboardwidget.h"

#include "netclient.h"
#include "protocol.h"

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include <QButtonGroup>
#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QEventLoop>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QTime>
#include <QToolTip>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

DashboardWidget::DashboardWidget(NetClient *netClient, QWidget *parent)
    : QWidget(parent),
      m_net(netClient),
      m_todayRevenueLabel(nullptr),
      m_monthRevenueLabel(nullptr),
      m_totalRevenueLabel(nullptr),
      m_lastUpdateLabel(nullptr),
      m_loadingLabel(nullptr),
      m_refreshButton(nullptr),
      m_sevenDaysButton(nullptr),
      m_thirtyDaysButton(nullptr),
      m_chart(nullptr),
      m_chartView(nullptr),
      m_series(nullptr),
      m_dateAxis(nullptr),
      m_valueAxis(nullptr),
      m_loading(false),
      m_currentDays(7)
{
    initUi();
    refreshData();
}

void DashboardWidget::initUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 20, 24, 20);
    mainLayout->setSpacing(18);

    auto *toolbar = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("数据总览 / 销售业绩"), this);
    title->setStyleSheet(QStringLiteral(
        "font-size: 22px; font-weight: 600; color: #2c3e50;"));
    m_loadingLabel = new QLabel(this);
    m_loadingLabel->setStyleSheet(QStringLiteral("color: #3498db;"));
    m_refreshButton = new QPushButton(QStringLiteral("刷新数据"), this);
    m_lastUpdateLabel = new QLabel(QStringLiteral("最后更新: --:--:--"), this);
    m_lastUpdateLabel->setStyleSheet(QStringLiteral("color: #7f8c8d;"));
    toolbar->addWidget(title);
    toolbar->addStretch();
    toolbar->addWidget(m_loadingLabel);
    toolbar->addWidget(m_refreshButton);
    toolbar->addWidget(m_lastUpdateLabel);
    mainLayout->addLayout(toolbar);

    auto *cardsLayout = new QHBoxLayout;
    cardsLayout->setSpacing(16);
    cardsLayout->addWidget(createKpiCard(
        QStringLiteral("今日营收"), &m_todayRevenueLabel));
    cardsLayout->addWidget(createKpiCard(
        QStringLiteral("本月营收"), &m_monthRevenueLabel));
    cardsLayout->addWidget(createKpiCard(
        QStringLiteral("总营收"), &m_totalRevenueLabel));
    mainLayout->addLayout(cardsLayout);

    auto *chartHeader = new QHBoxLayout;
    auto *chartTitle = new QLabel(QStringLiteral("营收趋势"), this);
    chartTitle->setStyleSheet(QStringLiteral(
        "font-size: 17px; font-weight: 600; color: #34495e;"));
    m_sevenDaysButton = new QRadioButton(QStringLiteral("近 7 日"), this);
    m_thirtyDaysButton = new QRadioButton(QStringLiteral("近 30 日"), this);
    m_sevenDaysButton->setChecked(true);
    auto *rangeGroup = new QButtonGroup(this);
    rangeGroup->setExclusive(true);
    rangeGroup->addButton(m_sevenDaysButton, 7);
    rangeGroup->addButton(m_thirtyDaysButton, 30);
    chartHeader->addWidget(chartTitle);
    chartHeader->addStretch();
    chartHeader->addWidget(m_sevenDaysButton);
    chartHeader->addWidget(m_thirtyDaysButton);
    mainLayout->addLayout(chartHeader);

    m_series = new QLineSeries(this);
    m_series->setName(QStringLiteral("营收"));
    m_series->setColor(QColor(QStringLiteral("#3498db")));
    m_series->setPointsVisible(true);
    m_series->setPointLabelsVisible(false);

    m_chart = new QChart;
    m_chart->addSeries(m_series);
    m_chart->legend()->hide();
    m_chart->setAnimationOptions(QChart::SeriesAnimations);
    m_chart->setBackgroundRoundness(8);
    m_chart->setMargins(QMargins(8, 8, 8, 8));

    m_dateAxis = new QDateTimeAxis(this);
    m_dateAxis->setFormat(QStringLiteral("MM-dd"));
    m_dateAxis->setTitleText(QStringLiteral("日期"));
    m_dateAxis->setTickCount(7);
    m_valueAxis = new QValueAxis(this);
    m_valueAxis->setTitleText(QStringLiteral("营业额（元）"));
    // 避免部分 Linux 字体无法显示 ¥ 导致刻度前出现问号；单位已在轴标题中说明。
    m_valueAxis->setLabelFormat(QStringLiteral("%.2f"));
    m_valueAxis->setRange(0.0, 1.0);
    m_valueAxis->setTickCount(6);
    m_chart->addAxis(m_dateAxis, Qt::AlignBottom);
    m_chart->addAxis(m_valueAxis, Qt::AlignLeft);
    m_series->attachAxis(m_dateAxis);
    m_series->attachAxis(m_valueAxis);

    m_chartView = new QChartView(m_chart, this);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    m_chartView->setMinimumHeight(380);
    m_chartView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mainLayout->addWidget(m_chartView, 1);

    connect(m_refreshButton, &QPushButton::clicked,
            this, &DashboardWidget::refreshData);
    connect(rangeGroup, &QButtonGroup::idClicked,
            this, &DashboardWidget::onRangeChanged);
    connect(m_series, &QLineSeries::hovered,
            this, &DashboardWidget::onPointHovered);
}

QWidget *DashboardWidget::createKpiCard(const QString &title, QLabel **valueLabel)
{
    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("kpiCard"));
    card->setStyleSheet(QStringLiteral(
        "QFrame#kpiCard { background: white; border: 1px solid #dfe6e9;"
        " border-radius: 8px; }"));
    card->setMinimumHeight(120);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(20, 16, 20, 16);
    auto *titleLabel = new QLabel(title, card);
    titleLabel->setStyleSheet(QStringLiteral("font-size: 14px; color: #7f8c8d;"));
    *valueLabel = new QLabel(QStringLiteral("¥0.00"), card);
    (*valueLabel)->setStyleSheet(QStringLiteral(
        "font-size: 28px; font-weight: 600; color: #2c3e50;"));
    layout->addWidget(titleLabel);
    layout->addStretch();
    layout->addWidget(*valueLabel);
    return card;
}

void DashboardWidget::onRangeChanged()
{
    m_currentDays = m_thirtyDaysButton->isChecked() ? 30 : 7;
    refreshData();
}

void DashboardWidget::refreshData()
{
    if (m_loading)
        return;
    if (!m_net) {
        QMessageBox::warning(this, QStringLiteral("刷新失败"),
                             QStringLiteral("网络客户端不可用"));
        return;
    }

    m_loading = true;
    m_refreshButton->setEnabled(false);
    m_sevenDaysButton->setEnabled(false);
    m_thirtyDaysButton->setEnabled(false);
    m_loadingLabel->setText(QStringLiteral("加载中..."));
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    const int days = m_currentDays;
    QJsonObject requestData;
    requestData.insert(QStringLiteral("days"), days);
    const QJsonObject response = m_net->request(
        Protocol::makeRequest(Protocol::MsgType::AdminRevenueTrend, requestData));

    m_loading = false;
    m_refreshButton->setEnabled(true);
    m_sevenDaysButton->setEnabled(true);
    m_thirtyDaysButton->setEnabled(true);
    m_loadingLabel->clear();

    if (response.value(QStringLiteral("code")).toInt(-1) != Protocol::Ok) {
        QString message = response.value(QStringLiteral("msg")).toString();
        if (message.isEmpty())
            message = response.value(QStringLiteral("message")).toString();
        if (message.isEmpty())
            message = QStringLiteral("营收数据请求失败，请稍后重试");
        QMessageBox::warning(this, QStringLiteral("刷新失败"), message);
        return;
    }

    const QJsonObject data = response.value(QStringLiteral("data")).toObject();
    m_todayRevenueLabel->setText(moneyText(numberValue(
        data, {QStringLiteral("todayRevenue"), QStringLiteral("today_revenue")})));
    m_monthRevenueLabel->setText(moneyText(numberValue(
        data, {QStringLiteral("monthRevenue"), QStringLiteral("month_revenue")})));
    m_totalRevenueLabel->setText(moneyText(numberValue(
        data, {QStringLiteral("totalRevenue"), QStringLiteral("total_revenue")})));
    updateChart(data, days);
    m_lastUpdateLabel->setText(
        QStringLiteral("最后更新: %1").arg(
            QTime::currentTime().toString(QStringLiteral("HH:mm:ss"))));
}

double DashboardWidget::numberValue(const QJsonObject &object,
                                    const QStringList &keys)
{
    for (const QString &key : keys) {
        const QJsonValue value = object.value(key);
        if (value.isDouble())
            return value.toDouble();
        if (value.isString()) {
            bool ok = false;
            const double result = value.toString().toDouble(&ok);
            if (ok)
                return result;
        }
    }
    return 0.0;
}

QString DashboardWidget::moneyText(double amount)
{
    if (!std::isfinite(amount) || amount < 0.0)
        amount = 0.0;
    const QLocale locale(QLocale::English, QLocale::UnitedStates);
    return QStringLiteral("¥%1").arg(locale.toString(amount, 'f', 2));
}

void DashboardWidget::updateChart(const QJsonObject &data, int days)
{
    QJsonArray trend = data.value(QStringLiteral("trend")).toArray();
    if (trend.isEmpty())
        trend = data.value(QStringLiteral("list")).toArray();
    if (trend.isEmpty())
        trend = data.value(QStringLiteral("items")).toArray();

    QHash<QDate, double> revenueByDate;
    for (const QJsonValue &value : trend) {
        const QJsonObject item = value.toObject();
        const QString dateString = item.value(QStringLiteral("date")).toString().trimmed();
        QDate date = QDate::fromString(dateString, QStringLiteral("yyyy-MM-dd"));
        if (!date.isValid())
            date = QDate::fromString(dateString, Qt::ISODate);
        if (!date.isValid())
            date = QDateTime::fromString(dateString, Qt::ISODate).date();
        if (!date.isValid())
            continue;
        double revenue = numberValue(
            item, {QStringLiteral("revenue"), QStringLiteral("amount"),
                   QStringLiteral("value")});
        if (!std::isfinite(revenue) || revenue < 0.0)
            revenue = 0.0;
        revenueByDate.insert(date, revenue);
    }

    const QDate endDate = QDate::currentDate();
    const QDate startDate = endDate.addDays(1 - days);
    QList<QPointF> points;
    points.reserve(days);
    double maximum = 0.0;
    for (int offset = 0; offset < days; ++offset) {
        const QDate date = startDate.addDays(offset);
        const double revenue = revenueByDate.value(date, 0.0);
        maximum = std::max(maximum, revenue);
        points.append(QPointF(
            QDateTime(date, QTime(0, 0)).toMSecsSinceEpoch(), revenue));
    }

    m_series->clear();
    m_series->replace(points);
    m_dateAxis->setRange(QDateTime(startDate, QTime(0, 0)),
                         QDateTime(endDate, QTime(23, 59, 59)));
    m_dateAxis->setFormat(QStringLiteral("MM-dd"));
    m_dateAxis->setTickCount(days == 30 ? 6 : 7);
    const double upperBound = maximum > 0.0 ? maximum * 1.15 : 1.0;
    m_valueAxis->setRange(0.0, upperBound);
}

void DashboardWidget::onPointHovered(const QPointF &point, bool state)
{
    if (!state) {
        QToolTip::hideText();
        return;
    }
    const QDate date = QDateTime::fromMSecsSinceEpoch(
        qRound64(point.x())).date();
    QToolTip::showText(
        QCursor::pos(),
        QStringLiteral("日期：%1\n金额：%2")
            .arg(date.toString(QStringLiteral("yyyy-MM-dd")),
                 moneyText(point.y())),
        m_chartView);
}
