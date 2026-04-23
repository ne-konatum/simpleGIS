#ifndef MBTILESVIEWER_H
#define MBTILESVIEWER_H

#include <QWidget>
#include <QSqlDatabase>
#include <QMap>
#include <QPair>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>
#include <QScrollArea>

class TileLoader;

class MBTilesViewer : public QWidget
{
    Q_OBJECT

public:
    explicit MBTilesViewer(QWidget *parent = nullptr);
    bool openMBTiles(const QString &filename);
    void paintMapWidget(QPaintEvent *event);
    ~MBTilesViewer();

public slots:
    void openFileDialog();

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void onTileLoaded(int zoom, int x, int y, const QImage& tileImage);

private:
    QSqlDatabase m_database;
    TileLoader *m_tileLoader;
    int m_currentZoom = 0;
    QPointF m_offset;
    QMap<QPair<int, QPair<int, int>>, QPixmap> m_loadedTiles;
    bool m_databaseReady = false;
    int m_minZoom = 0;  // Кэш минимального зума
    int m_maxZoom = 17; // Кэш максимального зума
    const int m_tileSize = 256; // Фиксированный размер тайла

    // UI-элементы
    QPushButton *m_openButton;
    QVBoxLayout *m_layout;
    QScrollArea *m_scrollArea;
    QWidget *m_mapWidget;

    QRectF getVisibleTileRect(int currentZoom, const QSize& viewportSize,
                           const QPointF& offset, int tileSize);
    void updateViewport();
    int getMinZoom() const;
    int getMaxZoom() const;
    bool isTileAvailable(int zoom, int x, int y);
    void loadParentTile(int zoom, int x, int y, int recursionDepth);
    QPointF tileToPixel(int x, int y, int zoom);
    QPair<int, int> pixelToTile(const QPointF &pos, int zoom) const;
    void drawTiles(QPainter *painter);
    void connectTileLoader();
    void setupUI();
    void setupMapWidget();
    void resetView();
    void debugDatabaseContents();
    void debugCoordinateSystem();
    QPair<int, int> findFirstAvailableTile(int zoom);

    // Вспомогательные функции для работы с БД
    bool checkDatabaseStructure();
    bool reopenDatabase();
    void loadVisibleTiles(const QRectF& visibleRect, int zoom);
    QSize m_viewportSize;
    bool ensureDatabaseConnection();
    void loadChildTiles(int zoom, int x, int y);

    // Новые методы для корректной работы с TMS-координатами
    int tmsToGoogleY(int y, int zoom) const;
    int googleToTmsY(int y, int zoom) const;
};

// Вложенный класс MapWidget
class MapWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MapWidget(MBTilesViewer *parent) : QWidget(parent), m_parent(parent) {}

protected:
    void paintEvent(QPaintEvent *event) override
    {
        m_parent->paintMapWidget(event);
    }

private:
    MBTilesViewer *m_parent;
};

#endif // MBTILESVIEWER_H
