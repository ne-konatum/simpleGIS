#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MBTilesViewer;
class DEMReader;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onCursorCoordinatesChanged(double longitude, double latitude);
    void openFile();
    void openDEMFile();

private:
    // Структура для хранения параметров эллипсоида
    struct Ellipsoid {
        double a; // Большая полуось
        double e2; // Квадрат эксцентриситета
    };

    // Структура для параметров трансформации датума
    struct TransformationParams {
        double dx;
        double dy;
        double dz;
    };

    // Методы конвертации
    void wgs84ToSK42(double lon, double lat, double &x, double &y, int &zone);

    // Вспомогательные методы преобразования координат
    void geodeticToGeocentric(double lat, double lon, double h, const Ellipsoid &ell, double &X, double &Y, double &Z);
    // Исправленная сигнатура: параметры эллипсоида перед выходными аргументами
    void geocentricToGeodetic(double X, double Y, double Z, const Ellipsoid &ell, double &lat, double &lon, double &h);

    double toRadians(double deg);
    double toDegrees(double rad);

    Ui::MainWindow *ui;
    MBTilesViewer *m_viewer;
    DEMReader *m_demReader;
    QLabel *m_coordLabel;
    QLabel *m_elevationLabel;
    QPushButton *m_btnOpen;
    QPushButton *m_btnOpenDEM;

    // Параметры эллипсоидов
    const Ellipsoid m_wgs84;
    const Ellipsoid m_krasovsky; // Для СК-42

    // Параметры трансформации (константные)
    const TransformationParams m_transformParams;
};
#endif // MAINWINDOW_H
