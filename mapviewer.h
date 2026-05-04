#ifndef MAPVIEWER_H
#define MAPVIEWER_H

#include <QWidget>
#include <QMap>
#include <QImage>
#include <QPoint>
#include "mapstreamclient.h"

// Вспомогательный класс для ключа QMap (вместо несуществующего QTuple)
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

using TileKey = QTriple<int, int, int>;

class MapViewer : public QWidget
{
    Q_OBJECT

public:
    explicit MapViewer(QWidget *parent = nullptr);
    ~MapViewer();
    
    void setServerAddress(const QString &host, quint16 port);
    void connectToServer();
    void disconnectFromServer();
    
    // Управление картой
    void setZoom(int zoom);
    void centerOn(double latitude, double longitude);
    
    int zoom() const { return m_zoom; }
    double centerLatitude() const { return m_centerLat; }
    double centerLongitude() const { return m_centerLon; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onConnected();
    void onDisconnected();
    void onError(const QString &error);
    void onTileReceived(int zoom, int x, int y, const QImage &image);
    void onElevationReceived(double latitude, double longitude, double elevation);
    void onMetadataReceived(const QVariantMap &metadata);

private:
    // Конвертация между координатами и тайлами
    static QPointF latLonToTileXY(double latitude, double longitude, int zoom);
    static QPointF tileXYToLatLon(int x, int y, int zoom);
    
    // Загрузка видимых тайлов
    void loadVisibleTiles();
    
    // Отрисовка тайла
    void drawTile(QPainter &painter, int x, int y, int zoom);
    
    // Преобразование экранных координат в географические
    QPointF screenToLatLon(const QPoint &screenPos);
    
    MapStreamClient *m_client;
    
    int m_zoom;
    double m_centerLat;
    double m_centerLon;
    
    // Кэш тайлов: key = (zoom, x, y)
    QMap<TileKey, QImage> m_tileCache;
    
    bool m_dragging;
    QPoint m_lastMousePos;
    
    QString m_serverHost;
    quint16 m_serverPort;
    
    // Для отображения высоты под курсором
    double m_currentElevation;
    QPoint m_elevationPos;
};

#endif // MAPVIEWER_H
