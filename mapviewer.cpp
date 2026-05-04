#include "mapviewer.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QResizeEvent>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Вспомогательный класс для ключа QMap
template<typename T1, typename T2, typename T3>
class QTriple {
public:
    QTriple(T1 a, T2 b, T3 c) : v1(a), v2(b), v3(c) {}
    
    bool operator<(const QTriple &other) const {
        if (v1 != other.v1) return v1 < other.v1;
        if (v2 != other.v2) return v2 < other.v2;
        return v3 < other.v3;
    }
    
    T1 v1;
    T2 v2;
    T3 v3;
};

// Используем QTriple вместо QTuple (который может быть недоступен в старых Qt)
using TileKey = QTriple<int, int, int>;

MapViewer::MapViewer(QWidget *parent)
    : QWidget(parent)
    , m_client(new MapStreamClient(this))
    , m_zoom(5)
    , m_centerLat(0.0)
    , m_centerLon(0.0)
    , m_dragging(false)
    , m_serverHost("127.0.0.1")
    , m_serverPort(5555)
    , m_currentElevation(0.0)
{
    setMinimumSize(400, 300);
    setFocusPolicy(Qt::StrongFocus);
    
    // Подключаем сигналы клиента
    connect(m_client, &MapStreamClient::connected, this, &MapViewer::onConnected);
    connect(m_client, &MapStreamClient::disconnected, this, &MapViewer::onDisconnected);
    connect(m_client, &MapStreamClient::errorOccurred, this, &MapViewer::onError);
    connect(m_client, &MapStreamClient::tileReceived, this, &MapViewer::onTileReceived);
    connect(m_client, &MapStreamClient::elevationReceived, this, &MapViewer::onElevationReceived);
    connect(m_client, &MapStreamClient::metadataReceived, this, &MapViewer::onMetadataReceived);
}

MapViewer::~MapViewer()
{
    disconnectFromServer();
}

void MapViewer::setServerAddress(const QString &host, quint16 port)
{
    m_serverHost = host;
    m_serverPort = port;
}

void MapViewer::connectToServer()
{
    m_client->connectToServer(m_serverHost, m_serverPort);
}

void MapViewer::disconnectFromServer()
{
    m_client->disconnectFromServer();
}

void MapViewer::setZoom(int zoom)
{
    m_zoom = qBound(0, zoom, 18);
    loadVisibleTiles();
    update();
}

void MapViewer::centerOn(double latitude, double longitude)
{
    m_centerLat = qBound(-85.0, latitude, 85.0);
    m_centerLon = qBound(-180.0, longitude, 180.0);
    loadVisibleTiles();
    update();
}

void MapViewer::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Очистка фона
    painter.fillRect(rect(), QColor(240, 240, 240));
    
    // Вычисляем видимую область в тайлах
    QPointF centerTile = latLonToTileXY(m_centerLat, m_centerLon, m_zoom);
    
    int tilesX = (width() / 256) + 2;
    int tilesY = (height() / 256) + 2;
    
    int startTileX = static_cast<int>(std::floor(centerTile.x() - tilesX / 2.0));
    int startTileY = static_cast<int>(std::floor(centerTile.y() - tilesY / 2.0));
    
    // Смещение для центрирования
    double offsetX = (centerTile.x() - std::floor(centerTile.x())) * 256;
    double offsetY = (centerTile.y() - std::floor(centerTile.y())) * 256;
    
    int screenX = static_cast<int>(width() / 2 - offsetX);
    int screenY = static_cast<int>(height() / 2 - offsetY);
    
    // Отрисовка тайлов
    for (int dy = 0; dy <= tilesY; ++dy) {
        for (int dx = 0; dx <= tilesX; ++dx) {
            int tileX = startTileX + dx;
            int tileY = startTileY + dy;
            
            // Проверка границ тайлов
            int maxTile = (1 << m_zoom) - 1;
            if (tileX < 0 || tileX > maxTile || tileY < 0 || tileY > maxTile)
                continue;
            
            drawTile(painter, tileX, tileY, m_zoom);
        }
    }
    
    // Рамка виджета
    painter.setPen(QPen(Qt::darkGray, 2));
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
    
    // Информация о зуме и координатах
    painter.setPen(Qt::black);
    QFont infoFont = painter.font();
    infoFont.setPointSize(10);
    painter.setFont(infoFont);
    QString info = QString("Zoom: %1 | Center: %2, %3")
        .arg(m_zoom)
        .arg(m_centerLat, 0, 'f', 4)
        .arg(m_centerLon, 0, 'f', 4);
    painter.drawText(10, 25, info);
    
    // Высота под курсором
    if (m_elevationPos.x() >= 0 && m_elevationPos.x() <= width() &&
        m_elevationPos.y() >= 0 && m_elevationPos.y() <= height()) {
        QString elevInfo = QString("Elevation: %1 m").arg(m_currentElevation, 0, 'f', 1);
        painter.drawText(10, 45, elevInfo);
    }
    
    // Статус соединения
    QString status = m_client->isConnected() ? "Connected" : "Disconnected";
    QColor statusColor = m_client->isConnected() ? Qt::green : Qt::red;
    painter.setPen(statusColor);
    painter.drawText(width() - 120, 25, status);
}

void MapViewer::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        
        // Запрос высоты для точки под курсором
        QPointF latLon = screenToLatLon(event->pos());
        m_client->requestElevation(latLon.y(), latLon.x());
        m_elevationPos = event->pos();
    }
}

void MapViewer::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
        QPoint delta = event->pos() - m_lastMousePos;
        m_lastMousePos = event->pos();
        
        // Преобразование смещения в изменение координат
        double pixelsPerDegree = (256.0 * (1 << m_zoom)) / 360.0;
        m_centerLon -= delta.x() / pixelsPerDegree;
        m_centerLat += delta.y() / pixelsPerDegree;
        
        // Ограничение широты
        m_centerLat = qBound(-85.0, m_centerLat, 85.0);
        
        loadVisibleTiles();
        update();
    } else {
        // Запрос высоты при движении мыши
        QPointF latLon = screenToLatLon(event->pos());
        m_client->requestElevation(latLon.y(), latLon.x());
        m_elevationPos = event->pos();
    }
}

void MapViewer::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        setCursor(Qt::ArrowCursor);
    }
}

void MapViewer::wheelEvent(QWheelEvent *event)
{
    int delta = event->angleDelta().y();
    int newZoom = m_zoom + (delta > 0 ? 1 : -1);
    newZoom = qBound(0, newZoom, 18);
    
    if (newZoom != m_zoom) {
        setZoom(newZoom);
    }
}

void MapViewer::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    loadVisibleTiles();
}

void MapViewer::onConnected()
{
    update();
    loadVisibleTiles();
}

void MapViewer::onDisconnected()
{
    update();
}

void MapViewer::onError(const QString &error)
{
    qWarning() << "MapViewer error:" << error;
    update();
}

void MapViewer::onTileReceived(int zoom, int x, int y, const QImage &image)
{
    if (zoom == m_zoom) {
        TileKey key(zoom, x, y);
        m_tileCache.insert(key, image);
        update();
    }
}

void MapViewer::onElevationReceived(double latitude, double longitude, double elevation)
{
    m_currentElevation = elevation;
    update();
}

void MapViewer::onMetadataReceived(const QVariantMap &metadata)
{
    qInfo() << "Metadata received:" << metadata;
}

QPointF MapViewer::latLonToTileXY(double latitude, double longitude, int zoom)
{
    double n = std::pow(2.0, zoom);
    double latRad = latitude * M_PI / 180.0;
    double lonRad = longitude * M_PI / 180.0;
    
    double x = (lonRad + M_PI) / (2 * M_PI) * n;
    double y = (1.0 - std::log(std::tan(latRad) + 1.0 / std::cos(latRad)) / M_PI) / 2.0 * n;
    
    return QPointF(x, y);
}

QPointF MapViewer::tileXYToLatLon(int x, int y, int zoom)
{
    double n = std::pow(2.0, zoom);
    double lonRad = (x / n) * 2 * M_PI - M_PI;
    double latRad = std::atan(std::sinh(M_PI * (1 - 2 * y / n)));
    
    double latitude = latRad * 180.0 / M_PI;
    double longitude = lonRad * 180.0 / M_PI;
    
    return QPointF(longitude, latitude);
}

void MapViewer::loadVisibleTiles()
{
    if (!m_client->isConnected())
        return;
    
    QPointF centerTile = latLonToTileXY(m_centerLat, m_centerLon, m_zoom);
    
    int tilesX = (width() / 256) + 3;
    int tilesY = (height() / 256) + 3;
    
    int startTileX = static_cast<int>(std::floor(centerTile.x() - tilesX / 2.0));
    int startTileY = static_cast<int>(std::floor(centerTile.y() - tilesY / 2.0));
    
    int maxTile = (1 << m_zoom) - 1;
    
    for (int dy = 0; dy <= tilesY; ++dy) {
        for (int dx = 0; dx <= tilesX; ++dx) {
            int tileX = startTileX + dx;
            int tileY = startTileY + dy;
            
            if (tileX < 0 || tileX > maxTile || tileY < 0 || tileY > maxTile)
                continue;
            
            TileKey key(m_zoom, tileX, tileY);
            if (!m_tileCache.contains(key)) {
                m_client->requestTile(m_zoom, tileX, tileY);
            }
        }
    }
}

void MapViewer::drawTile(QPainter &painter, int x, int y, int zoom)
{
    TileKey key(zoom, x, y);
    
    if (m_tileCache.contains(key)) {
        const QImage &image = m_tileCache[key];
        
        // Вычисляем позицию на экране
        QPointF centerTile = latLonToTileXY(m_centerLat, m_centerLon, zoom);
        
        double offsetX = (centerTile.x() - std::floor(centerTile.x())) * 256;
        double offsetY = (centerTile.y() - std::floor(centerTile.y())) * 256;
        
        int screenX = static_cast<int>(width() / 2 - offsetX + (x - std::floor(centerTile.x())) * 256);
        int screenY = static_cast<int>(height() / 2 - offsetY + (y - std::floor(centerTile.y())) * 256);
        
        painter.drawImage(screenX, screenY, image);
    } else {
        // Пустой тайл (серый фон)
        QPointF centerTile = latLonToTileXY(m_centerLat, m_centerLon, zoom);
        
        double offsetX = (centerTile.x() - std::floor(centerTile.x())) * 256;
        double offsetY = (centerTile.y() - std::floor(centerTile.y())) * 256;
        
        int screenX = static_cast<int>(width() / 2 - offsetX + (x - std::floor(centerTile.x())) * 256);
        int screenY = static_cast<int>(height() / 2 - offsetY + (y - std::floor(centerTile.y())) * 256);
        
        painter.fillRect(screenX, screenY, 256, 256, QColor(200, 200, 200));
        
        // Текст "Loading..."
        painter.setPen(Qt::gray);
        painter.drawText(screenX, screenY, 256, 256, Qt::AlignCenter, "Loading...");
    }
}

QPointF MapViewer::screenToLatLon(const QPoint &screenPos)
{
    QPointF centerTile = latLonToTileXY(m_centerLat, m_centerLon, m_zoom);
    
    double offsetX = (centerTile.x() - std::floor(centerTile.x())) * 256;
    double offsetY = (centerTile.y() - std::floor(centerTile.y())) * 256;
    
    double tileX = centerTile.x() + (screenPos.x() - width() / 2.0 + offsetX) / 256.0;
    double tileY = centerTile.y() + (screenPos.y() - height() / 2.0 + offsetY) / 256.0;
    
    return tileXYToLatLon(static_cast<int>(tileX), static_cast<int>(tileY), m_zoom);
}
