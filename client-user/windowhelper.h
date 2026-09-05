#pragma once

// 用户端窗口按常见手机 9:16 自适应，并居中。
// 高度约占可用桌面的 65%（不再用 9:19.5 + 78%，那在桌面上会显得过长）。
// 所有裁切之后仍保持 9:16，避免只限宽不限高把窗口拉成瘦长条。

#include <QGuiApplication>
#include <QScreen>
#include <QWidget>

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

    w->setFixedSize(width, h);
    w->move(area.center() - QPoint(width / 2, h / 2));
}
