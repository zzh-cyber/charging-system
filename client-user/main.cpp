#include "loginwindow.h"

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QFontDatabase>
#include <QPalette>


int main(
    int argc,
    char *argv[])
{
    QApplication app(
        argc,
        argv);


    // ========================================================================
    // 中文字体
    // 保留原逻辑：修复 Linux / WSL 下中文显示为空白方框
    // ========================================================================
    const int fontId =
        QFontDatabase::addApplicationFont(
            QStringLiteral(
                ":/fonts/wqy-microhei.ttc"));


    if (fontId != -1) {

        const QStringList families =
            QFontDatabase::applicationFontFamilies(
                fontId);


        if (!families.isEmpty()) {

            app.setFont(
                QFont(
                    families.first()));
        }
    }


    // ========================================================================
    // 全局基础色
    // 只修改视觉主题，不涉及任何业务逻辑
    // ========================================================================
    QPalette palette =
        app.palette();


    // 页面背景
    palette.setColor(
        QPalette::Window,
        QColor(
            "#F6F4EF"));


    // 输入控件背景
    palette.setColor(
        QPalette::Base,
        QColor(
            "#FFFFFF"));


    // 普通文字
    palette.setColor(
        QPalette::WindowText,
        QColor(
            "#202824"));


    palette.setColor(
        QPalette::Text,
        QColor(
            "#202824"));


    // 选中区域
    palette.setColor(
        QPalette::Highlight,
        QColor(
            "#315B4D"));


    palette.setColor(
        QPalette::HighlightedText,
        QColor(
            "#FFFFFF"));


    app.setPalette(
        palette);


    // ========================================================================
    // 全局 UI 样式
    //
    // 页面自己的 objectName 样式优先级更高，
    // 所以前面已经美化过的 Login / Station / Pile / Charge /
    // Profile / Navigation 页面不会被这里破坏。
    //
    // 这里主要负责：
    // 1. 没有单独样式的基础控件
    // 2. QMessageBox
    // 3. 普通 QDialog
    // 4. QDialogButtonBox
    // ========================================================================
    app.setStyleSheet(
        QStringLiteral(

            // ================================================================
            // 全局字体和文字
            // ================================================================
            "QWidget{"
            "color:#202824;"
            "font-size:14px;"
            "}"


            // ================================================================
            // 输入框
            // ================================================================
            "QLineEdit{"
            "background:#FFFFFF;"
            "color:#202824;"
            "border:1px solid #E1DDD4;"
            "border-radius:10px;"
            "padding:9px 12px;"
            "font-size:14px;"
            "selection-background-color:#315B4D;"
            "selection-color:#FFFFFF;"
            "}"

            "QLineEdit:hover{"
            "border-color:#D2CDC3;"
            "}"

            "QLineEdit:focus{"
            "border:1px solid #315B4D;"
            "}"


            // ================================================================
            // DoubleSpinBox
            // ================================================================
            "QDoubleSpinBox{"
            "background:#FFFFFF;"
            "color:#202824;"
            "border:1px solid #E1DDD4;"
            "border-radius:10px;"
            "padding:8px 10px;"
            "font-size:14px;"
            "}"

            "QDoubleSpinBox:focus{"
            "border:1px solid #315B4D;"
            "}"


            // ================================================================
            // ComboBox
            // ================================================================
            "QComboBox{"
            "background:#FFFFFF;"
            "color:#202824;"
            "border:1px solid #E1DDD4;"
            "border-radius:10px;"
            "padding:8px 12px;"
            "font-size:14px;"
            "}"

            "QComboBox:focus{"
            "border:1px solid #315B4D;"
            "}"

            "QComboBox QAbstractItemView{"
            "background:#FFFFFF;"
            "color:#202824;"
            "border:1px solid #E7E3DA;"
            "selection-background-color:#E9F0EC;"
            "selection-color:#315B4D;"
            "outline:0;"
            "}"


            // ================================================================
            // 普通按钮
            //
            // 页面中带 objectName 的按钮，
            // 仍然会使用对应页面自己的样式。
            // ================================================================
            "QPushButton{"
            "background:#315B4D;"
            "color:#FFFFFF;"
            "border:none;"
            "border-radius:10px;"
            "padding:9px 16px;"
            "font-size:14px;"
            "font-weight:700;"
            "}"

            "QPushButton:hover{"
            "background:#284C41;"
            "}"

            "QPushButton:pressed{"
            "background:#203F36;"
            "}"

            "QPushButton:disabled{"
            "background:#D7DAD7;"
            "color:#9AA09D;"
            "}"


            // ================================================================
            // ScrollArea
            // ================================================================
            "QScrollArea{"
            "background:transparent;"
            "border:none;"
            "}"


            // ================================================================
            // ScrollBar
            // ================================================================
            "QScrollBar:vertical{"
            "background:transparent;"
            "width:8px;"
            "margin:2px;"
            "}"

            "QScrollBar::handle:vertical{"
            "background:#D5D3CC;"
            "border-radius:4px;"
            "min-height:28px;"
            "}"

            "QScrollBar::handle:vertical:hover{"
            "background:#BFC3BF;"
            "}"

            "QScrollBar::add-line:vertical,"
            "QScrollBar::sub-line:vertical{"
            "height:0;"
            "}"

            "QScrollBar::add-page:vertical,"
            "QScrollBar::sub-page:vertical{"
            "background:transparent;"
            "}"


            // ================================================================
            // 底部导航
            // 保留原 objectName，但改为当前深绿色主题
            // ================================================================
            "QWidget#navBar{"
            "background:#FFFFFF;"
            "border-top:1px solid #E7E3DA;"
            "}"

            "QPushButton#navBtn{"
            "background:transparent;"
            "color:#8A928E;"
            "border:none;"
            "border-top:2px solid transparent;"
            "border-radius:0;"
            "padding:13px 0;"
            "font-size:14px;"
            "font-weight:500;"
            "}"

            "QPushButton#navBtn:hover{"
            "background:transparent;"
            "color:#315B4D;"
            "}"

            "QPushButton#navBtn:checked{"
            "background:transparent;"
            "color:#315B4D;"
            "border-top:2px solid #315B4D;"
            "font-weight:700;"
            "}"


            // ================================================================
            // QMessageBox
            //
            // mainwindow.cpp / pilelistpage.cpp /
            // loginwindow.cpp / profilepage.cpp 中原来的
            // QMessageBox 调用都不需要改。
            // ================================================================
            "QMessageBox{"
            "background:#F6F4EF;"
            "}"


            // QMessageBox 内部普通文字
            "QMessageBox QLabel{"
"background:transparent;"
"color:#202824;"
"font-size:14px;"
"}"


            // QMessageBox 按钮
            "QMessageBox QPushButton{"
            "background:#315B4D;"
            "color:#FFFFFF;"
            "border:none;"
            "border-radius:10px;"
            "min-width:82px;"
            "min-height:36px;"
            "padding:7px 16px;"
            "font-size:14px;"
            "font-weight:700;"
            "}"

            "QMessageBox QPushButton:hover{"
            "background:#284C41;"
            "}"

            "QMessageBox QPushButton:pressed{"
            "background:#203F36;"
            "}"


            // ================================================================
            // 普通 QDialog
            // =================================================================
            "QDialog{"
            "background:#F6F4EF;"
            "color:#202824;"
            "}"


            // Dialog 普通 Label
            "QDialog QLabel{"
            "background:transparent;"
            "color:#202824;"
            "}"


            // ================================================================
            // DialogButtonBox
            // ================================================================
            "QDialogButtonBox QPushButton{"
            "background:#315B4D;"
            "color:#FFFFFF;"
            "border:none;"
            "border-radius:10px;"
            "min-width:78px;"
            "min-height:34px;"
            "padding:7px 14px;"
            "font-size:14px;"
            "font-weight:700;"
            "}"

            "QDialogButtonBox QPushButton:hover{"
            "background:#284C41;"
            "}"

            "QDialogButtonBox QPushButton:pressed{"
            "background:#203F36;"
            "}"
        ));


    // ========================================================================
    // 登录窗口
    // ========================================================================
    LoginWindow window;

    window.show();


    return app.exec();
}
