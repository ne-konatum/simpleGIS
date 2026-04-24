#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MBTilesViewer;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onCursorCoordinatesChanged(double longitude, double latitude);
    void openFile();

private:
    // Структура для хранения параметров эллипсоида
    struct Ellipsoid {
        double a; // Большая полуось
        double e2; // Квадрат эксцентриситета
    };

    // Структура для координат Гаусса-Крюгера
    struct GKCoords {
        double x; // Север (метры)
        double y; // Восток (метры, с номером зоны)
        int zone; // Номер зоны
    };

    // Методы конвертации
    GKCoords wgs84ToSK42GK(double lon, double lat);
    double toRadians(double deg);
    double toDegrees(double rad);

    Ui::MainWindow *ui;
    MBTilesViewer *m_viewer;
    QLabel *m_coordLabel;
    QPushButton *m_btnOpen;
    
    // Параметры эллипсоидов
    const Ellipsoid m_wgs84;
    const Ellipsoid m_krasovsky; // Для СК-42
};
#endif // MAINWINDOW_H
