// main.cpp
#include <QApplication>
#include <QMainWindow>
#include <QPushButton>
#include <QFileDialog>
#include <QVBoxLayout>
#include <QWidget>
#include "mbtilesviewer.h"
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    MainWindow window;
    window.setWindowTitle("Simple GIS - MBTiles Viewer");
    window.resize(1024, 768);

    QWidget *centralWidget = new QWidget(&window);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    QPushButton *btnOpen = new QPushButton("Open MBTiles File");
    MBTilesViewer *viewer = new MBTilesViewer();

    layout->addWidget(btnOpen);
    layout->addWidget(viewer);

    window.setCentralWidget(centralWidget);
    window.show();

    // Исправленная лямбда: захватываем viewer по указателю, используем её как родителя для диалога
    QObject::connect(btnOpen, &QPushButton::clicked, [viewer]() {
        QString fileName = QFileDialog::getOpenFileName(viewer, "Open MBTiles", "", "MBTiles Files (*.mbtiles)");
        if (!fileName.isEmpty()) {
            viewer->openFile(fileName);
        }
    });

    return app.exec();
}
