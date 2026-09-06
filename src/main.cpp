#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setFont(QFont(QStringLiteral("Microsoft YaHei"), 9));
    MainWindow w;
    w.show();
    return app.exec();
}
