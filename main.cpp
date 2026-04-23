#include "mainwindow.h"
#include "mbtilesviewer.h"

#include <QApplication>
#include <QDebug>
#include <QFileInfo>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MBTilesViewer viewer;
    viewer.resize(800, 600);
    viewer.show();
    
    // Автоматически открываем тестовый файл если он существует
    QString testFile = "/workspace/test_map.mbtiles";
    QFileInfo fi(testFile);
    if (fi.exists()) {
        qDebug() << "Auto-opening test file:" << testFile;
        viewer.openMBTiles(testFile);
    }
    
    return a.exec();
}
