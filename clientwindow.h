#ifndef CLIENTWINDOW_H
#define CLIENTWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QSpinBox>
#include <QLabel>
#include "mapviewer.h"

class ClientWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ClientWindow(QWidget *parent = nullptr);

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onGoToClicked();
    void onZoomChanged(int zoom);

private:
    void setupUi();
    
    MapViewer *m_mapViewer;
    
    // Элементы управления
    QLineEdit *m_hostEdit;
    QSpinBox *m_portSpin;
    QLabel *m_statusLabel;
    
    QLineEdit *m_latEdit;
    QLineEdit *m_lonEdit;
    QSpinBox *m_zoomSpin;
};

#endif // CLIENTWINDOW_H
