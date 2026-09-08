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

    w.resize(1000, 680);

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
