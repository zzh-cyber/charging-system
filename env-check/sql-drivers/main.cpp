#include <QCoreApplication>
#include <QSqlDatabase>
#include <QStringList>
#include <QtGlobal>
#include <iostream>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    std::cout << "Qt version: " << qVersion() << std::endl;
    std::cout << "Available SQL drivers:" << std::endl;

    const QStringList drivers = QSqlDatabase::drivers();
    if (drivers.isEmpty()) {
        std::cout << "  (none)" << std::endl;
    } else {
        for (const QString &driver : drivers) {
            std::cout << "  - " << driver.toStdString() << std::endl;
        }
    }

    return 0;
}
