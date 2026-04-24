#ifndef MBTILESVIEWER_H
#define MBTILESVIEWER_H

#include <QWidget>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QMap>
#include <QPoint>
#include <QSize>
#include <QImage>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QSet>
#include <QDebug>
#include <limits>

// Структура ключа тайла
struct TileKey {
    int z, x, y;

    TileKey() : z(0), x(0), y(0) {}
    TileKey(int _z, int _x, int _y) : z(_z), x(_x), y(_y) {}

    bool operator==(const TileKey &other) const {
        return z == other.z && x == other.x && y == other.y;
    }

    bool operator<(const TileKey &other) const {
        if (z != other.z) return z < other.z;
        if (x != other.x) return x < other.x;
        return y < other.y;
    }
};

// Функция хеширования
inline uint qHash(const TileKey &key, uint seed = 0) {
    return ::qHash(key.z, seed) ^ ::qHash(key.x, seed) ^ ::qHash(key.y, seed);
}

class MBTilesViewer : public QWidget
{
    Q_OBJECT
public:
    explicit MBTilesViewer(QWidget *parent = nullptr);
    ~MBTilesViewer();

    void openFile(const QString &filePath);
    QString getMetadata(const QString &name);
    int getMinZoom();
    int getMaxZoom();
    
    // Проверка загружена ли карта
    bool isMapLoaded() const { return m_dbReady && !m_db.databaseName().isEmpty(); }

signals:
    // Сигнал больше не нужен для синхронной загрузки, но оставим для совместимости если потребуется асинхронность
    void tileLoaded(int z, int x, int y, const QImage& img);
    
    // Сигнал для обновления координат курсора
    void cursorCoordinatesChanged(double longitude, double latitude);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private slots:
    void onTileLoaded(int z, int x, int y, const QImage &img);

private:
    struct TileBounds {
        int minX = std::numeric_limits<int>::max();
        int maxX = std::numeric_limits<int>::min();
        int minY = std::numeric_limits<int>::max();
        int maxY = std::numeric_limits<int>::min();
        bool isValid() const { return minX <= maxX && minY <= maxY; }
    };

    void resetView();
    void updateViewport();
    void loadTile(int z, int x, int y);
    QRectF getVisibleTileRect();
    QPointF tileToPixel(int x, int y); // Убрали z, он не нужен для расчета пикселей
    void scanAvailableTiles();
    
    // Поиск лучшего начального зума и позиции
    void calculateInitialView();
    
    // Конвертация пиксельных координат в географические (долгота/широта)
    void pixelToLonLat(const QPoint& pixelPos, double& longitude, double& latitude);

    QSqlDatabase m_db;
    bool m_dbReady;

    int m_currentZoom;
    int m_minZoom;
    int m_maxZoom;
    int m_tileSize;

    QPointF m_offset;
    QPoint m_lastMousePos;

    QMap<TileKey, QImage> m_tileCache;
    QSet<TileKey> m_availableTiles;

    // Границы доступных тайлов по зумам
    QMap<int, TileBounds> m_zoomBounds;

    QString m_scheme;
};

#endif // MBTILESVIEWER_H
