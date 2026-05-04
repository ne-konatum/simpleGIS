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
class HgtManager;
class MapStreamServer;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onCursorCoordinatesChanged(double longitude, double latitude);
    void openFile();
    void selectHgtDirectory();
    void toggleStreamServer();

private:
    struct Ellipsoid {
        double a;
        double e2;
    };
    struct TransformationParams {
        double dx;
        double dy;
        double dz;
    };
    void wgs84ToSK42(double lon, double lat, double &x, double &y, int &zone);
    void geodeticToGeocentric(double lat, double lon, double h, const Ellipsoid &ell, double &X, double &Y, double &Z);
    void geocentricToGeodetic(double X, double Y, double Z, const Ellipsoid &ell, double &lat, double &lon, double &h);

    double toRadians(double deg);
    double toDegrees(double rad);

    Ui::MainWindow *ui;
    MBTilesViewer *m_viewer;

    DEMReader *m_demReader;
    HgtManager *m_hgtManager;
    
    MapStreamServer *m_streamServer;
    bool m_streamServerEnabled;

    QLabel *m_coordLabel;
    QLabel *m_elevationLabel;
    QLabel *m_streamStatusLabel;

    QPushButton *m_btnOpen;
    QPushButton *m_btnSelectHgtDir;
    QPushButton *m_btnToggleStream;

    const Ellipsoid m_wgs84;
    const Ellipsoid m_krasovsky;
    const TransformationParams m_transformParams;
};
#endif // MAINWINDOW_H
