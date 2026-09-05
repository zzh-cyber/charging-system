#pragma once

// 用户端窗口按常见手机 9:16 自适应，并居中。
// 高度约占可用桌面的 65%（不再用 9:19.5 + 78%，那在桌面上会显得过长）。
// 所有裁切之后仍保持 9:16，避免只限宽不限高把窗口拉成瘦长条。

#include <QGuiApplication>
#include <QScreen>
#include <QWidget>
#include <cmath>


inline void applyPhoneWindow(QWidget *w)
{
    if (!w)
        return;

    const QScreen *screen = w->screen()
        ? w->screen()
        : QGuiApplication::primaryScreen();

    QRect area;
    if (screen)
        area = screen->availableGeometry();
    else
        area = QRect(0, 0, 1280, 720);

    constexpr double kAspect = 16.0 / 9.0; // 高 / 宽

    int width = qRound(area.height() * 0.65 / kAspect);
    width = qMin(width, qRound(area.width() * 0.36));
    width = qBound(390, width, area.width() - 48);

    int h = qRound(width * kAspect);
    if (h > area.height() - 48) {
        h = area.height() - 48;
        width = qRound(h / kAspect);
    }

    w->setMinimumSize(390, 620);
    w->resize(width, h);
    w->move(area.center() - QPoint(width / 2, h / 2));

}

inline double uiScaleForWindow(const QWidget *w)
{
    if (!w)
        return 1.0;

    constexpr double kBaseWidth  = 390.0;
    constexpr double kBaseHeight = 680.0;

    const double sx =
        w->width() / kBaseWidth;

    const double sy =
        w->height() / kBaseHeight;

    // 宽高综合计算。
    // 不再只取较小值，否则大窗口横向拉宽时 UI 几乎不变。
    const double scale =
        std::sqrt(sx * sy);

    return qBound(
        0.5,
        scale,
        2.4);
}


inline int scaledUi(
    const QWidget *w,
    int value)
{
    return qRound(
        value * uiScaleForWindow(w));
}
