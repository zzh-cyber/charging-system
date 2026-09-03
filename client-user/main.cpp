#include "loginwindow.h"

#include <QApplication>
#include <QColor>
#include <QFontDatabase>
#include <QPalette>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 加载随程序打包的中文字体，修复 Linux/WSL 下中文显示为空白方框的问题
    const int fontId = QFontDatabase::addApplicationFont(
        QStringLiteral(":/fonts/wqy-microhei.ttc"));
    if (fontId != -1) {
        const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
        if (!families.isEmpty())
            app.setFont(QFont(families.first()));
    }

    // 蓝色主题：页面背景浅蓝，按钮主色深蓝（带边框）
    QPalette pal = app.palette();
    pal.setColor(QPalette::Window, QColor("#eef4ff"));
    pal.setColor(QPalette::Base,   QColor("#eef4ff"));
    app.setPalette(pal);

    app.setStyleSheet(QStringLiteral(
        "QWidget{font-size:14px;color:#1f2329;}"
        "QLineEdit{background:#ffffff;border:1px solid #d6e4ff;border-radius:8px;"
        "  padding:10px 12px;font-size:15px;}"
        "QLineEdit:focus{border-color:#1d4ed8;}"
        "QPushButton{background:#1d4ed8;color:#ffffff;border:1px solid #1e40af;"
        "  border-radius:8px;padding:10px 16px;font-size:15px;font-weight:600;}"
        "QPushButton:hover{background:#1e40af;}"
        "QPushButton:pressed{background:#1e3a8a;}"
        "QPushButton:disabled{background:#c8c9cc;border-color:#c8c9cc;color:#ffffff;}"
        "QScrollArea{border:none;background:transparent;}"
        "QWidget#navBar{background:#ffffff;border-top:1px solid #eef4ff;}"
        "QPushButton#navBtn{background:transparent;color:#86909c;border:none;"
        "  border-top:2px solid transparent;border-radius:0;padding:14px 0;"
        "  font-size:15px;font-weight:400;}"
        "QPushButton#navBtn:hover{background:transparent;color:#1d4ed8;}"
        "QPushButton#navBtn:checked{background:transparent;color:#1d4ed8;"
        "  border-top:2px solid #1d4ed8;font-weight:600;}"
    ));

    LoginWindow w;
    w.show();
    return app.exec();
}
