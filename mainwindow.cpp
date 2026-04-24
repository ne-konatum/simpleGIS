#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "mbtilesviewer.h"
#include <QVBoxLayout>
#include <QStatusBar>
#include <QFrame>
#include <QFileDialog>
#include <cmath>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    // Инициализация параметров эллипсоидов
    // WGS-84: a = 6378137.0 м, f = 1/298.257223563
    , m_wgs84{6378137.0, 0.00669437999014}
    // СК-42 (Красовский): a = 6378245.0 м, f = 1/298.3
    , m_krasovsky{6378245.0, 0.00669342162297}
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
    m_coordLabel->setMinimumWidth(450);
    m_coordLabel->setFrameShape(QFrame::Panel);
    m_coordLabel->setFrameShadow(QFrame::Sunken);
    m_coordLabel->setText("WGS-84: Lon: ---, Lat: --- | СК-42 (GK): X: ---, Y: ---");
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
    // Конвертируем в СК-42 Гаусс-Крюгер
    GKCoords gk = wgs84ToSK42GK(longitude, latitude);
    
    // Форматируем и отображаем координаты
    m_coordLabel->setText(QString("WGS-84: Lon: %1°, Lat: %2° | СК-42 (GK): X: %3 м, Y: %4 (зона %5)")
        .arg(longitude, 0, 'f', 6)
        .arg(latitude, 0, 'f', 6)
        .arg(gk.x, 0, 'f', 2)
        .arg(gk.y, 0, 'f', 2)
        .arg(gk.zone));
}

double MainWindow::toRadians(double deg)
{
    return deg * M_PI / 180.0;
}

double MainWindow::toDegrees(double rad)
{
    return rad * 180.0 / M_PI;
}

MainWindow::GKCoords MainWindow::wgs84ToSK42GK(double lon, double lat)
{
    GKCoords result;
    
    // Определяем номер зоны (6-градусная зона)
    result.zone = static_cast<int>((lon + 180.0) / 6.0) + 1;
    if (result.zone < 1) result.zone = 1;
    if (result.zone > 60) result.zone = 60;
    
    // Долгота осевого меридиана зоны
    double L0 = (result.zone - 1) * 6.0 - 180.0 + 3.0;
    
    // Параметры эллипсоида Красовского (СК-42)
    double a = m_krasovsky.a;
    double e2 = m_krasovsky.e2;
    double e_prime2 = e2 / (1.0 - e2); // Второй квадрат эксцентриситета
    
    // Переводим в радианы
    double phi = toRadians(lat);
    double lambda = toRadians(lon);
    double lambda0 = toRadians(L0);
    
    // Разность долгот
    double dLambda = lambda - lambda0;
    
    // Вычисляем вспомогательные величины
    double sinPhi = sin(phi);
    double cosPhi = cos(phi);
    double tanPhi = tan(phi);
    
    // Радиус кривизны в первом вертикале
    double N = a / sqrt(1.0 - e2 * sinPhi * sinPhi);
    
    // Меридианная дуга от экватора (формула для эллипсоида)
    double A0 = 1.0 - e2 / 4.0 - 3.0 * e2 * e2 / 64.0 - 5.0 * e2 * e2 * e2 / 256.0;
    double A2 = 3.0 / 8.0 * (e2 + e2 * e2 / 4.0 + 15.0 * e2 * e2 * e2 / 128.0);
    double A4 = 15.0 / 256.0 * (e2 * e2 + 3.0 * e2 * e2 * e2 / 4.0);
    double A6 = 35.0 * e2 * e2 * e2 / 3072.0;
    
    double M = a * (A0 * phi - A2 * sin(2.0 * phi) + A4 * sin(4.0 * phi) - A6 * sin(6.0 * phi));
    
    // Коэффициенты ряда для проекции Гаусса-Крюгера
    double c2 = N * sinPhi * cosPhi / 2.0;
    double c4 = N * sinPhi * pow(cosPhi, 3) / 24.0 * (5.0 - tanPhi * tanPhi + 9.0 * e_prime2 * cosPhi * cosPhi + 4.0 * e_prime2 * e_prime2 * pow(cosPhi, 4));
    double c6 = N * sinPhi * pow(cosPhi, 5) / 720.0 * (61.0 - 58.0 * tanPhi * tanPhi + tanPhi * tanPhi * tanPhi * tanPhi + 270.0 * e_prime2 * cosPhi * cosPhi - 330.0 * e_prime2 * cosPhi * cosPhi * tanPhi * tanPhi);
    
    double d2 = N * cosPhi * cosPhi / 2.0;
    double d4 = N * pow(cosPhi, 4) / 24.0 * (1.0 - tanPhi * tanPhi + e_prime2 * cosPhi * cosPhi);
    double d6 = N * pow(cosPhi, 6) / 720.0 * (5.0 - 18.0 * tanPhi * tanPhi + tanPhi * tanPhi * tanPhi * tanPhi + 14.0 * e_prime2 * cosPhi * cosPhi - 58.0 * e_prime2 * cosPhi * cosPhi * tanPhi * tanPhi);
    
    // Вычисляем координаты X и Y
    result.x = M + c2 * dLambda * dLambda + c4 * pow(dLambda, 4) + c6 * pow(dLambda, 6);
    result.y = d2 * dLambda + d4 * pow(dLambda, 3) + d6 * pow(dLambda, 5);
    
    // Добавляем номер зоны к координате Y (смещение на 500000 м + номер зоны * 1000000)
    result.y += 500000.0;
    result.y += result.zone * 1000000.0;
    
    return result;
}

void MainWindow::openFile()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Open MBTiles", "", "MBTiles Files (*.mbtiles)");
    if (!fileName.isEmpty()) {
        m_viewer->openFile(fileName);
    }
}

