#include "mainwindow.h"
#include "mbtilesviewer.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MBTilesViewer viewer;
    viewer.resize(800, 600);
    viewer.show();
    return a.exec();
}
