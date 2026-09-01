#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QtGlobal>
#include <iostream>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    const QString host = QStringLiteral("127.0.0.1");
    const int port = 3306;
    const QString dbName = QStringLiteral("charging_system");
    const QString user = QStringLiteral("charging_user");
    const QString password = QStringLiteral("123456");

    std::cout << "Qt version: " << qVersion() << std::endl;
    std::cout << "Available SQL drivers: "
              << QSqlDatabase::drivers().join(QStringLiteral(", ")).toStdString()
              << std::endl;

    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QMYSQL"))) {
        std::cout << "FAIL: QMYSQL driver not available." << std::endl;
        return 1;
    }

    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QMYSQL"));
        db.setHostName(host);
        db.setPort(port);
        db.setDatabaseName(dbName);
        db.setUserName(user);
        db.setPassword(password);

        if (!db.open()) {
            std::cout << "FAIL: cannot open database." << std::endl;
            std::cout << "Error: " << db.lastError().text().toStdString() << std::endl;
            return 1;
        }

        QSqlQuery query(QStringLiteral("SELECT DATABASE(), VERSION()"));
        if (query.next()) {
            std::cout << "OK: connected to database " << query.value(0).toString().toStdString()
                      << std::endl;
            std::cout << "MySQL version: " << query.value(1).toString().toStdString()
                      << std::endl;
        } else {
            std::cout << "FAIL: query failed." << std::endl;
            std::cout << "Error: " << query.lastError().text().toStdString() << std::endl;
            return 1;
        }
    }

    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
    return 0;
}
