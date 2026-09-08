#include "adminloginwindow.h"
#include "runtimebootstrap.h"

#include <QApplication>
#include <QFile>
#include <QScreen>

int main(int argc, char *argv[])
{
    RuntimeBootstrap::configureIbusEnvironment();
    RuntimeBootstrap::ensureIbusLibpinyin();

    QApplication app(argc, argv);
    app.setStyle("Fusion");

    QFile styleFile(QStringLiteral(":/admin.qss"));
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app.setStyleSheet(
            QString::fromUtf8(styleFile.readAll())
        );
    }

    AdminLoginWindow w;

    // 与 AdminMainWindow 的默认尺寸保持一致，避免登录前后视觉跳变。
    w.resize(1200, 800);

    // 移动到主屏幕中央
    if (QScreen *screen = QApplication::primaryScreen()) {
        const QRect area = screen->availableGeometry();

        w.move(
            area.center().x() - w.width() / 2,
            area.center().y() - w.height() / 2
        );
    }

    // 强制正常显示
    w.showNormal();
    w.raise();
    w.activateWindow();

    return app.exec();
}
