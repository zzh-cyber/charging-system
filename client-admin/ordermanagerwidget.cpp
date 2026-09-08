#include "ordermanagerwidget.h"
#include "netclient.h"
#include "protocol.h"
#include <QComboBox>
#include <QDateTimeEdit>
#include <QFormLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTextFormat>
#include <QVBoxLayout>
#include <QHBoxLayout>

OrderManagerWidget::OrderManagerWidget(NetClient *net, QWidget *parent) : QWidget(parent), m_net(net) { buildUi(); loadFilterOptions(); loadPage(1); }

void OrderManagerWidget::buildUi() {
    auto *root = new QVBoxLayout(this); auto *form = new QFormLayout;
    m_orderNo = new QLineEdit; m_orderNo->setPlaceholderText(QStringLiteral("订单号"));
    m_phone = new QLineEdit; m_phone->setPlaceholderText(QStringLiteral("手机号/用户"));
    m_station = new QComboBox; m_station->addItem(QStringLiteral("全部电站"), 0);
    m_pile = new QComboBox; m_pile->addItem(QStringLiteral("全部电桩"), 0);
    m_status = new QComboBox; m_status->addItem(QStringLiteral("全部"), "");
    for (const auto &p : {qMakePair(QStringLiteral("已预约"), QStringLiteral("reserved")), qMakePair(QStringLiteral("充电中"), QStringLiteral("charging")), qMakePair(QStringLiteral("待支付"), QStringLiteral("pending_payment")), qMakePair(QStringLiteral("已结算"), QStringLiteral("settled")), qMakePair(QStringLiteral("已取消"), QStringLiteral("cancelled"))}) m_status->addItem(p.first, p.second);
    const QDate today = QDate::currentDate();
    m_start = new QDateTimeEdit; m_start->setCalendarPopup(true); m_start->setDisplayFormat("yyyy-MM-dd HH:mm"); m_start->setMinimumWidth(190); m_start->setDateTime(today.addDays(-6).startOfDay());
    m_end = new QDateTimeEdit; m_end->setCalendarPopup(true); m_end->setDisplayFormat("yyyy-MM-dd HH:mm"); m_end->setMinimumWidth(190); m_end->setDateTime(today.endOfDay());
    form->addRow(QStringLiteral("订单号"), m_orderNo); form->addRow(QStringLiteral("手机号/用户"), m_phone); form->addRow(QStringLiteral("电站"), m_station); form->addRow(QStringLiteral("电桩"), m_pile); form->addRow(QStringLiteral("状态"), m_status); form->addRow(QStringLiteral("开始时间"), m_start); form->addRow(QStringLiteral("结束时间"), m_end);
    auto *buttons = new QHBoxLayout; auto *search = new QPushButton(QStringLiteral("查询")); auto *clear = new QPushButton(QStringLiteral("重置")); auto *refresh = new QPushButton(QStringLiteral("刷新")); buttons->addWidget(search); buttons->addWidget(clear); buttons->addWidget(refresh); buttons->addStretch(); root->addLayout(form); root->addLayout(buttons);
    m_table = new QTableWidget(0, 12); m_table->setHorizontalHeaderLabels({QStringLiteral("订单号"),QStringLiteral("用户/手机号"),QStringLiteral("电站"),QStringLiteral("电桩"),QStringLiteral("状态"),QStringLiteral("充电量"),QStringLiteral("充电时长"),QStringLiteral("单价"),QStringLiteral("订单金额"),QStringLiteral("开始时间"),QStringLiteral("结束时间"),QStringLiteral("操作")}); m_table->horizontalHeader()->setStretchLastSection(true); m_table->setSelectionBehavior(QAbstractItemView::SelectRows); m_table->setEditTriggers(QAbstractItemView::NoEditTriggers); root->addWidget(m_table);
    auto *pager = new QHBoxLayout; m_total = new QLabel; m_pageLabel = new QLabel; m_prev = new QPushButton(QStringLiteral("上一页")); m_next = new QPushButton(QStringLiteral("下一页")); pager->addWidget(m_total); pager->addStretch(); pager->addWidget(m_prev); pager->addWidget(m_pageLabel); pager->addWidget(m_next); root->addLayout(pager);
    connect(search,&QPushButton::clicked,this,&OrderManagerWidget::query); connect(clear,&QPushButton::clicked,this,&OrderManagerWidget::reset); connect(refresh,&QPushButton::clicked,[this]{loadPage(m_page);}); connect(m_prev,&QPushButton::clicked,[this]{if(m_page>1)loadPage(m_page-1);}); connect(m_next,&QPushButton::clicked,[this]{if(m_page*20<m_totalCount)loadPage(m_page+1);}); connect(m_table,&QTableWidget::cellDoubleClicked,this,&OrderManagerWidget::showDetail);
}

void OrderManagerWidget::query() { loadPage(1); }
void OrderManagerWidget::loadFilterOptions() {
    if (!m_net) return;
    auto stations = m_net->request(Protocol::makeRequest(Protocol::MsgType::AdminStationList));
    if (stations.value("code").toInt(-1) == Protocol::Ok) {
        for (const auto &v : stations.value("data").toObject().value("list").toArray()) {
            const auto o = v.toObject();
            const int id = o.value("id").toInt();
            const QString name = o.value("name").toString();
            if (id > 0 && !name.isEmpty()) m_station->addItem(name, id);
        }
    }
    auto piles = m_net->request(Protocol::makeRequest(Protocol::MsgType::AdminPileList));
    if (piles.value("code").toInt(-1) == Protocol::Ok) {
        for (const auto &v : piles.value("data").toObject().value("list").toArray()) {
            const auto o = v.toObject();
            const int id = o.value("id").toInt();
            QString label = o.value("code").toString();
            if (label.isEmpty()) label = QStringLiteral("电桩 #%1").arg(id);
            if (id > 0) m_pile->addItem(label, id);
        }
    }
}
void OrderManagerWidget::reset() { m_orderNo->clear(); m_phone->clear(); m_station->setCurrentIndex(0); m_pile->setCurrentIndex(0); m_status->setCurrentIndex(0); const QDate today = QDate::currentDate(); m_start->setDateTime(today.addDays(-6).startOfDay()); m_end->setDateTime(today.endOfDay()); loadPage(1); }
QString OrderManagerWidget::valueText(const QJsonObject &o,const QString &k) const { return o.value(k).toString(); }
QString OrderManagerWidget::statusText(const QString &s) const { static const QHash<QString,QString> m{{"reserved","已预约"},{"charging","充电中"},{"pending_payment","待支付"},{"settled","已结算"},{"cancelled","已取消"}}; return m.value(s,s); }
QString OrderManagerWidget::formatDuration(qint64 s) const { return QStringLiteral("%1小时%2分%3秒").arg(s/3600).arg((s%3600)/60,2,10,QChar('0')).arg(s%60,2,10,QChar('0')); }
void OrderManagerWidget::loadPage(int page) { if(!m_net) return; QJsonObject d{{"page",page},{"page_size",20},{"order_no",m_orderNo->text().trimmed()},{"phone",m_phone->text().trimmed()},{"station_id",m_station->currentData().toInt()},{"pile_id",m_pile->currentData().toInt()},{"status",m_status->currentData().toString()},{"start_time",m_start->dateTime().isValid()?m_start->dateTime().toString(Qt::ISODate):QString()},{"end_time",m_end->dateTime().isValid()?m_end->dateTime().toString(Qt::ISODate):QString()}}; auto r=m_net->request(Protocol::makeRequest(Protocol::MsgType::AdminOrderList,d)); if(r.value("code").toInt(-1)!=Protocol::Ok){QMessageBox::warning(this,QStringLiteral("查询失败"),r.value("msg").toString());return;} auto data=r.value("data").toObject(); m_page=data.value("page").toInt(page); m_totalCount=data.value("total").toInt(); auto list=data.value("list").toArray(); m_table->setRowCount(list.size()); for(int i=0;i<list.size();++i){auto o=list[i].toObject(); QStringList v={valueText(o,"order_no"),valueText(o,"user_phone"),valueText(o,"station_name"),valueText(o,"pile_code"),statusText(valueText(o,"status")),QString::number(o.value("kwh").toDouble(),'f',2)+" kWh",formatDuration(o.value("duration_seconds").toVariant().toLongLong()),QString::number(o.value("unit_price").toDouble(),'f',2),QString::number(o.value("amount").toDouble(),'f',2),valueText(o,"start_time"),valueText(o,"end_time"),QStringLiteral("双击查看")}; for(int c=0;c<v.size();++c)m_table->setItem(i,c,new QTableWidgetItem(v[c]));} m_total->setText(QStringLiteral("共 %1 条").arg(m_totalCount)); m_pageLabel->setText(QStringLiteral("第 %1 页").arg(m_page)); m_prev->setEnabled(m_page>1); m_next->setEnabled(m_page*20<m_totalCount); }
void OrderManagerWidget::showDetail(int row,int) { if(!m_net||!m_table->item(row,0))return; QJsonObject d{{"order_no",m_table->item(row,0)->text()}}; auto r=m_net->request(Protocol::makeRequest(QStringLiteral("admin_order_detail"),d)); if(r.value("code").toInt(-1)==Protocol::Ok) QMessageBox::information(this,QStringLiteral("订单详情"),QString::fromUtf8(QJsonDocument(r.value("data").toObject()).toJson(QJsonDocument::Indented))); else QMessageBox::warning(this,QStringLiteral("查询失败"),r.value("msg").toString()); }
