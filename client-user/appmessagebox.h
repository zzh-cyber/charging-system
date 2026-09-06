#ifndef APPMESSAGEBOX_H
#define APPMESSAGEBOX_H

#include "uitheme.h"

#include <QDialog>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QVBoxLayout>

class AppMessageBox : public QDialog
{
public:
    enum class Type
    {
        Information,
        Warning,
        Question
    };

    explicit AppMessageBox(
        QWidget *parent,
        Type type,
        const QString &title,
        const QString &message,
        const QString &acceptText = QStringLiteral("确定"),
        const QString &rejectText = QString())
        : QDialog(parent)
    {
        setWindowFlags(
            Qt::Dialog |
            Qt::FramelessWindowHint);

        setAttribute(
            Qt::WA_TranslucentBackground);

        setModal(true);

        setObjectName(
            QStringLiteral("appMessageDialog"));

        setMinimumWidth(360);
        setMaximumWidth(500);


        // ================================================================
        // 外层透明区域，用来给阴影留空间
        // ================================================================
        auto *outerLayout =
            new QVBoxLayout(this);

        outerLayout->setContentsMargins(
            20,
            20,
            20,
            20);


        // ================================================================
        // 米白色圆角主体
        // ================================================================
        auto *card =
            new QFrame(this);

        card->setObjectName(
            QStringLiteral("messageCard"));

        card->setAttribute(
            Qt::WA_StyledBackground,
            true);


        auto *shadow =
            new QGraphicsDropShadowEffect(card);

        shadow->setBlurRadius(28);
        shadow->setOffset(0, 7);
        shadow->setColor(
            QColor(40, 46, 42, 55));

        card->setGraphicsEffect(shadow);


        auto *cardLayout =
            new QVBoxLayout(card);

        cardLayout->setContentsMargins(
            22,
            18,
            22,
            20);

        cardLayout->setSpacing(16);


        // ================================================================
        // 标题行
        // ================================================================
        auto *headerLayout =
            new QHBoxLayout;

        headerLayout->setSpacing(10);


        auto *titleLabel =
            new QLabel(
                title,
                card);

        titleLabel->setObjectName(
            QStringLiteral("messageTitle"));


        auto *closeButton =
            new QPushButton(
                QStringLiteral("×"),
                card);

        closeButton->setObjectName(
            QStringLiteral("messageCloseButton"));

        closeButton->setFixedSize(
            30,
            30);

        closeButton->setCursor(
            Qt::PointingHandCursor);


        headerLayout->addWidget(
            titleLabel);

        headerLayout->addStretch();

        headerLayout->addWidget(
            closeButton);


        cardLayout->addLayout(
            headerLayout);


        // ================================================================
        // 内容
        // ================================================================
        auto *contentLayout =
            new QHBoxLayout;

        contentLayout->setSpacing(14);


        auto *iconLabel =
            new QLabel(card);

        iconLabel->setObjectName(
            QStringLiteral("messageIcon"));

        iconLabel->setAlignment(
            Qt::AlignCenter);

        iconLabel->setFixedSize(
            42,
            42);


        if (type == Type::Warning) {

            iconLabel->setText(
                QStringLiteral("!"));

            iconLabel->setProperty(
                "messageType",
                QStringLiteral("warning"));

        } else {

            iconLabel->setText(
                QStringLiteral("i"));

            iconLabel->setProperty(
                "messageType",
                QStringLiteral("info"));
        }


        auto *messageLabel =
            new QLabel(
                message,
                card);

        messageLabel->setObjectName(
            QStringLiteral("messageText"));

        messageLabel->setWordWrap(true);

        messageLabel->setAlignment(
            Qt::AlignLeft |
            Qt::AlignVCenter);


        contentLayout->addWidget(
            iconLabel,
            0,
            Qt::AlignTop);

        contentLayout->addWidget(
            messageLabel,
            1);


        cardLayout->addLayout(
            contentLayout);


        // ================================================================
        // 按钮区
        // ================================================================
        auto *buttonLayout =
            new QHBoxLayout;

        buttonLayout->setSpacing(10);

        buttonLayout->addStretch();


        if (!rejectText.isEmpty()) {

            auto *rejectButton =
                new QPushButton(
                    rejectText,
                    card);

            rejectButton->setObjectName(
                QStringLiteral("messageSecondaryButton"));

            rejectButton->setCursor(
                Qt::PointingHandCursor);

            rejectButton->setMinimumWidth(105);
            rejectButton->setMinimumHeight(42);

            buttonLayout->addWidget(
                rejectButton);

            QObject::connect(
                rejectButton,
                &QPushButton::clicked,
                this,
                &QDialog::reject);
        }


        auto *acceptButton =
            new QPushButton(
                acceptText,
                card);

        acceptButton->setObjectName(
            QStringLiteral("messagePrimaryButton"));

        acceptButton->setCursor(
            Qt::PointingHandCursor);

        acceptButton->setMinimumWidth(105);
        acceptButton->setMinimumHeight(42);

        acceptButton->setDefault(true);


        buttonLayout->addWidget(
            acceptButton);


        cardLayout->addLayout(
            buttonLayout);


        outerLayout->addWidget(
            card);


        // ================================================================
        // 信号
        // ================================================================
        QObject::connect(
            closeButton,
            &QPushButton::clicked,
            this,
            &QDialog::reject);

        QObject::connect(
            acceptButton,
            &QPushButton::clicked,
            this,
            &QDialog::accept);


        // ================================================================
        // 样式
        // ================================================================
        setStyleSheet(
            QStringLiteral(

                "QDialog#appMessageDialog{"
                "background:transparent;"
                "}"

                "QFrame#messageCard{"
                "background:#F7F4EE;"
                "border:1px solid #E2DBD0;"
                "border-radius:20px;"
                "}"

                "QLabel#messageTitle{"
                "background:transparent;"
                "border:none;"
                "color:%1;"
                "font-size:18px;"
                "font-weight:800;"
                "}"

                "QLabel#messageText{"
                "background:transparent;"
                "border:none;"
                "color:#59635E;"
                "font-size:15px;"
                "}"

                "QLabel#messageIcon{"
                "border:none;"
                "border-radius:21px;"
                "font-size:20px;"
                "font-weight:800;"
                "}"

                "QLabel#messageIcon[messageType=\"info\"]{"
                "background:#E8F0EB;"
                "color:%2;"
                "}"

                "QLabel#messageIcon[messageType=\"warning\"]{"
                "background:#FFF0D9;"
                "color:#B57828;"
                "}"

                "QPushButton#messageCloseButton{"
                "background:transparent;"
                "border:none;"
                "border-radius:15px;"
                "color:#8A918D;"
                "font-size:22px;"
                "font-weight:500;"
                "padding:0px;"
                "}"

                "QPushButton#messageCloseButton:hover{"
                "background:#ECE8E0;"
                "color:%1;"
                "}"

                "QPushButton#messagePrimaryButton{"
                "background:%2;"
                "color:#FFFFFF;"
                "border:none;"
                "border-radius:13px;"
                "font-size:15px;"
                "font-weight:700;"
                "padding:9px 18px;"
                "}"

                "QPushButton#messagePrimaryButton:hover{"
                "background:%3;"
                "}"

                "QPushButton#messagePrimaryButton:pressed{"
                "background:#24473C;"
                "}"

                "QPushButton#messageSecondaryButton{"
                "background:#ECE8E0;"
                "color:%1;"
                "border:1px solid #DDD6CA;"
                "border-radius:13px;"
                "font-size:15px;"
                "font-weight:650;"
                "padding:9px 18px;"
                "}"

                "QPushButton#messageSecondaryButton:hover{"
                "background:#E4DFD5;"
                "}")

                .arg(
                    UiTheme::textPrimary())

                .arg(
                    UiTheme::primary())

                .arg(
                    UiTheme::primaryHover()));
    }


    // 普通提示
    static void information(
        QWidget *parent,
        const QString &title,
        const QString &message)
    {
        AppMessageBox box(
            parent,
            Type::Information,
            title,
            message);

        box.exec();
    }


    // 警告
    static void warning(
        QWidget *parent,
        const QString &title,
        const QString &message)
    {
        AppMessageBox box(
            parent,
            Type::Warning,
            title,
            message);

        box.exec();
    }


    // 二选一
    static bool question(
        QWidget *parent,
        const QString &title,
        const QString &message,
        const QString &acceptText = QStringLiteral("确定"),
        const QString &rejectText = QStringLiteral("取消"))
    {
        AppMessageBox box(
            parent,
            Type::Question,
            title,
            message,
            acceptText,
            rejectText);

        return box.exec() ==
               QDialog::Accepted;
    }


protected:
    void showEvent(
        QShowEvent *event) override
    {
        QDialog::showEvent(event);

        adjustSize();

        // 在父窗口正中显示
        if (parentWidget()) {

            const QRect parentRect =
                parentWidget()
                    ->frameGeometry();

            const QPoint center =
                parentRect.center();

            move(
                center.x() -
                    width() / 2,
                center.y() -
                    height() / 2);
        }
    }
};

#endif // APPMESSAGEBOX_H
