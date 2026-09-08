#ifndef APPMESSAGEBOX_H
#define APPMESSAGEBOX_H

#include "uitheme.h"

#include <QColor>
#include <QDialog>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QShowEvent>
#include <QTimer>
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
        // =====================================================================
        // 窗口
        // =====================================================================
        setWindowFlags(
            Qt::Dialog |
            Qt::FramelessWindowHint);


        setAttribute(
            Qt::WA_TranslucentBackground);


        setModal(
            true);


        setObjectName(
            QStringLiteral(
                "appMessageDialog"));


        setMinimumWidth(
            360);

        setMaximumWidth(
            500);


        // =====================================================================
        // 类型属性
        //
        // 只影响视觉，不改变业务行为。
        // =====================================================================
        QString typeName =
            QStringLiteral(
                "information");


        if (type ==
            Type::Warning) {

            typeName =
                QStringLiteral(
                    "warning");

        } else if (
            type ==
            Type::Question) {

            typeName =
                QStringLiteral(
                    "question");
        }


        setProperty(
            "messageType",
            typeName);


        // =====================================================================
        // 外层透明区域
        // 为阴影保留空间
        // =====================================================================
        auto *outerLayout =
            new QVBoxLayout(
                this);


        outerLayout->setContentsMargins(
            24,
            24,
            24,
            24);


        outerLayout->setSpacing(
            0);


        // =====================================================================
        // 主卡片
        // =====================================================================
        auto *card =
            new QFrame(
                this);


        card->setObjectName(
            QStringLiteral(
                "messageCard"));


        card->setAttribute(
            Qt::WA_StyledBackground,
            true);


        // =====================================================================
        // 阴影
        // =====================================================================
        auto *shadow =
            new QGraphicsDropShadowEffect(
                card);


        shadow->setBlurRadius(
            34);


        shadow->setOffset(
            0,
            8);


        shadow->setColor(
            QColor(
                20,
                27,
                34,
                48));


        card->setGraphicsEffect(
            shadow);


        // =====================================================================
        // Card Layout
        // =====================================================================
        auto *cardLayout =
            new QVBoxLayout(
                card);


        cardLayout->setContentsMargins(
            22,
            20,
            22,
            20);


        cardLayout->setSpacing(
            17);


        // =====================================================================
        // Header
        // =====================================================================
        auto *headerLayout =
            new QHBoxLayout;


        headerLayout->setSpacing(
            10);


        auto *titleLabel =
            new QLabel(
                title,
                card);


        titleLabel->setObjectName(
            QStringLiteral(
                "messageTitle"));


        titleLabel->setWordWrap(
            true);


        auto *closeButton =
            new QPushButton(
                QStringLiteral(
                    "×"),
                card);


        closeButton->setObjectName(
            QStringLiteral(
                "messageCloseButton"));


        closeButton->setFixedSize(
            32,
            32);


        closeButton->setCursor(
            Qt::PointingHandCursor);


        closeButton->setFocusPolicy(
            Qt::NoFocus);


        headerLayout->addWidget(
            titleLabel,
            1);


        headerLayout->addWidget(
            closeButton,
            0,
            Qt::AlignTop);


        cardLayout->addLayout(
            headerLayout);


        // =====================================================================
        // Content
        // =====================================================================
        auto *contentLayout =
            new QHBoxLayout;


        contentLayout->setSpacing(
            14);


        // ---------------------------------------------------------------------
        // 状态图标
        //
        // 不使用 Emoji。
        // ---------------------------------------------------------------------
        auto *iconLabel =
            new QLabel(
                card);


        iconLabel->setObjectName(
            QStringLiteral(
                "messageIcon"));


        iconLabel->setAlignment(
            Qt::AlignCenter);


        iconLabel->setFixedSize(
            46,
            46);


        if (type ==
            Type::Warning) {

            iconLabel->setText(
                QStringLiteral(
                    "!"));


            iconLabel->setProperty(
                "messageType",
                QStringLiteral(
                    "warning"));

        } else if (
            type ==
            Type::Question) {

            iconLabel->setText(
                QStringLiteral(
                    "?"));


            iconLabel->setProperty(
                "messageType",
                QStringLiteral(
                    "question"));

        } else {

            iconLabel->setText(
                QStringLiteral(
                    "i"));


            iconLabel->setProperty(
                "messageType",
                QStringLiteral(
                    "information"));
        }


        // ---------------------------------------------------------------------
        // 正文
        // ---------------------------------------------------------------------
        auto *messageLabel =
            new QLabel(
                message,
                card);


        messageLabel->setObjectName(
            QStringLiteral(
                "messageText"));


        messageLabel->setWordWrap(
            true);


        messageLabel->setAlignment(
            Qt::AlignLeft |
            Qt::AlignVCenter);


        messageLabel->setTextInteractionFlags(
            Qt::TextSelectableByMouse);


        contentLayout->addWidget(
            iconLabel,
            0,
            Qt::AlignTop);


        contentLayout->addWidget(
            messageLabel,
            1);


        cardLayout->addLayout(
            contentLayout);


        // =====================================================================
        // Button Area
        // =====================================================================
        auto *buttonLayout =
            new QHBoxLayout;


        buttonLayout->setSpacing(
            10);


        buttonLayout->addStretch();


        // ---------------------------------------------------------------------
        // Secondary
        // ---------------------------------------------------------------------
        if (!rejectText.isEmpty()) {

            auto *rejectButton =
                new QPushButton(
                    rejectText,
                    card);


            rejectButton->setObjectName(
                QStringLiteral(
                    "messageSecondaryButton"));


            rejectButton->setCursor(
                Qt::PointingHandCursor);


            rejectButton->setMinimumWidth(
                104);


            rejectButton->setMinimumHeight(
                43);


            buttonLayout->addWidget(
                rejectButton);


            QObject::connect(
                rejectButton,
                &QPushButton::clicked,
                this,
                &QDialog::reject);
        }


        // ---------------------------------------------------------------------
        // Primary
        // ---------------------------------------------------------------------
        auto *acceptButton =
            new QPushButton(
                acceptText,
                card);


        acceptButton->setObjectName(
            QStringLiteral(
                "messagePrimaryButton"));


        acceptButton->setCursor(
            Qt::PointingHandCursor);


        acceptButton->setMinimumWidth(
            104);


        acceptButton->setMinimumHeight(
            43);


        acceptButton->setDefault(
            true);


        buttonLayout->addWidget(
            acceptButton);


        cardLayout->addLayout(
            buttonLayout);


        outerLayout->addWidget(
            card);


        // =====================================================================
        // Signals
        // =====================================================================
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


        // =====================================================================
        // Style
        // =====================================================================
        setStyleSheet(
            QStringLiteral(

                // =============================================================
                // Root
                // =============================================================
                "QDialog#appMessageDialog{"
                "background:transparent;"
                "}"


                // =============================================================
                // Card
                // =============================================================
                "QFrame#messageCard{"
                "background:#FFFFFF;"
                "border:1px solid %1;"
                "border-radius:24px;"
                "}"


                // =============================================================
                // Title
                // =============================================================
                "QLabel#messageTitle{"
                "background:transparent;"
                "border:none;"
                "color:%2;"
                "font-size:18px;"
                "font-weight:850;"
                "}"


                // =============================================================
                // Message
                // =============================================================
                "QLabel#messageText{"
                "background:transparent;"
                "border:none;"
                "color:%3;"
                "font-size:14px;"
                "line-height:1.4;"
                "}"


                // =============================================================
                // Icon Base
                // =============================================================
                "QLabel#messageIcon{"
                "border:none;"
                "border-radius:23px;"
                "font-size:20px;"
                "font-weight:900;"
                "}"


                // -------------------------------------------------------------
                // Information
                // -------------------------------------------------------------
                "QLabel#messageIcon"
                "[messageType=\"information\"]{"
                "background:%4;"
                "color:%5;"
                "}"


                // -------------------------------------------------------------
                // Warning
                // -------------------------------------------------------------
                "QLabel#messageIcon"
                "[messageType=\"warning\"]{"
                "background:#FFF3DD;"
                "color:#D0932E;"
                "}"


                // -------------------------------------------------------------
                // Question
                // -------------------------------------------------------------
                "QLabel#messageIcon"
                "[messageType=\"question\"]{"
                "background:%6;"
                "color:%2;"
                "}"


                // =============================================================
                // Close
                // =============================================================
                "QPushButton#messageCloseButton{"
                "background:transparent;"
                "border:none;"
                "border-radius:16px;"
                "color:%7;"
                "font-size:22px;"
                "font-weight:500;"
                "padding:0px;"
                "}"


                "QPushButton#messageCloseButton:hover{"
                "background:%8;"
                "color:%2;"
                "}"


                "QPushButton#messageCloseButton:pressed{"
                "background:#E8ECEA;"
                "}"


                // =============================================================
                // Primary Button
                // =============================================================
                "QPushButton#messagePrimaryButton{"
                "background:%9;"
                "color:#FFFFFF;"
                "border:none;"
                "border-radius:13px;"
                "font-size:14px;"
                "font-weight:750;"
                "padding:9px 18px;"
                "}"


                "QPushButton#messagePrimaryButton:hover{"
                "background:%10;"
                "}"


                "QPushButton#messagePrimaryButton:pressed{"
                "background:#10151C;"
                "}"


                // =============================================================
                // Question 主按钮使用亮绿色
                // =============================================================
                "QDialog#appMessageDialog"
                "[messageType=\"question\"] "
                "QPushButton#messagePrimaryButton{"
                "background:%6;"
                "color:%9;"
                "}"


                "QDialog#appMessageDialog"
                "[messageType=\"question\"] "
                "QPushButton#messagePrimaryButton:hover{"
                "background:#63E27C;"
                "}"


                "QDialog#appMessageDialog"
                "[messageType=\"question\"] "
                "QPushButton#messagePrimaryButton:pressed{"
                "background:#52D96E;"
                "}"


                // =============================================================
                // Secondary Button
                // =============================================================
                "QPushButton#messageSecondaryButton{"
                "background:%8;"
                "color:%2;"
                "border:1px solid %1;"
                "border-radius:13px;"
                "font-size:14px;"
                "font-weight:700;"
                "padding:9px 18px;"
                "}"


                "QPushButton#messageSecondaryButton:hover{"
                "background:#E9EEEB;"
                "border-color:%11;"
                "}"


                "QPushButton#messageSecondaryButton:pressed{"
                "background:#E1E7E3;"
                "}")

                .arg(
                    UiTheme::border())          // %1

                .arg(
                    UiTheme::textPrimary())     // %2

                .arg(
                    UiTheme::textSecondary())   // %3

                .arg(
                    UiTheme::limeSoft())        // %4

                .arg(
                    UiTheme::limeStrong())      // %5

                .arg(
                    UiTheme::lime())            // %6

                .arg(
                    UiTheme::textTertiary())    // %7

                .arg(
                    UiTheme::surfaceSoft())     // %8

                .arg(
                    UiTheme::dark())            // %9

                .arg(
                    UiTheme::darkHover())       // %10

                .arg(
                    UiTheme::borderStrong()));  // %11
    }


    // =========================================================================
    // 普通提示
    // =========================================================================
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


    // =========================================================================
    // 警告
    // =========================================================================
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


    // =========================================================================
    // 二选一
    // =========================================================================
    static bool question(
        QWidget *parent,
        const QString &title,
        const QString &message,
        const QString &acceptText =
            QStringLiteral(
                "确定"),
        const QString &rejectText =
            QStringLiteral(
                "取消"))
    {
        AppMessageBox box(
            parent,
            Type::Question,
            title,
            message,
            acceptText,
            rejectText);


        return
            box.exec() ==
            QDialog::Accepted;
    }


protected:
    // =========================================================================
    // WSLg / Frameless Dialog 居中处理
    // =========================================================================
    void showEvent(
        QShowEvent *event) override
    {
        QDialog::showEvent(
            event);


        adjustSize();


        recenterOnParentWindow();


        // ---------------------------------------------------------------------
        // WSLg：
        //
        // 无边框窗口第一次映射以后，
        // compositor 偶尔会再次移动窗口。
        //
        // 所以事件循环进入以后再次居中一次。
        // ---------------------------------------------------------------------
        QTimer::singleShot(
            0,
            this,
            [this]() {

                recenterOnParentWindow();
            });
    }


private:
    // =========================================================================
    // 根据父窗口重新定位
    // =========================================================================
    void recenterOnParentWindow()
    {
        QWidget *anchor =
            parentWidget();


        if (anchor) {

            anchor =
                anchor->window();
        }


        QPoint center;


        if (anchor) {

            // -----------------------------------------------------------------
            // 必须使用全局坐标。
            //
            // 子 QWidget 的 frameGeometry() 是父坐标，
            // WSLg 下直接拿它定位会跑到左上角。
            // -----------------------------------------------------------------
            center =
                anchor->mapToGlobal(
                    anchor->rect()
                        .center());

        } else if (
            QScreen *screen =
                QGuiApplication::
                    primaryScreen()) {

            center =
                screen
                    ->availableGeometry()
                    .center();

        } else {

            return;
        }


        QRect geometry(
            center.x() -
                width() /
                    2,

            center.y() -
                height() /
                    2,

            width(),
            height());


        // ---------------------------------------------------------------------
        // 防止弹窗超出当前显示器
        // ---------------------------------------------------------------------
        if (QScreen *screen =
                QGuiApplication::
                    screenAt(
                        center)) {

            const QRect available =
                screen
                    ->availableGeometry();


            geometry.moveLeft(
                qBound(
                    available.left(),
                    geometry.left(),
                    available.right() -
                        geometry.width() +
                        1));


            geometry.moveTop(
                qBound(
                    available.top(),
                    geometry.top(),
                    available.bottom() -
                        geometry.height() +
                        1));
        }


        move(
            geometry.topLeft());
    }
};


#endif // APPMESSAGEBOX_H
