#pragma once

#include "uitheme.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QComboBox>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSize>
#include <QString>
#include <QWidget>


namespace MainWindowStyle
{

inline QString rootStyle()
{
    return QStringLiteral(
        "QWidget#mainWindow{"
        "background:%1;"
        "color:%2;"
        "}"

        "QWidget#homePage{"
        "background:%1;"
        "}"

        "QStackedWidget#contentStack,"
        "QStackedWidget#homeStack{"
        "background:%1;"
        "border:none;"
        "}")
        .arg(
            UiTheme::pageBackground())
        .arg(
            UiTheme::textPrimary());
}


// ============================================================================
// 首页定位卡
// ============================================================================
inline QString locationPanelStyle()
{
    return QStringLiteral(
               "QFrame#locationPanel{"
               "background:#FFFFFF;"
               "border:1px solid %1;"
               "border-radius:22px;"
               "}")
        .arg(
            UiTheme::border());
}


// ============================================================================
// 城市下拉框
// ============================================================================
inline QString regionComboStyle(
    int fontSize)
{
    return QStringLiteral(
               "QComboBox{"
               "background:#F5F7F6;"
               "color:%1;"
               "border:1px solid %2;"
               "border-radius:14px;"
               "padding:9px 28px 9px 12px;"
               "font-size:%3px;"
               "font-weight:650;"
               "}"

               "QComboBox:hover{"
               "border-color:#D2D9D5;"
               "}"

               "QComboBox:focus{"
               "background:#FFFFFF;"
               "border:1px solid %4;"
               "}"

               "QComboBox::drop-down{"
               "border:none;"
               "width:24px;"
               "}"

               "QComboBox QAbstractItemView{"
               "background:#FFFFFF;"
               "color:%1;"
               "border:1px solid %2;"
               "selection-background-color:%5;"
               "selection-color:%1;"
               "outline:none;"
               "padding:4px;"
               "}")
        .arg(
            UiTheme::textPrimary())
        .arg(
            UiTheme::border())
        .arg(
            fontSize)
        .arg(
            UiTheme::limeStrong())
        .arg(
            UiTheme::limeSoft());
}


// ============================================================================
// 地址输入框
// ============================================================================
inline QString addressEditStyle(
    int fontSize)
{
    return QStringLiteral(
               "QLineEdit{"
               "background:#F5F7F6;"
               "color:%1;"
               "border:1px solid %2;"
               "border-radius:14px;"
               "padding:10px 12px;"
               "font-size:%3px;"
               "}"

               "QLineEdit:hover{"
               "border-color:#D2D9D5;"
               "}"

               "QLineEdit:focus{"
               "background:#FFFFFF;"
               "border:1px solid %4;"
               "}")
        .arg(
            UiTheme::textPrimary())
        .arg(
            UiTheme::border())
        .arg(
            fontSize)
        .arg(
            UiTheme::limeStrong());
}


// ============================================================================
// 定位按钮
// ============================================================================
inline QString locationButtonStyle(
    int fontSize)
{
    return QStringLiteral(
               "QPushButton{"
               "background:%1;"
               "color:#FFFFFF;"
               "border:none;"
               "border-radius:14px;"
               "padding:9px 14px;"
               "font-size:%2px;"
               "font-weight:700;"
               "}"

               "QPushButton:hover{"
               "background:%3;"
               "}"

               "QPushButton:pressed{"
               "background:#10151C;"
               "}"

               "QPushButton:disabled{"
               "background:#DDE2DF;"
               "color:#969D99;"
               "}")
        .arg(
            UiTheme::dark())
        .arg(
            fontSize)
        .arg(
            UiTheme::darkHover());
}


// ============================================================================
// 定位状态提示
// ============================================================================
inline QString locationTipStyle(
    int fontSize)
{
    return QStringLiteral(
               "QLabel{"
               "background:transparent;"
               "color:%1;"
               "border:none;"
               "font-size:%2px;"
               "padding:0 2px;"
               "}")
        .arg(
            UiTheme::textSecondary())
        .arg(
            fontSize);
}


// ============================================================================
// 底部导航容器
// ============================================================================
inline QString navBarStyle()
{
    return QStringLiteral(
               "QFrame#navBar{"
               "background:%1;"
               "border:none;"
               "border-top-left-radius:24px;"
               "border-top-right-radius:24px;"
               "}")
        .arg(
            UiTheme::dark());
}


// ============================================================================
// 单个导航按钮
// ============================================================================
inline QString navButtonStyle(
    int fontSize)
{
    return QStringLiteral(
               "QToolButton{"
               "background:transparent;"
               "color:#89939E;"
               "border:none;"
               "padding:4px 4px 5px 4px;"
               "font-size:%1px;"
               "font-weight:600;"
               "}"

               "QToolButton:hover{"
               "color:#D6DCE1;"
               "}"

               "QToolButton:checked{"
               "color:%2;"
               "font-weight:750;"
               "}")
        .arg(
            fontSize)
        .arg(
            UiTheme::lime());
}


// ============================================================================
// 设置底部导航按钮图标
// ============================================================================
inline void applyNavIcon(
    QAbstractButton *button,
    int id,
    bool active)
{
    if (!button)
        return;


    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(active ? QColor("#70E889") : QColor("#89939E"),
                        2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);

    if (id == 0) {
        QPainterPath home;
        home.moveTo(6, 15);
        home.lineTo(16, 7);
        home.lineTo(26, 15);
        home.lineTo(26, 26);
        home.lineTo(19, 26);
        home.lineTo(19, 19);
        home.lineTo(13, 19);
        home.lineTo(13, 26);
        home.lineTo(6, 26);
        home.closeSubpath();
        painter.drawPath(home);
    } else if (id == 1) {
        QPainterPath bolt;
        bolt.moveTo(18, 4);
        bolt.lineTo(8, 18);
        bolt.lineTo(15, 18);
        bolt.lineTo(14, 28);
        bolt.lineTo(24, 14);
        bolt.lineTo(17, 14);
        bolt.closeSubpath();
        painter.setBrush(active ? QColor("#70E889") : QColor("#89939E"));
        painter.setPen(Qt::NoPen);
        painter.drawPath(bolt);
    } else if (id == 2) {
        painter.drawEllipse(QPointF(16, 10), 5, 5);
        painter.drawArc(QRectF(7, 17, 18, 12), 0, 180 * 16);
    } else {
        return;
    }

    painter.end();
    button->setIcon(QIcon(pixmap));
}


// ============================================================================
// 更新三个底部导航按钮的 active 状态
// ============================================================================
inline void updateNavIcons(
    QButtonGroup *group,
    int activeId,
    int iconSize)
{
    if (!group)
        return;


    for (int id = 0;
         id < 3;
         ++id) {

        QAbstractButton *button =
            group->button(
                id);


        if (!button)
            continue;


        applyNavIcon(
            button,
            id,
            id == activeId);


        button->setIconSize(
            QSize(
                iconSize,
                iconSize));
    }
}

} // namespace MainWindowStyle
