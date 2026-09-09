#include "usermanagerwidget.h"
#include "netclient.h"
#include "protocol.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFormLayout>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
constexpr int IdColumn = 0, PhoneColumn = 1, NicknameColumn = 2, BalanceColumn = 3;
constexpr int CreatedAtColumn = 4, StatusColumn = 5, ActionColumn = 6;

QWidget *createSummary(const QString &title, QLabel **value, const QString &valueName, QWidget *parent)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("userSummaryCard"));
    auto *layout = new QHBoxLayout(card);
    layout->setContentsMargins(14, 9, 14, 9);
    layout->setSpacing(10);
    auto *titleLabel = new QLabel(title, card);
    titleLabel->setObjectName(QStringLiteral("userSummaryTitle"));
    *value = new QLabel(QStringLiteral("0"), card);
    (*value)->setObjectName(valueName);
    layout->addWidget(titleLabel);
    layout->addWidget(*value);
    return card;
}
}

UserManagerWidget::UserManagerWidget(NetClient *netClient, QWidget *parent)
    : QWidget(parent), m_net(netClient)
{
    setObjectName(QStringLiteral("userManagerPage"));
    initUI();
    loadUsers();
}

void UserManagerWidget::initUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(22, 20, 22, 20);
    mainLayout->setSpacing(14);
    auto *headingRow = new QHBoxLayout;
    auto *heading = new QLabel(QStringLiteral("用户运营台账"), this);
    heading->setObjectName(QStringLiteral("userPageHeading"));
    headingRow->addWidget(heading);
    headingRow->addStretch();
    headingRow->addWidget(createSummary(QStringLiteral("用户总数"), &m_totalValue, QStringLiteral("userSummaryValue"), this));
    headingRow->addWidget(createSummary(QStringLiteral("正常用户"), &m_normalValue, QStringLiteral("userSummaryValue"), this));
    headingRow->addWidget(createSummary(QStringLiteral("冻结用户"), &m_frozenValue, QStringLiteral("userFrozenSummaryValue"), this));
    mainLayout->addLayout(headingRow);

    auto *toolbar = new QFrame(this);
    toolbar->setObjectName(QStringLiteral("userToolbar"));
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(12, 10, 12, 10);
    toolbarLayout->setSpacing(10);
    m_searchEdit = new QLineEdit(toolbar);
    m_searchEdit->setPlaceholderText(QStringLiteral("搜索手机号 / 昵称"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setMinimumWidth(260);
    m_statusFilter = new QComboBox(toolbar);
    m_statusFilter->addItem(QStringLiteral("全部状态"), QString());
    m_statusFilter->addItem(QStringLiteral("正常"), QStringLiteral("normal"));
    m_statusFilter->addItem(QStringLiteral("冻结"), QStringLiteral("frozen"));
    auto *resetButton = new QToolButton(toolbar);
    resetButton->setObjectName(QStringLiteral("userTextAction"));
    resetButton->setText(QStringLiteral("重置"));
    m_refreshBtn = new QPushButton(QStringLiteral("刷新"), toolbar);
    toolbarLayout->addWidget(m_searchEdit);
    toolbarLayout->addWidget(m_statusFilter);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(resetButton);
    toolbarLayout->addWidget(m_refreshBtn);
    mainLayout->addWidget(toolbar);

    m_table = new QTableWidget(this);
    m_table->setObjectName(QStringLiteral("userLedgerTable"));
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({QStringLiteral("用户ID"), QStringLiteral("手机号"), QStringLiteral("昵称"),
        QStringLiteral("账户余额"), QStringLiteral("注册时间"), QStringLiteral("状态"), QStringLiteral("操作")});
    auto *header = m_table->horizontalHeader();
    header->setSectionResizeMode(IdColumn, QHeaderView::Fixed);
    header->setSectionResizeMode(PhoneColumn, QHeaderView::Stretch);
    header->setSectionResizeMode(NicknameColumn, QHeaderView::Stretch);
    header->setSectionResizeMode(BalanceColumn, QHeaderView::Fixed);
    header->setSectionResizeMode(CreatedAtColumn, QHeaderView::Stretch);
    header->setSectionResizeMode(StatusColumn, QHeaderView::Fixed);
    header->setSectionResizeMode(ActionColumn, QHeaderView::Fixed);
    m_table->setColumnWidth(IdColumn, 66);
    m_table->setColumnWidth(BalanceColumn, 100);
    m_table->setColumnWidth(StatusColumn, 78);
    m_table->setColumnWidth(ActionColumn, 108);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(48);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    mainLayout->addWidget(m_table, 1);
    m_emptyLabel = new QLabel(m_table->viewport());
    m_emptyLabel->setObjectName(QStringLiteral("userEmptyState"));
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_emptyLabel->hide();
    m_table->viewport()->installEventFilter(this);
    m_footerLabel = new QLabel(QStringLiteral("共 0 名用户"), this);
    m_footerLabel->setObjectName(QStringLiteral("userFooter"));
    mainLayout->addWidget(m_footerLabel);

    connect(m_refreshBtn, &QPushButton::clicked, this, &UserManagerWidget::loadUsers);
    connect(resetButton, &QToolButton::clicked, this, &UserManagerWidget::resetFilters);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &UserManagerWidget::applyFilters);
    connect(m_statusFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &UserManagerWidget::applyFilters);
}

void UserManagerWidget::loadUsers()
{
    if (!m_net) { QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("网络客户端不可用")); return; }
    m_refreshBtn->setEnabled(false);
    const QJsonObject response = m_net->request(Protocol::makeRequest(Protocol::MsgType::AdminUserList));
    m_refreshBtn->setEnabled(true);
    if (response.value(QStringLiteral("code")).toInt(-1) != Protocol::Ok) {
        QString message = response.value(QStringLiteral("msg")).toString();
        if (message.isEmpty()) message = QStringLiteral("用户列表加载失败，请稍后重试");
        QMessageBox::warning(this, QStringLiteral("获取用户列表失败"), message);
        return;
    }
    QVector<ManagedUserItem> users;
    const QJsonArray list = response.value(QStringLiteral("data")).toObject().value(QStringLiteral("list")).toArray();
    users.reserve(list.size());
    for (const QJsonValue &value : list) {
        const QJsonObject object = value.toObject();
        ManagedUserItem user;
        user.id = object.value(QStringLiteral("id")).toInteger();
        user.phone = object.value(QStringLiteral("phone")).toString();
        user.nickname = object.value(QStringLiteral("nickname")).toString();
        user.balance = object.value(QStringLiteral("balance")).toDouble();
        user.status = object.value(QStringLiteral("status")).toString().trimmed().toLower();
        user.createdAt = object.value(QStringLiteral("created_at")).toString();
        users.append(user);
    }
    m_users = users;
    updateSummary();
    applyFilters();
}

void UserManagerWidget::applyFilters()
{
    const QString keyword = m_searchEdit->text().trimmed();
    const QString status = m_statusFilter->currentData().toString();
    QVector<ManagedUserItem> filtered;
    for (const ManagedUserItem &user : m_users) {
        if (!keyword.isEmpty() && !user.phone.contains(keyword, Qt::CaseInsensitive)
            && !user.nickname.contains(keyword, Qt::CaseInsensitive)) continue;
        if (!status.isEmpty() && user.status != status) continue;
        filtered.append(user);
    }
    populateTable(filtered);
    const bool empty = filtered.isEmpty();
    m_emptyLabel->setText(m_users.isEmpty() ? QStringLiteral("暂无用户数据") : QStringLiteral("未找到符合条件的用户"));
    m_emptyLabel->setVisible(empty);
    if (empty) { m_emptyLabel->setGeometry(m_table->viewport()->rect()); m_emptyLabel->raise(); }
    m_footerLabel->setText(filtered.size() == m_users.size()
        ? QStringLiteral("共 %1 名用户").arg(m_users.size())
        : QStringLiteral("显示 %1 名，共 %2 名用户").arg(filtered.size()).arg(m_users.size()));
}

void UserManagerWidget::resetFilters()
{
    const QSignalBlocker searchBlocker(m_searchEdit), statusBlocker(m_statusFilter);
    m_searchEdit->clear();
    m_statusFilter->setCurrentIndex(0);
    applyFilters();
}

void UserManagerWidget::populateTable(const QVector<ManagedUserItem> &users)
{
    m_table->setRowCount(0);
    for (const ManagedUserItem &user : users) {
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        auto *id = tableItem(QString::number(user.id));
        id->setData(Qt::UserRole, user.id);
        m_table->setItem(row, IdColumn, id);
        m_table->setItem(row, PhoneColumn, tableItem(user.phone));
        m_table->setItem(row, NicknameColumn,
                         tableItem(user.nickname.trimmed().isEmpty() ? QStringLiteral("未设置")
                                                                    : user.nickname));
        m_table->setItem(row, BalanceColumn, tableItem(formatBalance(user.balance)));
        m_table->setItem(row, CreatedAtColumn, tableItem(formatDateTime(user.createdAt)));
        auto *statusItem = tableItem(QStringLiteral("●  %1").arg(statusText(user.status)));
        statusItem->setForeground(user.status == QStringLiteral("frozen") ? QColor(QStringLiteral("#EF4444")) : QColor(QStringLiteral("#22C55E")));
        m_table->setItem(row, StatusColumn, statusItem);
        auto *actions = new QWidget(m_table);
        actions->setObjectName(QStringLiteral("userActions"));
        auto *layout = new QHBoxLayout(actions);
        layout->setContentsMargins(3, 2, 3, 2);
        layout->setSpacing(2);
        auto *details = new QToolButton(actions);
        details->setObjectName(QStringLiteral("userTextAction"));
        details->setText(QStringLiteral("详情"));
        auto *more = new QToolButton(actions);
        more->setObjectName(QStringLiteral("userMoreAction"));
        more->setText(QStringLiteral("⋯"));
        more->setPopupMode(QToolButton::InstantPopup);
        auto *menu = new QMenu(more);
        const bool isFrozen = user.status == QStringLiteral("frozen");
        auto *toggle = menu->addAction(isFrozen ? QStringLiteral("解除冻结") : QStringLiteral("冻结用户"));
        more->setMenu(menu);
        connect(details, &QToolButton::clicked, this, [this, id = user.id] { showUserDetails(id); });
        connect(toggle, &QAction::triggered, this, [this, id = user.id, isFrozen, more] { setUserFrozen(id, !isFrozen, more); });
        layout->addWidget(details);
        layout->addWidget(more);
        m_table->setCellWidget(row, ActionColumn, actions);
    }
}

void UserManagerWidget::updateSummary()
{
    int normal = 0, frozen = 0;
    for (const ManagedUserItem &user : m_users) {
        if (user.status == QStringLiteral("normal")) ++normal;
        else if (user.status == QStringLiteral("frozen")) ++frozen;
    }
    m_totalValue->setText(QString::number(m_users.size()));
    m_normalValue->setText(QString::number(normal));
    m_frozenValue->setText(QString::number(frozen));
}

void UserManagerWidget::showUserDetails(qint64 userId)
{
    const ManagedUserItem *user = findUser(userId);
    if (!user) return;
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("用户详情"));
    dialog.setMinimumWidth(430);
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(22, 20, 22, 20);
    layout->setSpacing(14);
    auto *name = new QLabel(user->nickname.isEmpty() ? QStringLiteral("未设置昵称") : user->nickname, &dialog);
    name->setObjectName(QStringLiteral("userDetailHeading"));
    layout->addWidget(name);
    auto *status = new QLabel(QStringLiteral("●  %1").arg(statusText(user->status)), &dialog);
    status->setObjectName(QStringLiteral("userDetailStatus"));
    status->setProperty("userStatus", user->status);
    layout->addWidget(status);
    auto *section = new QLabel(QStringLiteral("基本信息"), &dialog);
    section->setObjectName(QStringLiteral("userDetailSection"));
    layout->addWidget(section);
    auto *form = new QFormLayout;
    form->setHorizontalSpacing(28); form->setVerticalSpacing(10);
    form->addRow(QStringLiteral("用户ID"), new QLabel(QString::number(user->id), &dialog));
    form->addRow(QStringLiteral("手机号"), new QLabel(user->phone, &dialog));
    form->addRow(QStringLiteral("注册时间"), new QLabel(formatDateTime(user->createdAt), &dialog));
    layout->addLayout(form);
    auto *account = new QLabel(QStringLiteral("账户信息"), &dialog);
    account->setObjectName(QStringLiteral("userDetailSection"));
    layout->addWidget(account);
    auto *accountForm = new QFormLayout;
    accountForm->addRow(QStringLiteral("钱包余额"), new QLabel(formatBalance(user->balance), &dialog));
    layout->addLayout(accountForm);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog.exec();
}

void UserManagerWidget::setUserFrozen(qint64 userId, bool frozen, QWidget *trigger)
{
    const ManagedUserItem *user = findUser(userId);
    if (!m_net || !user) return;
    const QString prompt = frozen
        ? QStringLiteral("确认冻结用户？\n\n用户：%1\n手机号：%2\n\n冻结后该用户将进入冻结状态。").arg(user->nickname, user->phone)
        : QStringLiteral("确认解除冻结 %1？").arg(user->nickname);
    const QString title = frozen ? QStringLiteral("确认冻结用户") : QStringLiteral("确认解除冻结");
    QMessageBox confirmation(QMessageBox::Question, title, prompt, QMessageBox::NoButton, this);
    auto *confirmButton = confirmation.addButton(
        frozen ? QStringLiteral("确认冻结") : QStringLiteral("确认解冻"), QMessageBox::AcceptRole);
    confirmation.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
    confirmation.exec();
    if (confirmation.clickedButton() != confirmButton) return;
    if (trigger) trigger->setEnabled(false);
    const QJsonObject data{{QStringLiteral("user_id"), userId}, {QStringLiteral("frozen"), frozen}};
    const QJsonObject response = m_net->request(Protocol::makeRequest(Protocol::MsgType::AdminUserFreeze, data));
    if (trigger) trigger->setEnabled(true);
    if (response.value(QStringLiteral("code")).toInt(-1) != Protocol::Ok) {
        QString message = response.value(QStringLiteral("msg")).toString();
        if (message.isEmpty()) message = frozen ? QStringLiteral("冻结失败") : QStringLiteral("解冻失败");
        QMessageBox::warning(this, QStringLiteral("操作失败"), message);
        return;
    }
    loadUsers();
    QMessageBox::information(this, QStringLiteral("操作成功"), frozen ? QStringLiteral("用户已冻结") : QStringLiteral("用户已解除冻结"));
}

const ManagedUserItem *UserManagerWidget::findUser(qint64 userId) const
{
    for (const ManagedUserItem &user : m_users) if (user.id == userId) return &user;
    return nullptr;
}

QString UserManagerWidget::statusText(const QString &status)
{
    if (status == QStringLiteral("normal")) return QStringLiteral("正常");
    if (status == QStringLiteral("frozen")) return QStringLiteral("冻结");
    return status;
}

QString UserManagerWidget::formatBalance(double balance) { return QStringLiteral("¥%1").arg(QString::number(balance, 'f', 2)); }

QString UserManagerWidget::formatDateTime(const QString &value)
{
    if (value.trimmed().isEmpty()) return QStringLiteral("--");
    QDateTime dateTime = QDateTime::fromString(value, Qt::ISODate);
    if (!dateTime.isValid()) dateTime = QDateTime::fromString(value, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    return dateTime.isValid() ? dateTime.toString(QStringLiteral("yyyy-MM-dd HH:mm")) : value;
}

QTableWidgetItem *UserManagerWidget::tableItem(const QString &text)
{
    auto *item = new QTableWidgetItem(text.isEmpty() ? QStringLiteral("--") : text);
    item->setToolTip(item->text());
    return item;
}

bool UserManagerWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_table->viewport() && event->type() == QEvent::Resize && m_emptyLabel)
        m_emptyLabel->setGeometry(m_table->viewport()->rect());
    return QWidget::eventFilter(watched, event);
}
