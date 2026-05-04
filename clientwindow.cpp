#include "clientwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QGroupBox>
#include <QMessageBox>
#include <QStatusBar>

ClientWindow::ClientWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
    
    setWindowTitle("Map Stream Client");
    resize(1024, 768);
}

void ClientWindow::setupUi()
{
    // Центральный виджет
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    
    // Панель управления
    QGroupBox *controlGroup = new QGroupBox("Connection & Navigation", this);
    QVBoxLayout *controlLayout = new QVBoxLayout(controlGroup);
    
    // Первая строка: хост и порт
    QHBoxLayout *connLayout = new QHBoxLayout();
    
    connLayout->addWidget(new QLabel("Host:", this));
    m_hostEdit = new QLineEdit("127.0.0.1", this);
    m_hostEdit->setMaximumWidth(150);
    connLayout->addWidget(m_hostEdit);
    
    connLayout->addWidget(new QLabel("Port:", this));
    m_portSpin = new QSpinBox(this);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(5555);
    m_portSpin->setMaximumWidth(80);
    connLayout->addWidget(m_portSpin);
    
    QPushButton *connectBtn = new QPushButton("Connect", this);
    connect(connectBtn, &QPushButton::clicked, this, &ClientWindow::onConnectClicked);
    connLayout->addWidget(connectBtn);
    
    QPushButton *disconnectBtn = new QPushButton("Disconnect", this);
    connect(disconnectBtn, &QPushButton::clicked, this, &ClientWindow::onDisconnectClicked);
    connLayout->addWidget(disconnectBtn);
    
    m_statusLabel = new QLabel("Disconnected", this);
    m_statusLabel->setStyleSheet("color: red; font-weight: bold;");
    connLayout->addWidget(m_statusLabel);
    
    connLayout->addStretch();
    controlLayout->addLayout(connLayout);
    
    // Вторая строка: навигация
    QHBoxLayout *navLayout = new QHBoxLayout();
    
    navLayout->addWidget(new QLabel("Latitude:", this));
    m_latEdit = new QLineEdit("0", this);
    m_latEdit->setMaximumWidth(100);
    navLayout->addWidget(m_latEdit);
    
    navLayout->addWidget(new QLabel("Longitude:", this));
    m_lonEdit = new QLineEdit("0", this);
    m_lonEdit->setMaximumWidth(100);
    navLayout->addWidget(m_lonEdit);
    
    navLayout->addWidget(new QLabel("Zoom:", this));
    m_zoomSpin = new QSpinBox(this);
    m_zoomSpin->setRange(0, 18);
    m_zoomSpin->setValue(5);
    m_zoomSpin->setMaximumWidth(60);
    connect(m_zoomSpin, QOverload<int>::of(&QSpinBox::valueChanged), 
            this, &ClientWindow::onZoomChanged);
    navLayout->addWidget(m_zoomSpin);
    
    QPushButton *goBtn = new QPushButton("Go To", this);
    connect(goBtn, &QPushButton::clicked, this, &ClientWindow::onGoToClicked);
    navLayout->addWidget(goBtn);
    
    navLayout->addStretch();
    controlLayout->addLayout(navLayout);
    
    mainLayout->addWidget(controlGroup);
    
    // Виджет карты
    m_mapViewer = new MapViewer(this);
    mainLayout->addWidget(m_mapViewer, 1);
    
    // Статусная строка
    statusBar()->showMessage("Ready");
}

void ClientWindow::onConnectClicked()
{
    QString host = m_hostEdit->text().trimmed();
    quint16 port = static_cast<quint16>(m_portSpin->value());
    
    if (host.isEmpty()) {
        QMessageBox::warning(this, "Warning", "Please enter a host address");
        return;
    }
    
    m_mapViewer->setServerAddress(host, port);
    m_mapViewer->connectToServer();
    
    m_statusLabel->setText("Connecting...");
    m_statusLabel->setStyleSheet("color: orange; font-weight: bold;");
    statusBar()->showMessage(QString("Connecting to %1:%2...").arg(host).arg(port));
}

void ClientWindow::onDisconnectClicked()
{
    m_mapViewer->disconnectFromServer();
    
    m_statusLabel->setText("Disconnected");
    m_statusLabel->setStyleSheet("color: red; font-weight: bold;");
    statusBar()->showMessage("Disconnected from server");
}

void ClientWindow::onGoToClicked()
{
    bool latOk, lonOk;
    double lat = m_latEdit->text().toDouble(&latOk);
    double lon = m_lonEdit->text().toDouble(&lonOk);
    
    if (!latOk || !lonOk) {
        QMessageBox::warning(this, "Warning", "Invalid coordinates");
        return;
    }
    
    if (lat < -90 || lat > 90) {
        QMessageBox::warning(this, "Warning", "Latitude must be between -90 and 90");
        return;
    }
    
    if (lon < -180 || lon > 180) {
        QMessageBox::warning(this, "Warning", "Longitude must be between -180 and 180");
        return;
    }
    
    int zoom = m_zoomSpin->value();
    
    m_mapViewer->setZoom(zoom);
    m_mapViewer->centerOn(lat, lon);
    
    statusBar()->showMessage(QString("Centered on %1, %2 at zoom %3")
        .arg(lat, 0, 'f', 4)
        .arg(lon, 0, 'f', 4)
        .arg(zoom));
}

void ClientWindow::onZoomChanged(int zoom)
{
    m_mapViewer->setZoom(zoom);
}
