#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "mbtilesviewer.h"
#include "demreader.h"
#include "hgtmanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStatusBar>
#include <QFrame>
#include <QFileDialog>
#include <QDir>
#include <cmath>
#include <QDebug>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_wgs84{6378137.0, 0.00669437999014}
    , m_krasovsky{6378245.0, 0.00669342162297}
    , m_transformParams{-26.0, -125.0, -7.0}
    , m_hgtManager(nullptr)
    , m_demReader(nullptr)
{
    ui->setupUi(this);

    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    QHBoxLayout *btnLayout = new QHBoxLayout();

    m_btnOpen = new QPushButton("Open MBTiles File", this);
    connect(m_btnOpen, &QPushButton::clicked, this, &MainWindow::openFile);

    m_btnSelectHgtDir = new QPushButton("Select HGT Directory", this);
    connect(m_btnSelectHgtDir, &QPushButton::clicked, this, &MainWindow::selectHgtDirectory);

    btnLayout->addWidget(m_btnOpen);
    btnLayout->addWidget(m_btnSelectHgtDir);

    m_viewer = new MBTilesViewer(this);

    // Создаем DEMReader без менеджера initially
    m_demReader = new DEMReader(nullptr, this);

    layout->addLayout(btnLayout);
    layout->addWidget(m_viewer);

    setCentralWidget(centralWidget);
    setWindowTitle("Simple GIS - MBTiles Viewer");
    resize(1024, 768);

    m_coordLabel = new QLabel(this);
    m_coordLabel->setMinimumWidth(600);
    m_coordLabel->setFrameShape(QFrame::Panel);
    m_coordLabel->setFrameShadow(QFrame::Sunken);
    m_coordLabel->setText("WGS-84: Lon: ---, Lat: --- | СК-42 (GK): X: ---, Y: ---");
    statusBar()->addPermanentWidget(m_coordLabel);

    m_elevationLabel = new QLabel(this);
    m_elevationLabel->setMinimumWidth(200);
    m_elevationLabel->setFrameShape(QFrame::Panel);
    m_elevationLabel->setFrameShadow(QFrame::Sunken);
    m_elevationLabel->setText("Height: --- m");
    statusBar()->addPermanentWidget(m_elevationLabel);

    statusBar()->show();

    connect(m_viewer, &MBTilesViewer::cursorCoordinatesChanged,
            this, &MainWindow::onCursorCoordinatesChanged);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::selectHgtDirectory()
{
    QString dirPath = QFileDialog::getExistingDirectory(this, "Select HGT Directory", QDir::homePath());

    if (dirPath.isEmpty()) {
        return;
    }

    qDebug() << "Selected HGT directory:" << dirPath;

    if (m_hgtManager) {
        delete m_hgtManager;
        m_hgtManager = nullptr;
    }

    m_hgtManager = new HgtManager(dirPath, this);
    m_demReader->setHgtManager(m_hgtManager);

    int count = m_hgtManager->tileCount();
    m_elevationLabel->setText(QString("HGT: %1 tiles").arg(count));
    statusBar()->showMessage(QString("HGT loaded: %1 tiles from %2").arg(count).arg(dirPath), 5000);

    // Принудительно обновить высоту для текущей позиции курсора, если карта уже открыта
    // Нам нужно где-то хранить последние координаты, или запросить их у viewer, если бы он их хранил.
    // Пока просто ждем движения мыши.
}

void MainWindow::onCursorCoordinatesChanged(double longitude, double latitude)
{
    if (!m_viewer || !m_viewer->isMapLoaded()) {
        m_coordLabel->setText("Нет данных (карта не загружена)");
        return;
    }

    double sk42_x, sk42_y;
    int zone;
    wgs84ToSK42(longitude, latitude, sk42_x, sk42_y, zone);

    QString text = QString("WGS-84: Lon: %1, Lat: %2 | "
                           "СК-42 (Гаусс-Крюгер): X: %3, Y: %4 (зона %5)")
                       .arg(longitude, 0, 'f', 6)
                       .arg(latitude, 0, 'f', 6)
                       .arg(sk42_x, 0, 'f', 3)
                       .arg(sk42_y, 0, 'f', 3)
                       .arg(zone);
    m_coordLabel->setText(text);

    // Логика получения высоты
    if (!m_hgtManager) {
        m_elevationLabel->setText("Height: Select HGT Dir");
        return;
    }

    bool found = false;
    double height = 0.0;

    // 1. Пробуем получить высоту из текущего загруженного файла
    if (m_demReader->isLoaded()) {
        if (m_demReader->getElevation(latitude, longitude, height)) {
            found = true;
        } else {
            // Если текущий файл не покрывает точку, пробуем найти и загрузить новый
            // Это может случиться на границе тайлов
            if (m_demReader->updateForLocation(latitude, longitude)) {
                // Если updateForLocation вернул true, значит файл найден (или был актуален)
                // Пробуем снова получить высоту
                if (m_demReader->getElevation(latitude, longitude, height)) {
                    found = true;
                }
            }
        }
    } else {
        // Файл еще не загружен, пытаемся загрузить для этой точки
        if (m_demReader->updateForLocation(latitude, longitude)) {
            if (m_demReader->getElevation(latitude, longitude, height)) {
                found = true;
            }
        }
    }

    if (found) {
        m_elevationLabel->setText(QString("Height: %1 m").arg(height, 0, 'f', 1));
    } else {
        // Проверим, есть ли вообще файл для этой зоны в менеджере
        QString file = m_hgtManager->findFileForLocation(latitude, longitude);
        if (file.isEmpty()) {
            m_elevationLabel->setText("Height: No HGT tile");
        } else {
            // Файл есть, но не загрузился или точка вне границ (странно)
            m_elevationLabel->setText("Height: Load Error");
            qDebug() << "DEMReader: Failed to get elevation despite tile existence:" << file;
        }
    }
}

double MainWindow::toRadians(double deg) { return deg * M_PI / 180.0; }
double MainWindow::toDegrees(double rad) { return rad * 180.0 / M_PI; }

void MainWindow::geodeticToGeocentric(double lat, double lon, double h, const Ellipsoid &ell, double &X, double &Y, double &Z)
{
    double phi = toRadians(lat);
    double lambda = toRadians(lon);
    double sin_phi = std::sin(phi);
    double cos_phi = std::cos(phi);
    double N = ell.a / std::sqrt(1.0 - ell.e2 * sin_phi * sin_phi);
    X = (N + h) * cos_phi * std::cos(lambda);
    Y = (N + h) * cos_phi * std::sin(lambda);
    Z = (N * (1.0 - ell.e2) + h) * sin_phi;
}

void MainWindow::geocentricToGeodetic(double X, double Y, double Z, const Ellipsoid &ell, double &lat, double &lon, double &h)
{
    double p = std::sqrt(X * X + Y * Y);
    double theta = std::atan2(Z * ell.a, p * ell.a * std::sqrt(1.0 - ell.e2));
    double lat_rad = std::atan2(Z + ell.a * ell.e2 * std::sin(theta)*std::sin(theta)*std::sin(theta),
                                p - ell.a * ell.e2 * std::cos(theta)*std::cos(theta)*std::cos(theta));
    lon = std::atan2(Y, X);
    double sin_lat = std::sin(lat_rad);
    double N = ell.a / std::sqrt(1.0 - ell.e2 * sin_lat * sin_lat);
    h = p / std::cos(lat_rad) - N;
    lat = toDegrees(lat_rad);
    lon = toDegrees(lon);
}

void MainWindow::wgs84ToSK42(double lon, double lat, double &x, double &y, int &zone)
{
    zone = static_cast<int>(std::floor(lon / 6.0)) + 1;
    if (zone < 1) zone = 1; if (zone > 60) zone = 60;
    double L0 = zone * 6.0 - 3.0;
    double X_wgs, Y_wgs, Z_wgs;
    geodeticToGeocentric(lat, lon, 0.0, m_wgs84, X_wgs, Y_wgs, Z_wgs);
    double X_sk = X_wgs + m_transformParams.dx;
    double Y_sk = Y_wgs + m_transformParams.dy;
    double Z_sk = Z_wgs + m_transformParams.dz;
    double lat_sk, lon_sk, h_sk;
    geocentricToGeodetic(X_sk, Y_sk, Z_sk, m_krasovsky, lat_sk, lon_sk, h_sk);

    double phi = toRadians(lat_sk);
    double lambda = toRadians(lon_sk);
    double lambda0 = toRadians(L0);
    double dl = lambda - lambda0;
    const double a = m_krasovsky.a;
    const double e2 = m_krasovsky.e2;
    const double e2_prime = e2 / (1.0 - e2);
    double sin_phi = std::sin(phi);
    double cos_phi = std::cos(phi);
    double tan_phi = std::tan(phi);
    double N = a / std::sqrt(1.0 - e2 * sin_phi * sin_phi);

    double A0 = 1.0 - e2/4.0 - 3.0*e2*e2/64.0 - 5.0*e2*e2*e2/256.0;
    double A2 = 3.0/8.0 * (e2 + e2*e2/4.0 + 15.0*e2*e2*e2/128.0);
    double A4 = 15.0/256.0 * (e2*e2 + 3.0*e2*e2*e2/4.0);
    double A6 = 35.0*e2*e2*e2/3072.0;
    double X_meridian = a * (A0 * phi - A2 * std::sin(2.0*phi) + A4 * std::sin(4.0*phi) - A6 * std::sin(6.0*phi));

    double t = tan_phi;
    double eta2 = e2_prime * cos_phi * cos_phi;

    x = X_meridian + (N * t / 2.0) * cos_phi * cos_phi * dl * dl +
        (N * t / 24.0) * std::pow(cos_phi, 4) * (5.0 - t*t + 9.0*eta2 + 4.0*eta2*eta2) * std::pow(dl, 4);
    y = N * cos_phi * dl +
        (N / 6.0) * std::pow(cos_phi, 3) * (eta2 - t*t) * std::pow(dl, 3) +
        (N / 120.0) * std::pow(cos_phi, 5) * (5.0 - 18.0*t*t + std::pow(t,4) + 14.0*eta2 - 58.0*t*t*eta2) * std::pow(dl, 5);
    y += 500000.0 + zone * 1000000.0;
}

void MainWindow::openFile()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Open MBTiles", "", "MBTiles Files (*.mbtiles)");
    if (!fileName.isEmpty()) m_viewer->openFile(fileName);
}
