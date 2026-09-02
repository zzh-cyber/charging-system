#include "chargepage.h"

#include <QLabel>
#include <QVBoxLayout>

ChargePage::ChargePage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);

    auto *title = new QLabel(QStringLiteral("充电"), this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(
        "font-size:20px;"
        "font-weight:bold;"
    );

    auto *tip = new QLabel(
        QStringLiteral("请选择充电站开始充电"),
        this
    );
    tip->setAlignment(Qt::AlignCenter);
    tip->setStyleSheet("color:gray;");

    layout->addStretch();
    layout->addWidget(title);
    layout->addSpacing(10);
    layout->addWidget(tip);
    layout->addStretch();
}
