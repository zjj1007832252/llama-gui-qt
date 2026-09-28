#include <QApplication>
#include <QPalette>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
#ifdef Q_OS_MAC
    // macOS 逻辑 DPI 为 72（Windows 为 96），会导致 pt 字号渲染偏小 25%，
    // 统一按 96 换算，使界面字号与 Windows 版像素大小一致
    qputenv("QT_FONT_DPI", "96");
#endif
    QApplication app(argc, argv);

    // macOS 原生 style 会直接用 AppKit 材质绘制（跟随系统深色模式，绕过 QPalette），
    // 改用 Fusion + 浅色调色板，与 Windows 版观感一致
    app.setStyle(QStringLiteral("Fusion"));

    // 界面按 Windows 浅色主题设计，强制浅色调色板，
    // 避免 macOS 深色模式下未被样式表覆盖的控件出现黑底深字
    QPalette pal = app.palette();
    pal.setColor(QPalette::Window, QColor(0xf0, 0xf0, 0xf0));
    pal.setColor(QPalette::WindowText, QColor(0x2b, 0x2b, 0x2b));
    pal.setColor(QPalette::Base, Qt::white);
    pal.setColor(QPalette::AlternateBase, QColor(0xf6, 0xf6, 0xf6));
    pal.setColor(QPalette::Text, QColor(0x2b, 0x2b, 0x2b));
    pal.setColor(QPalette::Button, QColor(0xfd, 0xfd, 0xfd));
    pal.setColor(QPalette::ButtonText, QColor(0x2b, 0x2b, 0x2b));
    pal.setColor(QPalette::PlaceholderText, QColor(0x8f, 0x8f, 0x8f));
    pal.setColor(QPalette::ToolTipBase, QColor(0xff, 0xff, 0xdc));
    pal.setColor(QPalette::ToolTipText, QColor(0x2b, 0x2b, 0x2b));
    pal.setColor(QPalette::Highlight, QColor(0x1a, 0x7a, 0xbf));
    pal.setColor(QPalette::HighlightedText, Qt::white);
    app.setPalette(pal);

    // 各平台使用实际存在的中文字体（原 Microsoft YaHei 在 macOS/Linux 上不存在）
#ifdef Q_OS_WIN
    app.setFont(QFont(QStringLiteral("Microsoft YaHei"), 9));
#elif defined(Q_OS_MAC)
    app.setFont(QFont(QStringLiteral("PingFang SC"), 9));
#else
    app.setFont(QFont(QStringLiteral("Noto Sans CJK SC"), 9));
#endif
    MainWindow w;
    w.show();
    return app.exec();
}
