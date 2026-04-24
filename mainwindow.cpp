#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "mbtilesviewer.h"
#include <QVBoxLayout>
#include <QStatusBar>
#include <QFrame>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    
    // Создаем виджет карты
    m_viewer = new MBTilesViewer(this);
    setCentralWidget(m_viewer);
    
    // Создаем метку для отображения координат в статусной строке
    m_coordLabel = new QLabel(this);
    m_coordLabel->setMinimumWidth(250);
    m_coordLabel->setFrameShape(QFrame::Panel);
    m_coordLabel->setFrameShadow(QFrame::Sunken);
    m_coordLabel->setText("Lon: ---, Lat: ---");
    statusBar()->addPermanentWidget(m_coordLabel);
    statusBar()->show();
    
    // Подключаем сигнал координат к слоту обновления метки
    connect(m_viewer, &MBTilesViewer::cursorCoordinatesChanged, 
            this, &MainWindow::onCursorCoordinatesChanged);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onCursorCoordinatesChanged(double longitude, double latitude)
{
    m_coordLabel->setText(QString("Lon: %1, Lat: %2")
        .arg(longitude, 0, 'f', 6)
        .arg(latitude, 0, 'f', 6));
}

