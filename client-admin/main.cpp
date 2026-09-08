#include "adminloginwindow.h"
#include "runtimebootstrap.h"

#include <QApplication>
#include <QFile>

int main(int argc, char *argv[])
{
    RuntimeBootstrap::configureIbusEnvironment();
    RuntimeBootstrap::ensureIbusLibpinyin();

    QApplication app(argc, argv);
    app.setStyle("Fusion");
    QFile styleFile(QStringLiteral(":/admin.qss"));
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text))
        app.setStyleSheet(QString::fromUtf8(styleFile.readAll()));

    AdminLoginWindow w;
    w.show();

    return app.exec();
}
