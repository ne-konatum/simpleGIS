#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>

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

private:
    Ui::MainWindow *ui;
    MBTilesViewer *m_viewer;
    QLabel *m_coordLabel;
};
#endif // MAINWINDOW_H
