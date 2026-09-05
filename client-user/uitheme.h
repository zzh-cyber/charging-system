#ifndef UITHEME_H
#define UITHEME_H

#include <QColor>
#include <QGraphicsDropShadowEffect>
#include <QString>
#include <QWidget>

namespace UiTheme
{

// ============================================================================
// 统一颜色
// ============================================================================
inline QString pageBackground()
{
    return QStringLiteral("#F6F4EF");
}

inline QString surface()
{
    return QStringLiteral("#FFFFFF");
}

inline QString surfaceSoft()
{
    return QStringLiteral("#FAF8F3");
}

inline QString textPrimary()
{
    return QStringLiteral("#202824");
}

inline QString textSecondary()
{
    return QStringLiteral("#7A837E");
}

inline QString primary()
{
    return QStringLiteral("#315B4D");
}

inline QString primaryHover()
{
    return QStringLiteral("#284C41");
}

inline QString primarySoft()
{
    return QStringLiteral("#E9F0EC");
}

inline QString accent()
{
    return QStringLiteral("#D79A4B");
}

inline QString border()
{
    return QStringLiteral("#E7E3DA");
}

inline QString success()
{
    return QStringLiteral("#4F8668");
}

inline QString danger()
{
    return QStringLiteral("#C96C66");
}


// ============================================================================
// 页面背景
// ============================================================================
inline QString pageStyle()
{
    return QStringLiteral(
        "background:%1;")
        .arg(pageBackground());
}


// ============================================================================
// 普通实体卡片
// ============================================================================
inline QString cardStyle(int radius = 18)
{
    return QStringLiteral(
        "background:%1;"
        "border:1px solid %2;"
        "border-radius:%3px;")
        .arg(surface())
        .arg(border())
        .arg(radius);
}


// ============================================================================
// 主按钮
// ============================================================================
inline QString primaryButtonStyle(
    int fontSize,
    int radius = 12)
{
    return QStringLiteral(
        "QPushButton{"
        "background:%1;"
        "color:#FFFFFF;"
        "border:none;"
        "border-radius:%2px;"
        "font-size:%3px;"
        "font-weight:700;"
        "padding:10px 18px;"
        "}"
        "QPushButton:hover{"
        "background:%4;"
        "}"
        "QPushButton:pressed{"
        "background:#203F36;"
        "}"
        "QPushButton:disabled{"
        "background:#D7DAD7;"
        "color:#9AA09D;"
        "}")
        .arg(primary())
        .arg(radius)
        .arg(fontSize)
        .arg(primaryHover());
}


// ============================================================================
// 次按钮
// ============================================================================
inline QString secondaryButtonStyle(
    int fontSize,
    int radius = 12)
{
    return QStringLiteral(
        "QPushButton{"
        "background:%1;"
        "color:%2;"
        "border:1px solid %3;"
        "border-radius:%4px;"
        "font-size:%5px;"
        "font-weight:600;"
        "padding:10px 16px;"
        "}"
        "QPushButton:hover{"
        "background:#E0E9E4;"
        "}")
        .arg(primarySoft())
        .arg(primary())
        .arg(border())
        .arg(radius)
        .arg(fontSize);
}


// ============================================================================
// 输入框
// ============================================================================
inline QString inputStyle(
    int fontSize,
    int radius = 12)
{
    return QStringLiteral(
        "QLineEdit,QComboBox,QDoubleSpinBox{"
        "background:#FFFFFF;"
        "color:%1;"
        "border:1px solid %2;"
        "border-radius:%3px;"
        "font-size:%4px;"
        "padding:9px 12px;"
        "}"
        "QLineEdit:focus,"
        "QComboBox:focus,"
        "QDoubleSpinBox:focus{"
        "border:1px solid %5;"
        "}")
        .arg(textPrimary())
        .arg(border())
        .arg(radius)
        .arg(fontSize)
        .arg(primary());
}


// ============================================================================
// 很轻的实体卡片阴影
// ============================================================================
inline void applyCardShadow(
    QWidget *widget,
    int blurRadius = 24,
    int yOffset = 6)
{
    if (!widget)
        return;

    auto *shadow =
        new QGraphicsDropShadowEffect(widget);

    shadow->setBlurRadius(
        blurRadius);

    shadow->setOffset(
        0,
        yOffset);

    shadow->setColor(
        QColor(
            42,
            53,
            48,
            24));

    widget->setGraphicsEffect(
        shadow);
}

} // namespace UiTheme

#endif // UITHEME_H
