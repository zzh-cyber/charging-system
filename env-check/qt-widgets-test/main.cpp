#include <QApplication>
#include <QLabel>
#include <QSqlDatabase>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>
#include <QtGlobal>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle(QStringLiteral("新能源汽车充电管理系统"));
    window.resize(480, 240);

    auto *layout = new QVBoxLayout(&window);

    auto *title = new QLabel(QStringLiteral("新能源汽车充电管理系统"), &window);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QStringLiteral("font-size: 22px; font-weight: bold;"));

    auto *info = new QLabel(&window);
    info->setAlignment(Qt::AlignCenter);
    info->setText(QStringLiteral("Qt 版本: %1\n可用 SQL 驱动: %2")
                      .arg(QString::fromLatin1(qVersion()),
                           QSqlDatabase::drivers().join(QStringLiteral(", "))));

    layout->addWidget(title);
    layout->addWidget(info);

    window.show();
    return app.exec();
}
