#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.setWindowTitle("图书与借阅管理系统 - 课程设计");
    w.show();
    return a.exec();
}
