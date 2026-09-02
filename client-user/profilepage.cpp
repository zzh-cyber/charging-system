#include "profilepage.h"

#include <QLabel>
#include <QVBoxLayout>

ProfilePage::ProfilePage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);

    auto *title = new QLabel(QStringLiteral("我的"), this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(
        "font-size:20px;"
        "font-weight:bold;"
    );

    m_nicknameLabel =
        new QLabel(QStringLiteral("昵称："), this);

    m_phoneLabel =
        new QLabel(QStringLiteral("手机号："), this);

    m_balanceLabel =
        new QLabel(QStringLiteral("钱包余额：￥0.00"), this);

    m_balanceLabel->setStyleSheet(
        "font-size:18px;"
        "font-weight:bold;"
    );

    layout->addWidget(title);
    layout->addSpacing(30);

    layout->addWidget(m_nicknameLabel);
    layout->addWidget(m_phoneLabel);
    layout->addSpacing(10);
    layout->addWidget(m_balanceLabel);

    layout->addStretch();
}

void ProfilePage::setUserInfo(
    const QString &nickname,
    const QString &phone,
    double balance)
{
    m_nicknameLabel->setText(
        QStringLiteral("昵称：%1").arg(nickname)
    );

    m_phoneLabel->setText(
        QStringLiteral("手机号：%1").arg(phone)
    );

    m_balanceLabel->setText(
        QStringLiteral("钱包余额：￥%1")
            .arg(balance, 0, 'f', 2)
    );
}
