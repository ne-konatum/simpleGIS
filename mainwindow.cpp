#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "mbtilesviewer.h"
#include <QVBoxLayout>
#include <QStatusBar>
#include <QFrame>
#include <QFileDialog>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    
    // Создаем центральный виджет с layout
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);
    
    // Создаем кнопку для открытия файла
    m_btnOpen = new QPushButton("Open MBTiles File", this);
    connect(m_btnOpen, &QPushButton::clicked, this, &MainWindow::openFile);
    
    // Создаем виджет карты
    m_viewer = new MBTilesViewer(this);
    
    layout->addWidget(m_btnOpen);
    layout->addWidget(m_viewer);
    
    setCentralWidget(centralWidget);
    
    // Устанавливаем заголовок и размер окна
    setWindowTitle("Simple GIS - MBTiles Viewer");
    resize(1024, 768);
    
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

void MainWindow::openFile()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Open MBTiles", "", "MBTiles Files (*.mbtiles)");
    if (!fileName.isEmpty()) {
        m_viewer->openFile(fileName);
    }
}

