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
    // Проверяем, загружена ли карта
    if (!m_viewer || !m_viewer->isMapLoaded()) {
        m_coordLabel->setText("Нет данных (карта не загружена)");
        return;
    }
    
    // Конвертируем в СК-42 Гаусс-Крюгер
    GKCoords gk = wgs84ToSK42GK(longitude, latitude);
    
    // Форматируем и отображаем координаты
    m_coordLabel->setText(QString("WGS-84: Lon: %1°, Lat: %2° | СК-42 (GK): X: %3 м, Y: %4 (зона %5)")
        .arg(longitude, 0, 'f', 6)
        .arg(latitude, 0, 'f', 6)
        .arg(gk.x, 0, 'f', 3)
        .arg(gk.y, 0, 'f', 3)
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
    // Для долготы 37.618335: 37.61/6 = 6.27 -> floor = 6 -> зона = 6+1 = 7
    result.zone = static_cast<int>(std::floor(lon / 6.0)) + 1;
    if (result.zone < 1) result.zone = 1;
    if (result.zone > 60) result.zone = 60;
    
    // Долгота осевого меридиана зоны: L0 = 6 * zone - 3
    // Для зоны 7: L0 = 6*7 - 3 = 39°
    double L0 = 6.0 * result.zone - 3.0;
    
    // Параметры эллипсоида Красовского (СК-42)
    double a = m_krasovsky.a;        // 6378245.0 м
    double f = 1.0 / 298.3;          // Сжатие
    double e2 = 2 * f - f * f;       // Квадрат первого эксцентриситета
    double e2_prime = e2 / (1.0 - e2); // Квадрат второго эксцентриситета
    
    // Переводим в радианы
    double B = toRadians(lat);       // Широта
    double L = toRadians(lon);       // Долгота
    double L0_rad = toRadians(L0);   // Осевой меридиан
    double l = L - L0_rad;           // Разность долгот
    
    // Вспомогательные величины
    double sinB = sin(B);
    double cosB = cos(B);
    double tanB = tan(B);
    
    // Радиус кривизны в первом вертикале
    double W = sqrt(1.0 - e2 * sinB * sinB);
    double N = a / W;
    
    // Длина дуги меридиана от экватора до широты B (ряд Бесселя для эллипсоида Красовского)
    // Коэффициенты ряда
    double A0 = 1.0 + 3.0/4.0*e2 + 45.0/64.0*e2*e2 + 175.0/256.0*e2*e2*e2 + 11025.0/16384.0*e2*e2*e2*e2;
    double A2 = 3.0/4.0*e2 + 15.0/16.0*e2*e2 + 525.0/512.0*e2*e2*e2 + 2205.0/2048.0*e2*e2*e2*e2;
    double A4 = 15.0/64.0*e2*e2 + 105.0/256.0*e2*e2*e2 + 2205.0/4096.0*e2*e2*e2*e2;
    double A6 = 35.0/512.0*e2*e2*e2 + 315.0/2048.0*e2*e2*e2*e2;
    double A8 = 315.0/16384.0*e2*e2*e2*e2;
    
    // Меридианная дуга X0
    double X0 = a * (A0 * B - A2 * sin(2*B) + A4 * sin(4*B) - A6 * sin(6*B) + A8 * sin(8*B));
    
    // Коэффициенты для рядов Гаусса-Крюгера
    double t = tanB;
    double eta2 = e2_prime * cosB * cosB;
    
    // Вычисление абсциссы X (северное направление)
    double c2 = (N * t * cosB * cosB) / 2.0;
    double c4 = (N * t * pow(cosB, 4)) / 24.0 * (5.0 - t*t + 9.0*eta2 + 4.0*eta2*eta2);
    double c6 = (N * t * pow(cosB, 6)) / 720.0 * (61.0 - 58.0*t*t + pow(t,4) + 270.0*eta2 - 330.0*eta2*t*t);
    
    result.x = X0 + c2 * l * l + c4 * pow(l, 4) + c6 * pow(l, 6);
    
    // Вычисление ординаты Y (восточное направление)
    double d1 = N * cosB;
    double d3 = (N * pow(cosB, 3)) / 6.0 * (1.0 - t*t + eta2);
    double d5 = (N * pow(cosB, 5)) / 120.0 * (5.0 - 18.0*t*t + pow(t,4) + 14.0*eta2 - 58.0*eta2*t*t);
    
    result.y = d1 * l + d3 * pow(l, 3) + d5 * pow(l, 5);
    
    // Добавляем ложное смещение: 500000 м + номер зоны * 1000000
    result.y = result.y + 500000.0 + (result.zone * 1000000.0);
    
    return result;
}

void MainWindow::openFile()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Open MBTiles", "", "MBTiles Files (*.mbtiles)");
    if (!fileName.isEmpty()) {
        m_viewer->openFile(fileName);
    }
}

