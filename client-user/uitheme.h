#ifndef UITHEME_H
#define UITHEME_H

#include <QColor>
#include <QGraphicsDropShadowEffect>
#include <QString>
#include <QWidget>


namespace UiTheme
{

// ============================================================================
// 新视觉体系
//
// 参考风格：
// - 深墨黑 / 深蓝灰
// - 白色卡片
// - 荧光绿色强调
// - 极浅灰绿色背景
//
// 注意：
// 保留原有函数名称，避免影响现有页面代码。
// ============================================================================


// ============================================================================
// 页面背景
// ============================================================================
inline QString pageBackground()
{
    return QStringLiteral(
        "#F4F6F5");
}


// ============================================================================
// 白色主体卡片
// ============================================================================
inline QString surface()
{
    return QStringLiteral(
        "#FFFFFF");
}


// ============================================================================
// 浅色辅助卡片
// ============================================================================
inline QString surfaceSoft()
{
    return QStringLiteral(
        "#F7F9F8");
}


// ============================================================================
// 深色主视觉
// ============================================================================
inline QString dark()
{
    return QStringLiteral(
        "#171D27");
}


inline QString darkSoft()
{
    return QStringLiteral(
        "#222A35");
}


inline QString darkHover()
{
    return QStringLiteral(
        "#252E3A");
}


// ============================================================================
// 文字
// ============================================================================
inline QString textPrimary()
{
    return QStringLiteral(
        "#151C24");
}


inline QString textSecondary()
{
    return QStringLiteral(
        "#7E8893");
}


inline QString textTertiary()
{
    return QStringLiteral(
        "#A2AAB2");
}


// ============================================================================
// 主按钮
//
// 新风格里：
// 主 CTA 使用深墨黑，而不是大面积绿色。
// ============================================================================
inline QString primary()
{
    return dark();
}


inline QString primaryHover()
{
    return darkHover();
}


inline QString primarySoft()
{
    return QStringLiteral(
        "#EEF1F0");
}


// ============================================================================
// 荧光绿强调色
// ============================================================================
inline QString lime()
{
    return QStringLiteral(
        "#74EC8B");
}


inline QString limeStrong()
{
    return QStringLiteral(
        "#45D86B");
}


inline QString limeSoft()
{
    return QStringLiteral(
        "#E2F9E7");
}


// ============================================================================
// Accent
//
// 保留原接口。
// 新风格改成绿色高亮。
// ============================================================================
inline QString accent()
{
    return limeStrong();
}


// ============================================================================
// 边框
// ============================================================================
inline QString border()
{
    return QStringLiteral(
        "#E7EBE9");
}


inline QString borderStrong()
{
    return QStringLiteral(
        "#D9DFDC");
}


// ============================================================================
// 状态色
// ============================================================================
inline QString success()
{
    return limeStrong();
}


inline QString warning()
{
    return QStringLiteral(
        "#E6AE46");
}


inline QString danger()
{
    return QStringLiteral(
        "#E26868");
}


// ============================================================================
// 页面背景样式
// ============================================================================
inline QString pageStyle()
{
    return QStringLiteral(
               "background:%1;")
        .arg(
            pageBackground());
}


// ============================================================================
// 普通卡片
// ============================================================================
inline QString cardStyle(
    int radius = 22)
{
    return QStringLiteral(
               "background:%1;"
               "border:1px solid %2;"
               "border-radius:%3px;")
        .arg(
            surface())
        .arg(
            border())
        .arg(
            radius);
}


// ============================================================================
// 深色主按钮
// ============================================================================
inline QString primaryButtonStyle(
    int fontSize,
    int radius = 15)
{
    return QStringLiteral(

               "QPushButton{"
               "background:%1;"
               "color:#FFFFFF;"
               "border:none;"
               "border-radius:%2px;"
               "font-size:%3px;"
               "font-weight:700;"
               "padding:11px 18px;"
               "}"

               "QPushButton:hover{"
               "background:%4;"
               "}"

               "QPushButton:pressed{"
               "background:#10151C;"
               "}"

               "QPushButton:disabled{"
               "background:#E1E5E3;"
               "color:#A4AAA7;"
               "}")

        .arg(
            primary())

        .arg(
            radius)

        .arg(
            fontSize)

        .arg(
            primaryHover());
}


// ============================================================================
// 白底次按钮
// ============================================================================
inline QString secondaryButtonStyle(
    int fontSize,
    int radius = 15)
{
    return QStringLiteral(

               "QPushButton{"
               "background:#FFFFFF;"
               "color:%1;"
               "border:1px solid %2;"
               "border-radius:%3px;"
               "font-size:%4px;"
               "font-weight:650;"
               "padding:10px 16px;"
               "}"

               "QPushButton:hover{"
               "background:#F4F6F5;"
               "border-color:#CFD6D2;"
               "}"

               "QPushButton:pressed{"
               "background:#ECEFED;"
               "}")

        .arg(
            textPrimary())

        .arg(
            borderStrong())

        .arg(
            radius)

        .arg(
            fontSize);
}


// ============================================================================
// 荧光绿按钮
//
// 只在少量高亮操作使用。
// ============================================================================
inline QString limeButtonStyle(
    int fontSize,
    int radius = 15)
{
    return QStringLiteral(

               "QPushButton{"
               "background:%1;"
               "color:#112018;"
               "border:none;"
               "border-radius:%2px;"
               "font-size:%3px;"
               "font-weight:750;"
               "padding:11px 18px;"
               "}"

               "QPushButton:hover{"
               "background:#82F199;"
               "}"

               "QPushButton:pressed{"
               "background:%4;"
               "}"

               "QPushButton:disabled{"
               "background:#E2E8E4;"
               "color:#A4AAA7;"
               "}")

        .arg(
            lime())

        .arg(
            radius)

        .arg(
            fontSize)

        .arg(
            limeStrong());
}


// ============================================================================
// 输入框
// ============================================================================
inline QString inputStyle(
    int fontSize,
    int radius = 15)
{
    return QStringLiteral(

               "QLineEdit,"
               "QComboBox,"
               "QDoubleSpinBox{"
               "background:#F7F9F8;"
               "color:%1;"
               "border:1px solid %2;"
               "border-radius:%3px;"
               "font-size:%4px;"
               "padding:10px 13px;"
               "selection-background-color:%5;"
               "selection-color:#102017;"
               "}"

               "QLineEdit:hover,"
               "QComboBox:hover,"
               "QDoubleSpinBox:hover{"
               "border-color:#D5DBD8;"
               "}"

               "QLineEdit:focus,"
               "QComboBox:focus,"
               "QDoubleSpinBox:focus{"
               "background:#FFFFFF;"
               "border:1px solid %5;"
               "}")

        .arg(
            textPrimary())

        .arg(
            border())

        .arg(
            radius)

        .arg(
            fontSize)

        .arg(
            limeStrong());
}


// ============================================================================
// 状态胶囊
// ============================================================================
inline QString successBadgeStyle(
    int fontSize,
    int radius = 10)
{
    return QStringLiteral(

               "background:%1;"
               "color:#278744;"
               "border:none;"
               "border-radius:%2px;"
               "font-size:%3px;"
               "font-weight:700;"
               "padding:5px 9px;")

        .arg(
            limeSoft())

        .arg(
            radius)

        .arg(
            fontSize);
}


// ============================================================================
// 卡片阴影
//
// 新设计阴影比旧版更轻，避免 Qt 页面显得厚重。
// ============================================================================
inline void applyCardShadow(
    QWidget *widget,
    int blurRadius = 28,
    int yOffset = 7)
{
    if (!widget)
        return;


    auto *shadow =
        new QGraphicsDropShadowEffect(
            widget);


    shadow->setBlurRadius(
        blurRadius);


    shadow->setOffset(
        0,
        yOffset);


    shadow->setColor(
        QColor(
            18,
            28,
            24,
            26));


    widget->setGraphicsEffect(
        shadow);
}


// ============================================================================
// 强一点的 Hero 卡阴影
// ============================================================================
inline void applyHeroShadow(
    QWidget *widget,
    int blurRadius = 36,
    int yOffset = 10)
{
    if (!widget)
        return;


    auto *shadow =
        new QGraphicsDropShadowEffect(
            widget);


    shadow->setBlurRadius(
        blurRadius);


    shadow->setOffset(
        0,
        yOffset);


    shadow->setColor(
        QColor(
            15,
            22,
            30,
            42));


    widget->setGraphicsEffect(
        shadow);
}

} // namespace UiTheme

#endif // UITHEME_H
